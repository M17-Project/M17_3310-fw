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
#include <math.h>

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
#define FW_VER					"1.0.0"
#define DAC_IDLE				2048
#define RES_X					84
#define RES_Y					48
#define DISP_BUFF_SIZ			(RES_X*RES_Y/8)
#define MEM_START				(0x080E0000U) //last sector, 128kB
#define CH_MAX_NUM				128

#define FIX_TIMER_TRIGGER(handle_ptr) (__HAL_TIM_CLEAR_FLAG(handle_ptr, TIM_SR_UIF))
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac1;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim7;

UART_HandleTypeDef huart4;

/* USER CODE BEGIN PV */
//superloop rotation counter
uint16_t r;

//display
uint8_t disp_buff[DISP_BUFF_SIZ];

//some typedefs
typedef enum text_entry
{
	TEXT_T9,
    TEXT_NORM
} text_entry_t;

text_entry_t text_mode = TEXT_NORM;

//menus state machine
#include "menus.h"
disp_state_t disp_state = DISP_NONE;
menu_t *menu;
uint8_t menu_pos, menu_pos_hl; //menu item position, highlighted menu item position

//keys
typedef enum key
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

//settings
typedef enum ch_bw
{
	RF_BW_12K5,
	RF_BW_25K
} ch_bw_t;

typedef enum rf_mode
{
	RF_MODE_FM,
	RF_MODE_4FSK
} rf_mode_t;

typedef enum rf_power
{
	RF_PWR_LOW,
	RF_PWR_HIGH
} rf_power_t;

//T9 related variables
volatile char code[15]="";
char message[256]="";
volatile uint8_t pos=0;

//usb-related
uint8_t usb_rx[APP_RX_DATA_SIZE];
uint32_t usb_len;
uint8_t usb_drdy;

//settings
typedef struct ch_settings
{
	rf_mode_t mode;
	ch_bw_t ch_bw;
	rf_power_t rf_pwr;
	char ch_name[24];
	uint32_t rx_frequency;
	uint32_t tx_frequency;
	char dst[12];
	uint8_t can;
} ch_settings_t;

//default channel settings
const ch_settings_t def_channel =
{
	RF_MODE_4FSK,
	RF_BW_12K5,
	RF_PWR_LOW,
	"M17 IARU R1",
	433475000,
	433475000,
	"@ALL",
	0
};

typedef struct codeplug
{
	char name[24];
	uint8_t num_items;
	ch_settings_t channel[CH_MAX_NUM];
} codeplug_t;

codeplug_t codeplug;

typedef enum tuning_mode
{
	TUNING_VFO,
	TUNING_MEM
} tuning_mode_t;

typedef struct dev_settings
{
	char src_callsign[12];
	char welcome_msg[2][24];

	uint8_t backlight_level;
	uint8_t backlight_timer;
	uint8_t backlight_always;
	uint16_t kbd_timeout; //keyboard keypress timeout (for text entry)
	uint16_t kbd_delay; //insensitivity delay after keypress detection
	float freq_corr;

	tuning_mode_t tuning_mode;
	ch_settings_t channel;
	uint16_t ch_num;

	char refl_name[12];
} dev_settings_t;

dev_settings_t def_dev_settings =
{
	"N0KIA",
	{"OpenRTX", "rulez"},

	160,
	1,
	0,
	750,
	150,
	0.0f,

	TUNING_VFO,
	def_channel,
	0,

	"M17-M17 C"
};

dev_settings_t dev_settings;

//M17
uint16_t frame_samples[2][SYM_PER_FRA*10]; //sps=10
int8_t frame_symbols[SYM_PER_FRA];
lsf_t lsf;
uint8_t frame_cnt; //frame counter, preamble=0
volatile uint8_t frame_pend; //frame generation pending?
uint8_t packet_payload[33*25];
uint8_t packet_bytes;

//radio
typedef enum radio_state
{
	RF_RX,
	RF_TX
} radio_state_t;

radio_state_t radio_state;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_DAC_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
static void MX_UART4_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM7_Init(void);
static void MX_TIM6_Init(void);
/* USER CODE BEGIN PFP */
void setRF(radio_state_t state);
void setFreqRF(uint32_t freq, float corr);
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

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
	if(radio_state==RF_TX)
	{
		frame_pend=1;
	}
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
	if(radio_state==RF_TX)
	{
		HAL_DAC_Start_DMA(hdac, DAC_CHANNEL_1, (uint32_t*)&frame_samples[frame_cnt%2][0], SYM_PER_FRA*10, DAC_ALIGN_12B_R);
	}
}

//ADC
void ADC_SetActiveChannel(ADC_HandleTypeDef *hadc, uint32_t channel)
{
	ADC_ChannelConfTypeDef sConfig = {0};
	sConfig.Channel = channel;
	sConfig.Rank = 1;
	sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
	if(HAL_ADC_ConfigChannel(hadc, &sConfig)!=HAL_OK)
	{
		Error_Handler();
	}
}

uint16_t getBattVoltage(void)
{
	ADC_SetActiveChannel(&hadc1, ADC_CHANNEL_1);
	HAL_ADC_Start(&hadc1);
	while(HAL_ADC_PollForConversion(&hadc1, 5)!=HAL_OK);

	return (ADC1->DR)/4095.0f * 3300.0f * 2.0f;
}

//get baseband sample - debug
uint16_t getBasebandSample(void)
{
	ADC_SetActiveChannel(&hadc1, ADC_CHANNEL_0);
	HAL_ADC_Start(&hadc1);
	while(HAL_ADC_PollForConversion(&hadc1, 0)!=HAL_OK);

	return (ADC1->DR)/4095.0f * 3300.0f;
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
	TIM2->CCR3 = level;
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
			xp=(RES_X-w)/2+1;
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

	dispClear(buff, 0);

	sprintf(str, "%s", (dev_settings.channel.mode==RF_MODE_4FSK)?"M17":"FM");
	setString(buff, 0, 0, &nokia_small, str, 0, ALIGN_LEFT);

	setString(buff, 0, 12, &nokia_big, dev_settings.channel.ch_name, 0, ALIGN_CENTER);

	sprintf(str, "R %ld.%04ld",
			dev_settings.channel.rx_frequency/1000000,
			(dev_settings.channel.rx_frequency - (dev_settings.channel.rx_frequency/1000000)*1000000)/100);
	setString(buff, 0, 27, &nokia_small, str, 0, ALIGN_CENTER);

	sprintf(str, "T %ld.%04ld",
			dev_settings.channel.tx_frequency/1000000,
			(dev_settings.channel.tx_frequency - (dev_settings.channel.tx_frequency/1000000)*1000000)/100);
	setString(buff, 0, 36, &nokia_small, str, 0, ALIGN_CENTER);

	//is the battery charging? (read /CHG signal)
	if(!(CHG_GPIO_Port->IDR & CHG_Pin))
	{
		setString(buff, RES_X-1, 0, &nokia_small, "B+", 0, ALIGN_RIGHT); //display charging status
	}
	else
	{
		char u_batt_str[8];
		uint16_t u_batt=getBattVoltage();
		sprintf(u_batt_str, "%1d.%1d", u_batt/1000, (u_batt-(u_batt/1000)*1000)/100);
		setString(buff, RES_X-1, 0, &nokia_small, u_batt_str, 0, ALIGN_RIGHT); //display voltage
	}
}

void dispSplash(uint8_t buff[DISP_BUFF_SIZ], char *line1, char *line2, char *callsign)
{
	setBacklight(0);

	setString(buff, 0, 9, &nokia_big, line1, 0, ALIGN_CENTER);
	setString(buff, 0, 22, &nokia_big, line2, 0, ALIGN_CENTER);
	setString(buff, 0, 40, &nokia_small, callsign, 0, ALIGN_CENTER);

	//fade in
	if(dev_settings.backlight_always==1)
	{
		for(uint16_t i=0; i<dev_settings.backlight_level; i++)
		{
			setBacklight(i);
			HAL_Delay(5);
		}
	}
}

void showTextEntry(uint8_t buff[DISP_BUFF_SIZ], text_entry_t text_mode)
{
	dispClear(buff, 0);

    if(text_mode==TEXT_T9)
		setString(buff, 0, 0, &nokia_small, "T9", 0, ALIGN_LEFT);
	else
		setString(buff, 0, 0, &nokia_small, "abc", 0, ALIGN_LEFT);

	setString(buff, 0, RES_Y-8, &nokia_small_bold, "Send", 0, ALIGN_CENTER);
}

//show menu with item highlighting
//start_item - absolute index of first item to display
//h_item - item to highlight, relative value: 0..3
void showMenu(uint8_t buff[DISP_BUFF_SIZ], const menu_t menu, const uint8_t start_item, const uint8_t h_item)
{
    dispClear(buff, 0);

    setString(buff, 0, 0, &nokia_small_bold, (char*)menu.title, 0, ALIGN_CENTER);

    for(uint8_t i=0; i<start_item+menu.num_items && i<4; i++)
    {
    	//highlight
    	if(i==h_item)
    		drawRect(buff, 0, (i+1)*9-1, RES_X-1, (i+1)*9+8, 0, 1);

    	setString(buff, 1, (i+1)*9, &nokia_small, (char*)menu.item[i+start_item], (i==h_item)?1:0, ALIGN_ARB);
    	if(menu.value[i+start_item][0]!=0)
    		setString(buff, 0, (i+1)*9, &nokia_small, (char*)menu.value[i+start_item], (i==h_item)?1:0, ALIGN_RIGHT);
    }
}

//messaging - initialize text packet transmission
//note: some variables inside this function are global
void initTextTX(const char *message)
{
	uint16_t msg_len = strlen(message);

	packet_payload[0]=0x05; //packet type: SMS

	strcpy((char*)&packet_payload[1], message);

	uint16_t crc = CRC_M17(packet_payload, 1+msg_len+1);
	packet_payload[msg_len+2] = crc>>8;
	packet_payload[msg_len+3] = crc&0xFF;

	packet_bytes=1+msg_len+1+2; //type, payload, null termination, crc

	radio_state = RF_TX;
	setRF(radio_state);
	frame_cnt=0;
	frame_pend=1;
}

//scan keyboard - 'rep' milliseconds delay after a valid keypress is detected
key_t scanKeys(uint8_t rep)
{
	key_t key = KEY_NONE;

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

void handleKey(uint8_t buff[DISP_BUFF_SIZ], disp_state_t *disp_state, text_entry_t *text_mode, radio_state_t *radio_state, dev_settings_t *dev_settings, key_t key)
{
	switch(key)
	{
		case KEY_OK:
			//main screen
			if(*disp_state==DISP_MAIN_SCR)
			{
				*disp_state = menu->next_state[0];
				menu_pos=menu_pos_hl=0;
				showMenu(buff, *((menu_t*)menu->next_menu[0]), 0, 0);
				menu = (menu_t*)menu->next_menu[0];
			}

			//main menu
			else if(*disp_state==DISP_MAIN_MENU)
			{
				//find out where to go next
				uint8_t item=menu_pos+menu_pos_hl;

				if(menu->next_state[item] != DISP_NONE)
					*disp_state = menu->next_state[item];

				if(item==0) //"Messaging"
				{
					//clear message and display text entry screen
					memset(message, 0, strlen(message));
					*disp_state = menu->next_state[0];
					showTextEntry(buff, *text_mode);
					menu = (menu_t*)menu->next_menu[0];
				}
				else
				{
					if((menu_t*)menu->next_menu[item] != NULL)
					{
						menu_pos=menu_pos_hl=0;
						*disp_state = menu->next_state[item];
						showMenu(buff, *((menu_t*)menu->next_menu[item]), 0, 0);
						menu = (menu_t*)menu->next_menu[item];
					}
				}
			}

			//text message entry - send message
			else if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
				//initialize packet transmission
				if(*radio_state==RF_RX)
				{
					initTextTX(message);
				}
			}

			//settings, info or debug
			else if(*disp_state==DISP_SETTINGS || \
					*disp_state==DISP_INFO || \
					*disp_state==DISP_DEBUG)
			{
				//find out where to go next
				uint8_t item=menu_pos+menu_pos_hl;

				if((menu_t*)menu->next_menu[item] != NULL)
				{
					menu_pos=menu_pos_hl=0;
					*disp_state = menu->next_state[item];
					showMenu(buff, *((menu_t*)menu->next_menu[item]), 0, 0);
					menu = (menu_t*)menu->next_menu[item];
				}
			}

			//
			else
			{
				;
			}
		break;

		case KEY_C:
			menu_pos=menu_pos_hl=0;

			//main screen
			if(*disp_state==DISP_MAIN_SCR)
			{
				; //nothing
			}

			//main menu
			else if(*disp_state==DISP_MAIN_MENU)
			{
				*disp_state = menu->prev_state;
				showMainScreen(buff);
				menu = (menu_t*)menu->prev_menu;
		    }

			//text message entry
			else if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
				//backspace
				if(strlen(message)>0)
				{
					memset(&message[strlen(message)-1], 0, sizeof(message)-strlen(message));
					pos=strlen(message);
					memset((char*)code, 0, strlen((char*)code));

					drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
					setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
				}
			}

			//settings, info or debug
			//anything else
			else /*if(*disp_state==DISP_SETTINGS || \
					*disp_state==DISP_INFO || \
					*disp_state==DISP_DEBUG)*/
			{
				*disp_state = menu->prev_state;
				showMenu(buff, *((menu_t*)menu->prev_menu), 0, 0);
				menu = (menu_t*)menu->prev_menu;
			}

			//
			/*else
			{
				;
			}*/
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
					drawRect(buff, 0, 36, RES_X-1, 36+8, 1, 1);
					setString(buff, 0, 36, &nokia_small, str, 0, ALIGN_CENTER);
				}
				else// if(dev_settings->tuning_mode==TUNING_MEM)
				{
					;
				}
			}

			//text message entry
			else if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
				;
			}

			//other menus
			else /*if(*disp_state==DISP_SETTINGS || \
					*disp_state==DISP_INFO || \
					*disp_state==DISP_DEBUG)*/
			{
				if(menu_pos_hl==3 || menu_pos_hl==menu->num_items-1)
				{
					if(menu_pos+3<settings_menu.num_items-1)
						menu_pos++;
					else //wrap around
					{
						menu_pos=0;
						menu_pos_hl=0;
					}
				}
				else
				{
					if(menu_pos_hl<3 && menu_pos+menu_pos_hl<settings_menu.num_items-1)
						menu_pos_hl++;
				}

				//no state change
				showMenu(buff, *menu, menu_pos, menu_pos_hl);
			}

			//else
			/*else
			{
				;
			}*/
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
					drawRect(buff, 0, 36, RES_X-1, 36+8, 1, 1);
					setString(buff, 0, 36, &nokia_small, str, 0, ALIGN_CENTER);
				}
				else// if(dev_settings->tuning_mode==TUNING_MEM)
				{
					;
				}
			}

			//text message entry
			else if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
				pos=strlen(message);
				memset((char*)code, 0, strlen((char*)code));
			}

			//other menus
			else /*if(*disp_state==DISP_SETTINGS || \
					*disp_state==DISP_INFO || \
					*disp_state==DISP_DEBUG)*/
			{
				if(menu_pos_hl==0)
				{
					if(menu_pos>0)
						menu_pos--;
					else //wrap around
					{
						if(menu->num_items>3)
						{
							menu_pos=menu->num_items-1-3;
							menu_pos_hl=3;
						}
						else
						{
							menu_pos=0;
							menu_pos_hl=menu->num_items-1;
						}
					}
				}
				else
				{
					if(menu_pos_hl>0)
						menu_pos_hl--;
				}

				//no state change
				showMenu(buff, *menu, menu_pos, menu_pos_hl);
			}

			//else
			/*else
			{
				;
			}*/
		break;

		case KEY_1:
			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
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
					message[pos] = '.';
				}

				memset((char*)code, 0, strlen((char*)code));

				drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
				setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);

				TIM7->CNT=0;
				HAL_TIM_Base_Start_IT(&htim7);
			}
		break;

		case KEY_2:
			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
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
						message[pos] = 'a';
					}

					TIM7->CNT=0;
					HAL_TIM_Base_Start_IT(&htim7);
				}

				drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
				setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
			}
		break;

		case KEY_3:
			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
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
						message[pos] = 'd';
					}

					TIM7->CNT=0;
					HAL_TIM_Base_Start_IT(&htim7);
				}

				drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
				setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
			}
		break;

		case KEY_4:
			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
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
						message[pos] = 'g';
					}

					TIM7->CNT=0;
					HAL_TIM_Base_Start_IT(&htim7);
				}

				drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
				setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
			}
		break;

		case KEY_5:
			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
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
						message[pos] = 'j';
					}

					TIM7->CNT=0;
					HAL_TIM_Base_Start_IT(&htim7);
				}

				drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
				setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
			}
		break;

		case KEY_6:
			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
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
						message[pos] = 'm';
					}

					TIM7->CNT=0;
					HAL_TIM_Base_Start_IT(&htim7);
				}

				drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
				setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
			}
		break;

		case KEY_7:
			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
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
						message[pos] = 'p';
					}

					TIM7->CNT=0;
					HAL_TIM_Base_Start_IT(&htim7);
				}

				drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
				setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
			}
		break;

		case KEY_8:
			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
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
						message[pos] = 't';
					}

					TIM7->CNT=0;
					HAL_TIM_Base_Start_IT(&htim7);
				}

				drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
				setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
			}
		break;

		case KEY_9:
			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
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
						message[pos] = 'w';
					}

					TIM7->CNT=0;
					HAL_TIM_Base_Start_IT(&htim7);
				}

				drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
				setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
			}
		break;

		case KEY_ASTERISK:
			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
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
						message[pos] = '*';
					}

					TIM7->CNT=0;
					HAL_TIM_Base_Start_IT(&htim7);
				}

				drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
				setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);
			}
		break;

		case KEY_0:
			if(*disp_state==DISP_TEXT_MSG_ENTRY)
			{
				HAL_TIM_Base_Stop(&htim7);
				pos=strlen(message);
				char *last = &message[pos>0 ? pos-1 : 0];

				if(TIM7->CNT>0)
				{
					switch(*last)
					{
						case(' '):
							*last = '0';
						break;

						case('0'):
							*last = ' ';
						break;

						default:
							message[pos] = ' ';
							if(*text_mode==TEXT_T9)
								pos++;
						break;
					}
				}
				else
				{
					message[pos] = ' ';
					if(*text_mode==TEXT_T9)
						pos++;
				}

				TIM7->CNT=0;
				HAL_TIM_Base_Start_IT(&htim7);

				drawRect(buff, 0, 10, RES_X-1, RES_Y-9, 1, 1);
				setString(buff, 0, 10, &nokia_small, message, 0, ALIGN_LEFT);

				memset((char*)code, 0, strlen((char*)code));
			}
		break;

		case KEY_HASH:
			if(*disp_state==DISP_TEXT_MSG_ENTRY)
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

//set keyboard insensitivity timer
void setKeysTimeout(const uint16_t delay)
{
	TIM7->ARR=delay*10-1;
}

//RF module
void setRegRF(uint8_t reg, uint16_t val)
{
	char data[64], rcv[64]={0};
	uint8_t len;

	len = sprintf(data, "AT+POKE=%d,%d\r\n", reg, val);
	HAL_UART_Transmit(&huart4, (uint8_t*)data, len, 25);

	HAL_UART_Receive(&huart4, (uint8_t*)rcv, 64, 10);

	/*char msg[128];
	sprintf(msg, "[RF module] POKE %02X %04X reply: %s\n", reg, val, rcv);
	CDC_Transmit_FS((uint8_t*)msg, strlen(msg));*/
}

uint16_t getRegRF(uint8_t reg)
{
	char data[64], rcv[64]={0};
	uint8_t len;

	len = sprintf(data, "AT+PEEK=%d\r\n", reg);
	HAL_UART_Transmit(&huart4, (uint8_t*)data, len, 25);
	HAL_UART_Receive(&huart4, (uint8_t*)rcv, 64, 10);

	/*char msg[128];
	sprintf(msg, "[RF module] PEEK %02X reply: %s\n", reg, rcv);
	CDC_Transmit_FS((uint8_t*)msg, strlen(msg));*/

	return atoi(rcv);
}

void maskSetRegRF(uint8_t reg, uint16_t mask, uint16_t value)
{
	uint16_t regVal = getRegRF(reg);
	regVal = (regVal & ~mask) | (value & mask);
	setRegRF(reg, regVal);
}

void reloadRF(void)
{
	uint16_t funcMode = getRegRF(0x30) & 0x0060; //Get current op. status
	maskSetRegRF(0x30, 0x0060, 0x0000); //RX and TX off
	maskSetRegRF(0x30, 0x0060, funcMode); //Restore op. status
}

void setFreqRF(uint32_t freq, const float corr)
{
	freq = (float)freq * (1.0f + corr/1e6);

	uint32_t val = (freq / 1000.0f) * 16.0f;
	uint16_t fHi = (val >> 16) & 0xFFFF;
	uint16_t fLo = val & 0xFFFF;

	setRegRF(0x29, fHi);
	setRegRF(0x2A, fLo);

	reloadRF();
}

void setRF(radio_state_t state)
{
	//0 for rx, 1 for tx
	maskSetRegRF(0x30, 0x0060, (state+1)<<5);
}

void chBwRF(ch_bw_t bw)
{
	if(bw==RF_BW_12K5)
	{
		setRegRF(0x15, 0x1100);
		setRegRF(0x32, 0x4495);
		setRegRF(0x3A, 0x40C3);
		setRegRF(0x3F, 0x29D1);
		setRegRF(0x3C, 0x1B34);
		setRegRF(0x48, 0x19B1);
		setRegRF(0x60, 0x0F17);
		setRegRF(0x62, 0x1425);
		setRegRF(0x65, 0x2494);
		setRegRF(0x66, 0xEB2E);
		setRegRF(0x7F, 0x0001);
		setRegRF(0x06, 0x0014);
		setRegRF(0x07, 0x020C);
		setRegRF(0x08, 0x0214);
		setRegRF(0x09, 0x030C);
		setRegRF(0x0A, 0x0314);
		setRegRF(0x0B, 0x0324);
		setRegRF(0x0C, 0x0344);
		setRegRF(0x0D, 0x1344);
		setRegRF(0x0E, 0x1B44);
		setRegRF(0x0F, 0x3F44);
		setRegRF(0x12, 0xE0EB);
		setRegRF(0x7F, 0x0000);

		maskSetRegRF(0x30, 0x3000, 0x0000);
	}
	else
	{
		setRegRF(0x15, 0x1F00);
		setRegRF(0x32, 0x7564);
		setRegRF(0x3A, 0x40C3);
		setRegRF(0x3C, 0x1B34);
		setRegRF(0x3F, 0x29D1);
		setRegRF(0x48, 0x1F3C);
		setRegRF(0x60, 0x0F17);
		setRegRF(0x62, 0x3263);
		setRegRF(0x65, 0x248A);
		setRegRF(0x66, 0xFFAE);
		setRegRF(0x7F, 0x0001);
		setRegRF(0x06, 0x0024);
		setRegRF(0x07, 0x0214);
		setRegRF(0x08, 0x0224);
		setRegRF(0x09, 0x0314);
		setRegRF(0x0A, 0x0324);
		setRegRF(0x0B, 0x0344);
		setRegRF(0x0C, 0x0384);
		setRegRF(0x0D, 0x1384);
		setRegRF(0x0E, 0x1B84);
		setRegRF(0x0F, 0x3F84);
		setRegRF(0x12, 0xE0EB);
		setRegRF(0x7F, 0x0000);

		maskSetRegRF(0x30, 0x3000, 0x3000);
	}

	reloadRF(); //reload
}

void setModeRF(rf_mode_t mode)
{
	if(mode==RF_MODE_4FSK)
	{
		setRegRF(0x3A, 0x00C2);
		setRegRF(0x33, 0x45F5);
		setRegRF(0x41, 0x4731);
		setRegRF(0x42, 0x1036);
		setRegRF(0x43, 0x00BB);
		setRegRF(0x58, 0xBCFD);	//Bit 0  = 1: CTCSS LPF bandwidth to 250Hz
								//Bit 3  = 1: bypass CTCSS HPF
								//Bit 4  = 1: bypass CTCSS LPF
								//Bit 5  = 1: bypass voice LPF
								//Bit 6  = 1: bypass voice HPF
								//Bit 7  = 1: bypass pre/de-emphasis
								//Bit 11 = 1: bypass VOX HPF
								//Bit 12 = 1: bypass VOX LPF
								//Bit 13 = 1: bypass RSSI LPF
		setRegRF(0x44, 0x06CC);
		setRegRF(0x40, 0x0031);
	}
	else
	{
		;
	}

	reloadRF(); //reload
}

void initRF(ch_settings_t ch_settings)
{
	uint32_t freq = ch_settings.rx_frequency;
	float freq_corr = dev_settings.freq_corr;
	ch_bw_t bw = ch_settings.ch_bw;
	rf_mode_t mode = ch_settings.mode;
	rf_power_t pwr = ch_settings.rf_pwr;

	char msg[128]; //debug
	uint8_t data[64]={0};

	//PTT off
	HAL_GPIO_WritePin(RF_PTT_GPIO_Port, RF_PTT_Pin, 1);

	//RF power
	if(pwr==RF_PWR_LOW)
		HAL_GPIO_WritePin(RF_PWR_GPIO_Port, RF_PWR_Pin, 1);
	else
		HAL_GPIO_WritePin(RF_PWR_GPIO_Port, RF_PWR_Pin, 0);

	//turn on the module
	HAL_GPIO_WritePin(RF_ENA_GPIO_Port, RF_ENA_Pin, 1);
	HAL_Delay(100);

	HAL_UART_Transmit(&huart4, (uint8_t*)"AT+VERSION\r\n", 12, 20);
	HAL_UART_Receive(&huart4, data, 64, 50); //naїve, blocking

	//display the received data
	uint8_t len=strlen((char*)data);
	if(len)
	{
		data[strlen((char*)data)-2]=0;
		sprintf(msg, "[RF module] Version: %s\n", data);
		CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
	}
	else
		CDC_Transmit_FS((uint8_t*)"[RF module] Nothing received\n", 30);

	//SA868S (AT1846S) init sequence (thx, edgetriggered)
	sprintf(msg, "[RF module] Init sequence start\n");
	CDC_Transmit_FS((uint8_t*)msg, strlen(msg));

	setRegRF(0x30, 0x0001);	//Soft reset
	HAL_Delay(160);

	setRegRF(0x30, 0x0004);	//Set pdn_reg (power down pin);

	setRegRF(0x04, 0x0FD0);	//Set clk_mode to 25.6MHz/26MHz
	setRegRF(0x0A, 0x7C20);	//Set 0x0A to its default value
	setRegRF(0x13, 0xA100);
	setRegRF(0x1F, 0x1001);	//Set gpio0 to ctcss_out/css_int/css_cmp
							//and gpio6 to sq, sq&ctcss/cdcss when sq_out_set=1
	setRegRF(0x31, 0x0031);
	setRegRF(0x33, 0x44A5);
	setRegRF(0x34, 0x2B89);
	setRegRF(0x41, 0x4122);	//Set voice_gain_tx (voice digital gain); to 0x22
	setRegRF(0x42, 0x1052);
	setRegRF(0x43, 0x0100);
	setRegRF(0x44, 0x07FF);	//Set gain_tx (voice digital gain after tx ADC downsample); to 0x7
	setRegRF(0x59, (68<<6) | 0x10);	//Set c_dev (CTCSS/CDCSS TX FM deviation); to 0x10
									//and xmitter_dev (voice/subaudio TX FM deviation); to 0x2E
									//original value: 0x0B90 = (0x2E<<6) | 0x10
									//xmitter_dev=68 gives good M17 deviation
	setRegRF(0x47, 0x7F2F);
	setRegRF(0x4F, 0x2C62);
	setRegRF(0x53, 0x0094);
	setRegRF(0x54, 0x2A3C);
	setRegRF(0x55, 0x0081);
	setRegRF(0x56, 0x0B02);
	setRegRF(0x57, 0x1C00);
	setRegRF(0x58, 0x9CDD);	//Bit 0  = 1: CTCSS LPF bandwidth to 250Hz
							//Bit 3  = 1: bypass CTCSS HPF
							//Bit 4  = 1: bypass CTCSS LPF
							//Bit 5  = 0: enable voice LPF
							//Bit 6  = 1: bypass voice HPF
							//Bit 7  = 1: bypass pre/de-emphasis
							//Bit 11 = 1: bypass VOX HPF
							//Bit 12 = 1: bypass VOX LPF
							//Bit 13 = 0: normal RSSI LPF bandwidth
	setRegRF(0x5A, 0x06DB);
	setRegRF(0x63, 0x16AD);
	setRegRF(0x67, 0x0628);	//Set DTMF C0 697Hz to ???
	setRegRF(0x68, 0x05E5);	//Set DTMF C1 770Hz to 13MHz and 26MHz
	setRegRF(0x69, 0x0555);	//Set DTMF C2 852Hz to ???
	setRegRF(0x6A, 0x04B8);	//Set DTMF C3 941Hz to ???
	setRegRF(0x6B, 0x02FE);	//Set DTMF C4 1209Hz to 13MHz and 26MHz
	setRegRF(0x6C, 0x01DD);	//Set DTMF C5 1336Hz
	setRegRF(0x6D, 0x00B1);	//Set DTMF C6 1477Hz
	setRegRF(0x6E, 0x0F82);	//Set DTMF C7 1633Hz
	setRegRF(0x6F, 0x017A);	//Set DTMF C0 2nd harmonic
	setRegRF(0x70, 0x004C);	//Set DTMF C1 2nd harmonic
	setRegRF(0x71, 0x0F1D);	//Set DTMF C2 2nd harmonic
	setRegRF(0x72, 0x0D91);	//Set DTMF C3 2nd harmonic
	setRegRF(0x73, 0x0A3E);	//Set DTMF C4 2nd harmonic
	setRegRF(0x74, 0x090F);	//Set DTMF C5 2nd harmonic
	setRegRF(0x75, 0x0833);	//Set DTMF C6 2nd harmonic
	setRegRF(0x76, 0x0806);	//Set DTMF C7 2nd harmonic

	setRegRF(0x30, 0x40A4);	//Set pdn_pin (power down enable);
							//and set rx_on
							//and set mute when rxno
							//and set xtal_mode to 26MHz/13MHz
	HAL_Delay(160);

	setRegRF(0x30, 0x40A6);	//Start calibration
	HAL_Delay(160);
	setRegRF(0x30, 0x4006);	//Stop calibration
	HAL_Delay(160);

	setRegRF(0x40, 0x0031);

	//set mode
	sprintf(msg, "[RF module] Setting mode to %s\n", (mode==RF_MODE_4FSK)?"M17":"FM");
	CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
	setModeRF(mode);

	//set bandwidth
	sprintf(msg, "[RF module] Setting bandwidth to %skHz\n", (bw==RF_BW_12K5)?"12.5":"25");
	CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
	chBwRF(bw);

	//some additional registers
	setRegRF(0x41, 0x0070); //VOX threshold? was 0x0070
	setRegRF(0x44, 0x00FF); //"RX voice volume", was 0x0022

	//set frequency
	sprintf(msg, "[RF module] Setting frequency to %ldHz (f_corr=%d.%d)\n",
			freq, (int8_t)freq_corr, (uint8_t)fabsf(10*freq_corr) - (int8_t)fabsf((int8_t)freq_corr*10.0f));
	CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
	setFreqRF(freq, freq_corr);
}

void shutdownRF(void)
{
	HAL_GPIO_WritePin(RF_PWR_GPIO_Port, RF_PWR_Pin, 0);
	char msg[128];
	sprintf(msg, "[RF module] Shutdown\n");
	CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
}

//sps=10
void filter_symbols(uint16_t out[SYM_PER_FRA*10], const int8_t in[SYM_PER_FRA], const float* flt, uint8_t phase_inv)
{
	static int8_t last[81]; //memory for last symbols

	for(uint8_t i=0; i<SYM_PER_FRA; i++)
	{
		for(uint8_t j=0; j<10; j++)
		{
			for(uint8_t k=0; k<80; k++)
				last[k]=last[k+1];

			if(j==0)
			{
				if(phase_inv) //normal phase - invert the symbol stream as GBA inverts the output
					last[80]=-in[i];
				else
					last[80]= in[i];
			}
			else
				last[80]=0;

			float acc=0.0f;
			for(uint8_t k=0; k<81; k++)
				acc+=last[k]*flt[k];

			out[i*10+j]=DAC_IDLE+acc*250.0f;
		}
	}
}

//memory
uint8_t eraseSector(void)
{
	char msg[128];

	if(HAL_FLASH_Unlock()==HAL_OK)
	{
		FLASH_EraseInitTypeDef erase =
		{
			FLASH_TYPEERASE_SECTORS,	//erase sectors
			FLASH_BANK_1,				//bank 1 (the only available, i guess)
			FLASH_SECTOR_11,			//start at sector 11 (last)
			1,							//1 sector to erase
			FLASH_VOLTAGE_RANGE_3,		//2.7 to 3.6V
		};
		uint32_t sector_error=0;

		HAL_StatusTypeDef ret = HAL_FLASHEx_Erase(&erase, &sector_error);

		if(ret==HAL_OK && sector_error==0xFFFFFFFFU) //successful erase
		{
			sprintf(msg, "[NVMEM] Sector erased.\n");
			CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
			HAL_FLASH_Lock();

			return 0;
		}

		sprintf(msg, "[NVMEM] Sector erasure error.\n");
		CDC_Transmit_FS((uint8_t*)msg, strlen(msg));

		return 1;
	}

	sprintf(msg, "[NVMEM] Error unlocking Flash memory.\n");
	CDC_Transmit_FS((uint8_t*)msg, strlen(msg));

	return 1;
}

uint8_t saveData(const void *data, const uint32_t addr, const uint16_t size)
{
	char msg[128];

	if(eraseSector()==0) //erase ok
	{
		if(HAL_FLASH_Unlock()==HAL_OK) //unlock ok
		{
			for(uint16_t i=0; i<size; i++)
				HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr+i, *(((uint8_t*)data)+i));

			HAL_FLASH_Lock();

			sprintf(msg, "[NVMEM] Data saved.\n");
			CDC_Transmit_FS((uint8_t*)msg, strlen(msg));

			return 0;
		}

		sprintf(msg, "[NVMEM] Error unlocking Flash memory.\n");
		CDC_Transmit_FS((uint8_t*)msg, strlen(msg));

		return 1;
	}

	sprintf(msg, "[NVMEM] Sector erasure error.\n");
	CDC_Transmit_FS((uint8_t*)msg, strlen(msg));

	return 1;
}

//device settings
void loadDeviceSettings(dev_settings_t *dev_settings, const dev_settings_t *def_dev_settings)
{
	char msg[128];

	//if the memory is uninitialized
	if(*((uint32_t*)MEM_START)==0xFFFFFFFFU)
	{
		memcpy((uint8_t*)dev_settings, (uint8_t*)def_dev_settings, sizeof(dev_settings_t));
		uint8_t ret = saveData(def_dev_settings, MEM_START, sizeof(dev_settings_t));

		if(ret==0)
			sprintf(msg, "[NVMEM] Default device settings loaded.\n");
		else
			sprintf(msg, "[NVMEM] Error saving default device settings.\n");
		CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
	}

	//if settings are available in the memory
	else
	{
		memcpy((uint8_t*)dev_settings, (uint8_t*)MEM_START, sizeof(dev_settings_t));

		sprintf(msg, "[NVMEM] Device settings loaded.\n");
		CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
	}

	//load settings into menus
	//frequency correction
	sprintf(radio_settings.value[0], "%d.%d",
			(int8_t)(dev_settings->freq_corr), (uint8_t)fabsf(10*dev_settings->freq_corr) - (int8_t)fabsf((int8_t)(dev_settings->freq_corr)*10.0f)
	);

	//RF power
	if(dev_settings->channel.rf_pwr==RF_PWR_HIGH)
		sprintf(radio_settings.value[1], "2W");
	else
		sprintf(radio_settings.value[1], "0.5W");

	//SRC callsign
	strcpy(m17_settings.value[0], dev_settings->src_callsign);

	//destination
	strcpy(m17_settings.value[1], dev_settings->channel.dst);

	//CAN
	sprintf(m17_settings.value[2], "%d", dev_settings->channel.can);
}

//USB control
void parseUSB(uint8_t *str, uint32_t len)
{
	//display backlight
	//"bl=VALUE"
	if(strstr((char*)str, "bl")==(char*)str)
	{
		setBacklight(atoi(strstr((char*)str, "=")+1));
	}

	//set frequency
	//"freq=VALUE"
	else if(strstr((char*)str, "freq")==(char*)str)
	{
		setFreqRF(atoi(strstr((char*)str, "=")+1), dev_settings.freq_corr);
	}

	//read byte from the Flash memory
	//"peek=ADDRESS"
	else if(strstr((char*)str, "peek")==(char*)str)
	{
		uint32_t addr = MEM_START + atoi(strstr((char*)str, "=")+1);
		char msg[128];
		sprintf(msg, "%08lX -> 0x%02X\n", addr, *((uint8_t*)addr));
		CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
	}

	//write byte to the Flash memory (use with caution)
	//"poke=ADDRESS val=VALUE"
	else if(strstr((char*)str, "poke")==(char*)str)
	{
		uint32_t addr = MEM_START + atoi(strstr((char*)str, "=")+1);
		uint8_t val = atoi(strstr((char*)str, "val=")+4);

		HAL_FLASH_Unlock();
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr, (uint64_t)val);
		HAL_FLASH_Lock();

		char msg[128];
		sprintf(msg, "%08lX <- 0x%02X\n", addr, val);
		CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
	}

	//load settings from nvmem
	//"load"
	else if(strstr((char*)str, "load")==(char*)str)
	{
		loadDeviceSettings(&dev_settings, &def_dev_settings);
	}

	//erase Flash memory sector
	//"erase"
	else if(strstr((char*)str, "erase")==(char*)str)
	{
		eraseSector();
	}

	//set SRC callsign
	//"src_call=STRING"
	else if(strstr((char*)str, "src_call")==(char*)str)
	{
		//dev_settings_t ds;

		//retrieve and update settings
		//memcpy((uint8_t*)&ds, (uint8_t*)&dev_settings, sizeof(dev_settings_t));
		strcpy(dev_settings.src_callsign, strstr((char*)str, "=")+1);
		//memcpy((uint8_t*)&dev_settings, (uint8_t*)&ds, sizeof(dev_settings_t));
		saveData(&dev_settings, MEM_START, sizeof(dev_settings_t));
	}

	//set frequency correction
	//"freq_corr=VALUE"
	else if(strstr((char*)str, "f_corr")==(char*)str)
	{
		float val = atof(strstr((char*)str, "=")+1);

		//dev_settings_t ds;

		//retrieve and update settings
		//memcpy((uint8_t*)&ds, (uint8_t*)&dev_settings, sizeof(dev_settings_t));
		dev_settings.freq_corr = val;
		//memcpy((uint8_t*)&dev_settings, (uint8_t*)&ds, sizeof(dev_settings_t));
		saveData(&dev_settings, MEM_START, sizeof(dev_settings_t));
	}

	//send text message
	//"msg=STRING"
	else if(strstr((char*)str, "msg")==(char*)str)
	{
		strcpy(message, strstr((char*)str, "=")+1);
		initTextTX(message);
	}

	//get LSF META field
	else if(strstr((char*)str, "meta")==(char*)str)
	{
		char msg[128];
		sprintf(msg, "[Settings] META=");
		for(uint8_t i=0; i<sizeof(lsf.meta); i++)
			sprintf(&msg[strlen(msg)], "%02X", lsf.meta[i]);
		sprintf(&msg[strlen(msg)], " REF=%s\n", dev_settings.refl_name);
		CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
	}

	//simple echo
	//CDC_Transmit_FS(str, len);
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
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_DAC_Init();
  MX_RTC_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_UART4_Init();
  MX_SPI1_Init();
  MX_TIM7_Init();
  MX_USB_DEVICE_Init();
  MX_TIM6_Init();
  /* USER CODE BEGIN 2 */
  #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
  	  SCB->CPACR |= ((3UL << 20U)|(3UL << 22U));  /* set CP10 and CP11 Full Access */
  #endif

  //set baseband DAC to idle
  frame_samples[0][0]=DAC_IDLE;
  HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)frame_samples, 1, DAC_ALIGN_12B_R);
  HAL_TIM_Base_Start(&htim6); //48kHz

  HAL_Delay(200);

  dispInit();
  dispClear(disp_buff, 0);
  setBacklight(0);

  loadDeviceSettings(&dev_settings, &def_dev_settings);
  dispSplash(disp_buff, dev_settings.welcome_msg[0], dev_settings.welcome_msg[1], dev_settings.src_callsign);

  radio_state = RF_RX;
  initRF(dev_settings.channel);
  setRF(radio_state);
  set_LSF(&lsf, dev_settings.src_callsign, dev_settings.channel.dst,
		  M17_TYPE_PACKET | M17_TYPE_DATA | M17_TYPE_CAN(dev_settings.channel.can) | \
		  M17_TYPE_META_TEXT | M17_TYPE_UNSIGNED, NULL);
  setKeysTimeout(dev_settings.kbd_timeout);
  //playBeep(50);

  //menu init
  menu = &main_scr;
  disp_state = DISP_MAIN_SCR;
  showMainScreen(disp_buff);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while(1)
  {
	  //superloop rotation counter
	  r++;

	  //handle key presses
	  handleKey(disp_buff, &disp_state, &text_mode, &radio_state, &dev_settings, scanKeys(dev_settings.kbd_delay));

	  //refresh main screen data
	  if(r%100==0 && disp_state==DISP_MAIN_SCR)
	  {
		  //clear the upper right portion of the screen
		  drawRect(disp_buff, RES_X-1-15, 0, RES_X-1, 8, 1, 1);

		  //is the battery charging? (read /CHG signal)
		  if(!(CHG_GPIO_Port->IDR & CHG_Pin))
		  {
			  setString(disp_buff, RES_X-1, 0, &nokia_small, "B+", 0, ALIGN_RIGHT);
		  }
		  else
		  {
			  char u_batt_str[8];
			  uint16_t u_batt=getBattVoltage();
			  if(u_batt>3500)
				  sprintf(u_batt_str, "%1d.%1d", u_batt/1000, (u_batt-(u_batt/1000)*1000)/100);
			  else
				  sprintf(u_batt_str, "Lo");
			  setString(disp_buff, RES_X-1, 0, &nokia_small, u_batt_str, 0, ALIGN_RIGHT);
		  }
	  }

	  //packet transfer triggered - start transmission
	  if(frame_pend)
	  {
		  //this is a workaround for the module's initial frequency wobble after RX->TX transition
		  const uint8_t warmup = 25; //each frame is 40ms, therefore this is 1s of a solid preamble

		  if(frame_cnt==0)
		  {
			  dispClear(disp_buff, 0);
			  setString(disp_buff, 0, 17, &nokia_big, "Sending...", 0, ALIGN_CENTER);

			  //preamble
			  uint32_t cnt=0;
			  gen_preamble_i8(frame_symbols, &cnt, PREAM_LSF);
			  filter_symbols(&frame_samples[0][0], frame_symbols, rrc_taps_10, 0);

			  HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t*)&frame_samples[0][0], SYM_PER_FRA*10, DAC_ALIGN_12B_R);
		  }
		  else if(frame_cnt<warmup)
		  {
			  //more preamble :D
			  uint32_t cnt=0;
			  gen_preamble_i8(frame_symbols, &cnt, PREAM_LSF);
			  filter_symbols(&frame_samples[frame_cnt%2][0], frame_symbols, rrc_taps_10, 0);
		  }
		  else if(frame_cnt==warmup)
		  {
			  //LSF with sample META field
			  encode_callsign_bytes(&lsf.meta[0], (uint8_t*)dev_settings.refl_name);
			  //encode_callsign_bytes(&lsf.meta[6], (uint8_t*)"");
			  update_LSF_CRC(&lsf);
			  gen_frame_i8(frame_symbols, lsf.meta, FRAME_LSF, &lsf, 0, 0);
			  filter_symbols(&frame_samples[frame_cnt%2][0], frame_symbols, rrc_taps_10, 0);
		  }
		  else
		  {
			  uint16_t bytes_left=packet_bytes-(frame_cnt-warmup-1)*25;
			  uint8_t payload[26];

			  memcpy(payload, &packet_payload[(frame_cnt-warmup-1)*25], 25);
			  if(bytes_left>25)
				  payload[25] = (frame_cnt-warmup-1)<<2;
			  else
				  payload[25] = 0x80 | ((bytes_left)<<2);

			  gen_frame_i8(frame_symbols, payload, FRAME_PKT, &lsf, 0, 0);
			  filter_symbols(&frame_samples[frame_cnt%2][0], frame_symbols, rrc_taps_10, 0);
		  }

		  if(frame_cnt<(packet_bytes/25+1)+warmup)
		  {
			  frame_cnt++;
		  }
		  else
		  {
			  radio_state=RF_RX;
			  setRF(radio_state);
			  HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
			  frame_cnt=0;

			  disp_state = DISP_MAIN_SCR;
			  showMainScreen(disp_buff);
		  }

		  frame_pend=0;
	  }

	  //received data over USB
	  if(usb_drdy)
	  {
		  parseUSB(usb_rx, usb_len);
		  usb_drdy=0;
	  }
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
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 2;
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
  sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 2;
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
  sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
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
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 1-1;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 1750-1;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

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
  HAL_GPIO_WritePin(GPIOC, RF_PWR_Pin|RF_PTT_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, COL_3_Pin|COL_2_Pin, GPIO_PIN_RESET);

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
