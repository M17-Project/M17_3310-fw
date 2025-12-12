#ifndef INC_KEYPAD_H_
#define INC_KEYPAD_H_

#include <stdint.h>
#include <stdlib.h>
#include "macros.h"
#include "typedefs.h"
#include "keymaps.h"
#include "display.h"
#include "../t9/t9.h"
#include "main.h"

extern void initTextTX(const char *message);
extern void loadDeviceSettings(dev_settings_t *dev_settings, const dev_settings_t *def_dev_settings);
extern void setFreqRF(uint32_t freq, float corr);
extern uint8_t saveData(const void *data, uint16_t size);

extern TIM_HandleTypeDef htim7;		//TIM7 - text entry timer
extern TIM_HandleTypeDef htim14;	//TIM14 - display backlight timeout timer

extern dev_settings_t def_dev_settings;
extern uint8_t menu_pos, menu_pos_hl;
extern uint8_t debug_tx;
extern uint8_t pos;
extern const char dict[];

//T9 related
const char *addCode(char *code, char symbol);
kbd_key_t scanKeys(radio_state_t radio_state, uint8_t rep);
void pushCharBuffer(char *text_entry, text_entry_t text_mode, const char key_map[][KEYMAP_COLS], kbd_key_t key);
void pushCharT9(char *text_entry, char *code, kbd_key_t key);
void handleKey(disp_dev_t *disp_dev, disp_state_t *disp_state, char *text_entry, char *code,
		text_entry_t *text_mode, radio_state_t *radio_state, dev_settings_t *dev_settings,
		kbd_key_t key, edit_set_t *edit_set);
void setKeysTimeout(const uint16_t delay);

#endif /* INC_KEYPAD_H_ */
