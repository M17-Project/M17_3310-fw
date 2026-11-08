#include "ringtones.h"
#include "macros.h"
#include <stddef.h>

static const note_t notes_beep[] =
{
	{1760.00, 80, 60},
	{2093.00, 120, 1}
};

static const note_t notes_mario[] =
{
	{659.25, 167-30, 30},	// E5
	{659.25, 167-30, 167},	// E5
	{659.25, 167-30, 167},	// E5
	{523.25, 167-30, 30},	// C5
	{659.25, 333-30, 30},	// E5
	{783.99, 333, 333},		// G5
	{392.00, 333, 1}		// G4
};

static const note_t notes_elise[] =
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
};

//F. Tarrega, Gran Vals (1902), bars 13-16
static const note_t notes_vals[] =
{
	{1318.51, 130, 20},	// E6
	{1174.66, 130, 20},	// D6
	{739.99, 280, 20},	// F#5
	{830.61, 280, 20},	// G#5
	{1108.73, 130, 20},	// C#6
	{987.77, 130, 20},	// B5
	{587.33, 280, 20},	// D5
	{659.25, 280, 20},	// E5
	{987.77, 130, 20},	// B5
	{880.00, 130, 20},	// A5
	{554.37, 280, 20},	// C#5
	{659.25, 280, 20},	// E5
	{880.00, 600, 1}	// A5
};

const melody_t ringtones[] =
{
	{"Off",			0,						NULL},
	{"Beep",		arrlen(notes_beep),		notes_beep},
	{"Mario",		arrlen(notes_mario),	notes_mario},
	{"Fur Elise",	arrlen(notes_elise),	notes_elise},
	{"Gran Vals",	arrlen(notes_vals),		notes_vals}
};

const uint8_t ringtones_num = arrlen(ringtones);

//--- non-blocking player ---
static const melody_t *play_mel;
static uint16_t play_idx;
static uint8_t play_in_gap;
static uint32_t play_next;

static void startNote(void)
{
	const note_t *n = &play_mel->notes[play_idx];

	if (n->frequency > 0.0f)
		buzzerStart(n->frequency);
	else
		buzzerStop();

	play_in_gap = 0;
	play_next = HAL_GetTick() + n->duration;
}

void ringtonePlay(uint8_t idx)
{
	ringtoneStop();

	if (idx >= ringtones_num || ringtones[idx].length == 0)
		return;

	play_mel = &ringtones[idx];
	play_idx = 0;
	startNote();
}

void ringtoneStop(void)
{
	if (play_mel != NULL)
		buzzerStop();

	play_mel = NULL;
}

uint8_t ringtoneActive(void)
{
	return play_mel != NULL;
}

void ringtoneTick(void)
{
	if (play_mel == NULL)
		return;

	if ((int32_t)(HAL_GetTick() - play_next) < 0)
		return;

	if (!play_in_gap)
	{
		//note done - silence
		buzzerStop();
		play_in_gap = 1;
		play_next = HAL_GetTick() + play_mel->notes[play_idx].gap;
		return;
	}

	//gap done - next note
	if (++play_idx >= play_mel->length)
	{
		play_mel = NULL;
		return;
	}

	startNote();
}

//blocking
void playMelody(const melody_t *melody)
{
	for (uint16_t i=0; i<melody->length; i++)
	{
		playBeep(melody->notes[i].frequency, melody->notes[i].duration);
		if (melody->notes[i].gap > 1) //HAL_Delay(0-1) would block forever
			HAL_Delay(melody->notes[i].gap-1);
	}
}
