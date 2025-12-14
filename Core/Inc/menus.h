//--------------------------------------------------------------------
// 3310 menus/displays state machine related stuff - menus.h
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 7 December 2025
//--------------------------------------------------------------------
#ifndef INC_MENUS_H_
#define INC_MENUS_H_

#include <stdint.h>
#include <stdlib.h>
#include <m17.h>
#include "typedefs.h"
#include "settings.h"
#include "rf_module.h"
#include "nvmem.h"
#include "ui_types.h"
#include "version.h"

typedef struct disp_dev_t disp_dev_t;

extern disp_t displays[13];
extern uint8_t pos;

void enterState(disp_dev_t *disp_dev, disp_state_t state, text_entry_t text_mode,
		char *text_entry, char *code);
void leaveState(disp_state_t state, char *text_entry, dev_settings_t *dev_settings,
		edit_set_t edit_set, radio_state_t *radio_state);

#endif
