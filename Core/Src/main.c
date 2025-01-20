/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>

#include "usbd_cdc_if.h"

#include "fonts.h"
#include "../t9/dict_en.h"
#include "../t9/t9.h"
#include <m17.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RES_X					84
#define RES_Y					48
#define DISP_BUFF_SIZ			(RES_X*RES_Y/8)

#define FIX_TIMER_TRIGGER(handle_ptr) (__HAL_TIM_CLEAR_FLAG(handle_ptr, TIM_SR_UIF))
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

DAC_HandleTypeDef hdac;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim7;

UART_HandleTypeDef huart4;

/* USER CODE BEGIN PV */
uint8_t disp_buff[DISP_BUFF_SIZ];

typedef enum
{
	TEXT_T9,
    TEXT_NORM
} text_entry_t;

text_entry_t text_mode = TEXT_NORM;

typedef enum
{
	DISP_NONE,
	DISP_SPLASH,
	DISP_MAIN_SCR,
	DISP_MENU,
    DISP_TEXT_ENTRY
} disp_state_t;

disp_state_t disp_state = DISP_NONE;

typedef enum
{
	KEY_NONE,
	KEY_OK,
	KEY_C,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_1,
	KEY_2,
	KEY_3,
	KEY_4,
	KEY_5,
	KEY_6,
	KEY_7,
	KEY_8,
	KEY_9,
	KEY_ASTERISK,
	KEY_0,
	KEY_HASH
} key_t;

//T9 related variables
volatile char code[15]="";
char message[128]="";
volatile uint8_t pos=0;

//settings (volatile!)
typedef struct
{
	char callsign[12];
	char welcome_msg[2][24];

	uint8_t backlight;

	char ch_name[24];
	uint32_t rx_frequency;
	uint32_t tx_frequency;
} dev_settings_t;

dev_settings_t dev_settings =
{
	"SP5WWP",
	{"Welcome", "message"},

	200,

	"SR5MS",
	431212500,
	438812500
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_DAC_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_UART4_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM7_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//interrupts
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	//text entry timer
	if(htim->Instance==TIM7)
	{
		HAL_TIM_Base_Stop(&htim7);
		TIM7->CNT=0;
	}
}

//T9 related
char *addCode(char *code, char symbol)
{
	code[strlen(code)] = symbol;

	return getWord(dict_en, code);
}

//hardware
void setBacklight(uint8_t level)
{
	TIM2->CCR3 = (level>255) ? 255 : level; //limiter
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
}

void actVibr(uint8_t period)
{
	VIBR_GPIO_Port->BSRR = (uint32_t)VIBR_Pin;
	HAL_Delay(period);
	VIBR_GPIO_Port->BSRR = ((uint32_t)VIBR_Pin<<16);
}

void playBeep(uint16_t duration) //blocking
{
	//1kHz (84MHz master clock)
	TIM1->ARR = 10-1;
	TIM1->PSC = 8400-1;

	HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
	HAL_Delay(duration);
	HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
}

//display
void dispWrite(uint8_t dc, uint8_t val)
{
	HAL_GPIO_WritePin(DISP_DC_GPIO_Port, DISP_DC_Pin, dc);

	HAL_GPIO_WritePin(DISP_CE_GPIO_Port, DISP_CE_Pin, 0);
	HAL_SPI_Transmit(&hspi1, &val, 1, 10);
	HAL_GPIO_WritePin(DISP_CE_GPIO_Port, DISP_CE_Pin, 1);
}

void dispInit(void)
{
	HAL_GPIO_WritePin(DISP_DC_GPIO_Port, DISP_DC_Pin, 1);
	HAL_GPIO_WritePin(DISP_CE_GPIO_Port, DISP_CE_Pin, 1);
	HAL_GPIO_WritePin(DISP_RST_GPIO_Port, DISP_RST_Pin, 1);

	//toggle LCD reset
	HAL_GPIO_WritePin(DISP_RST_GPIO_Port, DISP_RST_Pin, 0); //RES low
	HAL_Delay(10);
	HAL_GPIO_WritePin(DISP_RST_GPIO_Port, DISP_RST_Pin, 1); //RES high
	HAL_Delay(10);

	dispWrite(0, 0x21);			//extended commands
	dispWrite(0, 0x80|0x48);	//contrast (0x00 to 0x7F)
	dispWrite(0, 0x06);			//temp coeff.
	dispWrite(0, 0x13);			//LCD bias 1:48
	dispWrite(0, 0x20);			//standard commands
	dispWrite(0, 0x0C);			//normal mode
}

void dispGotoXY(uint8_t x, uint8_t y)
{
	dispWrite(0, 0x80|(6*x));
	dispWrite(0, 0x40|y);
}

void dispRefresh(uint8_t buff[DISP_BUFF_SIZ])
{
	dispGotoXY(0, 0);

	for(uint16_t i=0; i<DISP_BUFF_SIZ; i++)
		dispWrite(1, buff[i]);
}

void dispClear(uint8_t buff[DISP_BUFF_SIZ], uint8_t fill)
{
	uint8_t val = fill ? 0xFF : 0;

	memset(buff, val, DISP_BUFF_SIZ);

	dispGotoXY(0, 0);

    for(uint16_t i=0; i<DISP_BUFF_SIZ; i++)
    	dispWrite(1, val);
}

void setPixel(uint8_t buff[DISP_BUFF_SIZ], uint8_t x, uint8_t y, uint8_t set)
{
	if(x<RES_X && y<RES_Y)
	{
		uint16_t loc = x + (y/8)*RES_X;

		if(!set)
			buff[loc] |= (1<<(y%8));
		else
			buff[loc] &= ~(1<<(y%8));
	}
}

void setChar(uint8_t buff[DISP_BUFF_SIZ], uint8_t x, uint8_t y, const font_t *f, char c, uint8_t color)
{
	uint8_t h=f->height;
	c-=' ';

	for(uint8_t i=0; i<h; i++)
	{
		for(uint8_t j=0; j<f->symbol[(uint8_t)c].width; j++)
		{
			if(f->symbol[(uint8_t)c].rows[i] & 1<<(((h>8)?(h+3):(h))-1-j)) //fonts are right-aligned
				setPixel(buff, x+j, y+i, color);
		}
	}
}

//TODO: fix multiline text alignment when not in ALIGN_LEFT mode
void setString(uint8_t buff[DISP_BUFF_SIZ], uint8_t x, uint8_t y, const font_t *f, char *str, uint8_t color, align_t align)
{
	uint8_t xp=0, w=0;

    //get width
    for(uint8_t i=0; i<strlen(str); i++)
		w+=f->symbol[str[i]-' '].width;

	switch(align)
	{
		case ALIGN_LEFT:
			xp=0;
		break;

		case ALIGN_CENTER:
			xp=(RES_X-w)/2;
		break;

		case ALIGN_RIGHT:
			xp=RES_X-w;
		break;

		case ALIGN_ARB:
			xp=x;
		break;

		default: //ALIGN_LEFT
			xp=0;
        break;
	}

	for(uint8_t i=0; i<strlen(str); i++)
	{
		if(xp > RES_X-f->symbol[str[i]-' '].width)
		{
			y+=f->height+1;
			xp=0; //ALIGN_LEFT
		}

		setChar(buff, xp, y, f, str[i], color);
		xp+=f->symbol[str[i]-' '].width;
	}

	dispRefresh(buff);
}

void drawRect(uint8_t buff[DISP_BUFF_SIZ], uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color, uint8_t fill)
{
	x0 = (x0>=RES_X) ? RES_X-1 : x0;
	y0 = (y0>=RES_Y) ? RES_Y-1 : y0;
	x1 = (x1>=RES_X) ? RES_X-1 : x1;
	y1 = (y1>=RES_Y) ? RES_Y-1 : y1;

	if(fill)
	{
		for(uint8_t i=x0; i<=x1; i++)
		{
			for(uint8_t j=y0; j<=y1; j++)
			{
				setPixel(buff, i, j, color);
            }
		}
	}
	else
	{
		for(uint8_t i=x0; i<=x1; i++)
		{
			setPixel(buff, i, y0, color);
			setPixel(buff, i, y1, color);
		}
		for(uint8_t i=y0+1; i<=y1-1; i++)
		{
			setPixel(buff, x0, i, color);
			setPixel(buff, x1, i, color);
        }
    }

	dispRefresh(buff);
}

void showMainScreen(uint8_t buff[DISP_BUFF_SIZ])
{
	char str[24];

	disp_state = DISP_MAIN_SCR;
	dispClear(buff, 0);

	setString(buff, 0, 0, &nokia_small, "M17", 0, ALIGN_LEFT);

	setString(buff, 0, 9, &nokia_big, dev_settings.ch_name, 0, ALIGN_CENTER);

	sprintf(str, "%ld.%04ld",
			dev_settings.rx_frequency/1000000,
			dev_settings.rx_frequency - (dev_settings.rx_frequency/1000000)*1000000);
	setString(buff, 0, 24, &nokia_small, str, 0, ALIGN_CENTER);

	sprintf(str, "%ld.%04ld",
			dev_settings.tx_frequency/1000000,
			dev_settings.tx_frequency - (dev_settings.tx_frequency/1000000)*1000000);
	setString(buff, 0, 32, &nokia_small, str, 0, ALIGN_CENTER);

	;
}

void dispSplash(uint8_t buff[DISP_BUFF_SIZ], char *line1, char *line2, char *callsign)
{
	setBacklight(0);

	setString(buff, 0, 9, &nokia_big, line1, 0, ALIGN_CENTER);
	setString(buff, 0, 22, &nokia_big, line2, 0, ALIGN_CENTER);
	setString(buff, 0, 41, &nokia_small, callsign, 0, ALIGN_CENTER);

	//fade in
	for(uint8_t i=0; i<dev_settings.backlight; i++)
	{
		setBacklight(i);
		HAL_Delay(5);
	}
}

void showTextEntry(uint8_t buff[DISP_BUFF_SIZ], disp_state_t *disp_state, text_entry_t text_mode)
{
	*disp_state = DISP_TEXT_ENTRY;
	dispClear(buff, 0);

    if(text_mode==TEXT_T9)
		setString(buff, 0, 0, &nokia_small, "T9", 0, ALIGN_LEFT);
	else
		setString(buff, 0, 0, &nokia_small, "abc", 0, ALIGN_LEFT);

	setString(buff, 0, RES_Y-8, &nokia_small_bold, "Options", 0, ALIGN_CENTER);
}

void showMenu(uint8_t buff[DISP_BUFF_SIZ], disp_state_t *disp_state, char *title)
{
    *disp_state = DISP_MENU;
    dispClear(buff, 0);

	setString(buff, 0, 0, &nokia_small_bold, title, 0, ALIGN_CENTER);

    drawRect(buff, 0, 8, RES_X-1, 2*9-1, 0, 1);
	setString(buff, 1, 1*9, &nokia_small, (char*)"Messaging", 1, ALIGN_ARB);
	setString(buff, 1, 2*9, &nokia_small, (char*)"RF settings", 0, ALIGN_ARB);
	setString(buff, 1, 3*9, &nokia_small, (char*)"M17 settings", 0, ALIGN_ARB);
	setString(buff, 1, 4*9, &nokia_small, (char*)"Misc.", 0, ALIGN_ARB);
}

key_t scanKeys(void)
{
	key_t key = KEY_NONE;

	//PD2 down means KEY_OK is pressed
	if(BTN_OK_GPIO_Port->IDR & BTN_OK_Pin)
		return KEY_OK;

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
	if(key!=KEY_NONE) return key;

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
	if(key!=KEY_NONE) return key;

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
	return key;
}

void handleKey(uint8_t buff[DISP_BUFF_SIZ], disp_state_t disp_state, text_entry_t *text_mode, key_t key)
{
	switch(key)
	{
		case KEY_OK:
			/*drawRect(buff, 0, 41, RES_X-1, RES_Y-1, 1, 1);
			char line[16]="";
			//sprintf(line, "pos=%d", pos);
			//sprintf(line, "strlen=%d", strlen(message));
			sprintf(line, "cnt=%lu", TIM7->CNT);
			setString(buff, 0, 41, &nokia_small, line, 0, ALIGN_LEFT);*/
		break;

		case KEY_C:
			if(disp_state==DISP_MENU)
			{
				showMainScreen(buff);
		    }
			else if(disp_state==DISP_TEXT_ENTRY)
			{
				if(strlen(message)>0)
				{
					memset(&message[strlen(message)-1], 0, sizeof(message)-strlen(message));
					pos=strlen(message);
					memset((char*)code, 0, strlen((char*)code));

					drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
					setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
				}
			}
		break;

		case KEY_LEFT:
			;
		break;

		case KEY_RIGHT:
			pos=strlen(message);
			memset((char*)code, 0, strlen((char*)code));
		break;

		case KEY_1:
			HAL_TIM_Base_Stop(&htim7);
			pos=strlen(message);
			char *last = &message[pos>0 ? pos-1 : 0];

			if(TIM7->CNT>0)
			{
				if(*last=='.')
					*last=',';
				else if(*last==',')
					*last='\'';
				else if(*last=='\'')
					*last='!';
				else if(*last=='!')
					*last='?';
				else if(*last=='?')
					*last='1';
				else if(*last=='1')
					*last='.';
				else
					message[pos] = '.';
			}
			else
			{
				message[pos>0 ? pos : 0] = '.';
			}

			memset((char*)code, 0, strlen((char*)code));

			drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
			setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);

			TIM7->CNT=0;
			HAL_TIM_Base_Start_IT(&htim7);
		break;

		case KEY_2:
			if(*text_mode==TEXT_T9)
			{
				char *w = addCode((char*)code, '2');

				if(strlen(w)!=0)
				{
					strcpy(&message[pos], w);
				}
				else
				{
					message[strlen(message)]='?';
				}
			}
			else
			{
				HAL_TIM_Base_Stop(&htim7);
				pos=strlen(message);
				char *last = &message[pos>0 ? pos-1 : 0];

				if(TIM7->CNT>0)
				{
					switch(*last)
					{
						case('a'):
							*last = 'b';
						break;

						case('b'):
							*last = 'c';
						break;

						case('c'):
							*last = '2';
						break;

						case('2'):
							*last = 'a';
						break;

						default:
							message[pos] = 'a';
						break;
					}
				}
				else
				{
					message[pos>0 ? pos : 0] = 'a';
				}

				TIM7->CNT=0;
				HAL_TIM_Base_Start_IT(&htim7);
			}

			drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
			setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
		break;

		case KEY_3:
			if(*text_mode==TEXT_T9)
			{
				char *w = addCode((char*)code, '3');

				if(strlen(w)!=0)
				{
					strcpy(&message[pos], w);
				}
				else
				{
					message[strlen(message)]='?';
				}
			}
			else
			{
				HAL_TIM_Base_Stop(&htim7);
				pos=strlen(message);
				char *last = &message[pos>0 ? pos-1 : 0];

				if(TIM7->CNT>0)
				{
					switch(*last)
					{
						case('d'):
							*last = 'e';
						break;

						case('e'):
							*last = 'f';
						break;

						case('f'):
							*last = '3';
						break;

						case('3'):
							*last = 'd';
						break;

						default:
							message[pos] = 'd';
						break;
					}
				}
				else
				{
					message[pos>0 ? pos : 0] = 'd';
				}

				TIM7->CNT=0;
				HAL_TIM_Base_Start_IT(&htim7);
			}

			drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
			setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
		break;

		case KEY_4:
			if(*text_mode==TEXT_T9)
			{
				char *w = addCode((char*)code, '4');

				if(strlen(w)!=0)
				{
					strcpy(&message[pos], w);
				}
				else
				{
					message[strlen(message)]='?';
				}
			}
			else
			{
				HAL_TIM_Base_Stop(&htim7);
				pos=strlen(message);
				char *last = &message[pos>0 ? pos-1 : 0];

				if(TIM7->CNT>0)
				{
					switch(*last)
					{
						case('g'):
							*last = 'h';
						break;

						case('h'):
							*last = 'i';
						break;

						case('i'):
							*last = '4';
						break;

						case('4'):
							*last = 'g';
						break;

						default:
							message[pos] = 'g';
						break;
					}
				}
				else
				{
					message[pos>0 ? pos : 0] = 'g';
				}

				TIM7->CNT=0;
				HAL_TIM_Base_Start_IT(&htim7);
			}

			drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
			setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
		break;

		case KEY_5:
			if(*text_mode==TEXT_T9)
			{
				char *w = addCode((char*)code, '5');

				if(strlen(w)!=0)
				{
					strcpy(&message[pos], w);
				}
				else
				{
					message[strlen(message)]='?';
				}
			}
			else
			{
				HAL_TIM_Base_Stop(&htim7);
				pos=strlen(message);
				char *last = &message[pos>0 ? pos-1 : 0];

				if(TIM7->CNT>0)
				{
					switch(*last)
					{
						case('j'):
							*last = 'k';
						break;

						case('k'):
							*last = 'l';
						break;

						case('l'):
							*last = '5';
						break;

						case('5'):
							*last = 'j';
						break;

						default:
							message[pos] = 'j';
						break;
					}
				}
				else
				{
					message[pos>0 ? pos : 0] = 'j';
				}

				TIM7->CNT=0;
				HAL_TIM_Base_Start_IT(&htim7);
		    }

			drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
			setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
		break;

		case KEY_6:
			if(*text_mode==TEXT_T9)
			{
				char *w = addCode((char*)code, '6');

				if(strlen(w)!=0)
				{
					strcpy(&message[pos], w);
				}
				else
				{
					message[strlen(message)]='?';
				}
			}
			else
			{
				HAL_TIM_Base_Stop(&htim7);
				pos=strlen(message);
				char *last = &message[pos>0 ? pos-1 : 0];

				if(TIM7->CNT>0)
				{
					switch(*last)
					{
						case('m'):
							*last = 'n';
						break;

						case('n'):
							*last = 'o';
						break;

						case('o'):
							*last = '6';
						break;

						case('6'):
							*last = 'm';
						break;

						default:
							message[pos] = 'm';
						break;
					}
				}
				else
				{
					message[pos>0 ? pos : 0] = 'm';
				}

				TIM7->CNT=0;
				HAL_TIM_Base_Start_IT(&htim7);
			}

			drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
			setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
		break;

		case KEY_7:
			if(*text_mode==TEXT_T9)
			{
				char *w = addCode((char*)code, '7');

				if(strlen(w)!=0)
				{
					strcpy(&message[pos], w);
				}
				else
				{
					message[strlen(message)]='?';
				}
			}
			else
			{
				HAL_TIM_Base_Stop(&htim7);
				pos=strlen(message);
				char *last = &message[pos>0 ? pos-1 : 0];

				if(TIM7->CNT>0)
				{
					switch(*last)
					{
						case('p'):
							*last = 'q';
						break;

						case('q'):
							*last = 'r';
						break;

						case('r'):
							*last = 's';
						break;

						case('s'):
							*last = '7';
						break;

						case('7'):
							*last = 'p';
						break;

						default:
							message[pos] = 'p';
						break;
					}
				}
				else
				{
					message[pos>0 ? pos : 0] = 'p';
				}

				TIM7->CNT=0;
				HAL_TIM_Base_Start_IT(&htim7);
			}

			drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
			setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
		break;

		case KEY_8:
			if(*text_mode==TEXT_T9)
			{
				char *w = addCode((char*)code, '8');

				if(strlen(w)!=0)
				{
					strcpy(&message[pos], w);
				}
				else
				{
					message[strlen(message)]='?';
				}
			}
			else
			{
				HAL_TIM_Base_Stop(&htim7);
				pos=strlen(message);
				char *last = &message[pos>0 ? pos-1 : 0];

				if(TIM7->CNT>0)
				{
					switch(*last)
					{
						case('t'):
							*last = 'u';
						break;

						case('u'):
							*last = 'v';
						break;

						case('v'):
							*last = '8';
						break;

						case('8'):
							*last = 't';
						break;

						default:
							message[pos] = 't';
						break;
					}
				}
				else
				{
					message[pos>0 ? pos : 0] = 't';
				}

				TIM7->CNT=0;
				HAL_TIM_Base_Start_IT(&htim7);
			}

			drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
			setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
		break;

		case KEY_9:
			if(*text_mode==TEXT_T9)
			{
				char *w = addCode((char*)code, '9');

				if(strlen(w)!=0)
				{
					strcpy(&message[pos], w);
				}
				else
				{
					message[strlen(message)]='?';
				}
			}
			else
			{
				HAL_TIM_Base_Stop(&htim7);
				pos=strlen(message);
				char *last = &message[pos>0 ? pos-1 : 0];

				if(TIM7->CNT>0)
				{
					switch(*last)
					{
						case('w'):
							*last = 'x';
						break;

						case('x'):
							*last = 'y';
						break;

						case('y'):
							*last = 'z';
						break;

						case('z'):
							*last = '9';
						break;

						case('9'):
							*last = 'w';
						break;

						default:
							message[pos] = 'w';
						break;
					}
				}
				else
				{
					message[pos>0 ? pos : 0] = 'w';
				}

				TIM7->CNT=0;
				HAL_TIM_Base_Start_IT(&htim7);
			}

			drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
			setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
		break;

		case KEY_ASTERISK:
			if(*text_mode==TEXT_T9)
			{
				char *w = addCode((char*)code, '*');

				if(strlen(w)!=0)
				{
					strcpy(&message[pos], w);
				}
			}
			else
			{
				HAL_TIM_Base_Stop(&htim7);
				pos=strlen(message);
				char *last = &message[pos>0 ? pos-1 : 0];

				if(TIM7->CNT>0)
				{
					switch(*last)
					{
						case('*'):
							*last = '+';
						break;

						case('+'):
							*last = '*';
						break;

						default:
							message[pos] = '*';
						break;
					}
				}
				else
				{
					message[pos>0 ? pos : 0] = '*';
				}

				TIM7->CNT=0;
				HAL_TIM_Base_Start_IT(&htim7);
			}

			drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
			setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
		break;

		case KEY_0:
			message[strlen(message)]=' ';
		    pos=strlen(message);

			drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
		    setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);

			memset((char*)code, 0, strlen((char*)code));
		break;

		case KEY_HASH:
			if(disp_state==DISP_TEXT_ENTRY)
			{
				if(*text_mode==TEXT_T9)
					*text_mode = TEXT_NORM;
				else
					*text_mode = TEXT_T9;

				drawRect(buff, 0, 0, 15, 8, 1, 1);

				if(*text_mode==TEXT_T9)
					setString(buff, 0, 0, &nokia_small, (char*)"T9", 0, ALIGN_LEFT);
				else
					setString(buff, 0, 0, &nokia_small, (char*)"abc", 0, ALIGN_LEFT);
			}
		break;

		default:
			;
		break;
	}
}

//RF module
void setRegRF(uint8_t reg, uint16_t val)
{
	char data[64];

	sprintf(data, "AT+POKE=%d,%d\r\n", reg, val);
	HAL_UART_Transmit(&huart4, (uint8_t*)"AT+VERSION\r\n", 12, 10);
}

void initRF(void)
{
	uint8_t data[64];

	//turn on the module
	HAL_GPIO_WritePin(RF_ENA_GPIO_Port, RF_ENA_Pin, 1);
	HAL_Delay(50);

	memset(data, 0, 64);
	HAL_UART_Transmit(&huart4, (uint8_t*)"AT+VERSION\r\n", 12, 10);
	HAL_UART_Receive(&huart4, data, 64, 200); //naive, blocking

	//display the received data
	uint8_t len=strlen((char*)data);
	if(len)
		CDC_Transmit_FS(data, len);
	else
		CDC_Transmit_FS((uint8_t*)"Nothing received\n", 17);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_DAC_Init();
  MX_RTC_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_UART4_Init();
  MX_SPI1_Init();
  MX_TIM7_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(1000);

  dispInit();
  dispClear(disp_buff, 0);
  setBacklight(0);

  dispSplash(disp_buff, dev_settings.welcome_msg[0], dev_settings.welcome_msg[1], dev_settings.callsign);
  //playBeep(100);
  HAL_Delay(1000);

  showMainScreen(disp_buff);
  HAL_Delay(1000);

  showMenu(disp_buff, &disp_state, "Main menu");
  HAL_Delay(1000);
  initRF();

  showTextEntry(disp_buff, &disp_state, text_mode);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while(1)
  {
	  handleKey(disp_buff, disp_state, &text_mode, scanKeys());
	  HAL_Delay(100);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief DAC Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC_Init(void)
{

  /* USER CODE BEGIN DAC_Init 0 */

  /* USER CODE END DAC_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC_Init 1 */

  /* USER CODE END DAC_Init 1 */

  /** DAC Initialization
  */
  hdac.Instance = DAC;
  if (HAL_DAC_Init(&hdac) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC_Init 2 */

  /* USER CODE END DAC_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 8400-1;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 10-1;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 4;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_LOW;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 1600-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 256-1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM7 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM7_Init(void)
{

  /* USER CODE BEGIN TIM7_Init 0 */

  /* USER CODE END TIM7_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM7_Init 1 */

  /* USER CODE END TIM7_Init 1 */
  htim7.Instance = TIM7;
  htim7.Init.Prescaler = 8400-1;
  htim7.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim7.Init.Period = 5000-1;
  htim7.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim7) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_OnePulse_Init(&htim7, TIM_OPMODE_SINGLE) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim7, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM7_Init 2 */

  /* USER CODE END TIM7_Init 2 */

}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 9600;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, RF_PWR_Pin|COL_3_Pin|COL_2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(RF_PTT_GPIO_Port, RF_PTT_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(VIBR_GPIO_Port, VIBR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, COL_1_Pin|PWR_OFF_Pin|DISP_CE_Pin|DISP_DC_Pin
                          |DISP_RST_Pin|RF_ENA_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : RF_PWR_Pin RF_PTT_Pin */
  GPIO_InitStruct.Pin = RF_PWR_Pin|RF_PTT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : COL_3_Pin COL_2_Pin */
  GPIO_InitStruct.Pin = COL_3_Pin|COL_2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : VIBR_Pin */
  GPIO_InitStruct.Pin = VIBR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(VIBR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : CHG_Pin ROW_3_Pin ROW_2_Pin ROW_1_Pin */
  GPIO_InitStruct.Pin = CHG_Pin|ROW_3_Pin|ROW_2_Pin|ROW_1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : COL_1_Pin DISP_CE_Pin DISP_DC_Pin DISP_RST_Pin */
  GPIO_InitStruct.Pin = COL_1_Pin|DISP_CE_Pin|DISP_DC_Pin|DISP_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PWR_OFF_Pin RF_ENA_Pin */
  GPIO_InitStruct.Pin = PWR_OFF_Pin|RF_ENA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : ROW_5_Pin ROW_4_Pin */
  GPIO_InitStruct.Pin = ROW_5_Pin|ROW_4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN_OK_Pin */
  GPIO_InitStruct.Pin = BTN_OK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(BTN_OK_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
