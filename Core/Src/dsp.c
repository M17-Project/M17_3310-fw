#include "dsp.h"

//RX baseband filtering (sps=5)
float fltSample(const uint16_t sample)
{
	const float gain = 18.0f*1.8f/2048.0f/sqrtf(5.0f); //gain found experimentally
	static int16_t sr[41];

	//push the shift register
	for(uint8_t i=0; i<40; i++)
		sr[i]=sr[i+1];
	sr[40]=(int16_t)sample-2048; //push the sample, remove DC offset

	float acc=0.0f;
	for(uint8_t i=0; i<41; i++)
	{
		acc += (float)sr[i]*rrc_taps_5[i];
	}

	return acc*gain;
}

//flush RX baseband filter
void flushBsbFlt(void)
{
	for(uint8_t i=0; i<41; i++)
		fltSample(0);
}

//TX baseband filtering (sps=10)
void filter_symbols(uint16_t out[SYM_PER_FRA*10], const int8_t in[SYM_PER_FRA], const float* flt, uint8_t phase_inv)
{
	static int8_t last[81]; //memory for last symbols

	for(uint8_t i=0; i<SYM_PER_FRA; i++)
	{
		for(uint8_t j=0; j<10; j++)
		{
			for(uint8_t k=0; k<80; k++)
				last[k]=last[k+1];

			if(j==0)
			{
				if(phase_inv) //optional phase inversion
					last[80]=-in[i];
				else
					last[80]= in[i];
			}
			else
				last[80]=0;

			float acc=0.0f;
			for(uint8_t k=0; k<81; k++)
				acc+=last[k]*flt[k];

			out[i*10+j]=DAC_IDLE+acc*250.0f;
		}
	}
}
