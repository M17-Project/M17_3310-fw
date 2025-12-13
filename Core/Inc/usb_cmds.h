#ifndef INC_USB_CMDS_H_
#define INC_USB_CMDS_H_

#include "platform.h"
#include "settings.h"
#include "nvmem.h"
#include "rf_module.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

extern dev_settings_t dev_settings;
extern dev_settings_t def_dev_settings;
extern char text_entry[256];
extern lsf_t lsf_tx;

extern void initTextTX(const char *message);

void parseUSB(uint8_t *str, uint32_t len);

#endif /* INC_USB_CMDS_H_ */
