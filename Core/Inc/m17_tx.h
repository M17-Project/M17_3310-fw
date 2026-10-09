#ifndef INC_M17_TX_H_
#define INC_M17_TX_H_

#include <stdint.h>
#include <m17.h>
#include "settings.h"

//Preamble length in 40 ms frames. The M17 spec needs a single preamble frame;
//the long default (1 s) is kept until a shorter value has been verified with
//real receivers (SA868 TX ramp-up, receiver AGC/squelch).
#ifndef TX_PREAMBLE_FRAMES
#define TX_PREAMBLE_FRAMES	25
#endif

//hard limit for a single transmission
#define TX_TIMEOUT_MS		10000U

//longest SMS payload (excluding type, null terminator and CRC)
#define SMS_MAX_LEN			(33*25-1-1-2)

extern lsf_t lsf_tx;

void m17TxInit(void);
int8_t m17TxStart(const char *message, const dev_settings_t *dev_settings);
uint8_t m17TxProcess(void);
uint8_t m17TxActive(void);
uint8_t m17TxRefillPending(void);
void m17TxDmaCallback(uint8_t half_done);

#endif /* INC_M17_TX_H_ */
