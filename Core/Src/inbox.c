//Message inbox: RAM copy of the newest messages + append-only log in Flash
//
//Flash record (32-bit words):
//  [magic][flags][layout:16|len:16][crc32][payload, padded to 4 bytes]
//  payload: src[10] dst[10] type[2] meta[14] text[len] (no terminator)
//
//The magic word is programmed last (an interrupted write is never valid).
//'flags' stays erased (0xFFFFFFFF) when the record is written; reading and
//deleting a message only clear bits in it, which Flash allows without an
//erase. The sector is erased (and the live messages rewritten) only when the
//log is full - about 140 messages of 900 bytes, many more short ones.
#include "inbox.h"
#include <string.h>
#include "main.h"
#include "nvmem.h"
#include "debug.h"

#define IB_MAGIC		0x4D31374DU		//"M17M"
#define IB_LAYOUT		1U
#define IB_HDR_SIZE		16U
#define IB_FIXED		36U				//src, dst, type, meta
#define IB_END			(INBOX_FLASH_START + INBOX_FLASH_SIZE)
#define ALIGN4(x)		(((x) + 3U) & ~3U)

#define FLAG_UNREAD		(1U<<0)			//cleared: message has been read
#define FLAG_LIVE		(1U<<1)			//cleared: message deleted

extern uint32_t _sidata, _sdata, _edata;	//linker script symbols

static inbox_msg_t pool[INBOX_MAX];
static uint8_t order[INBOX_MAX];			//order[0] = newest, pool indices
static uint8_t count;
static uint16_t used;						//pool slots in use (bitmask)
static uint32_t next_seq = 1;

static uint8_t persist;
static uint32_t log_end;					//first free address in the log
static uint8_t log_dirty;					//garbage behind the log - compact before appending

//--- Flash helpers ---
static uint8_t isErased(uint32_t addr, uint32_t len)
{
	for (uint32_t i=0; i<len; i+=4)
		if (*(const uint32_t*)(addr + i) != 0xFFFFFFFFU)
			return 0;

	return 1;
}

static void clearFlags(uint32_t addr, uint32_t mask)
{
	if (!persist || addr == 0)
		return;

	uint32_t f = *(const uint32_t*)(addr + 4);

	if ((f & mask) == 0)
		return;

	if (HAL_FLASH_Unlock() == HAL_OK)
	{
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + 4, f & ~mask);
		HAL_FLASH_Lock();
	}
	nvmemBlockingDone();
}

static uint16_t payloadLen(const inbox_msg_t *m)
{
	return IB_FIXED + m->len;
}

//write one record at 'addr', returns 0 on success
static uint8_t writeRecord(uint32_t addr, const inbox_msg_t *m)
{
	static uint8_t payload[IB_FIXED + sizeof(m->text)];
	uint16_t len = payloadLen(m);
	uint8_t ok = 1;

	memcpy(&payload[0], m->src, 10);
	memcpy(&payload[10], m->dst, 10);
	payload[20] = m->type >> 8;
	payload[21] = m->type & 0xFF;
	memcpy(&payload[22], m->meta, 14);
	memcpy(&payload[IB_FIXED], m->text, m->len);

	if (HAL_FLASH_Unlock() != HAL_OK)
		return 1;

	ok &= HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + 8, ((uint32_t)IB_LAYOUT << 16) | len) == HAL_OK;
	ok &= HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + 12, nvCrc32(payload, len)) == HAL_OK;
	if (ok)
		ok &= flashProgramWords(addr + IB_HDR_SIZE, payload, len) == 0;
	if (ok && !m->unread)
		ok &= HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + 4, ~FLAG_UNREAD) == HAL_OK;
	if (ok)
		ok &= HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, IB_MAGIC) == HAL_OK;

	HAL_FLASH_Lock();

	if (ok && memcmp((const void*)(addr + IB_HDR_SIZE), payload, len) != 0)
		ok = 0;

	return ok ? 0 : 1;
}

//erase the sector and rewrite all messages kept in RAM (oldest first)
static void compact(void)
{
	dbg_print("[Inbox] Compacting the log\n");

	log_end = INBOX_FLASH_START;
	log_dirty = 0;

	if (flashEraseSector(INBOX_FLASH_SECTOR))
	{
		log_dirty = 1;
		for (uint8_t i=0; i<count; i++)
			pool[order[i]].flash_addr = 0;
		return;
	}

	for (int8_t i=count-1; i>=0; i--)
	{
		inbox_msg_t *m = &pool[order[i]];
		uint32_t rec = IB_HDR_SIZE + ALIGN4(payloadLen(m));

		if (writeRecord(log_end, m) == 0)
		{
			m->flash_addr = log_end;
			log_end += rec;
		}
		else
		{
			m->flash_addr = 0;
			log_dirty = 1;
			break;
		}
	}

	nvmemBlockingDone();
}

static void storeMsg(inbox_msg_t *m)
{
	uint32_t rec = IB_HDR_SIZE + ALIGN4(payloadLen(m));

	m->flash_addr = 0;

	if (!persist)
		return;

	if (log_dirty || log_end + rec > IB_END || !isErased(log_end, rec))
		compact(); //'m' is not in the RAM list yet, so it is not written twice

	if (log_dirty || log_end + rec > IB_END)
		return;

	if (writeRecord(log_end, m) == 0)
	{
		m->flash_addr = log_end;
		log_end += rec;
	}
	else
	{
		log_dirty = 1;
	}

	nvmemBlockingDone();
}

//--- RAM list ---
static void dropOldest(void)
{
	if (count == 0)
		return;

	uint8_t slot = order[--count];
	clearFlags(pool[slot].flash_addr, FLAG_LIVE);
	used &= ~(1U << slot);
}

//take a free pool slot (drops the oldest message if needed)
static inbox_msg_t *allocSlot(uint8_t *slot)
{
	if (count >= INBOX_MAX)
		dropOldest();

	for (uint8_t i=0; i<INBOX_MAX; i++)
	{
		if (!(used & (1U << i)))
		{
			*slot = i;
			memset(&pool[i], 0, sizeof(pool[i]));
			return &pool[i];
		}
	}

	return NULL; //cannot happen
}

static void pushNewest(uint8_t slot)
{
	memmove(&order[1], &order[0], count);
	order[0] = slot;
	count++;
	used |= 1U << slot;
}

//--- public ---
void inboxInit(void)
{
	count = 0;
	used = 0;

	//never touch the sector if the firmware image reaches into it
	uint32_t img_end = (uint32_t)&_sidata + ((uint32_t)&_edata - (uint32_t)&_sdata);
	persist = (img_end <= INBOX_FLASH_START) ? 1 : 0;

	if (!persist)
	{
		dbg_print("[Inbox] Firmware overlaps the inbox sector, messages are not stored\n");
		return;
	}

	uint32_t addr = INBOX_FLASH_START;
	log_dirty = 0;

	while (addr + IB_HDR_SIZE <= IB_END)
	{
		const uint32_t *hdr = (const uint32_t*)addr;

		if (hdr[0] != IB_MAGIC)
		{
			if (hdr[0] != 0xFFFFFFFFU || !isErased(addr, IB_HDR_SIZE))
				log_dirty = 1; //garbage (e.g. power loss during a write)
			break;
		}

		uint16_t len = hdr[2] & 0xFFFF;
		uint32_t rec = IB_HDR_SIZE + ALIGN4(len);

		if (addr + rec > IB_END || len < IB_FIXED || len > IB_FIXED + sizeof(pool[0].text) - 1)
		{
			log_dirty = 1;
			break;
		}

		const uint8_t *p = (const uint8_t*)(addr + IB_HDR_SIZE);

		if ((hdr[2] >> 16) == IB_LAYOUT && (hdr[1] & FLAG_LIVE) && nvCrc32(p, len) == hdr[3])
		{
			uint8_t slot;
			inbox_msg_t *m = allocSlot(&slot);

			memcpy(m->src, &p[0], 10);
			memcpy(m->dst, &p[10], 10);
			m->src[9] = m->dst[9] = 0;
			m->type = ((uint16_t)p[20] << 8) | p[21];
			memcpy(m->meta, &p[22], 14);
			m->len = len - IB_FIXED;
			memcpy(m->text, &p[IB_FIXED], m->len);
			m->text[m->len] = 0;
			m->unread = (hdr[1] & FLAG_UNREAD) ? 1 : 0;
			m->rx_tick = 0;
			m->flash_addr = addr;
			m->seq = next_seq++;

			pushNewest(slot);
		}

		addr += rec;
	}

	log_end = addr;

	dbg_print("[Inbox] %u message(s) loaded, log %lu bytes%s\n", count,
			(unsigned long)(log_end - INBOX_FLASH_START), log_dirty ? " (dirty)" : "");
}

uint8_t inboxAdd(const msg_t *msg)
{
	uint8_t slot;
	inbox_msg_t *m = allocSlot(&slot);

	if (m == NULL)
		return 1;

	memcpy(m->src, msg->src, sizeof(m->src));
	memcpy(m->dst, msg->dst, sizeof(m->dst));
	m->src[sizeof(m->src)-1] = 0;
	m->dst[sizeof(m->dst)-1] = 0;
	m->type = msg->type;
	memcpy(m->meta, msg->meta, sizeof(m->meta));
	m->len = strnlen(msg->text, sizeof(m->text) - 1);
	memcpy(m->text, msg->text, m->len);
	m->text[m->len] = 0;
	m->unread = 1;
	m->rx_tick = HAL_GetTick();
	if (m->rx_tick == 0) //0 means "loaded from Flash"
		m->rx_tick = 1;
	m->seq = next_seq++;

	storeMsg(m);
	pushNewest(slot);

	return 0;
}

uint8_t inboxCount(void)
{
	return count;
}

uint8_t inboxUnread(void)
{
	uint8_t n = 0;

	for (uint8_t i=0; i<count; i++)
		n += pool[order[i]].unread;

	return n;
}

const inbox_msg_t *inboxGet(uint8_t idx)
{
	return (idx < count) ? &pool[order[idx]] : NULL;
}

int16_t inboxFind(uint32_t seq)
{
	for (uint8_t i=0; i<count; i++)
		if (pool[order[i]].seq == seq)
			return i;

	return -1;
}

void inboxMarkRead(uint8_t idx)
{
	if (idx >= count)
		return;

	inbox_msg_t *m = &pool[order[idx]];

	if (m->unread)
	{
		m->unread = 0;
		clearFlags(m->flash_addr, FLAG_UNREAD);
	}
}

void inboxDelete(uint8_t idx)
{
	if (idx >= count)
		return;

	uint8_t slot = order[idx];

	clearFlags(pool[slot].flash_addr, FLAG_LIVE);
	used &= ~(1U << slot);

	memmove(&order[idx], &order[idx + 1], count - idx - 1);
	count--;
}

void inboxClear(void)
{
	while (count)
		inboxDelete(0);
}

uint8_t inboxPersistent(void)
{
	return persist;
}
