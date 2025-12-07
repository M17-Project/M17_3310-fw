#include "platform.h"

//set backlight - 0..255
void setBacklight(uint8_t level)
{
	TIM2->CCR3 = level;
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
}

//activate the vibrator
void actVibr(uint8_t period)
{
	VIBR_GPIO_Port->BSRR = (uint32_t)VIBR_Pin;
	HAL_Delay(period);
	VIBR_GPIO_Port->BSRR = ((uint32_t)VIBR_Pin<<16);
}

//blocking
void playBeep(uint16_t duration)
{
	//1kHz (84MHz master clock)
	TIM1->ARR = 10-1;
	TIM1->PSC = 8400-1; //TODO: check if this really works

	HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
	HAL_Delay(duration);
	HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
}
