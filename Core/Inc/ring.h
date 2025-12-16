#ifndef INC_RING_H_
#define INC_RING_H_

#include <stdint.h>
#include "main.h"

#define BSB_BUFF_SIZ (960*2)

extern ADC_HandleTypeDef hadc1;

extern uint16_t raw_bsb_buff[BSB_BUFF_SIZ];
extern volatile uint16_t raw_bsb_buff_tail;

uint16_t demodGetHead(void);
uint8_t demodIsOverrun(void);
uint16_t demodSamplesGetNum(void);
uint16_t demodSamplePop(void);

#endif /* INC_RING_H_ */
