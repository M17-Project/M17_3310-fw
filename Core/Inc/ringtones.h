#ifndef INC_RINGTONES_H_
#define INC_RINGTONES_H_

#include <stdint.h>
#include "platform.h"

typedef struct note_t
{
	float frequency;
	uint16_t duration;
	uint16_t gap;
} note_t;

typedef struct melody_t
{
	uint16_t length;
	note_t *notes;
} melody_t;

extern const melody_t ringtones[2];

void playMelody(melody_t melody);

#endif /* INC_RINGTONES_H_ */
