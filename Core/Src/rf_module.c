#include "rf_module.h"

uint8_t setRegRF(uint8_t reg, uint16_t val)
{
	char data[20], rcv[5]={0};
	uint8_t len;

	len = sprintf(data, "AT+POKE=%d,%d\r\n", reg, val);
	HAL_UART_Transmit(&huart4, (uint8_t*)data, len, 25);
	HAL_UART_Receive(&huart4, (uint8_t*)rcv, 4, 50);

	//dbg_print("[RF module] POKE %02X %04X reply: %s\n", reg, val, rcv);

	return strcmp(rcv, "OK\r\n")==0 ? 0 : 1;
}

uint16_t getRegRF(uint8_t reg)
{
	char data[64], rcv[64]={0};
	uint8_t len;

	len = sprintf(data, "AT+PEEK=%d\r\n", reg);
	HAL_UART_Transmit(&huart4, (uint8_t*)data, len, 25);
	HAL_UART_Receive(&huart4, (uint8_t*)rcv, 64, 10);

	//dbg_print("[RF module] PEEK %02X reply: %s\n", reg, rcv);

	return atoi(rcv);
}

void maskSetRegRF(uint8_t reg, uint16_t mask, uint16_t value)
{
	uint16_t regVal = getRegRF(reg);
	regVal = (regVal & ~mask) | (value & mask);
	setRegRF(reg, regVal);
}

void reloadRF(void)
{
	uint16_t funcMode = getRegRF(0x30) & 0x0060; //Get current op. status
	maskSetRegRF(0x30, 0x0060, 0x0000); //RX and TX off
	maskSetRegRF(0x30, 0x0060, funcMode); //Restore op. status
}

void setFreqRF(uint32_t freq, float corr)
{
	freq = (float)freq * (1.0f + corr/1e6);

	uint32_t val = (freq / 1000.0f) * 16.0f;
	uint16_t fHi = (val >> 16) & 0xFFFF;
	uint16_t fLo = val & 0xFFFF;

	setRegRF(0x29, fHi);
	setRegRF(0x2A, fLo);

	reloadRF();
}

void setRF(radio_state_t state)
{
	//0 for rx, 1 for tx
	maskSetRegRF(0x30, 0x0060, (state+1)<<5);
}

void chBwRF(ch_bw_t bw)
{
	if(bw==RF_BW_12K5)
	{
		setRegRF(0x15, 0x1100);
		setRegRF(0x32, 0x4495);
		setRegRF(0x3A, 0x40C3);
		setRegRF(0x3F, 0x29D1);
		setRegRF(0x3C, 0x1B34);
		setRegRF(0x48, 0x19B1);
		setRegRF(0x60, 0x0F17);
		setRegRF(0x62, 0x1425);
		setRegRF(0x65, 0x2494);
		setRegRF(0x66, 0xEB2E);
		setRegRF(0x7F, 0x0001);
		setRegRF(0x06, 0x0014);
		setRegRF(0x07, 0x020C);
		setRegRF(0x08, 0x0214);
		setRegRF(0x09, 0x030C);
		setRegRF(0x0A, 0x0314);
		setRegRF(0x0B, 0x0324);
		setRegRF(0x0C, 0x0344);
		setRegRF(0x0D, 0x1344);
		setRegRF(0x0E, 0x1B44);
		setRegRF(0x0F, 0x3F44);
		setRegRF(0x12, 0xE0EB);
		setRegRF(0x7F, 0x0000);

		maskSetRegRF(0x30, 0x3000, 0x0000);
	}
	else
	{
		setRegRF(0x15, 0x1F00);
		setRegRF(0x32, 0x7564);
		setRegRF(0x3A, 0x40C3);
		setRegRF(0x3C, 0x1B34);
		setRegRF(0x3F, 0x29D1);
		setRegRF(0x48, 0x1F3C);
		setRegRF(0x60, 0x0F17);
		setRegRF(0x62, 0x3263);
		setRegRF(0x65, 0x248A);
		setRegRF(0x66, 0xFFAE);
		setRegRF(0x7F, 0x0001);
		setRegRF(0x06, 0x0024);
		setRegRF(0x07, 0x0214);
		setRegRF(0x08, 0x0224);
		setRegRF(0x09, 0x0314);
		setRegRF(0x0A, 0x0324);
		setRegRF(0x0B, 0x0344);
		setRegRF(0x0C, 0x0384);
		setRegRF(0x0D, 0x1384);
		setRegRF(0x0E, 0x1B84);
		setRegRF(0x0F, 0x3F84);
		setRegRF(0x12, 0xE0EB);
		setRegRF(0x7F, 0x0000);

		maskSetRegRF(0x30, 0x3000, 0x3000);
	}

	reloadRF(); //reload
}

void setModeRF(rf_mode_t mode)
{
	if(mode==RF_MODE_DIG)
	{
		setRegRF(0x3A, 0x00C2);
		setRegRF(0x33, 0x45F5);
		setRegRF(0x41, 0x4731);
		setRegRF(0x42, 0x1036);
		setRegRF(0x43, 0x00BB);
		setRegRF(0x58, 0xBCFD);	//Bit 0  = 1: CTCSS LPF bandwidth to 250Hz
								//Bit 3  = 1: bypass CTCSS HPF
								//Bit 4  = 1: bypass CTCSS LPF
								//Bit 5  = 1: bypass voice LPF
								//Bit 6  = 1: bypass voice HPF
								//Bit 7  = 1: bypass pre/de-emphasis
								//Bit 11 = 1: bypass VOX HPF
								//Bit 12 = 1: bypass VOX LPF
								//Bit 13 = 1: bypass RSSI LPF
		setRegRF(0x44, 0x06CC);
		setRegRF(0x40, 0x0031);
	}
	else
	{
		;
	}

	reloadRF(); //reload
}

void initRF(dev_settings_t dev_settings)
{
	ch_settings_t ch_settings = dev_settings.channel;

	uint32_t freq = ch_settings.rx_frequency;
	float freq_corr = dev_settings.freq_corr;
	ch_bw_t bw = ch_settings.ch_bw;
	rf_mode_t mode = ch_settings.mode;
	rf_power_t pwr = ch_settings.rf_pwr;

	uint8_t data[64] = {0};

	//PTT off
	HAL_GPIO_WritePin(RF_PTT_GPIO_Port, RF_PTT_Pin, 1);

	//RF power
	if(pwr==RF_PWR_LOW)
		HAL_GPIO_WritePin(RF_PWR_GPIO_Port, RF_PWR_Pin, 1);
	else
		HAL_GPIO_WritePin(RF_PWR_GPIO_Port, RF_PWR_Pin, 0);

	//turn on the module
	HAL_GPIO_WritePin(RF_ENA_GPIO_Port, RF_ENA_Pin, 1);
	HAL_Delay(100);

	HAL_UART_Transmit(&huart4, (uint8_t*)"AT+VERSION\r\n", 12, 20);
	HAL_UART_Receive(&huart4, data, 64, 50); //naїve, blocking

	//display the received data
	uint8_t len=strlen((char*)data);
	if(len)
	{
		data[strlen((char*)data)-2]=0;
		dbg_print("[RF module] Version: %s\n", data);
	}
	else
		dbg_print("[RF module] Nothing received\n");

	//SA868S (AT1846S) init sequence (thx, edgetriggered)
	dbg_print("[RF module] Init sequence start\n");

	setRegRF(0x30, 0x0001);	//Soft reset
	HAL_Delay(160);

	setRegRF(0x30, 0x0004);	//Set pdn_reg (power down pin);

	setRegRF(0x04, 0x0FD0);	//Set clk_mode to 25.6MHz/26MHz
	setRegRF(0x0A, 0x7C20);	//Set 0x0A to its default value
	setRegRF(0x13, 0xA100);
	setRegRF(0x1F, 0x1001);	//Set gpio0 to ctcss_out/css_int/css_cmp
							//and gpio6 to sq, sq&ctcss/cdcss when sq_out_set=1
	setRegRF(0x31, 0x0031);
	setRegRF(0x33, 0x44A5);
	setRegRF(0x34, 0x2B89);
	setRegRF(0x41, 0x4122);	//Set voice_gain_tx (voice digital gain); to 0x22
	setRegRF(0x42, 0x1052);
	setRegRF(0x43, 0x0100);
	setRegRF(0x44, 0x07FF);	//Set gain_tx (voice digital gain after tx ADC downsample); to 0x7
	setRegRF(0x59, (68<<6) | 0x10);	//Set c_dev (CTCSS/CDCSS TX FM deviation); to 0x10
									//and xmitter_dev (voice/subaudio TX FM deviation); to 0x2E
									//original value: 0x0B90 = (0x2E<<6) | 0x10
									//xmitter_dev=68 gives good M17 deviation
	setRegRF(0x47, 0x7F2F);
	setRegRF(0x4F, 0x2C62);
	setRegRF(0x53, 0x0094);
	setRegRF(0x54, 0x2A3C);
	setRegRF(0x55, 0x0081);
	setRegRF(0x56, 0x0B02);
	setRegRF(0x57, 0x1C00);
	setRegRF(0x58, 0x9CDD);	//Bit 0  = 1: CTCSS LPF bandwidth to 250Hz
							//Bit 3  = 1: bypass CTCSS HPF
							//Bit 4  = 1: bypass CTCSS LPF
							//Bit 5  = 0: enable voice LPF
							//Bit 6  = 1: bypass voice HPF
							//Bit 7  = 1: bypass pre/de-emphasis
							//Bit 11 = 1: bypass VOX HPF
							//Bit 12 = 1: bypass VOX LPF
							//Bit 13 = 0: normal RSSI LPF bandwidth
	setRegRF(0x5A, 0x06DB);
	setRegRF(0x63, 0x16AD);
	setRegRF(0x67, 0x0628);	//Set DTMF C0 697Hz to ???
	setRegRF(0x68, 0x05E5);	//Set DTMF C1 770Hz to 13MHz and 26MHz
	setRegRF(0x69, 0x0555);	//Set DTMF C2 852Hz to ???
	setRegRF(0x6A, 0x04B8);	//Set DTMF C3 941Hz to ???
	setRegRF(0x6B, 0x02FE);	//Set DTMF C4 1209Hz to 13MHz and 26MHz
	setRegRF(0x6C, 0x01DD);	//Set DTMF C5 1336Hz
	setRegRF(0x6D, 0x00B1);	//Set DTMF C6 1477Hz
	setRegRF(0x6E, 0x0F82);	//Set DTMF C7 1633Hz
	setRegRF(0x6F, 0x017A);	//Set DTMF C0 2nd harmonic
	setRegRF(0x70, 0x004C);	//Set DTMF C1 2nd harmonic
	setRegRF(0x71, 0x0F1D);	//Set DTMF C2 2nd harmonic
	setRegRF(0x72, 0x0D91);	//Set DTMF C3 2nd harmonic
	setRegRF(0x73, 0x0A3E);	//Set DTMF C4 2nd harmonic
	setRegRF(0x74, 0x090F);	//Set DTMF C5 2nd harmonic
	setRegRF(0x75, 0x0833);	//Set DTMF C6 2nd harmonic
	setRegRF(0x76, 0x0806);	//Set DTMF C7 2nd harmonic

	setRegRF(0x30, 0x40A4);	//Set pdn_pin (power down enable);
							//and set rx_on
							//and set mute when rxno
							//and set xtal_mode to 26MHz/13MHz
	HAL_Delay(160);

	setRegRF(0x30, 0x40A6);	//Start calibration
	HAL_Delay(160);
	setRegRF(0x30, 0x4006);	//Stop calibration
	HAL_Delay(160);

	setRegRF(0x40, 0x0031);

	//set mode
	dbg_print("[RF module] Setting mode to %s\n", (mode==RF_MODE_DIG)?"digital":"analog");
	setModeRF(mode);

	//set bandwidth
	dbg_print("[RF module] Setting bandwidth to %skHz\n", (bw==RF_BW_12K5)?"12.5":"25");
	chBwRF(bw);

	//some additional registers
	setRegRF(0x41, 0x0070); //VOX threshold? was 0x0070
	setRegRF(0x44, 0x00FF); //"RX voice volume", was 0x0022

	//set frequency
	dbg_print("[RF module] Setting frequency to %ldHz (%+d.%dppm)\n",
			freq, (int8_t)freq_corr, (uint8_t)fabsf(10*freq_corr) - (int8_t)fabsf((int8_t)freq_corr*10.0f));
	setFreqRF(freq, freq_corr);
}

void shutdownRF(void)
{
	HAL_GPIO_WritePin(RF_PWR_GPIO_Port, RF_PWR_Pin, 0);
	dbg_print("[RF module] Shutdown\n");
}
