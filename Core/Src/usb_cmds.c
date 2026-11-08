#include "usb_cmds.h"
#include "ui.h"
#include "tuning.h"
#include "inbox.h"
#include "msginfo.h"
#include "menus.h"

//USB control
void parseUSB(uint8_t *str, uint32_t len)
{
	//backlight timeout
	//"blt=VALUE"
	if(strncmp((char*)str, "blt=", 4)==0)
	{
		int t = atoi((char*)str+4);
		dev_settings.backlight_timer = (t < 1) ? 1 : (t > 32) ? 32 : t;
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

	//set RF module's register
	//"rfregset=ADDR val=VALUE" (decimal or 0x-prefixed hex)
	else if(strncmp((char*)str, "rfregset=", 9)==0)
	{
		char *v = strstr((char*)str, "val=");

		if (v == NULL)
		{
			dbg_print("[RF module] usage: rfregset=ADDR val=VALUE\n");
		}
		else
		{
			uint8_t addr = (uint8_t)strtoul((char*)str+9, NULL, 0);
			uint16_t val = (uint16_t)strtoul(v+4, NULL, 0);

			uint8_t err = setRegRF(addr, val);

			//don't reload while page 1 may be selected - reloadRF() writes 0x30
			if (addr != 0x7F)
				reloadRF();

			dbg_print("[RF module] %02X <- 0x%04X %s\n", addr, val, err ? "(no OK)" : "OK");
		}
	}

	//load settings from nvmem
	//"load"
	else if(strncmp((char*)str, "load", 4)==0)
	{
		loadDeviceSettings(&dev_settings, &def_dev_settings);
		tuneInit(&dev_settings);
		tuneApply();
		applyDeviceSettings();
		uiRefresh();
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
		copyCallsign(dev_settings.src_callsign, sizeof(dev_settings.src_callsign), (char*)str+9);
		saveData(&dev_settings, sizeof(dev_settings_t));
		uiRefresh();
	}

	//set frequency correction
	//"freq_corr=VALUE"
	else if(strncmp((char*)str, "f_corr=", 7)==0)
	{
		float corr = atof((char*)str+7);
		if (fabsf(corr) <= 50.0f)
		{
			dev_settings.freq_corr = corr;
			setFreqRF(dev_settings.channel.rx_frequency, dev_settings.freq_corr);
			saveData(&dev_settings, sizeof(dev_settings_t));
		}
	}

	//send text message (UTF-8), 200 bytes max for now
	//"msg=STRING"
	else if(len<(4+200) && strncmp((char*)str, "msg=", 4)==0)
	{
		static char msg[200];
		snprintf(msg, sizeof(msg), "%s", (char*)str+4);
		uiSendUtf8(msg, NULL, 0);
	}

	//test: display the newest message
	else if(strncmp((char*)str, "disp_msg", 8)==0)
	{
		uiShowNewestMessage();
	}

	//RF module firmware version and model (queried again)
	//"rfver?"
	else if(strncmp((char*)str, "rfver?", 6)==0)
	{
		readInfoRF();
	}

	//list the inbox with message details
	//"inbox?"
	else if(strncmp((char*)str, "inbox?", 6)==0)
	{
		static char info[400];

		dbg_print("[Inbox] %u message(s), %u unread%s\n", inboxCount(), inboxUnread(),
				inboxPersistent() ? "" : " (not stored)");
		for(uint8_t i=0; i<inboxCount(); i++)
		{
			const inbox_msg_t *m = inboxGet(i);
			msgInfoText(m, info, sizeof(info));
			dbg_print("--- #%u%s, TYPE=%04X\n%s\nText: %s\n", i+1, m->unread ? " (new)" : "",
					m->type, info, m->text);
		}
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
