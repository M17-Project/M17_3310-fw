#ifndef INC_NVMEM_H_
#define INC_NVMEM_H_

#include <stdint.h>
#include "settings.h"
#include "menus.h"
#include "typedefs.h"
#include "debug.h"
#include "main.h"

#define MEM_START	(0x080E0000U)	//last sector, 128kB

uint8_t eraseSector(void);

uint8_t saveData(const void *data, uint16_t size);
void loadDeviceSettings(dev_settings_t *dev_settings, const dev_settings_t *def_dev_settings);

#endif /* INC_NVMEM_H_ */
