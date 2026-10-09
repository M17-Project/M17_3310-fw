#include "interrupts.h"
#include "ring.h"
#include "m17_tx.h"

//interrupts
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	//TIM1 - buzzer timer
	//TIM2 - LED backlight PWM timer
	//TIM3 - 5Hz ADC timer (for the battery voltage ADC)
	//TIM6 - 48kHz base timer (for the TX baseband DAC), runs only during TX

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

//TX baseband DMA: first half played
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
	(void)hdac;
	m17TxDmaCallback(0);
}

//TX baseband DMA: second half played
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
	(void)hdac;
	m17TxDmaCallback(1);
}

//RX baseband DMA: count completed halves (overrun detection)
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADC1)
		bsbRxDmaIrq();
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADC1)
		bsbRxDmaIrq();
}
