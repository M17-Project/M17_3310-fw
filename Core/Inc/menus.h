#ifndef INC_MENUS_H_
#define INC_MENUS_H_

#include <stdint.h>
#include "typedefs.h"
#include "settings.h"
#include "rf_module.h"
#include "nvmem.h"
#include "ui_types.h"
#include "text_entry.h"
#include "version.h"

typedef struct disp_dev_t disp_dev_t;

extern disp_t displays[13];
extern disp_dev_t disp_dev;

void loadMenuValues(disp_state_t state, dev_settings_t *dev_settings);
void enterState(disp_dev_t *disp_dev, disp_state_t state, abc_t *text_entry,
		dev_settings_t *dev_settings);
void leaveState(disp_state_t state, char *text_entry, dev_settings_t *dev_settings,
		edit_set_t edit_set, radio_state_t *radio_state);

#endif
