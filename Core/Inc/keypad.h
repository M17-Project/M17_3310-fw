#ifndef INC_KEYPAD_H_
#define INC_KEYPAD_H_

#include <stdint.h>
#include <stdlib.h>
#include "keymaps.h"
#include "display.h"
#include "menus.h"
#include "platform.h"
#include "text_entry.h"
#include "../t9/t9.h"

extern void initTextTX(const char *message);
extern void loadDeviceSettings(dev_settings_t *dev_settings, const dev_settings_t *def_dev_settings);
extern void setFreqRF(uint32_t freq, float corr);
extern uint8_t saveData(const void *data, uint16_t size);

extern TIM_HandleTypeDef htim7;		//TIM7 - text entry timer
extern TIM_HandleTypeDef htim14;	//TIM14 - display backlight timeout timer

extern dev_settings_t def_dev_settings;
extern uint8_t menu_pos, menu_pos_hl;
extern uint8_t debug_flag;
extern const char dict[];

extern codeplug_t codeplug;

//text entry related
void resetTextEntry(void);
kbd_key_t scanKeys(radio_state_t radio_state, uint8_t rep);
void pushCharBuffer(abc_t *text_entry, const char key_map[][KEYMAP_COLS], kbd_key_t key);
void pushCharT9(abc_t *text_entry, kbd_key_t key);
void handleKey(disp_dev_t *disp_dev, disp_state_t *disp_state, abc_t *text_entry,
		radio_state_t *radio_state, dev_settings_t *dev_settings,
		kbd_key_t key, edit_set_t *edit_set);
void setKeysTimeout(uint16_t delay);

#endif /* INC_KEYPAD_H_ */
