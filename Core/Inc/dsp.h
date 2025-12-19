#ifndef INC_DSP_H_
#define INC_DSP_H_

#include <m17.h>

#define DAC_IDLE		2048
#define SPS_TX			10
#define SPS_RX			5
#define TAPS_PER_PHASE	9

extern const float rrc_taps_10_poly[];

float fltSample(const uint16_t sample);
void flushBsbFlt(void);
//void fltSymbols(uint16_t out[restrict SYM_PER_FRA*10], const int8_t in[restrict SYM_PER_FRA], const float* __restrict flt, uint8_t phase_inv);
void fltSymbolsPoly(uint16_t out[restrict SYM_PER_FRA*10], const int8_t in[restrict SYM_PER_FRA], const float* __restrict flt, uint8_t phase_inv);

#endif /* INC_DSP_H_ */
