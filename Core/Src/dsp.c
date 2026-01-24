#include "dsp.h"

//RX baseband filtering (sps=5)
float fltSample(uint16_t sample)
{
	static const float gain = 18.0f * 1.8f / 2048.0f / sqrtf(5.0f); // found experimentally

	static int16_t sr[41*2] = {0};
	static uint8_t w = 0;

	int16_t x = (int16_t)sample - 2048;

	sr[w]      = x;
	sr[w + 41] = x;		// mirror to make region contiguous

	// __restrict allows for some optimizations
	const int16_t * __restrict hp = &sr[w];
	const float   * __restrict tp = rrc_taps_5;

	float acc = 0.0f;

	// unroll
	for (uint_fast8_t i = 0; i < 40; i += 4)
	{
		acc += (float)hp[i]     * tp[i];
		acc += (float)hp[i + 1] * tp[i + 1];
		acc += (float)hp[i + 2] * tp[i + 2];
		acc += (float)hp[i + 3] * tp[i + 3];
	}

	acc += (float)hp[40] * tp[40];

	if (w == 0)
		w = 40;
	else
		w--;

	return acc * gain;
}

//flush RX baseband filter
void flushBsbFlt(void)
{
	for(uint_fast8_t i=0; i<41; i++)
		fltSample(0);
}

//faster TX baseband filtering (sps=10)
void fltSymbolsPoly(uint16_t out[restrict SYM_PER_FRA*10], const int8_t in[restrict SYM_PER_FRA], const float* __restrict flt, uint8_t phase_inv)
{
	//history
	static float sr[TAPS_PER_PHASE * 2] = {0};
	static uint8_t w = 0;

	//precompute sign once
	const float sign = phase_inv ? -1.0f : 1.0f;

	for (uint16_t i = 0; i < SYM_PER_FRA; i++)
	{
		//insert new sample per symbol
		const float x = (float)in[i] * sign;

		//store once, duplicated for linear access
		float * __restrict hp = &sr[w];
		hp[0]			   = x;
		hp[TAPS_PER_PHASE] = x;

		//phase pointer
		const float * __restrict tp = flt;

		//generate sps (10) output samples
		for (uint8_t ph = 0; ph < 10; ph++)
		{
			float acc;

			//fully unrolled 9-tap dot product
			acc  = hp[0] * tp[0];
			acc += hp[1] * tp[1];
			acc += hp[2] * tp[2];
			acc += hp[3] * tp[3];
			acc += hp[4] * tp[4];
			acc += hp[5] * tp[5];
			acc += hp[6] * tp[6];
			acc += hp[7] * tp[7];
			acc += hp[8] * tp[8];

			out[i*10 + ph] = (uint16_t)(DAC_IDLE + acc * 250.0f);

			//advance to next phase coefficients
			tp += TAPS_PER_PHASE;
		}

		//circular index update without modulo
		if (w == 0)
			w = TAPS_PER_PHASE-1;
		else
			w--;
	}
}
