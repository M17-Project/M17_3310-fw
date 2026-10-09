//M17 packet transmitter
//
//The DAC plays frame_samples[] in circular DMA mode, one 40 ms frame per half.
//Each half/full-transfer interrupt marks the half that was just played as free
//and the main loop refills it with the next frame:
//
//  frames 0..P-1   preamble
//  frame  P        LSF
//  frames P+1..P+N packet frames
//  frame  P+N+1    EOT
//  then one idle half, so that the EOT is played completely before unkeying
#include "m17_tx.h"
#include <string.h>
#include "main.h"
#include "dsp.h"
#include "ring.h"
#include "rf_module.h"
#include "m17_rx.h"
#include "debug.h"

extern DAC_HandleTypeDef hdac;
extern TIM_HandleTypeDef htim6;
extern radio_state_t radio_state;
extern dev_settings_t dev_settings;

lsf_t lsf_tx;

static uint16_t frame_samples[2][SYM_PER_FRA*SPS_TX];
static uint8_t packet_payload[33*25];
static uint16_t packet_bytes;
static uint8_t num_pkt_frames;

static uint8_t next_frame;					//next frame index to generate
static uint8_t total_frames;				//preamble + LSF + packet frames + EOT
static uint32_t tx_start_tick;
static volatile uint8_t tx_active;
static volatile uint8_t refill_pend;		//a DMA half is waiting for new samples
static volatile uint8_t free_half;			//which one
static volatile uint16_t underruns;

//DAC output -> idle level, then stop its trigger timer (TIM6) to save power
static void dacIdle(void)
{
	HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
	HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, DAC_IDLE);

	//the DAC is triggered by TIM6 - wait for one trigger to latch the value
	HAL_TIM_Base_Start(&htim6);
	uint32_t t0 = HAL_GetTick();
	while ((DAC->DOR1 & 0xFFFU) != DAC_IDLE && (HAL_GetTick() - t0) < 2U)
		;
	HAL_TIM_Base_Stop(&htim6);
}

void m17TxInit(void)
{
	dacIdle();
}

static void genFrame(uint8_t idx, uint16_t *out)
{
	int8_t symbols[SYM_PER_FRA];
	uint32_t cnt = 0;

	if (idx < TX_PREAMBLE_FRAMES)
	{
		gen_preamble_i8(symbols, &cnt, PREAM_LSF);
	}
	else if (idx == TX_PREAMBLE_FRAMES)
	{
		gen_frame_i8(symbols, NULL, FRAME_LSF, &lsf_tx, 0, 0);
	}
	else if (idx <= TX_PREAMBLE_FRAMES + num_pkt_frames)
	{
		uint8_t index = idx - TX_PREAMBLE_FRAMES - 1;
		uint16_t off = (uint16_t)index * 25;
		uint16_t remaining = packet_bytes - off;
		uint8_t payload[26] = {0};				//zero padding in the last frame

		memcpy(payload, &packet_payload[off], (remaining > 25) ? 25 : remaining);
		if (remaining > 25)
			payload[25] = index << 2;
		else
			payload[25] = 0x80 | (remaining << 2);

		gen_frame_i8(symbols, payload, FRAME_PKT, &lsf_tx, 0, 0);
	}
	else if (idx == total_frames - 1)
	{
		gen_eot_i8(symbols, &cnt);
	}
	else
	{
		//idle
		for (uint16_t i=0; i<SYM_PER_FRA*SPS_TX; i++)
			out[i] = DAC_IDLE;
		return;
	}

	fltSymbolsPoly(out, symbols, rrc_taps_10_poly, 0);
}

//returns 0 on success, -1 if busy, -2 on invalid callsigns
int8_t m17TxStart(const char *message, const dev_settings_t *ds)
{
	if (tx_active || radio_state != RF_RX)
		return -1;

	//LSF - rebuilt every time, so that new settings are used immediately
	if (set_LSF(&lsf_tx, ds->src_callsign, ds->channel.dst,
			M17_TYPE_PACKET | M17_TYPE_CAN(ds->channel.can), NULL) != 0)
		return -2;

	//payload: type, text, null terminator, CRC
	uint16_t msg_len = strlen(message);
	if (msg_len > SMS_MAX_LEN)
		msg_len = SMS_MAX_LEN;

	packet_payload[0] = 0x05; //packet type: SMS
	memcpy(&packet_payload[1], message, msg_len);
	packet_payload[msg_len+1] = 0; //null termination

	uint16_t crc = CRC_M17(packet_payload, 1+msg_len+1);
	packet_payload[msg_len+2] = crc>>8;
	packet_payload[msg_len+3] = crc&0xFF;

	packet_bytes = 1+msg_len+1+2;
	num_pkt_frames = (packet_bytes + 24) / 25;
	total_frames = TX_PREAMBLE_FRAMES + 1 + num_pkt_frames + 1;

	//stop baseband sampling
	bsbRxStop();

	//tune for TX while still unkeyed (avoids seconds of dead carrier)
	setFreqRF(ds->channel.tx_frequency, ds->freq_corr);
	chBwRF(ds->channel.ch_bw);

	//both DMA halves must hold valid frames before the DMA starts
	flushTxFlt();
	genFrame(0, frame_samples[0]);
	genFrame(1, frame_samples[1]);
	next_frame = 2;
	refill_pend = 0;
	underruns = 0;

	//key up and start
	radio_state = RF_TX;
	tx_active = 1;
	tx_start_tick = HAL_GetTick();
	setRF(RF_TX);
	//TODO: RF PTT line should work
	//HAL_GPIO_WritePin(RF_PTT_GPIO_Port, RF_PTT_Pin, 0);

	HAL_TIM_Base_Start(&htim6);
	HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)&frame_samples[0][0],
			2*SYM_PER_FRA*SPS_TX, DAC_ALIGN_12B_R);

	return 0;
}

static void m17TxStop(void)
{
	HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
	tx_active = 0;
	refill_pend = 0;

	//unkey first
	setRF(RF_RX);
	//TODO: RF PTT line should work
	//HAL_GPIO_WritePin(RF_PTT_GPIO_Port, RF_PTT_Pin, 1);
	radio_state = RF_RX;

	dacIdle();

	//back to the RX frequency, wide RX filter
	setFreqRF(dev_settings.channel.rx_frequency, dev_settings.freq_corr);
	chBwRF(RF_BW_25K); //TODO: this is a workaround

	m17RxReset();
	bsbRxStart();

	if (underruns)
		dbg_print("[Debug] TX underruns: %u\n", underruns);
}

//main loop part - returns 1 when the transmission has just ended
uint8_t m17TxProcess(void)
{
	if (!tx_active)
		return 0;

	if ((uint32_t)(HAL_GetTick() - tx_start_tick) > TX_TIMEOUT_MS)
	{
		dbg_print("[Debug] TX timeout\n");
		m17TxStop();
		return 1;
	}

	if (!refill_pend)
		return 0;

	uint8_t half = free_half;
	refill_pend = 0;

	if (next_frame <= total_frames) //last one is the idle frame
	{
		genFrame(next_frame, frame_samples[half]);
		next_frame++;
		return 0;
	}

	//the EOT has been played completely
	m17TxStop();
	return 1;
}

uint8_t m17TxActive(void)
{
	return tx_active;
}

uint8_t m17TxRefillPending(void)
{
	return refill_pend;
}

//DAC DMA interrupt: half_done = the half that has just been played
void m17TxDmaCallback(uint8_t half_done)
{
	if (!tx_active)
		return;

	if (refill_pend)
		underruns++;

	free_half = half_done;
	refill_pend = 1;
}
