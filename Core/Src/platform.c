#include "platform.h"

//get battery voltage
uint16_t getBattVoltage(void)
{
	uint16_t v[2];

	do
	{
		v[0] = batt_adc;
		v[1] = batt_adc;
	} while (v[0] != v[1]);

	return (float)*v/4095.0f * 3300.0f * 2.0f;
}

//set backlight - 0..255
void setBacklight(uint8_t level)
{
	//set intensity
	TIM2->CCR3 = level;
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

//activate the vibrator
void actVibr(uint8_t period)
{
	VIBR_GPIO_Port->BSRR = (uint32_t)VIBR_Pin;
	HAL_Delay(period);
	VIBR_GPIO_Port->BSRR = ((uint32_t)VIBR_Pin<<16);
}

//blocking
void playBeep(float freq, uint16_t duration)
{
	if (freq < 10.0f)
		return;

	const float tim_clk = 84e6f; //hard-coded

	uint32_t div = (uint32_t)(tim_clk / freq); //total division factor

	if (div < 2)
		div = 2;  //avoid ARR = 0

	uint32_t psc = (div / 65536); //minimum prescaler needed
	if (psc > 0xFFFF)
		psc = 0xFFFF; //clamp

	uint32_t arr = div / (psc + 1);

	if (arr > 0xFFFF)
		arr = 0xFFFF; //clamp

	__HAL_TIM_DISABLE(&htim1);
	TIM1->ARR = arr - 1;
	TIM1->PSC = psc;
	TIM1->EGR = TIM_EGR_UG; //update ARR/PSC parameters

	HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
	HAL_Delay(duration);
	HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
}
