#include "ring.h"
#include "macros.h"

static uint16_t raw_bsb_buff[BSB_BUFF_SIZ];
static uint16_t raw_bsb_buff_tail;

//overrun detection: the DMA half/full-transfer interrupts count completed
//halves, so the total number of produced samples is known even after the
//write pointer has lapped the read pointer
static volatile uint32_t dma_half_cnt;
static uint32_t consumed;

#define HALF_SIZ (BSB_BUFF_SIZ/2)

static uint16_t demodGetHead(void)
{
	return (BSB_BUFF_SIZ - hadc1.DMA_Handle->Instance->NDTR) % BSB_BUFF_SIZ;
}

//total samples written by the DMA since bsbRxStart()
static uint32_t demodProduced(void)
{
	uint32_t c;
	uint16_t head;

	do
	{
		c = dma_half_cnt;
		head = demodGetHead();
	} while (c != dma_half_cnt);

	//after c completed halves the DMA writes into half (c & 1); if the write
	//pointer is already in the other half, its interrupt is still pending
	if ((head / HALF_SIZ) != (c & 1))
		c++;

	return c * HALF_SIZ + (head % HALF_SIZ);
}

//start baseband sampling (ADC1 triggered by TIM8)
void bsbRxStart(void)
{
	HAL_TIM_Base_Stop(&htim8);
	HAL_ADC_Stop_DMA(&hadc1);

	raw_bsb_buff_tail = 0;
	consumed = 0;
	dma_half_cnt = 0;

	HAL_ADC_Start_DMA(&hadc1, (uint32_t*)raw_bsb_buff, BSB_BUFF_SIZ);
	HAL_TIM_Base_Start(&htim8);
}

void bsbRxStop(void)
{
	HAL_TIM_Base_Stop(&htim8);
	HAL_ADC_Stop_DMA(&hadc1);
}

//call from the ADC1 DMA half- and full-transfer callbacks
void bsbRxDmaIrq(void)
{
	dma_half_cnt++;
}

//drop everything that is buffered and continue with fresh samples
void demodResync(void)
{
	uint32_t p = demodProduced();

	consumed = p;
	raw_bsb_buff_tail = p % BSB_BUFF_SIZ;
}

//returns 1 (and resyncs) if unread samples were overwritten
uint8_t demodCheckOverrun(void)
{
	if (demodProduced() - consumed > BSB_BUFF_SIZ)
	{
		demodResync();
		return 1;
	}

	return 0;
}

uint16_t demodSamplesGetNum(void)
{
	uint32_t n = demodProduced() - consumed;

	return (n > BSB_BUFF_SIZ) ? BSB_BUFF_SIZ : (uint16_t)n;
}

uint16_t demodSamplePop(void)
{
	uint16_t v = raw_bsb_buff[raw_bsb_buff_tail];

	if (++raw_bsb_buff_tail >= BSB_BUFF_SIZ)
		raw_bsb_buff_tail = 0;
	consumed++;

	return v;
}
