#ifndef INC_UI_TYPES_H_
#define INC_UI_TYPES_H_

#include <stdint.h>
#include "typedefs.h"
#include "settings.h"

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
    DISP_ON_OFF_ENTRY,

	DISP_TEXT_MSG_RCVD,

	DISP_NUM_ITEMS
} disp_state_t;

#define MAX_NUM_ITEMS  16

typedef struct disp
{
    char title[18];                             // title
    uint8_t num_items;                          // number of items
    char item[MAX_NUM_ITEMS][24];               // entries
    char value[MAX_NUM_ITEMS][24];              // values

    disp_state_t next_disp[MAX_NUM_ITEMS];      // next display per item
    disp_state_t prev_disp;                     // previous display

    edit_set_t edit[MAX_NUM_ITEMS];				// edit: parameter entry
} disp_t;


//display device (defined in display.h)
typedef struct disp_dev_t disp_dev_t;

#endif /* INC_UI_TYPES_H_ */
