#ifndef INC_KEYMAPS_H_
#define INC_KEYMAPS_H_

#include <stdint.h>

#define KEYMAP_ROWS 11
#define KEYMAP_COLS 15

extern const char key_map_lc[KEYMAP_ROWS][KEYMAP_COLS];
extern const char key_map_uc[KEYMAP_ROWS][KEYMAP_COLS];

extern const uint8_t key_map_len[KEYMAP_ROWS];

#endif /* INC_KEYMAPS_H_ */
