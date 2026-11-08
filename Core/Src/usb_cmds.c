#include "usb_cmds.h"
#include "ui.h"
#include "tuning.h"
#include "inbox.h"
#include "msginfo.h"
#include "menus.h"
#include "codeplug.h"
#include "display.h"

extern disp_dev_t disp_dev;

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

	//list memory channels
	//"ch?"
	else if(strncmp((char*)str, "ch?", 3)==0)
	{
		dbg_print("[Codeplug] %u channel(s)\n", codeplug.num_items);
		for(uint8_t i=0; i<codeplug.num_items; i++)
		{
			const ch_settings_t *c = &codeplug.channel[i];
			dbg_print("%u: \"%s\" RX=%lu TX=%lu DST=%s CAN=%u PWR=%s\n", i+1, c->ch_name,
					(unsigned long)c->rx_frequency, (unsigned long)c->tx_frequency, c->dst, c->can,
					(c->rf_pwr==RF_PWR_HIGH) ? "high" : "low");
		}
	}

	//add a memory channel (frequencies in MHz with a dot, kHz or Hz)
	//"ch_add=NAME,RX,TX[,DST[,CAN]]"  e.g. "ch_add=Kyiv R1,439.4125,431.8125,@ALL,0"
	else if(strncmp((char*)str, "ch_add=", 7)==0)
	{
		char buf[96];
		char *f[5] = {0};
		uint8_t n = 0;

		snprintf(buf, sizeof(buf), "%s", (char*)str+7);
		buf[strcspn(buf, "\r\n")] = 0;
		for(char *p = strtok(buf, ","); p && n < 5; p = strtok(NULL, ","))
			f[n++] = p;

		ch_settings_t ch = dev_settings.ext.vfo;
		uint32_t rx, tx;

		if(n < 3 || parseFreq(f[1], &rx) || parseFreq(f[2], &tx) || !tuneFreqValid(rx) || !tuneFreqValid(tx))
		{
			dbg_print("[Codeplug] usage: ch_add=NAME,RX,TX[,DST[,CAN]]\n");
		}
		else
		{
			snprintf(ch.ch_name, sizeof(ch.ch_name), "%s", f[0]);
			ch.rx_frequency = rx;
			ch.tx_frequency = tx;
			if(n > 3)
				copyCallsign(ch.dst, sizeof(ch.dst), f[3]);
			if(n > 4)
				ch.can = atoi(f[4]) & 0xF;

			int16_t idx = codeplugAdd(&ch);
			if(idx < 0 || codeplugSave())
				dbg_print("[Codeplug] Could not add the channel\n");
			else
				dbg_print("[Codeplug] Channel %d added\n", idx+1);
			uiRefresh();
		}
	}

	//delete a memory channel (1-based)
	//"ch_del=N"
	else if(strncmp((char*)str, "ch_del=", 7)==0)
	{
		int idx = atoi((char*)str+7) - 1;

		if(idx < 0 || codeplugDelete(idx))
		{
			dbg_print("[Codeplug] No such channel\n");
		}
		else
		{
			if(dev_settings.tuning_mode == TUNING_MEM && dev_settings.ch_num > idx)
				dev_settings.ch_num--;
			codeplugSave();
			tuneChannelsChanged();
			saveData(&dev_settings, sizeof(dev_settings_t));
			uiRefresh();
			dbg_print("[Codeplug] Channel %d deleted\n", idx+1);
		}
	}

	//LCD contrast 0..127 (not stored)
	//"contrast=VALUE"
	else if(strncmp((char*)str, "contrast=", 9)==0)
	{
		uint8_t c = atoi((char*)str+9) & 0x7F;
		dispSetContrast(&disp_dev, c);
		dbg_print("[LCD] Vop=%u\n", c);
	}

	//raw LCD command bytes, for testing the controller
	//"lcd=0x21,0xC8,0x20"
	else if(strncmp((char*)str, "lcd=", 4)==0)
	{
		char *p = (char*)str+4;

		while(*p)
		{
			char *end;
			uint8_t b = (uint8_t)strtoul(p, &end, 0);
			if(end == p)
				break;
			dispWrite(&disp_dev, 0, b);
			dbg_print("[LCD] cmd 0x%02X\n", b);
			p = (*end == ',') ? end+1 : end;
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
