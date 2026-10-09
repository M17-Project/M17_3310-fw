#include "nvmem.h"
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
#define NV_LAYOUT		1U				//bump when dev_settings_t changes
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
	for (uint8_t i=0; i<4 && i<n; i++)
	{
		uint32_t a = cand[(n - 1 - i) % 4];
		const uint32_t *hdr = (const uint32_t*)a;

		if ((hdr[1] >> 16) == NV_LAYOUT && (hdr[1] & 0xFFFF) == size &&
			crc32((const uint8_t*)(a + NV_HDR_SIZE), size) == hdr[2])
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

//memory
uint8_t eraseSector(void)
{
	if(HAL_FLASH_Unlock()==HAL_OK)
	{
		FLASH_EraseInitTypeDef erase =
		{
			FLASH_TYPEERASE_SECTORS,	//erase sectors
			FLASH_BANK_1,				//bank 1 (the only available, i guess)
			FLASH_SECTOR_11,			//start at sector 11 (last)
			1,							//1 sector to erase
			FLASH_VOLTAGE_RANGE_3,		//2.7 to 3.6V
		};
		uint32_t sector_error=0;

		HAL_StatusTypeDef ret = HAL_FLASHEx_Erase(&erase, &sector_error);

		HAL_FLASH_Lock();
		nvmemBlockingDone();

		if(ret==HAL_OK && sector_error==0xFFFFFFFFU) //successful erase
		{
			dbg_print("[NVMEM] Sector erased.\n");

			return 0;
		}

		dbg_print("[NVMEM] Sector erasure error.\n");

		return 1;
	}

	dbg_print("[NVMEM] Error unlocking Flash memory.\n");

	return 1;
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
	for(uint16_t i=0; i<size && ok; i+=4)
	{
		uint32_t w = 0xFFFFFFFFU;
		uint16_t n = (size - i < 4) ? (size - i) : 4;
		memcpy(&w, &src[i], n);
		ok &= HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr+NV_HDR_SIZE+i, w)==HAL_OK;
	}

	//magic last - validates the record
	if(ok)
		ok &= HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, NV_MAGIC)==HAL_OK;

	HAL_FLASH_Lock();

	//verify
	if(ok && memcmp((const void*)(addr+NV_HDR_SIZE), src, size)!=0)
		ok = 0;

	return ok ? 0 : 1;
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
}

//device settings
void loadDeviceSettings(dev_settings_t *dev_settings, const dev_settings_t *def_dev_settings)
{
	uint32_t rec = nvScan(sizeof(dev_settings_t), NULL);

	//newest valid record
	if(rec)
	{
		memcpy((uint8_t*)dev_settings, (const uint8_t*)(rec + NV_HDR_SIZE), sizeof(dev_settings_t));

		dbg_print("[NVMEM] Device settings loaded.\n");
	}

	//settings stored by an older firmware version (raw struct, no header)
	else if(*((uint32_t*)MEM_START)!=0xFFFFFFFFU && *((uint32_t*)MEM_START)!=NV_MAGIC)
	{
		memcpy((uint8_t*)dev_settings, (uint8_t*)MEM_START, sizeof(dev_settings_t));
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

	//load settings into menus
	//frequency correction
	float v = dev_settings->freq_corr;
	char sign = '+';

	if (v < 0.0f)
	{
	    sign = '-';
	    v = -v;
	}

	int ip = (int)v;
	int fp = (int)((v * 10.0f) + 0.5f) % 10;

	sprintf(displays[DISP_RADIO_SETTINGS].value[0], "%c%d.%dppm", sign, ip, fp);

	//RF power
	if(dev_settings->channel.rf_pwr==RF_PWR_HIGH)
		sprintf(displays[DISP_RADIO_SETTINGS].value[1], "2W");
	else
		sprintf(displays[DISP_RADIO_SETTINGS].value[1], "0.5W");

	//SRC callsign
	snprintf(displays[DISP_M17_SETTINGS].value[0], sizeof(displays[0].value[0]), "%s", dev_settings->src_callsign);

	//destination
	snprintf(displays[DISP_M17_SETTINGS].value[1], sizeof(displays[0].value[0]), "%s", dev_settings->channel.dst);

	//CAN
	sprintf(displays[DISP_M17_SETTINGS].value[2], "%d", dev_settings->channel.can);

	//backlight timeout
	sprintf(displays[DISP_DISPLAY_SETTINGS].value[1], "%ds", dev_settings->backlight_timer);
}
