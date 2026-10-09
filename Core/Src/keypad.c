#include "keypad.h"
#include "display.h"
#include "menus.h"
#include "platform.h"
#include "keymaps.h"
#include "settings.h"
#include "../t9/t9.h"
#include "../t9/dict/dict_en.h"
#include "rf_module.h"

#define KBD_SCAN_MS		10		//keypad scan period
#define VFO_STEP_HZ		12500

extern TIM_HandleTypeDef htim7;		//TIM7 - text entry timer
extern TIM_HandleTypeDef htim14;	//TIM14 - display backlight timeout timer

extern msg_t rcvd_msg;
extern uint8_t rx_scroll;
extern uint8_t rx_total_lines;

static uint32_t next_key_time;

typedef struct
{
	char code[16];
	uint8_t code_len;
	uint8_t start_pos;
	uint8_t last_len;
} t9_t;

static t9_t t9;

//T9 related
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

// backspace key handler
static inline uint8_t keyBackspace(abc_t *text_entry)
{
	if (!(text_entry->pos))
		return 0;

	text_entry->buffer[--(text_entry->pos)] = 0;
	clearCode();

	return 1;
}

static void menuMoveDown(disp_state_t *disp_state)
{
	if (menu_pos_hl==3 || menu_pos_hl==displays[*disp_state].num_items-1)
	{
		if (menu_pos+3<displays[*disp_state].num_items-1)
			menu_pos++;
		else //wrap around
		{
			menu_pos=0;
			menu_pos_hl=0;
		}
	}
	else
	{
		if (menu_pos_hl<3 && menu_pos+menu_pos_hl<displays[*disp_state].num_items-1)
			menu_pos_hl++;
	}
}

static void menuMoveUp(disp_state_t *disp_state)
{
	if (menu_pos_hl==0)
	{
		if (menu_pos>0)
			menu_pos--;
		else //wrap around
		{
			if (displays[*disp_state].num_items>3)
			{
				menu_pos=displays[*disp_state].num_items-1-3;
				menu_pos_hl=3;
			}
			else
			{
				menu_pos=0;
				menu_pos_hl=displays[*disp_state].num_items-1;
			}
		}
	}
	else
	{
		if (menu_pos_hl>0)
			menu_pos_hl--;
	}
}

void resetTextEntry(void)
{
	clearCode();
	HAL_TIM_Base_Stop(&htim7);
	TIM7->CNT = 0;
}

//VFO: move RX and TX frequency together (keeps a split offset)
static void stepVFO(dev_settings_t *dev_settings, int32_t step)
{
	dev_settings->channel.rx_frequency += step;
	dev_settings->channel.tx_frequency += step;
	setFreqRF(dev_settings->channel.rx_frequency, dev_settings->freq_corr); //we are receiving
}

//memory mode: select the next/previous codeplug channel
static void stepChannel(dev_settings_t *dev_settings, int8_t dir)
{
	if (codeplug.num_items == 0)
		return;

	if (dir > 0)
		dev_settings->ch_num = (dev_settings->ch_num + 1) % codeplug.num_items;
	else
		dev_settings->ch_num = (dev_settings->ch_num == 0) ? codeplug.num_items - 1 : dev_settings->ch_num - 1;

	memcpy(&dev_settings->channel, &codeplug.channel[dev_settings->ch_num], sizeof(ch_settings_t));

	//apply the new channel (RX always uses the 25 kHz filter, see chBwRF() workaround)
	setPowerRF(dev_settings->channel.rf_pwr);
	setFreqRF(dev_settings->channel.rx_frequency, dev_settings->freq_corr);
}

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

//push a character into the text message buffer
void pushCharBuffer(abc_t *text_entry, const char key_map[][KEYMAP_COLS], kbd_key_t key)
{
	const char *key_chars = key_map[key-KEY_1]; // start indexing at 0
	uint8_t map_len = key_map_len[key-KEY_1];
	uint8_t new = 1;

	HAL_TIM_Base_Stop(&htim7);
	char *last = &text_entry->buffer[text_entry->pos>0 ? text_entry->pos-1 : 0];

	if(TIM7->CNT)
	{
		for(uint8_t i=0; i<map_len; i++)
		{
			if(*last==key_chars[i])
			{
				*last=key_chars[(i+1)%map_len];
				new = 0;
				break;
			}
		}

		if(new)
		{
			// check if the buffer can hold it
			if (text_entry->pos < sizeof(text_entry->buffer) - 1)
			{
				text_entry->buffer[(text_entry->pos)++] = key_chars[0];
				text_entry->buffer[text_entry->pos] = 0; //terminate!
			}
		}
	}
	else
	{
		// check if the buffer can hold it
		if (text_entry->pos < sizeof(text_entry->buffer) - 1)
		{
			text_entry->buffer[(text_entry->pos)++] = key_chars[0];
			text_entry->buffer[text_entry->pos] = 0; //terminate!
		}
	}

	TIM7->CNT=0;
	HAL_TIM_Base_Start_IT(&htim7);
}

//push character for T9 code
void pushCharT9(abc_t *text_entry, kbd_key_t key)
{
	char c = (key != KEY_ASTERISK) ? ('2' + key - KEY_2) : '*';
	const char *w = addCode(c);

	// first T9 digit
	if (t9.code_len == 1)
	{
		t9.start_pos = text_entry->pos;
		t9.last_len  = 0;
	}

	if (*w)
	{
		uint8_t l = strlen(w);
		memset(&text_entry->buffer[t9.start_pos], 0, t9.last_len);
		memcpy(&text_entry->buffer[t9.start_pos], w, l);
		t9.last_len = l;
	}
	else
	{
		text_entry->buffer[t9.start_pos + t9.last_len] = '?';
		t9.last_len++;
	}

	text_entry->buffer[t9.start_pos + t9.last_len] = 0;
	text_entry->pos = t9.start_pos + t9.last_len;
}

void handleKey(disp_dev_t *disp_dev, disp_state_t *disp_state, abc_t *text_entry,
		radio_state_t *radio_state, dev_settings_t *dev_settings, kbd_key_t key, edit_set_t *edit_set)
{
	if (key==KEY_NONE)
		return;

	//backlight on
	if(dev_settings->backlight_always==0)
	{
		if(TIM14->CNT==0)
		{
			setBacklight(dev_settings->backlight_level);
			startBacklightTimer();
		}
		else
		{
			TIM14->CNT=0;
		}
	}

	//find out where to go next
	uint8_t item = menu_pos + menu_pos_hl;
	disp_state_t old = *disp_state;
	disp_state_t next = displays[*disp_state].next_disp[item];

	//do something based on the key pressed and current state
	switch(key)
	{
		case KEY_OK:
			// reset menu cursor position when changing menus
			if (next != DISP_NONE)
				menu_pos = menu_pos_hl = 0;

			// select editor if not editing already (param or message)
			if (*disp_state != DISP_TEXT_VALUE_ENTRY && *disp_state != DISP_TEXT_MSG_ENTRY)
			{
				clearCode();
				*edit_set = displays[*disp_state].edit[item];
			}

			// messaging: OK sends, but stays in same state
			if (*disp_state == DISP_TEXT_MSG_ENTRY)
			{
				// already inside: OK sends message
				if (old == DISP_TEXT_MSG_ENTRY)
				{
					if (*radio_state == RF_RX)
						initTextTX(text_entry->buffer);
					break;
				}
			}

			// leave current state (commit if needed)
			leaveState(old, text_entry->buffer, dev_settings, *edit_set, radio_state);

			// clear edit selection after commit
			if (old == DISP_TEXT_VALUE_ENTRY)
				*edit_set = EDIT_NONE;

			// perform transition
			if (next != DISP_NONE)
			{
				*disp_state = next;
				if (next != old)
					enterState(disp_dev, *disp_state, text_entry, dev_settings);
			}
		break;

		case KEY_C:
			menu_pos = menu_pos_hl = 0;
			disp_state_t prev = displays[*disp_state].prev_disp;

			// text editors: try backspace first
			if (*disp_state == DISP_TEXT_MSG_ENTRY ||
				*disp_state == DISP_TEXT_VALUE_ENTRY)
			{
				if (keyBackspace(text_entry))
				{
					redrawText(disp_dev, *disp_state, text_entry->buffer);
					break;
				}
			}

			// cancel/back
			leaveState(*disp_state, text_entry->buffer, dev_settings, *edit_set, radio_state);

			*disp_state = prev;

			enterState(disp_dev, *disp_state, text_entry, dev_settings);
		break;

		case KEY_LEFT:
			//main screen
			if(*disp_state==DISP_MAIN_SCR)
			{
				if(dev_settings->tuning_mode==TUNING_VFO)
					stepVFO(dev_settings, -VFO_STEP_HZ);
				else //if (dev_settings->tuning_mode==TUNING_MEM)
					stepChannel(dev_settings, 1);

				showMainScreen(disp_dev);
			}

			//text/value entry
			else if(*disp_state==DISP_TEXT_MSG_ENTRY ||
					*disp_state==DISP_TEXT_VALUE_ENTRY)
			{
				; //nothing yet
			}

			//received message
			else if(*disp_state==DISP_TEXT_MSG_RCVD)
			{
				if (rx_scroll + 3 < rx_total_lines)
					rx_scroll++;
				drawRect(disp_dev, 0, 18, RES_X-1, RES_Y-1, COL_WHITE, 1);
				setStringWordWrapFromLine(disp_dev, 0, 18, &nokia_small, rcvd_msg.text, COL_BLACK, rx_scroll, 3);
			}

			//other menus
			else
			{
				//move down
				menuMoveDown(disp_state);

				//no state change
				showMenu(disp_dev, &displays[*disp_state], menu_pos, menu_pos_hl);
			}
		break;

		case KEY_RIGHT:
			//main screen
			if(*disp_state==DISP_MAIN_SCR)
			{
				if(dev_settings->tuning_mode==TUNING_VFO)
					stepVFO(dev_settings, VFO_STEP_HZ);
				else //if (dev_settings->tuning_mode==TUNING_MEM)
					stepChannel(dev_settings, -1);

				showMainScreen(disp_dev);
			}

			//text/value entry
			else if(*disp_state==DISP_TEXT_MSG_ENTRY ||
					*disp_state==DISP_TEXT_VALUE_ENTRY)
			{
				clearCode(); // clear the T9 code
				HAL_TIM_Base_Stop(&htim7);
				TIM7->CNT=0;
			}

			//received message
			else if(*disp_state==DISP_TEXT_MSG_RCVD)
			{
				if (rx_scroll)
					rx_scroll--;
				drawRect(disp_dev, 0, 18, RES_X-1, RES_Y-1, COL_WHITE, 1);
				setStringWordWrapFromLine(disp_dev, 0, 18, &nokia_small, rcvd_msg.text, COL_BLACK, rx_scroll, 3);
			}

			//other menus
			else
			{
				//move up
				menuMoveUp(disp_state);

				//no state change
				showMenu(disp_dev, &displays[*disp_state], menu_pos, menu_pos_hl);
			}
		break;

		case KEY_1:
			if(text_entry->mode==TEXT_LOWERCASE)
			{
				pushCharBuffer(text_entry, key_map_lc, key);
			}
			else if (text_entry->mode==TEXT_UPPERCASE)
			{
				pushCharBuffer(text_entry, key_map_uc, key);
			}
			else //if (*text_mode==TEXT_T9)
			{
				clearCode(); // clear the T9 code
				pushCharBuffer(text_entry, key_map_lc, key);
			}

			redrawText(disp_dev, *disp_state, text_entry->buffer);
		break;

		//T9 keys
		case KEY_2:
		case KEY_3:
		case KEY_4:
		case KEY_5:
		case KEY_6:
		case KEY_7:
		case KEY_8:
		case KEY_9:
		case KEY_ASTERISK:
			if(text_entry->mode==TEXT_LOWERCASE)
			{
				pushCharBuffer(text_entry, key_map_lc, key);
			}
			else if (text_entry->mode==TEXT_UPPERCASE)
			{
				pushCharBuffer(text_entry, key_map_uc, key);
			}
			else //if (*text_mode==TEXT_T9)
			{
				pushCharT9(text_entry, key);
			}

			redrawText(disp_dev, *disp_state, text_entry->buffer);
		break;

		case KEY_0:
			if(text_entry->mode==TEXT_LOWERCASE)
			{
				pushCharBuffer(text_entry, key_map_lc, key);
			}
			else if (text_entry->mode==TEXT_UPPERCASE)
			{
				pushCharBuffer(text_entry, key_map_uc, key);
			}
			else //if (*text_mode==TEXT_T9)
			{
				clearCode(); // clear the T9 code
				pushCharBuffer(text_entry, key_map_lc, key);
			}

			redrawText(disp_dev, *disp_state, text_entry->buffer);
		break;

		case KEY_HASH:
			if(*disp_state==DISP_TEXT_MSG_ENTRY || *disp_state==DISP_TEXT_VALUE_ENTRY)
			{
				if(text_entry->mode==TEXT_LOWERCASE)
					text_entry->mode = TEXT_UPPERCASE;
				else if(text_entry->mode==TEXT_UPPERCASE)
				{
					text_entry->mode = TEXT_T9;
					clearCode();
				}
				else
					text_entry->mode = TEXT_LOWERCASE;

				redrawTextEntryIcon(disp_dev, text_entry->mode);
			}
		break;

		default:
			;
		break;
	}
}

//set keyboard insensitivity timer
void setKeysTimeout(uint16_t delay)
{
	TIM7->ARR=delay*10-1;
}
