#ifndef INC_USB_CMDS_H_
#define INC_USB_CMDS_H_

#include "platform.h"
#include "settings.h"
#include "nvmem.h"
#include "rf_module.h"
#include "text_entry.h"
#include <stdint.h>
#include <m17.h>

extern dev_settings_t dev_settings;
extern dev_settings_t def_dev_settings;
extern lsf_t lsf_tx;

extern void initTextTX(const char *message);

void parseUSB(abc_t *text_entry, uint8_t *str, uint32_t len);

#endif /* INC_USB_CMDS_H_ */
