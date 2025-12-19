#include "dsp.h"

const float rrc_taps_10_poly[90] =
{
	//phase 0
	-0.003195702904062073f, 0.000958710871218619f,  0.013421952197060707f,
	-0.033554880492651755f, 0.359452932027607974f, -0.033554880492651755f,
	 0.013421952197060707f, 0.000958710871218619f, -0.003195702904062073f,

	//phase 1
	-0.002930279157647190f, -0.001749908029791816f,  0.012730475385624438f,
	-0.008513823955725943f,  0.350895727088112619f, -0.047340680079891187f,
	 0.011231038205363532f,  0.003297923526848786f,  0.000000000000000000f,

	//phase 2
	-0.001940667871554463f, -0.004238694106631223f,  0.008449554307303753f,
	 0.027696543159614194f,  0.326040247765297220f, -0.050979050575999614f,
	 0.007215575568844704f,  0.004824746306020221f,  0.000000000000000000f,

	//phase 3
	-0.000356087678023658f, -0.005881783042101693f,  0.000436744366018287f,
	 0.073664520037517042f,  0.287235637987091563f, -0.046500883189991064f,
	 0.002547854551539951f,  0.005310860846138910f,  0.000000000000000000f,

	//phase 4
	 0.001547011339077758f, -0.006150256456781309f, -0.010735380379191660f,
	 0.126689053778116234f,  0.238080025892859704f, -0.036498030780605324f,
	-0.001704189656473565f,  0.004761898604225673f,  0.000000000000000000f,

	//phase 5
	 0.003389554791179751f, -0.004745376707651645f, -0.023726883538258272f,
	 0.182990955139333916f,  0.182990955139333916f, -0.023726883538258272f,
	-0.004745376707651645f,  0.003389554791179751f,  0.000000000000000000f,

	//phase 6
	 0.004761898604225673f, -0.001704189656473565f, -0.036498030780605324f,
	 0.238080025892859704f,  0.126689053778116234f, -0.010735380379191660f,
	-0.006150256456781309f,  0.001547011339077758f,  0.000000000000000000f,

	//phase 7
	 0.005310860846138910f,  0.002547854551539951f, -0.046500883189991064f,
	 0.287235637987091563f,  0.073664520037517042f,  0.000436744366018287f,
	-0.005881783042101693f, -0.000356087678023658f,  0.000000000000000000f,

	//phase 8
	 0.004824746306020221f,  0.007215575568844704f, -0.050979050575999614f,
	 0.326040247765297220f,  0.027696543159614194f,  0.008449554307303753f,
	-0.004238694106631223f, -0.001940667871554463f,  0.000000000000000000f,

	//phase 9
	 0.003297923526848786f,  0.011231038205363532f, -0.047340680079891187f,
	 0.350895727088112619f, -0.008513823955725943f,  0.012730475385624438f,
	-0.001749908029791816f, -0.002930279157647190f,  0.000000000000000000f
};

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

//TX baseband filtering (sps=10)
/*void fltSymbols(uint16_t out[restrict SYM_PER_FRA*10], const int8_t in[restrict SYM_PER_FRA], const float* __restrict flt, uint8_t phase_inv)
{
	static float sr[81*2] = {0};
	static uint8_t w = 0;

	for (uint8_t i = 0; i < SYM_PER_FRA; i++)
	{
        const float symbol = phase_inv ? -(float)in[i] : (float)in[i];

		for (uint8_t j = 0; j < 10; j++)
		{
			const float x = (j == 0) ? symbol : 0.0f;

			sr[w]	   = x;
			sr[w + 81] = x;

			const float * __restrict hp = &sr[w];
			const float * __restrict tp = flt;

			float acc = 0.0f;

			for (uint8_t k = 0; k < 80; k += 4)
			{
				acc += hp[k]     * tp[k];
				acc += hp[k + 1] * tp[k + 1];
				acc += hp[k + 2] * tp[k + 2];
				acc += hp[k + 3] * tp[k + 3];
			}

			acc += hp[80] * tp[80];

			out[i*10 + j] = (uint16_t)(DAC_IDLE + acc * 250.0f);

			if (w == 0)
                w = 80;
            else
                w--;
		}
	}
}*/

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
