#ifndef INC_RING_H_
#define INC_RING_H_

//ring buffer
typedef struct ring
{
    void *buffer;
    uint16_t size;
    volatile uint16_t wr_pos;
    volatile uint16_t rd_pos;
} ring_t;

//ring buffer functions
void initRing(ring_t *ring, void *buffer, uint16_t size);

uint16_t getNumItems(ring_t *ring);
uint16_t getSize(ring_t *ring);

uint8_t pushU16Value(ring_t *ring, uint16_t val);
uint16_t popU16Value(ring_t *ring);

uint8_t pushFloatValue(ring_t *ring, float val);
float popFloatValue(ring_t *ring);

#endif /* INC_RING_H_ */
