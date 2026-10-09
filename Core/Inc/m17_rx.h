#ifndef INC_M17_RX_H_
#define INC_M17_RX_H_

#include <stdint.h>
#include "typedefs.h"

extern msg_t rcvd_msg;	//last correctly received text message

void m17RxReset(void);
uint16_t m17RxSamplesNeeded(void);
uint8_t m17RxProcess(void);

#endif /* INC_M17_RX_H_ */
