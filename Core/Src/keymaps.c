#include "keymaps.h"

//lowercase
const char key_map_lc[KEYMAP_ROWS][KEYMAP_COLS] =
{
	".,?!1'\"-+()@/:", //KEY_1
	"abc2", //KEY_2
	"def3", //KEY_3
	"ghi4", //KEY_4
	"jkl5", //KEY_5
	"mno6", //KEY_6
	"pqrs7", //KEY_7
	"tuv8", //KEY_8
	"wxyz9", //KEY_9
	"*=#", //KEY_ASTERISK
	" 0", //KEY_0
};

//uppercase
const char key_map_uc[KEYMAP_ROWS][KEYMAP_COLS] =
{
	".,?!1'\"-+()@/:", //KEY_1
	"ABC2", //KEY_2
	"DEF3", //KEY_3
	"GHI4", //KEY_4
	"JKL5", //KEY_5
	"MNO6", //KEY_6
	"PQRS7", //KEY_7
	"TUV8", //KEY_8
	"WXYZ9", //KEY_9
	"*=#", //KEY_ASTERISK
	" 0", //KEY_0
};

const uint8_t key_map_len[KEYMAP_ROWS] = {14, 4, 4, 4, 4, 4, 5, 4, 5, 3, 2};
