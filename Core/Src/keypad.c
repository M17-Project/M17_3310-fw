#include "keypad.h"
#include <string.h>
#include "main.h"
#include "../t9/t9.h"
#include "../t9/dict/dict_en.h"

#define KBD_SCAN_MS		10		//keypad scan period

extern TIM_HandleTypeDef htim7;		//TIM7 - text entry (multi-tap) timer

static uint32_t next_key_time;

//--- key matrix ---

//read the key matrix (columns driven high, rows have pull-downs)
static kbd_key_t scanMatrix(void)
{
	static const struct
	{
		GPIO_TypeDef *port;
		uint16_t pin;
		kbd_key_t keys[5]; //ROW_1..ROW_5
	} cols[3] =
	{
		{COL_1_GPIO_Port, COL_1_Pin, {KEY_C,     KEY_1, KEY_4, KEY_7, KEY_ASTERISK}},
		{COL_2_GPIO_Port, COL_2_Pin, {KEY_LEFT,  KEY_2, KEY_5, KEY_8, KEY_0}},
		{COL_3_GPIO_Port, COL_3_Pin, {KEY_RIGHT, KEY_3, KEY_6, KEY_9, KEY_HASH}}
	};

	for (uint8_t c=0; c<3; c++)
	{
		kbd_key_t key = KEY_NONE;

		cols[c].port->BSRR = (uint32_t)cols[c].pin;
		for (volatile uint8_t i=0; i<100; i++); //settle

		if(ROW_1_GPIO_Port->IDR & ROW_1_Pin)
			key = cols[c].keys[0];
		else if(ROW_2_GPIO_Port->IDR & ROW_2_Pin)
			key = cols[c].keys[1];
		else if(ROW_3_GPIO_Port->IDR & ROW_3_Pin)
			key = cols[c].keys[2];
		else if(ROW_4_GPIO_Port->IDR & ROW_4_Pin)
			key = cols[c].keys[3];
		else if(ROW_5_GPIO_Port->IDR & ROW_5_Pin)
			key = cols[c].keys[4];

		cols[c].port->BSRR = ((uint32_t)cols[c].pin<<16);

		if (key != KEY_NONE)
			return key;
	}

	return KEY_NONE;
}

uint8_t keyHeld(void)
{
	return ((BTN_OK_GPIO_Port->IDR & BTN_OK_Pin) || scanMatrix() != KEY_NONE) ? 1 : 0;
}

//scan keyboard - 'rep' milliseconds delay after a valid keypress is detected
//the matrix is scanned every KBD_SCAN_MS, a key must be stable for two scans
kbd_key_t scanKeys(radio_state_t radio_state, uint16_t rep)
{
	static uint32_t last_scan;
	static kbd_key_t last_raw = KEY_NONE;
	static uint8_t ok_reported;

	uint32_t now = HAL_GetTick();

	// not in RX mode?
	if (radio_state != RF_RX)
		return KEY_NONE;

	if ((uint32_t)(now - last_scan) < KBD_SCAN_MS)
		return KEY_NONE;
	last_scan = now;

	//PD2 high means KEY_OK is pressed
	kbd_key_t raw = (BTN_OK_GPIO_Port->IDR & BTN_OK_Pin) ? KEY_OK : scanMatrix();

	//debounce: wait until the reading is stable
	if (raw != last_raw)
	{
		last_raw = raw;
		return KEY_NONE;
	}

	if (raw == KEY_NONE)
	{
		ok_reported = 0;
		return KEY_NONE;
	}

	//OK button: report once per press
	if (raw == KEY_OK)
	{
		if (ok_reported)
			return KEY_NONE;

		ok_reported = 1;
		next_key_time = now + rep;
		return KEY_OK;
	}

	ok_reported = 0;

	// too early to report another key? (wrap-around safe)
	if ((int32_t)(now - next_key_time) < 0)
		return KEY_NONE;

	next_key_time = now + rep;
	return raw;
}

//set the multi-tap timeout (TIM7 runs at 10 kHz)
void setKeysTimeout(uint16_t delay)
{
	TIM7->ARR=delay*10-1;
}

//--- text entry ---
typedef struct
{
	char code[16];
	uint8_t code_len;
	uint16_t start_pos;
	uint16_t last_len;
} t9_t;

static t9_t t9;

static const char *addCode(char symbol)
{
	if (t9.code_len < sizeof(t9.code) - 1)
	{
		t9.code[t9.code_len++] = symbol;
		t9.code[t9.code_len] = 0;
	}

	return getWord(dict_en, t9.code);
}

static inline void clearCode(void)
{
	t9.code[0] = 0;
	t9.code_len = 0;
	t9.last_len = 0;
}

static void stopMultiTap(abc_t *te)
{
	HAL_TIM_Base_Stop(&htim7);
	TIM7->CNT = 0;
	if (te != NULL)
		te->last_key = KEY_NONE;
}

void resetTextEntry(void)
{
	clearCode();
	stopMultiTap(NULL);
}

void textEntryInit(abc_t *te, const char *initial, uint16_t max_len, uint8_t modes, text_entry_t mode)
{
	resetTextEntry();

	if (max_len == 0 || max_len > sizeof(te->buffer) - 1)
		max_len = sizeof(te->buffer) - 1;

	te->max_len = max_len;
	te->modes = modes ? modes : TM(TEXT_LOWERCASE);
	te->last_key = KEY_NONE;

	if (!(te->modes & TM(mode)))
	{
		//first allowed mode
		mode = TEXT_LOWERCASE;
		for (uint8_t m=0; m<TEXT_NUM_MODES; m++)
		{
			if (te->modes & TM(m))
			{
				mode = (text_entry_t)m;
				break;
			}
		}
	}
	te->mode = mode;

	te->pos = 0;
	if (initial != NULL)
	{
		while (initial[te->pos] && te->pos < max_len)
		{
			te->buffer[te->pos] = initial[te->pos];
			te->pos++;
		}
	}
	te->buffer[te->pos] = 0;
	te->cursor = te->pos;
}

//replace 'old_len' characters at 'start' with 'src' (shortened if needed)
//returns the number of characters inserted
static uint16_t replaceRange(abc_t *te, uint16_t start, uint16_t old_len, const char *src, uint16_t new_len)
{
	uint16_t tail = te->pos - (start + old_len);

	if (start + new_len + tail > te->max_len)
		new_len = te->max_len - start - tail;

	memmove(&te->buffer[start + new_len], &te->buffer[start + old_len], tail + 1); //incl. terminator
	memcpy(&te->buffer[start], src, new_len);
	te->pos = start + new_len + tail;

	return new_len;
}

//multi-tap character entry
static uint8_t pushCharBuffer(abc_t *te, text_entry_t map_mode, kbd_key_t key)
{
	const char *chars = keymapRow(map_mode, key);
	uint8_t map_len = strlen(chars);
	uint8_t changed = 0;

	if (map_len == 0)
		return 0;

	HAL_TIM_Base_Stop(&htim7);

	//same key again before the timeout: cycle the character left of the cursor
	if (TIM7->CNT && key == te->last_key && map_len > 1 && te->cursor > 0)
	{
		char *last = &te->buffer[te->cursor-1];
		uint8_t i = 0;

		while (i < map_len && chars[i] != *last)
			i++;

		*last = chars[(i < map_len) ? (i+1) % map_len : 0];
		changed = 1;
	}
	else if (te->pos < te->max_len)
	{
		te->cursor += replaceRange(te, te->cursor, 0, chars, 1); //insert at the cursor
		changed = 1;
	}

	te->last_key = key;

	//numbers: no need to wait for the timeout
	TIM7->CNT = 0;
	if (map_len > 1)
		HAL_TIM_Base_Start_IT(&htim7);

	return changed;
}

//T9 predictive entry (the word being typed ends at the cursor)
static uint8_t pushCharT9(abc_t *te, kbd_key_t key)
{
	char c = (key != KEY_ASTERISK) ? ('2' + key - KEY_2) : '*';
	char word[sizeof(t9.code) + 1];
	uint16_t l;

	// first T9 digit
	if (t9.code_len == 0)
	{
		if (te->pos >= te->max_len)
			return 0;

		t9.start_pos = te->cursor;
		t9.last_len  = 0;
	}

	const char *w = addCode(c);

	if (*w)
	{
		l = strlen(w);
		if (l > sizeof(word) - 1)
			l = sizeof(word) - 1;
		memcpy(word, w, l);
	}
	else //no such word: the previous one + '?'
	{
		l = (t9.last_len < sizeof(word) - 1) ? t9.last_len : sizeof(word) - 2;
		memcpy(word, &te->buffer[t9.start_pos], l);
		word[l++] = '?';
	}

	t9.last_len = replaceRange(te, t9.start_pos, t9.last_len, word, l);
	te->cursor = t9.start_pos + t9.last_len;

	return 1;
}

uint8_t textEntryKey(abc_t *te, kbd_key_t key)
{
	if (key < KEY_1 || key > KEY_0)
		return 0;

	if (te->mode == TEXT_T9)
	{
		if ((key >= KEY_2 && key <= KEY_9) || key == KEY_ASTERISK)
			return pushCharT9(te, key);

		//1 and 0: punctuation and space, end of the T9 word
		clearCode();
		return pushCharBuffer(te, TEXT_LOWERCASE, key);
	}

	return pushCharBuffer(te, te->mode, key);
}

uint8_t textEntryBackspace(abc_t *te)
{
	clearCode();
	stopMultiTap(te);

	if (te->cursor == 0)
		return 0;

	te->cursor--;
	replaceRange(te, te->cursor, 1, "", 0);

	return 1;
}

uint8_t textEntryCursor(abc_t *te, int8_t dir)
{
	clearCode();
	stopMultiTap(te);

	if (dir < 0 && te->cursor > 0)
		te->cursor--;
	else if (dir > 0 && te->cursor < te->pos)
		te->cursor++;
	else
		return 0;

	return 1;
}

void textEntryNextMode(abc_t *te)
{
	clearCode();
	stopMultiTap(te);

	for (uint8_t i=1; i<=TEXT_NUM_MODES; i++)
	{
		text_entry_t m = (text_entry_t)((te->mode + i) % TEXT_NUM_MODES);

		if (te->modes & TM(m))
		{
			te->mode = m;
			return;
		}
	}
}

void textEntryConfirm(abc_t *te)
{
	clearCode();
	stopMultiTap(te);
}
