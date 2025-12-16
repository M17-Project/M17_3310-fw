#include "ring.h"

uint16_t raw_bsb_buff[BSB_BUFF_SIZ];
volatile uint16_t raw_bsb_buff_tail;

//ring buffer functions
uint16_t demodGetHead(void)
{
    return (BSB_BUFF_SIZ - hadc1.DMA_Handle->Instance->NDTR) % BSB_BUFF_SIZ;
}

uint8_t demodIsOverrun(void)
{
    return (demodGetHead() + 1) % BSB_BUFF_SIZ == raw_bsb_buff_tail;
}

uint16_t demodSamplesGetNum(void)
{
    int16_t n = demodGetHead() - raw_bsb_buff_tail;
    if (n < 0)
    	n += BSB_BUFF_SIZ;
    return n;
}

uint16_t demodSamplePop(void)
{
    uint16_t v = raw_bsb_buff[raw_bsb_buff_tail];
    raw_bsb_buff_tail = (raw_bsb_buff_tail + 1) % BSB_BUFF_SIZ;
    return v;
}
