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

	DISP_MESSAGES,
	DISP_INBOX,
	DISP_MSG_VIEW,
	DISP_MSG_OPTIONS,
	DISP_MSG_NOTIFY,

	DISP_SETTINGS,
	DISP_RADIO_SETTINGS,
	DISP_DISPLAY_SETTINGS,
	DISP_ALERT_SETTINGS,
	DISP_KEYBOARD_SETTINGS,
	DISP_M17_SETTINGS,

	DISP_INFO,
	DISP_DEBUG,

	DISP_TEXT_MSG_ENTRY,
	DISP_TEXT_VALUE_ENTRY,
	DISP_OPTION_SELECT,
	DISP_TEXT_VIEW,

	DISP_NUM_ITEMS
} disp_state_t;

#define MAX_NUM_ITEMS	8		//items per static menu
#define LIST_ROWS		4		//visible list rows

//static menu definition
typedef struct disp
{
	const char *title;							// title
	uint8_t num_items;							// number of items
	const char *item[MAX_NUM_ITEMS];			// entries
	disp_state_t next_disp[MAX_NUM_ITEMS];		// submenu per item (DISP_NONE: use the field)
	edit_set_t edit[MAX_NUM_ITEMS];				// field per item: value column, editor, action
	disp_state_t prev_disp;						// fallback parent
} disp_t;

//display device (defined in display.h)
typedef struct disp_dev_t disp_dev_t;

#endif /* INC_UI_TYPES_H_ */
