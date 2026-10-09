#ifndef INC_INTERRUPTS_H_
#define INC_INTERRUPTS_H_

#include "main.h"
#include "typedefs.h"
#include "platform.h"

extern TIM_HandleTypeDef htim7;
extern TIM_HandleTypeDef htim14;

//interrupts
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac);
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac);
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc);
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);

#endif /* INC_INTERRUPTS_H_ */
