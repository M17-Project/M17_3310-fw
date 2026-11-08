#ifndef INC_CODEPLUG_H_
#define INC_CODEPLUG_H_

#include <stdint.h>
#include "settings.h"

//Memory channels, stored in Flash sector 9 (append-only log, like the settings).
//Only the used channels are written, so a record is small and the sector is
//erased rarely.

#define CP_FLASH_START		0x080A0000U
#define CP_FLASH_SIZE		0x20000U
#define CP_FLASH_SECTOR		FLASH_SECTOR_9

extern codeplug_t codeplug;

void codeplugLoad(void);					//stored channels, or the built-in defaults
uint8_t codeplugSave(void);					//0 on success
void codeplugTouch(void);					//mark as changed
uint8_t codeplugDirty(void);

int16_t codeplugAdd(const ch_settings_t *ch);	//index of the new channel, -1 if full
uint8_t codeplugDelete(uint16_t idx);			//0 on success

#endif /* INC_CODEPLUG_H_ */
