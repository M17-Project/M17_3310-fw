#ifndef INC_TEXT_ENTRY_H_
#define INC_TEXT_ENTRY_H_

#include <stdint.h>
#include "typedefs.h"

//text is stored in the internal 8-bit charset (see charset.h)
typedef struct
{
	text_entry_t mode;
	char buffer[256];
	uint16_t pos;
	uint16_t max_len;		//max number of characters (<= sizeof(buffer)-1)
	uint8_t modes;			//allowed entry modes (TM() bitmask)
	kbd_key_t last_key;		//multi-tap: key that produced the last character
} abc_t;

#endif /* INC_TEXT_ENTRY_H_ */
