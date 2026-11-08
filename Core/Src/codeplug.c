//Memory channels
//
//Flash record: [magic][layout:16|len:16][crc32][payload, padded to 4 bytes]
//payload: codeplug_t truncated after the last used channel
//The newest valid record wins, the magic word is programmed last.
#include "codeplug.h"
#include <string.h>
#include <stddef.h>
#include "main.h"
#include "nvmem.h"
#include "debug.h"

#define CP_MAGIC		0x4337314DU		//"M17C"
#define CP_LAYOUT		1U
#define CP_HDR_SIZE		12U
#define CP_END			(CP_FLASH_START + CP_FLASH_SIZE)
#define ALIGN4(x)		(((x) + 3U) & ~3U)
#define CP_SIZE(n)		(offsetof(codeplug_t, channel) + (uint32_t)(n) * sizeof(ch_settings_t))

extern uint32_t _sidata, _sdata, _edata;	//linker script symbols

//built-in defaults (used until something is stored)
static const codeplug_t def_codeplug =
{
	"MyCodeplug",
	2,
	{
		{
			RF_MODE_DIG,		//channel mode (digital or analog)
			RF_BW_12K5,			//channel bandwidth
			RF_PWR_LOW,			//RF power
			"MyChannel 1",		//channel name
			433475000,			//RX frequency
			433475000,			//TX frequency
			"@ALL",				//DST
			0					//CAN
		},
		{
			RF_MODE_DIG,		//channel mode (digital or analog)
			RF_BW_12K5,			//channel bandwidth
			RF_PWR_LOW,			//RF power
			"MyChannel 2",		//channel name
			439425000,			//RX frequency
			431812500,			//TX frequency
			"@ALL",				//DST
			0					//CAN
		}
	}
};

codeplug_t codeplug;

static uint8_t persist;
static uint8_t dirty;

static uint8_t isErased(uint32_t addr, uint32_t len)
{
	for (uint32_t i=0; i<len; i+=4)
		if (*(const uint32_t*)(addr + i) != 0xFFFFFFFFU)
			return 0;

	return 1;
}

//walk the log: newest valid record (0 if none), *next_free: end of the log
static uint32_t scan(uint32_t *next_free)
{
	uint32_t addr = CP_FLASH_START;
	uint32_t newest = 0;

	while (addr + CP_HDR_SIZE <= CP_END)
	{
		const uint32_t *hdr = (const uint32_t*)addr;

		if (hdr[0] != CP_MAGIC)
			break;

		uint16_t len = hdr[1] & 0xFFFF;
		uint32_t rec = CP_HDR_SIZE + ALIGN4(len);

		if (addr + rec > CP_END)
			break;

		const codeplug_t *cp = (const codeplug_t*)(addr + CP_HDR_SIZE);

		if ((hdr[1] >> 16) == CP_LAYOUT && len >= CP_SIZE(0) && len <= sizeof(codeplug_t) &&
			nvCrc32(cp, len) == hdr[2] && cp->num_items <= CH_MAX_NUM && len == CP_SIZE(cp->num_items))
			newest = addr;

		addr += rec;
	}

	if (next_free != NULL)
		*next_free = addr;

	return newest;
}

static void sanitize(void)
{
	if (codeplug.num_items > CH_MAX_NUM)
		codeplug.num_items = CH_MAX_NUM;

	codeplug.name[sizeof(codeplug.name)-1] = 0;

	for (uint16_t i=0; i<codeplug.num_items; i++)
	{
		ch_settings_t *c = &codeplug.channel[i];
		c->ch_name[sizeof(c->ch_name)-1] = 0;
		c->dst[sizeof(c->dst)-1] = 0;
		if (c->can > 15)
			c->can = 0;
	}
}

void codeplugLoad(void)
{
	uint32_t img_end = (uint32_t)&_sidata + ((uint32_t)&_edata - (uint32_t)&_sdata);
	persist = (img_end <= CP_FLASH_START) ? 1 : 0;

	memset(&codeplug, 0, sizeof(codeplug));

	uint32_t rec = persist ? scan(NULL) : 0;

	if (rec)
	{
		memcpy(&codeplug, (const void*)(rec + CP_HDR_SIZE), *(const uint32_t*)(rec + 4) & 0xFFFF);
		dbg_print("[Codeplug] %u channel(s) loaded\n", codeplug.num_items);
	}
	else
	{
		memcpy(&codeplug, &def_codeplug, sizeof(codeplug));
		dbg_print("[Codeplug] Defaults\n");
	}

	sanitize();
	dirty = 0;
}

uint8_t codeplugSave(void)
{
	if (!persist)
	{
		dirty = 0;
		return 1;
	}

	uint32_t len = CP_SIZE(codeplug.num_items);
	uint32_t rec = CP_HDR_SIZE + ALIGN4(len);
	uint32_t addr;
	uint8_t ok = 1;

	scan(&addr);

	if (addr + rec > CP_END || !isErased(addr, rec))
	{
		if (flashEraseSector(CP_FLASH_SECTOR))
			return 1;
		addr = CP_FLASH_START;
	}

	if (HAL_FLASH_Unlock() != HAL_OK)
		return 1;

	ok &= HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + 4, ((uint32_t)CP_LAYOUT << 16) | len) == HAL_OK;
	ok &= HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + 8, nvCrc32(&codeplug, len)) == HAL_OK;
	if (ok)
		ok &= flashProgramWords(addr + CP_HDR_SIZE, &codeplug, len) == 0;
	if (ok)
		ok &= HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, CP_MAGIC) == HAL_OK;

	HAL_FLASH_Lock();
	nvmemBlockingDone();

	if (!ok || memcmp((const void*)(addr + CP_HDR_SIZE), &codeplug, len) != 0)
	{
		dbg_print("[Codeplug] Write error\n");
		return 1;
	}

	dirty = 0;
	dbg_print("[Codeplug] Saved (%u channels)\n", codeplug.num_items);

	return 0;
}

void codeplugTouch(void)
{
	dirty = 1;
}

uint8_t codeplugDirty(void)
{
	return dirty;
}

int16_t codeplugAdd(const ch_settings_t *ch)
{
	if (codeplug.num_items >= CH_MAX_NUM)
		return -1;

	codeplug.channel[codeplug.num_items] = *ch;
	dirty = 1;

	return codeplug.num_items++;
}

uint8_t codeplugDelete(uint16_t idx)
{
	if (idx >= codeplug.num_items)
		return 1;

	memmove(&codeplug.channel[idx], &codeplug.channel[idx + 1],
			(codeplug.num_items - idx - 1) * sizeof(ch_settings_t));
	codeplug.num_items--;
	memset(&codeplug.channel[codeplug.num_items], 0, sizeof(ch_settings_t));
	dirty = 1;

	return 0;
}
