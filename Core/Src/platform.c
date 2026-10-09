#include "platform.h"
#include <math.h>

//get battery voltage in mV (U_BATT is divided by 2: R11/R12 = 47k/47k)
uint16_t getBattVoltage(void)
{
	return (uint32_t)batt_adc * 3300U * 2U / 4095U;
}

//is the charger active? (/CHG low)
uint8_t isCharging(void)
{
	return (CHG_GPIO_Port->IDR & CHG_Pin) ? 0 : 1;
}

//set backlight - 0..255
void setBacklight(uint8_t level)
{
	if (level == 0)
	{
		//stop the PWM timer completely (Q5 gate is pulled down by R17)
		TIM2->CCR3 = 0;
		HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
		return;
	}

	//set intensity
	TIM2->CCR3 = (uint16_t)level * (uint16_t)level / 255; //apply gamma correction
	if (TIM_CHANNEL_STATE_GET(&htim2, TIM_CHANNEL_3) != HAL_TIM_CHANNEL_STATE_BUSY)
		HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
}

//set backlight timeout timer (seconds)
void setBacklightTimer(uint8_t t)
{
	if (t<=32)
	{
		__HAL_TIM_DISABLE(&htim14);
		TIM14->PSC = (uint32_t)t*2000-1;	//1s corresponds to 2000
		TIM14->EGR = TIM_EGR_UG;			//update ARR/PSC parameters
	}
}

//start the backlight timer
void startBacklightTimer(void)
{
	FIX_TIMER_TRIGGER(&htim14);
	HAL_TIM_Base_Start_IT(&htim14);
}

//activate the vibrator (non-blocking, platformTick() turns it off)
static uint32_t vibr_off_tick;
static uint8_t vibr_on;

void actVibr(uint8_t period)
{
	VIBR_GPIO_Port->BSRR = (uint32_t)VIBR_Pin;
	vibr_off_tick = HAL_GetTick() + period;
	vibr_on = 1;
}

//call from the main loop
void platformTick(void)
{
	if (vibr_on && (int32_t)(HAL_GetTick() - vibr_off_tick) >= 0)
	{
		VIBR_GPIO_Port->BSRR = ((uint32_t)VIBR_Pin<<16);
		vibr_on = 0;
	}
}

//TIM1 kernel clock (APB2 timer clock) - independent of the clock tree setup
static uint32_t getTim1Clock(void)
{
	uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();

	//timer clock is 2x PCLK2 if the APB2 prescaler is not 1
	if ((RCC->CFGR & RCC_CFGR_PPRE2) != RCC_CFGR_PPRE2_DIV1)
		pclk2 *= 2U;

	return pclk2;
}

//blocking
void playBeep(float freq, uint16_t duration)
{
	if (freq < 10.0f)
		return;

	const float tim_clk = (float)getTim1Clock();

	uint32_t div = (uint32_t)(tim_clk / freq);
	if (div < 2)
		div = 2;

	uint32_t psc = div / 65535;
	if (psc > 0xFFFF)
		psc = 0xFFFF;

	uint32_t arr = div / (psc + 1);

	if (arr < 2)
		arr = 2;
	else if (arr > 65535)
		arr = 65535;

	//update PSC/ARR
	__HAL_TIM_DISABLE(&htim1);
	TIM1->PSC = psc;
	TIM1->ARR = arr - 1;
	TIM1->CCR1 = (arr - 1) / 2;
	TIM1->EGR = TIM_EGR_UG;

	HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
	HAL_Delay(duration);
	HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
}

//independent watchdog, clocked from LSI (17..47 kHz, 32 kHz typical)
//prescaler 64, reload 4095 -> ~8.2 s typical, at least ~5.6 s
//(longest legitimate blocking operation: Flash sector erase, ~2 s max)
void wdgInit(void)
{
	DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;	//pause while halted by a debugger

	IWDG->KR = 0xCCCCU;		//start
	IWDG->KR = 0x5555U;		//unlock PR/RLR
	IWDG->PR = 4U;			//divide by 64
	IWDG->RLR = 4095U;
	while (IWDG->SR != 0U)	//wait for the registers to update
		;
	IWDG->KR = 0xAAAAU;		//reload
}

void wdgKick(void)
{
	IWDG->KR = 0xAAAAU;
}

//battery protection - call once per second while receiving
//returns 1 if the battery has been below BATT_CUTOFF_MV for BATT_CUTOFF_SECS
uint8_t battCheckEmpty(void)
{
#if BATT_CUTOFF_MV > 0
	static uint8_t low_cnt;

	if (!isCharging() && getBattVoltage() < BATT_CUTOFF_MV)
	{
		if (low_cnt < BATT_CUTOFF_SECS)
			low_cnt++;
	}
	else
	{
		low_cnt = 0;
	}

	return (low_cnt >= BATT_CUTOFF_SECS) ? 1 : 0;
#else
	return 0;
#endif
}

//switch the whole device off (PWR_OFF drives the MAX16054 CLR input)
void powerOff(void)
{
	RF_ENA_GPIO_Port->BSRR = (uint32_t)RF_ENA_Pin << 16U;	//RF module off
	setBacklight(0);
	PWR_OFF_GPIO_Port->BSRR = (uint32_t)PWR_OFF_Pin;

	//wait for the supply to drop; should it stay up anyway,
	//the watchdog resets the MCU (no kicking here)
	while (1)
		;
}
