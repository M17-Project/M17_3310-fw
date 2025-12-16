#ifndef INC_TEXT_ENTRY_H_
#define INC_TEXT_ENTRY_H_

#include <stdint.h>
#include "typedefs.h"

typedef struct
{
	text_entry_t mode;
	char buffer[256];
	uint16_t pos;
} abc_t;

#endif /* INC_TEXT_ENTRY_H_ */
