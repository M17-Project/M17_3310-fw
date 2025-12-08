#include "keypad.h"

//T9 related
const char *addCode(char *code, char symbol)
{
	code[strlen(code)] = symbol;
	return getWord(dict, code);
}

//scan keyboard - 'rep' milliseconds delay after a valid keypress is detected
kbd_key_t scanKeys(radio_state_t radio_state, uint8_t rep)
{
	kbd_key_t key = KEY_NONE;

	//PD2 down means KEY_OK is pressed
	if(BTN_OK_GPIO_Port->IDR & BTN_OK_Pin)
	{
		if(radio_state==RF_RX)
			HAL_Delay(rep);
		return KEY_OK;
	}

	//column 1
	COL_1_GPIO_Port->BSRR = (uint32_t)COL_1_Pin;
	HAL_Delay(1);
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
	if(key!=KEY_NONE)
	{
		if(radio_state==RF_RX)
			HAL_Delay(rep);
		return key;
	}

	//column 2
	COL_2_GPIO_Port->BSRR = (uint32_t)COL_2_Pin;
	HAL_Delay(1);
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
	if(key!=KEY_NONE)
	{
		if(radio_state==RF_RX)
			HAL_Delay(rep);
		return key;
	}

	//column 3
	COL_3_GPIO_Port->BSRR = (uint32_t)COL_3_Pin;
	HAL_Delay(1);
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
	if(key!=KEY_NONE)
	{
		if(radio_state==RF_RX)
			HAL_Delay(rep);
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
			if(key==KEY_0 && text_mode==TEXT_T9)
			{
				pos++;
			}
		}
	}
	else
	{
		text_entry[pos] = key_chars[0];
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
	if(key!=KEY_ASTERISK)
		c = '2'+key-KEY_2;
	else
		c = '*';
	const char *w = addCode(code, c);

	if(strlen(w)!=0)
	{
		strcpy(&text_entry[pos], w);
	}
	else if(key!=KEY_ASTERISK)
	{
		text_entry[strlen(text_entry)]='?';
	}
}

void handleKey(disp_dev_t *disp_dev, disp_state_t *disp_state, char *text_entry, char *code,
		text_entry_t *text_mode, radio_state_t *radio_state, dev_settings_t *dev_settings,
		kbd_key_t key, edit_set_t *edit_set)
{
	//backlight on
	if(key!=KEY_NONE && dev_settings->backlight_always==0)
	{
		if(TIM14->CNT==0)
		{
			setBacklight(dev_settings->backlight_level);
			FIX_TIMER_TRIGGER(&htim14);
			HAL_TIM_Base_Start_IT(&htim14);
		}
		else
		{
			TIM14->CNT=0;
		}
	}

	//find out where to go next
	uint8_t item = menu_pos + menu_pos_hl;
	disp_state_t next_disp = displays[*disp_state].next_disp[item];

	//do something based on the key pressed and current state
	switch(key)
	{
		case KEY_OK:
			//dbg_print("[Debug] Start disp_state: %d\n", *disp_state);

			if(next_disp != DISP_NONE)
			{
				menu_pos=menu_pos_hl=0;
			}

			//main screen
			if(*disp_state==DISP_MAIN_SCR)
			{
				//clear the text entry buffer before entering the main menu
				//TODO: this will have to be moved later
				memset(text_entry, 0, strlen(text_entry));

				*disp_state = next_disp;
				showMenu(disp_dev, displays[*disp_state], 0, 0);
			}

			//main menu
			else if(*disp_state==DISP_MAIN_MENU)
			{
				*disp_state = next_disp;

				if(item==0) //"Messaging"
				{
					showTextMessageEntry(disp_dev, *text_mode);
				}
				else if(item==1) //"Settings"
				{
					//load settings from NVMEM
					loadDeviceSettings(dev_settings, &def_dev_settings);
					showMenu(disp_dev, displays[*disp_state], 0, 0);
				}
				else
				{
					showMenu(disp_dev, displays[*disp_state], 0, 0);
				}
			}

			//Radio settings
			else if(*disp_state==DISP_RADIO_SETTINGS)
			{
				if(item==0)
					*edit_set=EDIT_RF_PPM;
				else if(item==1)
					*edit_set=EDIT_RF_PWR;

				//all menu entries are editable
				*disp_state = next_disp;
				showTextValueEntry(disp_dev, *text_mode);
			}

			//M17 settings
			else if(*disp_state==DISP_M17_SETTINGS)
			{
				if(item==0)
					*edit_set=EDIT_M17_SRC_CALLSIGN;
				else if(item==1)
					*edit_set=EDIT_M17_DST_CALLSIGN;
				else if(item==2)
					*edit_set=EDIT_M17_CAN;

				//all menu entries are editable
				*disp_state = next_disp;
				showTextValueEntry(disp_dev, *text_mode);
			}

			//text message entry - send message
			else if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
				//initialize packet transmission
				if(*radio_state==RF_RX)
				{
					initTextTX(text_entry);
				}
			}

			else if(*disp_state==DISP_TEXT_VALUE_ENTRY)
			{
				if(*edit_set==EDIT_RF_PPM)
				{
					float val = atof(text_entry);
					if(fabsf(val)<=50.0f)
					{
						dev_settings->freq_corr = val;
						//TODO: fix how the frequency correction is applied for TX
						if(radio_state==RF_RX)
						{
							setFreqRF(dev_settings->channel.rx_frequency, dev_settings->freq_corr);
						}
						/*else //i believe this is an impossible case
						{
							setFreqRF(dev_settings->channel.tx_frequency, dev_settings->freq_corr);
						}*/
					}
				}
				else if(*edit_set==EDIT_RF_PWR)
				{
					float val = atof(text_entry);
					if(val>1.0f)
					{
						dev_settings->channel.rf_pwr=RF_PWR_HIGH; //2W output power
						HAL_GPIO_WritePin(RF_PWR_GPIO_Port, RF_PWR_Pin, 0);
					}
					else
					{
						dev_settings->channel.rf_pwr=RF_PWR_LOW; //0.5W output power
						HAL_GPIO_WritePin(RF_PWR_GPIO_Port, RF_PWR_Pin, 1);
					}
				}
				else if(*edit_set==EDIT_M17_SRC_CALLSIGN)
				{
					memset(dev_settings->src_callsign, 0, sizeof(dev_settings->src_callsign));
					strcpy(dev_settings->src_callsign, text_entry);
				}
				else if(*edit_set==EDIT_M17_DST_CALLSIGN)
				{
					memset(dev_settings->channel.dst, 0, sizeof(dev_settings->channel.dst));
					strcpy(dev_settings->channel.dst, text_entry);
				}
				else if(*edit_set==EDIT_M17_CAN)
				{
					uint8_t val = atoi(text_entry);
					if(val<16)
						dev_settings->channel.can = val;
				}

				*edit_set = EDIT_NONE;
				saveData(dev_settings, MEM_START, sizeof(dev_settings_t));

				*disp_state = DISP_MAIN_SCR;
				showMainScreen(disp_dev);
			}

			//debug menu
			else if(*disp_state==DISP_DEBUG)
			{
				if(item==0)
				{
					debug_tx ^= 1;
					//initialize dummy transmission
					if(*radio_state==RF_RX && debug_tx)
					{
						;//initDebugTX();
					}
				}
			}

			//settings or info
			else /*if(*disp_state==DISP_SETTINGS || \
					*disp_state==DISP_INFO)*/
			{
				if(next_disp!=DISP_NONE)
				{
					*disp_state = next_disp;
					showMenu(disp_dev, displays[*disp_state], 0, 0);
				}
			}

			//
			/*else
			{
				;
			}*/

			//dbg_print("[Debug] End disp_state: %d\n", *disp_state);
		break;

		case KEY_C:
			menu_pos=menu_pos_hl=0;
			next_disp = displays[*disp_state].prev_disp;

			//dbg_print("[Debug] Start disp_state: %d\n", *disp_state);

			//main screen
			if(*disp_state==DISP_MAIN_SCR)
			{
				; //nothing
			}

			//main menu
			else if(*disp_state==DISP_MAIN_MENU)
			{
				*disp_state = next_disp;
				showMainScreen(disp_dev);
		    }

			//text message entry
			else if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
				//backspace
				if(strlen(text_entry)>0)
				{
					//memset(&text_entry[strlen(text_entry)-1], 0, sizeof(text_entry)-strlen(text_entry));
					text_entry[strlen(text_entry)-1] = 0;
					pos=strlen(text_entry);
					if (*code)
						memset(code, 0, strlen(code)); // clear T9 code only when required

					drawRect(disp_dev, 0, 10, RES_X-1, RES_Y-9, 1, 1);
					setString(disp_dev, 0, 10, &nokia_small, text_entry, 0, ALIGN_LEFT);
				}
				else
				{
					*disp_state = next_disp;
					showMenu(disp_dev, displays[*disp_state], 0, 0);
				}
			}

			//text value entry
			else if(*disp_state==DISP_TEXT_VALUE_ENTRY)
			{
				//backspace
				if(strlen(text_entry)>0)
				{
					//memset(&text_entry[strlen(text_entry)-1], 0, sizeof(text_entry)-strlen(text_entry));
					text_entry[strlen(text_entry)-1] = 0;
					pos=strlen(text_entry);
					if (*code)
						memset(code, 0, strlen(code)); // clear T9 code only when required

					drawRect(disp_dev, 1, 10, RES_X-2, RES_Y-12, 1, 1);
					setString(disp_dev, 3, 13, &nokia_big, text_entry, 0, ALIGN_ARB);
				}
				else
				{
					*disp_state = next_disp;
					showMainScreen(disp_dev);
				}
			}

			//anything else
			else
			{
				*disp_state = displays[*disp_state].prev_disp;
				showMenu(disp_dev, displays[*disp_state], 0, 0);
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
							(dev_settings->channel.tx_frequency - (dev_settings->channel.tx_frequency/1000000)*1000000)/100);
					drawRect(disp_dev, 0, 36, RES_X-1, 36+8, 1, 1);
					setString(disp_dev, 0, 36, &nokia_small, str, 0, ALIGN_CENTER);
				}
				else// if(dev_settings->tuning_mode==TUNING_MEM)
				{
					;
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
				showMenu(disp_dev, displays[*disp_state], menu_pos, menu_pos_hl);
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
							(dev_settings->channel.tx_frequency - (dev_settings->channel.tx_frequency/1000000)*1000000)/100);
					drawRect(disp_dev, 0, 36, RES_X-1, 36+8, 1, 1);
					setString(disp_dev, 0, 36, &nokia_small, str, 0, ALIGN_CENTER);
				}
				else// if(dev_settings->tuning_mode==TUNING_MEM)
				{
					;
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
				showMenu(disp_dev, displays[*disp_state], menu_pos, menu_pos_hl);
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
				if (*code)
					memset(code, 0, strlen(code)); // clear the T9 code if needed
				pushCharBuffer(text_entry, *text_mode, key_map_lc, key);
			}

			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
				drawRect(disp_dev, 0, 10, RES_X-1, RES_Y-9, 1, 1);
				setString(disp_dev, 0, 10, &nokia_small, text_entry, 0, ALIGN_LEFT);
			}
			else if(*disp_state==DISP_TEXT_VALUE_ENTRY)
			{
				drawRect(disp_dev, 1, 10, RES_X-2, RES_Y-12, 1, 1);
				setString(disp_dev, 3, 13, &nokia_big, text_entry, 0, ALIGN_ARB);
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
				drawRect(disp_dev, 0, 10, RES_X-1, RES_Y-9, 1, 1);
				setString(disp_dev, 0, 10, &nokia_small, text_entry, 0, ALIGN_LEFT);
			}
			else if(*disp_state==DISP_TEXT_VALUE_ENTRY)
			{
				drawRect(disp_dev, 1, 10, RES_X-2, RES_Y-12, 1, 1);
				setString(disp_dev, 3, 13, &nokia_big, text_entry, 0, ALIGN_ARB);
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
				if (*code)
					memset(code, 0, strlen(code)); // clear the T9 code if needed
				pushCharBuffer(text_entry, *text_mode, key_map_lc, key);
			}


			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
				drawRect(disp_dev, 0, 10, RES_X-1, RES_Y-9, 1, 1);
				setString(disp_dev, 0, 10, &nokia_small, text_entry, 0, ALIGN_LEFT);
			}
			else if(*disp_state==DISP_TEXT_VALUE_ENTRY)
			{
				drawRect(disp_dev, 1, 10, RES_X-2, RES_Y-12, 1, 1);
				setString(disp_dev, 3, 13, &nokia_big, text_entry, 0, ALIGN_ARB);
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

				drawRect(disp_dev, 0, 0, 21, 8, 1, 1);

				if(*text_mode==TEXT_LOWERCASE)
					setString(disp_dev, 0, 0, &nokia_small, ICON_PEN"abc", 0, ALIGN_LEFT);
				else if(*text_mode==TEXT_UPPERCASE)
					setString(disp_dev, 0, 0, &nokia_small, ICON_PEN"ABC", 0, ALIGN_LEFT);
				else
					setString(disp_dev, 0, 0, &nokia_small, ICON_PEN"T9", 0, ALIGN_LEFT);
			}
		break;

		default:
			;
		break;
	}
}

//set keyboard insensitivity timer
void setKeysTimeout(const uint16_t delay)
{
	TIM7->ARR=delay*10-1;
}
