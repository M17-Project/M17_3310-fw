#include "usb_cmds.h"

extern volatile disp_state_t pending_disp_state;

//USB control
void parseUSB(abc_t *text_entry, uint8_t *str, uint32_t len)
{
	//backlight timeout
	//"blt=VALUE"
	if(strncmp((char*)str, "blt=", 4)==0)
	{
		dev_settings.backlight_timer=atoi(strstr((char*)str, "=")+1);
		saveData(&dev_settings, sizeof(dev_settings_t));
		setBacklightTimer(dev_settings.backlight_timer);
	}

	//display backlight
	//"bl=VALUE"
	else if(strncmp((char*)str, "bl=", 3)==0)
	{
		setBacklight(atoi(strstr((char*)str, "=")+1));
	}

	//set frequency
	//"freq=VALUE"
	else if(strncmp((char*)str, "freq=", 5)==0)
	{
		setFreqRF(atoi(strstr((char*)str, "=")+1), dev_settings.freq_corr);
	}

	//read byte from the Flash memory
	//"peek=ADDRESS"
	else if(strncmp((char*)str, "peek=", 5)==0)
	{
		uint32_t addr = MEM_START + atoi(strstr((char*)str, "=")+1);
		dbg_print("%08lX -> 0x%02X\n", addr, *((uint8_t*)addr));
	}

	//write byte to the Flash memory (use with caution)
	//"poke=ADDRESS val=VALUE"
	else if(strncmp((char*)str, "poke=", 5)==0)
	{
		uint32_t addr = MEM_START + atoi(strstr((char*)str, "=")+1);
		uint8_t val = atoi(strstr((char*)str, "val=")+4);

		HAL_FLASH_Unlock();
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr, (uint64_t)val);
		HAL_FLASH_Lock();

		dbg_print("%08lX <- 0x%02X\n", addr, val);
	}

	//load settings from nvmem
	//"load"
	else if(strncmp((char*)str, "load", 4)==0)
	{
		loadDeviceSettings(&dev_settings, &def_dev_settings);
	}

	//erase Flash memory sector
	//"erase"
	else if(strncmp((char*)str, "erase", 5)==0)
	{
		eraseSector();
	}

	//set SRC callsign
	//"src_call=STRING"
	else if(len<=(9+9) && strncmp((char*)str, "src_call=", 9)==0)
	{
		strcpy(dev_settings.src_callsign, strstr((char*)str, "=")+1);
		saveData(&dev_settings, sizeof(dev_settings_t));
	}

	//set frequency correction
	//"freq_corr=VALUE"
	else if(strncmp((char*)str, "f_corr=", 7)==0)
	{
		dev_settings.freq_corr = atof(strstr((char*)str, "=")+1);
		saveData(&dev_settings, sizeof(dev_settings_t));
	}

	//send text message, 200 bytes max for now
	//"msg=STRING"
	else if(len<(4+200) && strncmp((char*)str, "msg=", 4)==0)
	{
		strcpy(text_entry->buffer, strstr((char*)str, "=")+1);
		initTextTX(text_entry->buffer);
	}

	//test: display last text message
	else if(strncmp((char*)str, "disp_msg", 8)==0)
	{
		pending_disp_state = DISP_TEXT_MSG_RCVD;
	}

	//get LSF META field
	else if(strncmp((char*)str, "meta?", 5)==0)
	{
		dbg_print("[Settings] META=");
		for(uint8_t i=0; i<sizeof(lsf_tx.meta); i++)
			dbg_print("%02X", lsf_tx.meta[i]);
		dbg_print(" REF=\"%s\"\n", dev_settings.refl_name);
	}

	//simple echo
	//CDC_Transmit_FS(str, len);
}
