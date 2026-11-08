#include "nvmem.h"
#include "tuning.h"
#include "ringtones.h"
#include <string.h>
#include <stdio.h>

//Settings are stored as an append-only log of records in Flash sector 11.
//A new record is written behind the last one, and the sector is erased only
//when it is full (or damaged). The newest valid record wins.
//
//record: [magic][layout:16|len:16][crc32][payload, padded to 4 bytes]
//The magic word is programmed last, so a record interrupted by a power loss
//is never mistaken for a valid one.

#define NV_MAGIC		0x5337314DU		//"M17S"
#define NV_LAYOUT		2U				//bump when dev_settings_t changes
#define NV_V1_SIZE		144U			//sizeof(dev_settings_t) in layout 1

//New fields are appended (dev_settings_ext_t), so a layout 1 record is
//simply copied over the defaults. This only works if 'ext' starts right
//behind the old structure:
_Static_assert(offsetof(dev_settings_t, ext) == NV_V1_SIZE, "dev_settings_t layout 1 changed");
#define NV_HDR_SIZE		12U
#define NV_END			(MEM_START + MEM_SIZE)
#define ALIGN4(x)		(((x) + 3U) & ~3U)

//called after Flash operations; the CPU stalls during an erase,
//so the RX code has to drop its (now stale) baseband buffer
__attribute__((weak)) void nvmemBlockingDone(void)
{
	;
}

static uint32_t crc32(const uint8_t *data, uint32_t len)
{
	uint32_t crc = 0xFFFFFFFFU;

	while (len--)
	{
		crc ^= *data++;
		for (uint8_t i=0; i<8; i++)
			crc = (crc >> 1) ^ (0xEDB88320U & (0U - (crc & 1U)));
	}

	return ~crc;
}

//walk the log: returns the address of the newest valid record (0 if none),
//*next_free gets the address behind the last record
static uint32_t nvScan(uint16_t size, uint32_t *next_free)
{
	uint32_t addr = MEM_START;
	uint32_t cand[4] = {0};					//newest record candidates
	uint8_t n = 0;

	while (addr + NV_HDR_SIZE <= NV_END)
	{
		const uint32_t *hdr = (const uint32_t*)addr;

		if (hdr[0] != NV_MAGIC) //erased (end of log) or garbage
			break;

		uint16_t len = hdr[1] & 0xFFFF;
		uint32_t rec = NV_HDR_SIZE + ALIGN4(len);

		if (addr + rec > NV_END)
			break;

		cand[n++ % 4] = addr;
		addr += rec;
	}

	if (next_free != NULL)
		*next_free = addr;

	//verify the newest records only, newest first
	//(accepts the current layout and layout 1, which is migrated on load)
	for (uint8_t i=0; i<4 && i<n; i++)
	{
		uint32_t a = cand[(n - 1 - i) % 4];
		const uint32_t *hdr = (const uint32_t*)a;
		uint16_t layout = hdr[1] >> 16;
		uint16_t len = hdr[1] & 0xFFFF;

		if (((layout == NV_LAYOUT && len == size) || (layout == 1U && len == NV_V1_SIZE)) &&
			crc32((const uint8_t*)(a + NV_HDR_SIZE), len) == hdr[2])
			return a;
	}

	return 0;
}

static uint8_t isErased(uint32_t addr, uint32_t len)
{
	for (uint32_t i=0; i<len; i+=4)
	{
		if (*(const uint32_t*)(addr + i) != 0xFFFFFFFFU)
			return 0;
	}

	return 1;
}

//erase a Flash sector (blocks the CPU for up to ~2 s)
uint8_t flashEraseSector(uint32_t sector)
{
	if(HAL_FLASH_Unlock()==HAL_OK)
	{
		FLASH_EraseInitTypeDef erase =
		{
			FLASH_TYPEERASE_SECTORS,	//erase sectors
			FLASH_BANK_1,				//bank 1 (the only available, i guess)
			sector,						//sector to erase
			1,							//1 sector to erase
			FLASH_VOLTAGE_RANGE_3,		//2.7 to 3.6V
		};
		uint32_t sector_error=0;

		HAL_StatusTypeDef ret = HAL_FLASHEx_Erase(&erase, &sector_error);

		HAL_FLASH_Lock();
		nvmemBlockingDone();

		if(ret==HAL_OK && sector_error==0xFFFFFFFFU) //successful erase
		{
			dbg_print("[NVMEM] Sector %lu erased.\n", (unsigned long)sector);

			return 0;
		}

		dbg_print("[NVMEM] Sector erasure error.\n");

		return 1;
	}

	dbg_print("[NVMEM] Error unlocking Flash memory.\n");

	return 1;
}

//settings sector
uint8_t eraseSector(void)
{
	return flashEraseSector(FLASH_SECTOR_11);
}

//program 32-bit words (Flash must be unlocked by the caller)
uint8_t flashProgramWords(uint32_t addr, const void *data, uint32_t size)
{
	const uint8_t *src = (const uint8_t*)data;

	for(uint32_t i=0; i<size; i+=4)
	{
		uint32_t w = 0xFFFFFFFFU;
		uint32_t n = (size - i < 4) ? (size - i) : 4;
		memcpy(&w, &src[i], n);
		if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr+i, w)!=HAL_OK)
			return 1;
	}

	return 0;
}

uint32_t nvCrc32(const void *data, uint32_t len)
{
	return crc32((const uint8_t*)data, len);
}

static uint8_t writeRecord(uint32_t addr, const void *data, uint16_t size)
{
	const uint8_t *src = (const uint8_t*)data;
	uint8_t ok = 1;

	if(HAL_FLASH_Unlock()!=HAL_OK)
		return 1;

	//header (without the magic word)
	ok &= HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr+4, ((uint32_t)NV_LAYOUT<<16) | size)==HAL_OK;
	ok &= HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr+8, crc32(src, size))==HAL_OK;

	//payload, word by word (the last word is padded with 0xFF)
	if(ok)
		ok &= flashProgramWords(addr+NV_HDR_SIZE, src, size)==0;

	//magic last - validates the record
	if(ok)
		ok &= HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, NV_MAGIC)==HAL_OK;

	HAL_FLASH_Lock();

	//verify
	if(ok && memcmp((const void*)(addr+NV_HDR_SIZE), src, size)!=0)
		ok = 0;

	return ok ? 0 : 1;
}

//1 if the next saveData() call has to erase the sector first (slow)
uint8_t saveNeedsErase(uint16_t size)
{
	uint32_t rec = NV_HDR_SIZE + ALIGN4(size);
	uint32_t addr;

	nvScan(size, &addr);

	return (addr + rec > NV_END || !isErased(addr, rec)) ? 1 : 0;
}

uint8_t saveData(const void *data, uint16_t size)
{
	uint32_t rec = NV_HDR_SIZE + ALIGN4(size);
	uint32_t addr;

	if(rec > MEM_SIZE)
		return 1;

	nvScan(size, &addr);

	//no room left, or the free space is not clean (e.g. old format, power loss)
	if(addr + rec > NV_END || !isErased(addr, rec))
	{
		if(eraseSector())
			return 1;
		addr = MEM_START;
	}

	if(writeRecord(addr, data, size))
	{
		//retry once on a freshly erased sector
		if(eraseSector() || writeRecord(MEM_START, data, size))
		{
			dbg_print("[NVMEM] Write error.\n");
			nvmemBlockingDone();
			return 1;
		}
	}

	nvmemBlockingDone();
	dbg_print("[NVMEM] Data saved.\n");

	return 0;
}

//copy a callsign, at most 9 characters (M17 address limit)
void copyCallsign(char *dst, size_t dst_size, const char *src)
{
	snprintf(dst, dst_size, "%.9s", src);
}

//make sure that loaded data cannot break anything
static void sanitizeSettings(dev_settings_t *s)
{
	s->src_callsign[sizeof(s->src_callsign)-1] = 0;
	s->welcome_msg[0][sizeof(s->welcome_msg[0])-1] = 0;
	s->welcome_msg[1][sizeof(s->welcome_msg[1])-1] = 0;
	s->channel.ch_name[sizeof(s->channel.ch_name)-1] = 0;
	s->channel.dst[sizeof(s->channel.dst)-1] = 0;
	s->refl_name[sizeof(s->refl_name)-1] = 0;

	if(s->backlight_timer > 32)
		s->backlight_timer = 32;
	if(s->channel.can > 15)
		s->channel.can = 0;
	if(!(s->freq_corr >= -50.0f && s->freq_corr <= 50.0f)) //also catches NaN
		s->freq_corr = 0.0f;

	//layout 2
	s->ext.vfo.ch_name[sizeof(s->ext.vfo.ch_name)-1] = 0;
	s->ext.vfo.dst[sizeof(s->ext.vfo.dst)-1] = 0;
	if(s->ext.vfo.can > 15)
		s->ext.vfo.can = 0;
	if(!tuneFreqValid(s->ext.vfo.rx_frequency))
		s->ext.vfo.rx_frequency = 433475000U;
	if(!tuneFreqValid(s->ext.vfo.tx_frequency))
		s->ext.vfo.tx_frequency = s->ext.vfo.rx_frequency;
	if(s->tuning_mode != TUNING_VFO && s->tuning_mode != TUNING_MEM)
		s->tuning_mode = TUNING_VFO;
	if(s->ext.vfo_step == 0 || s->ext.vfo_step > 1000000U)
		s->ext.vfo_step = VFO_STEP_DEFAULT;
	if(s->ext.splash_time > 10)
		s->ext.splash_time = 2;
	if(s->ext.contrast > 127)
		s->ext.contrast = 0x48;
	if(s->ext.ringtone >= ringtones_num)
		s->ext.ringtone = 1;
	s->ext.vibration = s->ext.vibration ? 1 : 0;
	s->ext.ukrainian = s->ext.ukrainian ? 1 : 0;
	if(s->kbd_timeout < 100 || s->kbd_timeout > 3000)
		s->kbd_timeout = 750;
	if(s->kbd_delay < 50 || s->kbd_delay > 1000)
		s->kbd_delay = 150;
}

//layout 1 -> current: the old structure is a prefix of the new one
static void migrateSettings(dev_settings_t *s, const void *old, uint16_t old_len,
		const dev_settings_t *defaults)
{
	memcpy(s, defaults, sizeof(dev_settings_t));
	memcpy(s, old, old_len);

	//the old active channel becomes the VFO
	s->ext.vfo = s->channel;
}

//device settings
void loadDeviceSettings(dev_settings_t *dev_settings, const dev_settings_t *def_dev_settings)
{
	uint32_t rec = nvScan(sizeof(dev_settings_t), NULL);

	//newest valid record
	if(rec)
	{
		const uint32_t *hdr = (const uint32_t*)rec;

		if((hdr[1] >> 16) == NV_LAYOUT)
		{
			memcpy((uint8_t*)dev_settings, (const uint8_t*)(rec + NV_HDR_SIZE), sizeof(dev_settings_t));
			dbg_print("[NVMEM] Device settings loaded.\n");
		}
		else
		{
			migrateSettings(dev_settings, (const void*)(rec + NV_HDR_SIZE), hdr[1] & 0xFFFF, def_dev_settings);
			sanitizeSettings(dev_settings);
			saveData(dev_settings, sizeof(dev_settings_t));
			dbg_print("[NVMEM] Device settings migrated from layout %lu.\n", (unsigned long)(hdr[1] >> 16));
		}
	}

	//settings stored by an older firmware version (raw struct, no header)
	else if(*((uint32_t*)MEM_START)!=0xFFFFFFFFU && *((uint32_t*)MEM_START)!=NV_MAGIC)
	{
		migrateSettings(dev_settings, (const void*)MEM_START, NV_V1_SIZE, def_dev_settings);
		sanitizeSettings(dev_settings);
		saveData(dev_settings, sizeof(dev_settings_t)); //convert

		dbg_print("[NVMEM] Device settings converted to the new format.\n");
	}

	//if the memory is uninitialized (or no record is valid)
	else
	{
		memcpy((uint8_t*)dev_settings, (uint8_t*)def_dev_settings, sizeof(dev_settings_t));
		uint8_t ret = saveData(def_dev_settings, sizeof(dev_settings_t));

		if(ret==0)
			dbg_print("[NVMEM] Default device settings loaded.\n");
		else
			dbg_print("[NVMEM] Error saving default device settings.\n");
	}

	sanitizeSettings(dev_settings);
}
