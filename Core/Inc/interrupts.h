#ifndef INC_INTERRUPTS_H_
#define INC_INTERRUPTS_H_

#include "main.h"
#include "typedefs.h"
#include "platform.h"
#include <m17.h>

extern TIM_HandleTypeDef htim7;
extern TIM_HandleTypeDef htim14;

extern radio_state_t radio_state;
extern volatile uint8_t frame_pend;
extern volatile uint8_t bsb_tx_dma_half;
extern uint16_t frame_samples[2][SYM_PER_FRA*10];
extern uint8_t frame_cnt;

//interrupts
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac);
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac);
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);

#endif /* INC_INTERRUPTS_H_ */
