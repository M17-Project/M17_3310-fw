//--------------------------------------------------------------------
// 3310 menus/displays state machine related stuff - menus.h
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 7 December 2025
//--------------------------------------------------------------------
#ifndef INC_MENUS_H_
#define INC_MENUS_H_

#include <stdint.h>
#include <m17.h>
#include "version.h"

#define MAX_NUM_ITEMS		16

typedef enum disp_state
{
	DISP_NONE,
	DISP_SPLASH,
	DISP_MAIN_SCR,
	DISP_MAIN_MENU,
	DISP_SETTINGS,
	DISP_RADIO_SETTINGS,
	DISP_DISPLAY_SETTINGS,
	DISP_KEYBOARD_SETTINGS,
	DISP_M17_SETTINGS,
	DISP_INFO,
	DISP_DEBUG,
    DISP_TEXT_MSG_ENTRY,
	DISP_TEXT_VALUE_ENTRY,
	DISP_NUM_VALUE_ENTRY,
	DISP_ON_OFF_ENTRY
} disp_state_t;

typedef struct disp
{
	char title[18];							//title
	uint8_t num_items;						//number of items
	char item[MAX_NUM_ITEMS][24];			//entries
	char value[MAX_NUM_ITEMS][24];			//values
	disp_state_t next_disp[MAX_NUM_ITEMS];	//next display, one per item
	disp_state_t prev_disp;					//previous display
} disp_t;

extern disp_t displays[13];

#endif
