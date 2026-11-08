#ifndef INC_INBOX_H_
#define INC_INBOX_H_

#include <stdint.h>
#include "typedefs.h"

//Received text messages. The newest INBOX_MAX messages are kept in RAM and
//mirrored to Flash sector 10 (see inbox.c), so they survive a power cycle.

#define INBOX_MAX			16

//Flash log: sector 10, 128 kB, right below the settings sector
#define INBOX_FLASH_START	0x080C0000U
#define INBOX_FLASH_SIZE	0x20000U
#define INBOX_FLASH_SECTOR	FLASH_SECTOR_10

typedef struct
{
	uint32_t seq;			//unique, increasing (stable message handle)
	uint32_t rx_tick;		//HAL_GetTick() at reception, 0 if loaded from Flash
	uint32_t flash_addr;	//record address, 0 if not stored
	uint8_t unread;
	char src[10];
	char dst[10];
	uint16_t type;			//LSF TYPE
	uint8_t meta[14];		//LSF META
	uint16_t len;			//text length (bytes)
	char text[sizeof(((msg_t*)0)->text)];	//UTF-8, null-terminated
} inbox_msg_t;

void inboxInit(void);						//load the stored messages
uint8_t inboxAdd(const msg_t *msg);			//0 on success (the oldest message is dropped if full)

uint8_t inboxCount(void);
uint8_t inboxUnread(void);
const inbox_msg_t *inboxGet(uint8_t idx);	//0 = newest, NULL if out of range
int16_t inboxFind(uint32_t seq);			//index of a message, -1 if gone

void inboxMarkRead(uint8_t idx);
void inboxDelete(uint8_t idx);
void inboxClear(void);

uint8_t inboxPersistent(void);				//0 if the Flash log is disabled

#endif /* INC_INBOX_H_ */
