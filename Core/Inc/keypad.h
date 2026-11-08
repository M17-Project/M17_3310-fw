#ifndef INC_KEYPAD_H_
#define INC_KEYPAD_H_

#include <stdint.h>
#include "typedefs.h"
#include "ui_types.h"
#include "text_entry.h"
#include "keymaps.h"

//key scanning
kbd_key_t scanKeys(radio_state_t radio_state, uint16_t rep);
uint8_t keyHeld(void);						//any key down right now (raw)
void setKeysTimeout(uint16_t delay);		//multi-tap timeout, ms

//text entry
void textEntryInit(abc_t *te, const char *initial, uint16_t max_len, uint8_t modes, text_entry_t mode);
uint8_t textEntryKey(abc_t *te, kbd_key_t key);		//keys 1..9, *, 0 - returns 1 if the text changed
uint8_t textEntryBackspace(abc_t *te);				//returns 0 if there was nothing to delete
void textEntryNextMode(abc_t *te);					//# key
void textEntryConfirm(abc_t *te);					//finish the current character / T9 word
void resetTextEntry(void);

#endif /* INC_KEYPAD_H_ */
