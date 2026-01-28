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
#include "typedefs.h"
#include "keymaps.h"
#include "menus.h"
#include "settings.h"
#include "ring.h"
#include "display.h"
#include "dsp.h"
#include "platform.h"
#include "keypad.h"
#include "ringtones.h"
#include "rf_module.h"
#include "nvmem.h"
#include "usb_cmds.h"
#include "text_entry.h"
#include "debug.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#include "macros.h"
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc1;
DMA_HandleTypeDef hdma_adc2;

DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac1;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim7;
TIM_HandleTypeDef htim8;
TIM_HandleTypeDef htim14;

UART_HandleTypeDef huart4;

/* USER CODE BEGIN PV */
//timing
uint32_t t_now, t_last;

//display
uint8_t disp_buff[DISP_BUFF_SIZ];
disp_dev_t disp_dev;

//this handles all kinds of text entry
abc_t text_entry; //entry mode is TEXT_LOWERCASE by default

//menus state machine
uint8_t menu_pos, menu_pos_hl; //menu item position, highlighted menu item position
disp_state_t curr_disp_state = DISP_NONE;
volatile disp_state_t pending_disp_state = DISP_NONE;

//usb-related
uint8_t usb_rx[APP_RX_DATA_SIZE + 1];
uint32_t usb_len;
uint8_t usb_drdy;

//default channel settings
const ch_settings_t def_channel =
{
	RF_MODE_DIG,
	RF_BW_12K5,
	RF_PWR_LOW,
	"M17 IARU R1",
	433475000,
	433475000,
	"@ALL",
	0
};

dev_settings_t def_dev_settings =
{
	"N0KIA",
	{"OpenRTX", "rulez"},

	200,
	5,
	0,
	750,
	150,
	0.0f,

	TUNING_VFO,
	def_channel,
	0,

	"@ALL"
};

dev_settings_t dev_settings;

//codeplug
extern codeplug_t codeplug;

edit_set_t edit_set = EDIT_NONE;

//M17
uint16_t frame_samples[2][SYM_PER_FRA*10];	//sps=10
int8_t frame_symbols[SYM_PER_FRA];
lsf_t lsf_rx, lsf_tx;
uint8_t frame_cnt;							//frame counter, preamble=0
volatile uint8_t frame_pend;				//frame generation pending?
volatile uint8_t bsb_tx_dma_half;
uint8_t packet_payload[33*25];
uint8_t packet_bytes;
uint8_t payload[26];						//frame payload
const uint16_t sms_max_len = sizeof(packet_payload)-1-1-2;
uint8_t debug_flag;							//debug flag (for testing)

//radio
radio_state_t radio_state;

//ADC
volatile uint16_t batt_adc;
float sw_corr_samples[8*5+5];				//samples for syncword search
float pld_symbs[SYM_PER_PLD];				//payload symbols

uint8_t sample_offset;						//location of the squared-L2 minimum
uint8_t lsf_found, str_found, pkt_found;	//syncd with the incoming stream?

//received message
msg_t rcvd_msg;
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
static void MX_TIM14_Init(void);
static void MX_TIM8_Init(void);
static void MX_ADC2_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//messaging - initialize text packet transmission
//note: some variables inside this function are global
void initTextTX(const char *message)
{
	HAL_ADC_Stop_DMA(&hadc1);
	HAL_TIM_Base_Stop(&htim8); //stop 24kHz ADC baseband sample clock

	memset(payload, 0, 26);

	uint16_t msg_len = strlen(message);
	if (msg_len > sms_max_len)
		msg_len = sms_max_len;

	packet_payload[0] = 0x05; //packet type: SMS

	strncpy((char*)&packet_payload[1], message, msg_len);
	packet_payload[msg_len+1] = 0; //null terminaton

	uint16_t crc = CRC_M17(packet_payload, 1+msg_len+1);
	packet_payload[msg_len+2] = crc>>8;
	packet_payload[msg_len+3] = crc&0xFF;

	packet_bytes = 1+msg_len+1+2; //type, payload, null termination, crc

	radio_state = RF_TX;
	setRF(radio_state);
	//TODO: RF PTT line should work
	//HAL_GPIO_WritePin(RF_PTT_GPIO_Port, RF_PTT_Pin, 0);

	frame_cnt = 0;
	frame_pend = 1;
}

//initialize debug M17 transmission
void initDebugTX(void)
{
	// start
	HAL_ADC_Stop_DMA(&hadc1);
	HAL_TIM_Base_Stop(&htim8); //stop 24kHz ADC baseband sample clock

	radio_state = RF_TX;
	setRF(radio_state);
	//TODO: RF PTT line should work
	//HAL_GPIO_WritePin(RF_PTT_GPIO_Port, RF_PTT_Pin, 0);

	// ...then do this
	HAL_Delay(10000);

	// cleanup
    radio_state = RF_RX;
    setRF(radio_state);
    //TODO: RF PTT line should work
    //HAL_GPIO_WritePin(RF_PTT_GPIO_Port, RF_PTT_Pin, 1);

    chBwRF(RF_BW_25K);

    HAL_ADC_Stop_DMA(&hadc1);
    raw_bsb_buff_tail = 0;
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&raw_bsb_buff, arrlen(raw_bsb_buff));
    HAL_TIM_Base_Start(&htim8);
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
  MX_TIM14_Init();
  MX_TIM8_Init();
  MX_ADC2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  #if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
  SCB->CPACR |= ((3UL << 20U)|(3UL << 22U));  /* set CP10 and CP11 Full Access */
  #endif

  //set baseband DAC to idle
  HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
  HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, DAC_IDLE);
  HAL_TIM_Base_Start(&htim6); //48kHz - DAC (baseband out) timer for later DMA transfers

  //start ADC sampling
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&raw_bsb_buff, arrlen(raw_bsb_buff));
  HAL_ADC_Start_DMA(&hadc2, (uint32_t*)&batt_adc, 1);
  HAL_TIM_Base_Start(&htim3); // 5Hz for battery voltage sampling
  HAL_TIM_Base_Start(&htim8); // 24kHz - ADC (baseband in)

  HAL_Delay(200);

  //init display
  disp_dev = (disp_dev_t){&hspi1, disp_buff};
  dispInit(&disp_dev);
  dispClear(&disp_dev, 0);
  setBacklight(0);

  //load settings from NVMEM
  loadDeviceSettings(&dev_settings, &def_dev_settings);

  //display splash screen
  curr_disp_state = DISP_SPLASH;
  dispSplash(&disp_dev, dev_settings.welcome_msg[0], dev_settings.welcome_msg[1], dev_settings.src_callsign);

  //turn on backlight if required
  setBacklight(dev_settings.backlight_level);
  if(!dev_settings.backlight_always)
  {
	  //enable backlight timeout
	  setBacklightTimer(dev_settings.backlight_timer);
	  startBacklightTimer();
  }

  //init SA868S RF module
  radio_state = RF_RX;
  initRF(dev_settings);
  setRF(radio_state);
  chBwRF(RF_BW_25K); //TODO: get rid of this workaround

  //fill LSF
  set_LSF(&lsf_tx, dev_settings.src_callsign, dev_settings.channel.dst,
		  M17_TYPE_PACKET | M17_TYPE_CAN(dev_settings.channel.can), NULL);

  //keypad timeout
  setKeysTimeout(dev_settings.kbd_timeout);

  //play beep if required
  /*if (dev_settings.beep_on_start == 1)
  {
	  playBeep(1000, 50);
  }*/

  //menu init
  curr_disp_state = DISP_MAIN_SCR;
  showMainScreen(&disp_dev);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while(1)
  {
	  //current tick
	  t_now = HAL_GetTick();

	  //get any pending transitions (triggered externally, not through the keypad)
	  if (pending_disp_state != DISP_NONE)
	  {
	      leaveState(curr_disp_state, text_entry.buffer, &dev_settings, EDIT_NONE, &radio_state);

	      curr_disp_state = pending_disp_state;
	      pending_disp_state = DISP_NONE; //reset

	      enterState(&disp_dev, curr_disp_state, &text_entry, &dev_settings);
	  }

	  //handle key presses
	  handleKey(&disp_dev, &curr_disp_state, &text_entry, &radio_state,
			  &dev_settings, scanKeys(radio_state, dev_settings.kbd_delay), &edit_set);

	  //refresh main screen data
	  if(t_now-t_last>=1000 && curr_disp_state==DISP_MAIN_SCR)
	  {
		  //clear the upper right portion of the screen
		  drawRect(&disp_dev, RES_X-1-15, 0, RES_X-1, 8, COL_WHITE, 1);

		  //is the battery charging? (read /CHG signal)
		  if(!(CHG_GPIO_Port->IDR & CHG_Pin))
		  {
			  setString(&disp_dev, RES_X-1, 0, &nokia_small, "B+", COL_BLACK, ALIGN_RIGHT);
		  }
		  else
		  {
			  char u_batt_str[8];
			  uint16_t u_batt = getBattVoltage();
			  if(u_batt>3500)
				  sprintf(u_batt_str, "%1d.%1d", u_batt/1000, (u_batt-(u_batt/1000)*1000)/100);
			  else
				  sprintf(u_batt_str, "Lo");
			  setString(&disp_dev, RES_X-1, 0, &nokia_small, u_batt_str, COL_BLACK, ALIGN_RIGHT);
		  }

		  t_last = HAL_GetTick();
	  }

	  //packet transfer triggered - start transmission
	  if (frame_pend)
	  {
	      const uint8_t warmup = 25;
	      uint8_t N = (packet_bytes + 24) / 25;

	      if (frame_cnt == 0)
	      {
	          // first preamble frame (start DMA)
	          dispClear(&disp_dev, 0);
	          setString(&disp_dev, 0, 17, &nokia_big, "Sending...", COL_BLACK, ALIGN_CENTER);

	          chBwRF(dev_settings.channel.ch_bw);

	          uint32_t cnt = 0;
	          gen_preamble_i8(frame_symbols, &cnt, PREAM_LSF);
	          fltSymbolsPoly(&frame_samples[0][0], frame_symbols, rrc_taps_10_poly, 0);

	          HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *)&frame_samples[0][0],
	                            2*SYM_PER_FRA*10, DAC_ALIGN_12B_R);
	      }
	      else if (frame_cnt < warmup)
	      {
	          // remaining preamble frames
	          uint32_t cnt = 0;
	          gen_preamble_i8(frame_symbols, &cnt, PREAM_LSF);
	          fltSymbolsPoly(&frame_samples[bsb_tx_dma_half][0], frame_symbols, rrc_taps_10_poly, 0);
	      }
	      else if (frame_cnt == warmup)
	      {
	          // LSF frame
	          gen_frame_i8(frame_symbols, NULL, FRAME_LSF, &lsf_tx, 0, 0);
	          fltSymbolsPoly(&frame_samples[bsb_tx_dma_half][0], frame_symbols, rrc_taps_10_poly, 0);
	      }
	      else if (frame_cnt <= warmup + N)
	      {
	          // PKT frames (1..N)
	          uint8_t index = frame_cnt - warmup - 1;

	          uint16_t off = index * 25;
	          uint16_t remaining = packet_bytes - off;

	          memcpy(payload, &packet_payload[off], 25);
	          if (remaining > 25)
	              payload[25] = index << 2;
	          else
	              payload[25] = 0x80 | (remaining << 2);

	          gen_frame_i8(frame_symbols, payload, FRAME_PKT, &lsf_tx, 0, 0);
	          fltSymbolsPoly(&frame_samples[bsb_tx_dma_half][0], frame_symbols, rrc_taps_10_poly, 0);
	      }
	      else if (frame_cnt == warmup + N + 1)
	      {
	          // EOT frame
	          uint32_t cnt = 0;
	          gen_eot_i8(frame_symbols, &cnt);
	          fltSymbolsPoly(&frame_samples[bsb_tx_dma_half][0], frame_symbols, rrc_taps_10_poly, 0);
	      }
	      else
	      {
	          // DONE — cleanup
	          radio_state = RF_RX;
	          setRF(radio_state);
	          //TODO: RF PTT line should work
	          //HAL_GPIO_WritePin(RF_PTT_GPIO_Port, RF_PTT_Pin, 1);

	          HAL_DAC_Stop_DMA(&hdac, DAC_CHANNEL_1);
	          HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, DAC_IDLE);

	          frame_cnt = 0;
	          curr_disp_state = DISP_MAIN_SCR;
	          showMainScreen(&disp_dev);

	          text_entry.buffer[0] = 0;
	          text_entry.pos = 0;
	          // keep the current text entry mode

	          chBwRF(RF_BW_25K); // TODO: this is a workaround

	          HAL_ADC_Stop_DMA(&hadc1);
	          raw_bsb_buff_tail = 0;
	          HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&raw_bsb_buff, arrlen(raw_bsb_buff));
	          HAL_TIM_Base_Start(&htim8);
	      }

	      frame_cnt++;
	      frame_pend = 0;
	  }

	  //debug
	  if (debug_flag)
	  {
		  //playMelody(ringtones[0]);
		  initDebugTX();
		  debug_flag = 0;
	  }

	  //received data over USB
	  if(usb_drdy)
	  {
		  parseUSB(&text_entry, usb_rx, usb_len);
		  usb_drdy=0;
	  }

	  //tail==head - buffer overrun
	  if (demodIsOverrun())
	  {
	      dbg_print("[Debug] Baseband buffer overrun!\n");
	  }

	  // repeat while there are baseband samples available
	  while (demodSamplesGetNum() && radio_state==RF_RX)
	  {
		  //debug data dump over USB UART
		  /*uint16_t b[BSB_BUFF_SIZ];
		  for(uint16_t i=0; i<num_samples; i++)
			  b[i] = demodSamplePop();
		  CDC_Transmit_FS((uint8_t*)b, num_samples*sizeof(uint16_t));*/

		  if(!lsf_found && !str_found && !pkt_found)
		  {
			  //consume sample
			  for(uint16_t i=0; i<arrlen(sw_corr_samples)-1; i++)
				  sw_corr_samples[i]=sw_corr_samples[i+1];
			  sw_corr_samples[arrlen(sw_corr_samples)-1] = -fltSample(demodSamplePop());

			  //squared-L2 check against syncwords
			  float symbols[8];
			  for(uint8_t i=0; i<8; i++)
				  symbols[i]=sw_corr_samples[i*5];

			  //find LSF
			  float dist = sq_eucl_norm(symbols, lsf_sync_symbols, 8);
			  if(dist < 2.5)
			  {
				  //find L2 minimum
				  sample_offset = 0;
				  for (uint8_t i=1; i<=2; i++) //search further, up to floor(5/2)=2 symbols
				  {
					  for(uint8_t j=0; j<8; j++)
						  symbols[j]=sw_corr_samples[i+j*5];

					  float d = sq_eucl_norm(symbols, lsf_sync_symbols, 8);

					  if(d < dist)
					  {
						  sample_offset = i;
						  dist = d;
					  }
				  }

				  //dbg_print("[Debug] LSF syncword found at offset %d, dist=%.1f\n", sample_offset, dist);

				  lsf_found = 1;
				  continue;
			  }

			  //find stream frame
			  dist = sq_eucl_norm(symbols, str_sync_symbols, 8);
			  if(dist < 2.5)
			  {
				  //find L2 minimum
				  sample_offset = 0;
				  for (uint8_t i=1; i<=2; i++) //search further, up to floor(5/2)=2 symbols
				  {
					  for(uint8_t j=0; j<8; j++)
						  symbols[j]=sw_corr_samples[i+j*5];

					  float d = sq_eucl_norm(symbols, str_sync_symbols, 8);

					  if(d < dist)
					  {
						  sample_offset = i;
						  dist = d;
					  }
				  }

				  //dbg_print("[Debug] STR syncword found at offset %d, dist=%.1f\n", sample_offset, dist);

				  str_found = 1;
				  continue;
			  }

			  //find stream frame
			  dist = sq_eucl_norm(symbols, pkt_sync_symbols, 8);
			  if(dist < 2.5)
			  {
				  //find L2 minimum
				  sample_offset = 0;
				  for (uint8_t i=1; i<=2; i++) //search further, up to floor(5/2)=2 symbols
				  {
					  for(uint8_t j=0; j<8; j++)
						  symbols[j]=sw_corr_samples[i+j*5];

					  float d = sq_eucl_norm(symbols, pkt_sync_symbols, 8);

					  if(d < dist)
					  {
						  sample_offset = i;
						  dist = d;
					  }
				  }

				  //dbg_print("[Debug] PKT syncword found at offset %d, dist=%.1f\n", sample_offset, dist);

				  pkt_found = 1;
				  continue;
			  }
		  }
		  else
		  {
			  if (demodSamplesGetNum() >= SYM_PER_PLD*5+sample_offset)
			  {
				  // we need to use the sample from sw_corr_samples[]
				  pld_symbs[0] = sw_corr_samples[8*5+sample_offset];
				  for (uint8_t i=0; i<sample_offset; i++)
					  fltSample(demodSamplePop());

				  // push the rest of the samples
				  for (uint16_t i=1; i<SYM_PER_PLD-1; i++)
				  {
					  pld_symbs[i] = -fltSample(demodSamplePop());
					  for (uint8_t j=0; j<4; j++)
						  fltSample(demodSamplePop());
				  }

				  //decode stuff based on what it is
				  if (lsf_found)
				  {
					  uint32_t e = decode_LSF(&lsf_rx, pld_symbs); // this func returns viterbi metric 'e'
					  float err = (float)e/0xFFFFU;

					  decode_callsign_bytes(rcvd_msg.dst, lsf_rx.dst);
					  decode_callsign_bytes(rcvd_msg.src, lsf_rx.src);
					  uint16_t type=((uint16_t)lsf_rx.type[0]<<8|lsf_rx.type[1]);
					  uint8_t can=(type>>7)&0xFU;
					  uint16_t crc=(((uint16_t)lsf_rx.crc[0]<<8)|lsf_rx.crc[1]);

					  // if CRC matches data
					  if (LSF_CRC(&lsf_rx)==crc)
					  {
						  dbg_print("[Debug] LSF received\n SRC: %s\n DST: %s\n TYPE: %04X\n CAN: %d\n META: ",
								  rcvd_msg.src, rcvd_msg.dst, type, can);
						  for (uint8_t i=0; i<sizeof(lsf_rx.meta); i++)
							  dbg_print("%02X", lsf_rx.meta[i]);
						  dbg_print("\n ERR %.1f\n", err);
					  }

					  lsf_found = 0;
				  }
				  else if (str_found)
				  {
					  uint8_t frame_data[16];
					  uint8_t lich[5];
					  uint16_t fn;
					  uint8_t lich_cnt;
					  decode_str_frame(frame_data, lich, &fn, &lich_cnt, pld_symbs);

					  dbg_print("(%04X) ", fn);
					  for (uint8_t i=0; i<16; i++)
					  	  dbg_print("%02X", frame_data[i]);
					  dbg_print("\n");

					  str_found = 0;
				  }
				  else //pkt_found
				  {
					  uint8_t frame_data[25] = {0};
					  uint8_t eof = 0;
					  uint8_t fn = 0;

					  decode_pkt_frame(frame_data, &eof, &fn, pld_symbs);

					  dbg_print("(%02X) ", fn);
					  for (uint8_t i=0; i<25; i++)
					  	  dbg_print("%02X", frame_data[i]);
					  dbg_print("\n");

					  ; //TODO: add packet collector here

					  //display last message contents
					  //we are using last-received LSF here, which might be wrong
					  //TODO: this logic assumes that the message is single-framed. fix this
					  if (eof)
					  {
						  size_t len = fn - 4;
						  if (CRC_M17(frame_data, fn)==0)
						  {
							  memcpy(rcvd_msg.text, &frame_data[1], len);
							  rcvd_msg.text[len] = 0;
							  pending_disp_state = DISP_TEXT_MSG_RCVD;
						  }
					  }

					  pkt_found = 0;
				  }

				  // work done: clear old syncword detection buffer
				  memset(sw_corr_samples, 0, sizeof(sw_corr_samples));
			  }
		  }
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
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T8_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.ScanConvMode = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc2.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DMAContinuousRequests = ENABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

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
  htim2.Init.Prescaler = 35-1;
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
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 2000-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 16800-1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

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
  * @brief TIM8 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM8_Init(void)
{

  /* USER CODE BEGIN TIM8_Init 0 */

  /* USER CODE END TIM8_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM8_Init 1 */

  /* USER CODE END TIM8_Init 1 */
  htim8.Instance = TIM8;
  htim8.Init.Prescaler = 1-1;
  htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim8.Init.Period = 7000-1;
  htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim8.Init.RepetitionCounter = 0;
  htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim8) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim8, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM8_Init 2 */

  /* USER CODE END TIM8_Init 2 */

}

/**
  * @brief TIM14 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM14_Init(void)
{

  /* USER CODE BEGIN TIM14_Init 0 */

  /* USER CODE END TIM14_Init 0 */

  /* USER CODE BEGIN TIM14_Init 1 */

  /* USER CODE END TIM14_Init 1 */
  htim14.Instance = TIM14;
  htim14.Init.Prescaler = 2000-1;
  htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim14.Init.Period = 42000-1;
  htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim14) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM14_Init 2 */

  /* USER CODE END TIM14_Init 2 */

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
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  /* DMA2_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);

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
#ifdef USE_FULL_ASSERT
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
