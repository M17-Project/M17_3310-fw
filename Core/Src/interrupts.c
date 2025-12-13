#include "interrupts.h"

//interrupts
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	//TIM1 - buzzer timer (by default at 1kHz)
	//TIM2 - LED backlight PWM timer
	//TIM3 - 5Hz ADC timer (for the battery voltage ADC)
	//TIM6 - 48kHz base timer (for the TX baseband DAC)

	//TIM7 - text entry timer
	if(htim->Instance==TIM7)
	{
		HAL_TIM_Base_Stop(&htim7);
		TIM7->CNT=0;
	}

	//TIM8 - 24kHz base timer (for the RX baseband ADC)

	//TIM14 - display backlight timeout timer
	else if(htim->Instance==TIM14)
	{
		HAL_TIM_Base_Stop(&htim14);
		TIM14->CNT=0;
		setBacklight(0);
	}
}

void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
	if(radio_state==RF_TX)
	{
		bsb_tx_dma_half = 0;
		frame_pend = 1;
	}
}

void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
	if(radio_state==RF_TX)
	{
		bsb_tx_dma_half = 1;
		frame_pend = 1;
	}
}

//ADC
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	;
}
