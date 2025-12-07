#include <stdint.h>
#include "ring.h"

//ring buffer functions
void initRing(ring_t *ring, void *buffer, uint16_t size)
{
    ring->buffer = buffer;
    ring->size = size;
    ring->wr_pos = 0;
    ring->rd_pos = 0;
}

uint16_t getNumItems(ring_t *ring)
{
	return (ring->wr_pos - ring->rd_pos + ring->size) % ring->size;
}

uint16_t getSize(ring_t *ring)
{
    return ring->size;
}

uint8_t pushU16Value(ring_t *ring, uint16_t val)
{
    uint16_t size = getSize(ring);

    if(getNumItems(ring)<size)
    {
        ((uint16_t*)ring->buffer)[ring->wr_pos]=val;
        ring->wr_pos = (ring->wr_pos+1) % size;
        return 0;
    }
    else
    {
        return 1;
    }
}

uint16_t popU16Value(ring_t *ring)
{
    if(getNumItems(ring))
    {
        uint16_t size = getSize(ring);
        uint16_t pos = ring->rd_pos;

        ring->rd_pos = (ring->rd_pos+1) % size;

        return ((uint16_t*)ring->buffer)[pos];
    }

    return 0;
}

uint8_t pushFloatValue(ring_t *ring, float val)
{
    uint16_t size = getSize(ring);

    if(getNumItems(ring)<size)
    {
        ((float*)ring->buffer)[ring->wr_pos]=val;
        ring->wr_pos = (ring->wr_pos+1) % size;
        return 0;
    }
    else
    {
        return 1;
    }
}

float popFloatValue(ring_t *ring)
{
    if(getNumItems(ring))
    {
        uint16_t size = getSize(ring);
        uint16_t pos = ring->rd_pos;

        ring->rd_pos = (ring->rd_pos+1) % size;

        return ((float*)ring->buffer)[pos];
    }

    return 0.0f;
}
