#ifndef INC_RINGTONES_H_
#define INC_RINGTONES_H_

#include <stdint.h>
#include "platform.h"

typedef struct note_t
{
	float frequency;	//Hz, 0 = rest
	uint16_t duration;	//ms
	uint16_t gap;		//ms of silence after the note
} note_t;

typedef struct melody_t
{
	const char *name;
	uint16_t length;
	const note_t *notes;
} melody_t;

//ringtones[0] is "Off" (no notes)
extern const melody_t ringtones[];
extern const uint8_t ringtones_num;

//non-blocking player, ringtoneTick() has to be called from the main loop
void ringtonePlay(uint8_t idx);
void ringtoneStop(void);
uint8_t ringtoneActive(void);
void ringtoneTick(void);

//blocking
void playMelody(const melody_t *melody);

#endif /* INC_RINGTONES_H_ */
