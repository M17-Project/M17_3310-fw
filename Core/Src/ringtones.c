#include "ringtones.h"
#include "macros.h"

void playMelody(melody_t melody)
{
	for (uint16_t i=0; i<melody.length; i++)
	{
		playBeep(melody.notes[i].frequency, melody.notes[i].duration);
		HAL_Delay(melody.notes[i].gap-1);
	}
}

const melody_t ringtone1 =
{
	7,
	(note_t[])
	{
		{659.25, 167-30, 30},	// E5
		{659.25, 167-30, 167},	// E5
		{659.25, 167-30, 167},	// E5
		{523.25, 167-30, 30},	// C5
		{659.25, 333-30, 30},	// E5
		{783.99, 333, 333},		// G5
		{392.00, 333, 1}		// G4
	}
};

const melody_t ringtone2 =
{
	9,
	(note_t[])
	{
		{659.25, 180, 30},	// E5
		{622.25, 180, 30},	// D#5
		{659.25, 180, 30},	// E5
		{622.25, 180, 30},	// D#5
		{659.25, 180, 30},	// E5
		{493.88, 200, 30},	// B4
		{587.33, 200, 30},	// D5
		{523.25, 200, 30},	// C5
		{440.00, 350, 1}	// A4
	}
};

const melody_t ringtones[2] =
{
	ringtone1,
	ringtone2
};
