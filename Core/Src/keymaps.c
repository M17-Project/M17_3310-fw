#include "keymaps.h"
#include <string.h>

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

//numbers: one character per key ('*' cycles the symbols needed for values)
const char key_map_num[KEYMAP_ROWS][KEYMAP_COLS] =
{
	"1", "2", "3", "4", "5", "6", "7", "8", "9",
	".-+", //KEY_ASTERISK
	"0", //KEY_0
};

//The Cyrillic letters are stored in the internal charset (Windows-1251 codes,
//see charset.h), hence the escapes:
//  2 ABVHG  3 DEYeZhZ  4 YIYiY  5 KLMN  6 OPRS  7 TUFKh  8 TsChShShch  9 SoftYuYa
//(the apostrophe is on key 1)
//Ukrainian, lowercase (DSTU layout)
const char key_map_ukr_lc[KEYMAP_ROWS][KEYMAP_COLS] =
{
	".,?!1'\"-+()@/:", //KEY_1
	"\xE0\xE1\xE2\xE3\xB4" "2", //KEY_2
	"\xE4\xE5\xBA\xE6\xE7" "3", //KEY_3
	"\xE8\xB3\xBF\xE9" "4", //KEY_4
	"\xEA\xEB\xEC\xED" "5", //KEY_5
	"\xEE\xEF\xF0\xF1" "6", //KEY_6
	"\xF2\xF3\xF4\xF5" "7", //KEY_7
	"\xF6\xF7\xF8\xF9" "8", //KEY_8
	"\xFC\xFE\xFF" "9", //KEY_9
	"*=#", //KEY_ASTERISK
	" 0", //KEY_0
};

//Ukrainian, uppercase
const char key_map_ukr_uc[KEYMAP_ROWS][KEYMAP_COLS] =
{
	".,?!1'\"-+()@/:", //KEY_1
	"\xC0\xC1\xC2\xC3\xA5" "2", //KEY_2
	"\xC4\xC5\xAA\xC6\xC7" "3", //KEY_3
	"\xC8\xB2\xAF\xC9" "4", //KEY_4
	"\xCA\xCB\xCC\xCD" "5", //KEY_5
	"\xCE\xCF\xD0\xD1" "6", //KEY_6
	"\xD2\xD3\xD4\xD5" "7", //KEY_7
	"\xD6\xD7\xD8\xD9" "8", //KEY_8
	"\xDC\xDE\xDF" "9", //KEY_9
	"*=#", //KEY_ASTERISK
	" 0", //KEY_0
};

//characters assigned to a key in a given entry mode (T9 uses the lowercase map)
const char *keymapRow(text_entry_t mode, kbd_key_t key)
{
	if (key < KEY_1 || key > KEY_0)
		return "";

	uint8_t r = key - KEY_1;

	switch (mode)
	{
		case TEXT_UPPERCASE:	return key_map_uc[r];
		case TEXT_NUMERIC:		return key_map_num[r];
		case TEXT_UKR_LOWER:	return key_map_ukr_lc[r];
		case TEXT_UKR_UPPER:	return key_map_ukr_uc[r];
		default:				return key_map_lc[r];
	}
}
