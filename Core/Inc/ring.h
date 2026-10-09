#ifndef INC_RING_H_
#define INC_RING_H_

#include <stdint.h>
#include "main.h"

//RX baseband ring buffer, filled by ADC1 + DMA (circular) at 24 kHz
//4096 samples = ~170 ms of slack for the main loop (must be even)
#define BSB_BUFF_SIZ (2048*2)

extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim8;

void bsbRxStart(void);
void bsbRxStop(void);
void bsbRxDmaIrq(void);

uint8_t demodCheckOverrun(void);
void demodResync(void);
uint16_t demodSamplesGetNum(void);
uint16_t demodSamplePop(void);

#endif /* INC_RING_H_ */
