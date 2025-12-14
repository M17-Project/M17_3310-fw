#include "keypad.h"

static uint32_t next_key_time = 0;

//T9 related
const char *addCode(char *code, char symbol)
{
	code[strlen(code)] = symbol;
	return getWord(dict, code);
}

void clearCode(char *code)
{
	if (code && *code)
		memset(code, 0, strlen(code));
}

//scan keyboard - 'rep' milliseconds delay after a valid keypress is detected
kbd_key_t scanKeys(radio_state_t radio_state, uint8_t rep)
{
	uint32_t now = HAL_GetTick();

	// too early to report another key?
	if (now < next_key_time)
		return KEY_NONE;

	kbd_key_t key = KEY_NONE;

	//PD2 high means KEY_OK is pressed
	uint8_t ok_now = (BTN_OK_GPIO_Port->IDR & BTN_OK_Pin) ? 1 : 0;
	static uint8_t ok_prev = 0; //for press/release detection

	//OK button pressed
	if(ok_now && !ok_prev)
	{
		ok_prev = 1;
		if(radio_state==RF_RX)
		{
			next_key_time = now + rep;
			return KEY_OK;
		}
	}
	else if (!ok_now) //released
	{
		ok_prev = 0;
	}

	//column 1
	COL_1_GPIO_Port->BSRR = (uint32_t)COL_1_Pin;
	for (volatile uint8_t i=0; i<100; i++);
	if(ROW_1_GPIO_Port->IDR & ROW_1_Pin)
		key = KEY_C;
	else if(ROW_2_GPIO_Port->IDR & ROW_2_Pin)
		key = KEY_1;
	else if(ROW_3_GPIO_Port->IDR & ROW_3_Pin)
		key = KEY_4;
	else if(ROW_4_GPIO_Port->IDR & ROW_4_Pin)
		key = KEY_7;
	else if(ROW_5_GPIO_Port->IDR & ROW_5_Pin)
		key = KEY_ASTERISK;
	COL_1_GPIO_Port->BSRR = ((uint32_t)COL_1_Pin<<16);
	if(key!=KEY_NONE && radio_state==RF_RX)
	{
		next_key_time = now + rep;
		return key;
	}

	//column 2
	COL_2_GPIO_Port->BSRR = (uint32_t)COL_2_Pin;
	for (volatile uint8_t i=0; i<100; i++);
	if(ROW_1_GPIO_Port->IDR & ROW_1_Pin)
		key = KEY_LEFT;
	else if(ROW_2_GPIO_Port->IDR & ROW_2_Pin)
		key = KEY_2;
	else if(ROW_3_GPIO_Port->IDR & ROW_3_Pin)
		key = KEY_5;
	else if(ROW_4_GPIO_Port->IDR & ROW_4_Pin)
		key = KEY_8;
	else if(ROW_5_GPIO_Port->IDR & ROW_5_Pin)
		key = KEY_0;
	COL_2_GPIO_Port->BSRR = ((uint32_t)COL_2_Pin<<16);
	if(key!=KEY_NONE && radio_state==RF_RX)
	{
		next_key_time = now + rep;
		return key;
	}

	//column 3
	COL_3_GPIO_Port->BSRR = (uint32_t)COL_3_Pin;
	for (volatile uint8_t i=0; i<100; i++);
	if(ROW_1_GPIO_Port->IDR & ROW_1_Pin)
		key = KEY_RIGHT;
	else if(ROW_2_GPIO_Port->IDR & ROW_2_Pin)
		key = KEY_3;
	else if(ROW_3_GPIO_Port->IDR & ROW_3_Pin)
		key = KEY_6;
	else if(ROW_4_GPIO_Port->IDR & ROW_4_Pin)
		key = KEY_9;
	else if(ROW_5_GPIO_Port->IDR & ROW_5_Pin)
		key = KEY_HASH;
	COL_3_GPIO_Port->BSRR = ((uint32_t)COL_3_Pin<<16);
	if(key!=KEY_NONE && radio_state==RF_RX)
	{
		next_key_time = now + rep;
		return key;
	}

	return key;
}

//push a character into the text message buffer
void pushCharBuffer(char *text_entry, text_entry_t text_mode, const char key_map[][KEYMAP_COLS], kbd_key_t key)
{
	const char *key_chars = key_map[key-KEY_1]; // start indexing at 0
	uint8_t map_len = strlen(key_chars);
	uint8_t new = 1;

	HAL_TIM_Base_Stop(&htim7);
	pos = strlen(text_entry);
	char *last = &text_entry[pos>0 ? pos-1 : 0];

	if(TIM7->CNT>0)
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
			text_entry[pos] = key_chars[0];
			text_entry[pos+1] = 0; //terminate!
			if(key==KEY_0 && text_mode==TEXT_T9)
			{
				pos++;
			}
		}
	}
	else
	{
		text_entry[pos] = key_chars[0];
		text_entry[pos+1] = 0; //terminate!
		if(key==KEY_0 && text_mode==TEXT_T9)
		{
			pos++;
		}
	}

	TIM7->CNT=0;
	HAL_TIM_Base_Start_IT(&htim7);
}

//push character for T9 code
void pushCharT9(char *text_entry, char *code, kbd_key_t key)
{
	char c;
	if (key != KEY_ASTERISK)
		c = '2'+key-KEY_2;
	else
		c = '*';
	const char *w = addCode(code, c);

	if (strlen(w) != 0)
	{
		strcpy(&text_entry[pos], w);
	}
	else if (key != KEY_ASTERISK)
	{
		uint8_t len = strlen(text_entry);
		text_entry[len] = '?';
		text_entry[len+1] = 0;
	}
}

void handleKey(disp_dev_t *disp_dev, disp_state_t *disp_state, char *text_entry, char *code,
		text_entry_t *text_mode, radio_state_t *radio_state, dev_settings_t *dev_settings,
		kbd_key_t key, edit_set_t *edit_set)
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

			// select editor
			if (*disp_state == DISP_RADIO_SETTINGS)
			{
				if (item == 0) *edit_set = EDIT_RF_PPM;
				else if (item == 1) *edit_set = EDIT_RF_PWR;
			}
			else if (*disp_state == DISP_M17_SETTINGS)
			{
				if (item == 0) *edit_set = EDIT_M17_SRC_CALLSIGN;
				else if (item == 1) *edit_set = EDIT_M17_DST_CALLSIGN;
				else if (item == 2) *edit_set = EDIT_M17_CAN;
			}

			// messaging: OK sends, but stays in same state
			if (*disp_state == DISP_TEXT_MSG_ENTRY)
			{
				// already inside: OK sends message
				if (old == DISP_TEXT_MSG_ENTRY)
				{
					if (*radio_state == RF_RX)
						initTextTX(text_entry);
					break;
				}
			}

			// leave current state (commit if needed)
			leaveState(old, text_entry, dev_settings, *edit_set, radio_state);

			// clear edit selection after commit
			if (old == DISP_TEXT_VALUE_ENTRY)
				*edit_set = EDIT_NONE;

			// perform transition
			if (next != DISP_NONE)
			{
				*disp_state = next;
				if (next != old)
					enterState(disp_dev, *disp_state, *text_mode, text_entry, code);
			}
		break;

		case KEY_C:
			menu_pos=menu_pos_hl=0;
			next = displays[*disp_state].prev_disp;

			//dbg_print("[Debug] Start disp_state: %d\n", *disp_state);

			//main screen
			if(*disp_state==DISP_MAIN_SCR)
			{
				; //nothing
			}

			//main menu
			else if(*disp_state==DISP_MAIN_MENU)
			{
				*disp_state = next;
				showMainScreen(disp_dev);
			}

			//text message entry
			else if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
				uint8_t len = strlen(text_entry);

				//backspace
				if(len)
				{
					text_entry[len-1] = 0;
					pos = len - 1;
					if (*code)
						memset(code, 0, strlen(code)); // clear T9 code only when required

					drawRect(disp_dev, 0, 10, RES_X-1, RES_Y-9, 1, 1);
					setString(disp_dev, 0, 10, &nokia_small, text_entry, 0, ALIGN_LEFT);
				}
				else
				{
					*disp_state = next;
					showMenu(disp_dev, &displays[*disp_state], 0, 0);
				}
			}

			//text value entry
			else if(*disp_state==DISP_TEXT_VALUE_ENTRY)
			{
				uint8_t len = strlen(text_entry);

				//backspace
				if(len)
				{
					text_entry[len-1] = 0;
					pos = len - 1;
					if (*code)
						memset(code, 0, strlen(code)); // clear T9 code only when required

					drawRect(disp_dev, 1, 10, RES_X-2, RES_Y-12, 1, 1);
					setString(disp_dev, 3, 13, &nokia_big, text_entry, 0, ALIGN_ARB);
				}
				else
				{
					*disp_state = next;
					showMainScreen(disp_dev);
				}
			}

			//anything else
			else
			{
				*disp_state = displays[*disp_state].prev_disp;
				showMenu(disp_dev, &displays[*disp_state], 0, 0);
			}

			//dbg_print("[Debug] End disp_state: %d\n", *disp_state);
		break;

		case KEY_LEFT:
			//main screen
			if(*disp_state==DISP_MAIN_SCR)
			{
				if(dev_settings->tuning_mode==TUNING_VFO)
				{
					dev_settings->channel.tx_frequency -= 12500;
					setFreqRF(dev_settings->channel.tx_frequency, dev_settings->freq_corr);

					char str[24];
					sprintf(str, "T %ld.%04ld",
							dev_settings->channel.tx_frequency/1000000,
							(dev_settings->channel.tx_frequency%1000000)/100);
					drawRect(disp_dev, 0, 36, RES_X-1, 36+8, 1, 1);
					setString(disp_dev, 0, 36, &nokia_small, str, 0, ALIGN_CENTER);
				}
				else //if (dev_settings->tuning_mode==TUNING_MEM)
				{
					dev_settings->ch_num++;
					dev_settings->ch_num %= codeplug.num_items;
					memcpy(&dev_settings->channel, &codeplug.channel[dev_settings->ch_num], sizeof(ch_settings_t));
				}
			}

			//text message entry
			else if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
				; //nothing yet
			}

			//other menus
			else if(*disp_state!=DISP_TEXT_VALUE_ENTRY)
			{
				if(menu_pos_hl==3 || menu_pos_hl==displays[*disp_state].num_items-1)
				{
					if(menu_pos+3<displays[*disp_state].num_items-1)
						menu_pos++;
					else //wrap around
					{
						menu_pos=0;
						menu_pos_hl=0;
					}
				}
				else
				{
					if(menu_pos_hl<3 && menu_pos+menu_pos_hl<displays[*disp_state].num_items-1)
						menu_pos_hl++;
				}

				//no state change
				showMenu(disp_dev, &displays[*disp_state], menu_pos, menu_pos_hl);
			}

			else
			{
				;
			}
		break;

		case KEY_RIGHT:
			//main screen
			if(*disp_state==DISP_MAIN_SCR)
			{
				if(dev_settings->tuning_mode==TUNING_VFO)
				{
					dev_settings->channel.tx_frequency += 12500;
					setFreqRF(dev_settings->channel.tx_frequency, dev_settings->freq_corr);

					char str[24];
					sprintf(str, "T %ld.%04ld",
							dev_settings->channel.tx_frequency/1000000,
							(dev_settings->channel.tx_frequency%1000000)/100);
					drawRect(disp_dev, 0, 36, RES_X-1, 36+8, 1, 1);
					setString(disp_dev, 0, 36, &nokia_small, str, 0, ALIGN_CENTER);
				}
				else //if (dev_settings->tuning_mode==TUNING_MEM)
				{
					dev_settings->ch_num = dev_settings->ch_num==0 ? codeplug.num_items-1: dev_settings->ch_num-1;
					memcpy(&dev_settings->channel, &codeplug.channel[dev_settings->ch_num], sizeof(ch_settings_t));
				}
			}

			//text message entry
			else if(*disp_state==DISP_TEXT_MSG_ENTRY || *disp_state==DISP_TEXT_VALUE_ENTRY)
			{
				pos=strlen(text_entry);
				if (*code)
					memset(code, 0, strlen(code));
				HAL_TIM_Base_Stop(&htim7);
				TIM7->CNT=0;
			}

			//other menus
			else
			{
				if(menu_pos_hl==0)
				{
					if(menu_pos>0)
						menu_pos--;
					else //wrap around
					{
						if(displays[*disp_state].num_items>3)
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
					if(menu_pos_hl>0)
						menu_pos_hl--;
				}

				//no state change
				showMenu(disp_dev, &displays[*disp_state], menu_pos, menu_pos_hl);
			}

			//else
			/*{
				;
			}*/
		break;

		case KEY_1:
			if(*text_mode==TEXT_LOWERCASE)
			{
				pushCharBuffer(text_entry, *text_mode, key_map_lc, key);
			}
			else if (*text_mode==TEXT_UPPERCASE)
			{
				pushCharBuffer(text_entry, *text_mode, key_map_uc, key);
			}
			else //if (*text_mode==TEXT_T9)
			{
				clearCode(code); // clear the T9 code if needed
				pushCharBuffer(text_entry, *text_mode, key_map_lc, key);
			}

			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
				redrawMsgEntry(disp_dev, text_entry);
			}
			else if(*disp_state==DISP_TEXT_VALUE_ENTRY)
			{
				redrawValueEntry(disp_dev, text_entry);
			}
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
			if(*text_mode==TEXT_LOWERCASE)
			{
				pushCharBuffer(text_entry, *text_mode, key_map_lc, key);
			}
			else if (*text_mode==TEXT_UPPERCASE)
			{
				pushCharBuffer(text_entry, *text_mode, key_map_uc, key);
			}
			else //if (*text_mode==TEXT_T9)
			{
				pushCharT9(text_entry, code, key);
			}

			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
				redrawMsgEntry(disp_dev, text_entry);
			}
			else if(*disp_state==DISP_TEXT_VALUE_ENTRY)
			{
				redrawValueEntry(disp_dev, text_entry);
			}
		break;

		case KEY_0:
			if(*text_mode==TEXT_LOWERCASE)
			{
				pushCharBuffer(text_entry, *text_mode, key_map_lc, key);
			}
			else if (*text_mode==TEXT_UPPERCASE)
			{
				pushCharBuffer(text_entry, *text_mode, key_map_uc, key);
			}
			else //if (*text_mode==TEXT_T9)
			{
				clearCode(code); // clear the T9 code if needed
				pushCharBuffer(text_entry, *text_mode, key_map_lc, key);
			}


			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
				redrawMsgEntry(disp_dev, text_entry);
			}
			else if(*disp_state==DISP_TEXT_VALUE_ENTRY)
			{
				redrawValueEntry(disp_dev, text_entry);
			}
		break;

		case KEY_HASH:
			if(*disp_state==DISP_TEXT_MSG_ENTRY || *disp_state==DISP_TEXT_VALUE_ENTRY)
			{
				if(*text_mode==TEXT_LOWERCASE)
					*text_mode = TEXT_UPPERCASE;
				else if(*text_mode==TEXT_UPPERCASE)
					*text_mode = TEXT_T9;
				else
					*text_mode = TEXT_LOWERCASE;

				redrawTextEntryIcon(disp_dev, *text_mode);
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
