#include "platform.h"

//set backlight - 0..255
void setBacklight(uint8_t level)
{
	TIM2->CCR3 = level;
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
}

//set backlight timeout timer (seconds)
void setBacklightTimer(uint8_t t)
{
	if (t<=32)
		TIM14->PSC = (uint32_t)t*2000-1; //1s corresponds to 2000
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

	uint16_t arr = 10, psc = 0;

	while (84e6f/freq/arr > 0xFFFF)
		arr+=10;
	psc = 84e6f/freq/arr;

	TIM1->ARR = arr - 1;
	TIM1->PSC = psc - 1;

	HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
	HAL_Delay(duration);
	HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
}
