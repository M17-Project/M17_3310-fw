#ifndef INC_NVMEM_H_
#define INC_NVMEM_H_

#include <stdint.h>
#include "settings.h"
#include "typedefs.h"
#include "debug.h"
#include "main.h"

#include <stddef.h>

#define MEM_START	(0x080E0000U)	//last sector, 128kB
#define MEM_SIZE	(0x20000U)

void nvmemBlockingDone(void);
uint8_t eraseSector(void);
uint8_t flashEraseSector(uint32_t sector);
uint8_t flashProgramWords(uint32_t addr, const void *data, uint32_t size);
uint32_t nvCrc32(const void *data, uint32_t len);
uint8_t saveNeedsErase(uint16_t size);
void copyCallsign(char *dst, size_t dst_size, const char *src);

uint8_t saveData(const void *data, uint16_t size);
void loadDeviceSettings(dev_settings_t *dev_settings, const dev_settings_t *def_dev_settings);

#endif /* INC_NVMEM_H_ */
