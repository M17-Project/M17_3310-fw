#ifndef INC_KEYMAPS_H_
#define INC_KEYMAPS_H_

#include <stdint.h>
#include "typedefs.h"

#define KEYMAP_ROWS 11
#define KEYMAP_COLS 15

extern const char key_map_lc[KEYMAP_ROWS][KEYMAP_COLS];
extern const char key_map_uc[KEYMAP_ROWS][KEYMAP_COLS];
extern const char key_map_num[KEYMAP_ROWS][KEYMAP_COLS];
extern const char key_map_ukr_lc[KEYMAP_ROWS][KEYMAP_COLS];
extern const char key_map_ukr_uc[KEYMAP_ROWS][KEYMAP_COLS];

const char *keymapRow(text_entry_t mode, kbd_key_t key);

#endif /* INC_KEYMAPS_H_ */
