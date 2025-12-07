#ifndef INC_DSP_H_
#define INC_DSP_H_

#include <m17.h>

#define DAC_IDLE 2048

float fltSample(const uint16_t sample);
void flushBsbFlt(void);
void filter_symbols(uint16_t out[SYM_PER_FRA*10], const int8_t in[SYM_PER_FRA], const float* flt, uint8_t phase_inv);

#endif /* INC_DSP_H_ */
