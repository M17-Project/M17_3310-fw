//M17 receiver: syncword search, frame decoding and packet reassembly
#include "m17_rx.h"
#include <string.h>
#include <m17.h>
#include "main.h"
#include "ring.h"
#include "dsp.h"
#include "debug.h"
#include "nvmem.h"

#define SPS				SPS_RX							//samples per symbol
#define SW_LEN			(SYM_PER_SWD*SPS + SPS)			//syncword search window (45 samples)
#define SYNC_THRESH		2.5f							//max squared-L2 distance for a syncword hit
#define PKT_TIMEOUT_MS	500								//drop a partial packet after this much silence

msg_t rcvd_msg;

typedef enum
{
	RX_SYNC_NONE,
	RX_SYNC_LSF,
	RX_SYNC_STR,
	RX_SYNC_PKT
} sync_t;

//syncword search window - a circular buffer stored twice, so that the
//last SW_LEN samples are always contiguous at &sw_buf[sw_w]
static float sw_buf[2*SW_LEN];
static uint8_t sw_w;

static sync_t sync_found;
static uint8_t sample_offset;				//location of the squared-L2 minimum
static float pld_symbs[SYM_PER_PLD];		//payload symbols

//LSF data of the transmission being received
static char lsf_src[10], lsf_dst[10];

//packet reassembly
static char pkt_buf[sizeof(rcvd_msg.text)];
static uint16_t pkt_wr;
static uint8_t pkt_next_fn;
static uint32_t pkt_last_tick;

static void pktReset(void)
{
	pkt_wr = 0;
	pkt_next_fn = 0;
}

void m17RxReset(void)
{
	memset(sw_buf, 0, sizeof(sw_buf));
	sw_w = 0;
	sync_found = RX_SYNC_NONE;
	sample_offset = 0;
	flushBsbFlt();
	pktReset();
}

//how many buffered samples the next processing step needs
uint16_t m17RxSamplesNeeded(void)
{
	if (sync_found == RX_SYNC_NONE)
		return 1;

	return (SYM_PER_PLD-1)*SPS + sample_offset;
}

static float syncDist(const float *win, uint8_t offset, const int8_t *sync)
{
	float d = 0.0f;

	for (uint8_t i=0; i<SYM_PER_SWD; i++)
	{
		float t = win[offset + i*SPS] - (float)sync[i];
		d += t*t;
	}

	return d;
}

//check the window against all syncwords, find the best sampling offset
static sync_t findSync(const float *win, uint8_t *offset)
{
	static const struct
	{
		const int8_t *sync;
		sync_t type;
	} tab[] =
	{
		{lsf_sync_symbols, RX_SYNC_LSF},
		{str_sync_symbols, RX_SYNC_STR},
		{pkt_sync_symbols, RX_SYNC_PKT}
	};

	for (uint8_t k=0; k<sizeof(tab)/sizeof(tab[0]); k++)
	{
		float dist = syncDist(win, 0, tab[k].sync);

		if (dist < SYNC_THRESH)
		{
			//find L2 minimum, search further, up to floor(5/2)=2 samples
			*offset = 0;
			for (uint8_t i=1; i<=SPS/2; i++)
			{
				float d = syncDist(win, i, tab[k].sync);
				if (d < dist)
				{
					*offset = i;
					dist = d;
				}
			}

			return tab[k].type;
		}
	}

	return RX_SYNC_NONE;
}

static void decodeLSF(void)
{
	lsf_t lsf;
	uint32_t e = decode_LSF(&lsf, pld_symbs); //returns viterbi metric 'e'
	uint16_t crc = ((uint16_t)lsf.crc[0]<<8) | lsf.crc[1];

	if (LSF_CRC(&lsf) != crc)
		return;

	uint16_t type = ((uint16_t)lsf.type[0]<<8) | lsf.type[1];
	uint8_t can = (type>>7) & 0xFU;

	decode_callsign_bytes(lsf_dst, lsf.dst);
	decode_callsign_bytes(lsf_src, lsf.src);

	//a new transmission starts with an LSF
	pktReset();

	dbg_print("[Debug] LSF received\n SRC: %s\n DST: %s\n TYPE: %04X\n CAN: %d\n META: ",
			lsf_src, lsf_dst, type, can);
	for (uint8_t i=0; i<sizeof(lsf.meta); i++)
		dbg_print("%02X", lsf.meta[i]);
	dbg_print("\n ERR %.1f\n", (double)((float)e/0xFFFFU));
}

static void decodeStream(void)
{
	uint8_t frame_data[16];
	uint8_t lich[5];
	uint16_t fn;
	uint8_t lich_cnt;

	decode_str_frame(frame_data, lich, &fn, &lich_cnt, pld_symbs);

	dbg_print("(%04X) ", fn);
	for (uint8_t i=0; i<16; i++)
		dbg_print("%02X", frame_data[i]);
	dbg_print("\n");
}

//returns 1 if a complete, CRC-valid text message is now in rcvd_msg
static uint8_t decodePacket(void)
{
	uint8_t frame_data[25] = {0};
	uint8_t eof = 0;
	uint8_t fn = 0;
	uint32_t now = HAL_GetTick();

	decode_pkt_frame(frame_data, &eof, &fn, pld_symbs);

	dbg_print("(%02X) ", fn);
	for (uint8_t i=0; i<25; i++)
		dbg_print("%02X", frame_data[i]);
	dbg_print("\n");

	//stale partial packet?
	if ((uint32_t)(now - pkt_last_tick) > PKT_TIMEOUT_MS)
		pktReset();
	pkt_last_tick = now;

	if (!eof)
	{
		//frames must arrive in order, starting at 0
		if (fn != pkt_next_fn)
		{
			pktReset();
			if (fn != 0)
				return 0;
		}

		if ((uint32_t)pkt_wr + 25U > sizeof(pkt_buf))
		{
			pktReset();
			return 0;
		}

		memcpy(&pkt_buf[pkt_wr], frame_data, 25);
		pkt_wr += 25;
		pkt_next_fn++;

		return 0;
	}

	//last frame: 'fn' holds the number of valid bytes (5-bit field, 0..31)
	uint8_t ok = 0;

	if (fn <= 25 && (uint32_t)pkt_wr + fn < sizeof(pkt_buf))
	{
		memcpy(&pkt_buf[pkt_wr], frame_data, fn);
		uint16_t len = pkt_wr + fn;

		//type (1) + text + null (1) + CRC (2)
		if (len >= 4 && pkt_buf[0] == 0x05 && CRC_M17((uint8_t*)pkt_buf, len) == 0)
		{
			uint16_t text_len = len - 4;

			memcpy(rcvd_msg.text, &pkt_buf[1], text_len);
			rcvd_msg.text[text_len] = 0;
			rcvd_msg.len = text_len;

			//SRC/DST come from the last correctly received LSF
			memcpy(rcvd_msg.src, lsf_src, sizeof(rcvd_msg.src));
			memcpy(rcvd_msg.dst, lsf_dst, sizeof(rcvd_msg.dst));
			rcvd_msg.src[sizeof(rcvd_msg.src)-1] = 0;
			rcvd_msg.dst[sizeof(rcvd_msg.dst)-1] = 0;

			ok = 1;
		}
	}

	pktReset();

	return ok;
}

//process buffered baseband samples
//returns 1 if a new text message was received
uint8_t m17RxProcess(void)
{
	uint8_t new_msg = 0;

	if (demodCheckOverrun())
	{
		dbg_print("[Debug] Baseband buffer overrun!\n");
		sync_found = RX_SYNC_NONE;
	}

	uint16_t avail = demodSamplesGetNum();

	while (avail >= m17RxSamplesNeeded())
	{
		if (sync_found == RX_SYNC_NONE)
		{
			//consume sample
			float x = -fltSample(demodSamplePop());
			avail--;
			sw_buf[sw_w] = x;
			sw_buf[sw_w + SW_LEN] = x;
			if (++sw_w >= SW_LEN)
				sw_w = 0;

			//squared-L2 check against syncwords
			sync_found = findSync(&sw_buf[sw_w], &sample_offset);
			continue;
		}

		//syncword found - gather payload symbols
		const float *win = &sw_buf[sw_w];
		avail -= m17RxSamplesNeeded();

		//the first payload symbol is already in the search window
		pld_symbs[0] = win[SYM_PER_SWD*SPS + sample_offset];
		for (uint8_t i=0; i<sample_offset; i++)
			fltSample(demodSamplePop());

		//the rest: 1 sample per symbol, the remaining SPS-1 only feed the filter
		for (uint16_t i=1; i<SYM_PER_PLD; i++)
		{
			pld_symbs[i] = -fltSample(demodSamplePop());
			for (uint8_t j=0; j<SPS-1; j++)
				fltSample(demodSamplePop());
		}

		//decode stuff based on what it is
		if (sync_found == RX_SYNC_LSF)
			decodeLSF();
		else if (sync_found == RX_SYNC_STR)
			decodeStream();
		else if (decodePacket())
			new_msg = 1;

		//work done: clear old syncword detection buffer
		memset(sw_buf, 0, sizeof(sw_buf));
		sync_found = RX_SYNC_NONE;
	}

	return new_msg;
}

//Flash erase/program stalls the CPU (and the interrupts) while the ADC DMA
//keeps running - the buffered samples cannot be trusted afterwards
void nvmemBlockingDone(void)
{
	demodResync();
	sync_found = RX_SYNC_NONE;
}
