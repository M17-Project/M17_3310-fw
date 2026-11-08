#ifndef INC_MENUS_H_
#define INC_MENUS_H_

#include <stdint.h>
#include "typedefs.h"
#include "settings.h"
#include "ui_types.h"
#include "text_entry.h"

//what a menu item does when selected
typedef enum
{
	FK_NONE,		//nothing (or a submenu via next_disp)
	FK_TEXT,		//text editor
	FK_NUMBER,		//text editor, numbers only
	FK_OPTION,		//option list
	FK_INFO,		//read only, OK shows the full value
	FK_ACTION		//OK runs action()
} field_kind_t;

typedef struct
{
	field_kind_t kind;
	const char *title;								//editor title (NULL: menu item label)
	void (*get)(char *out, uint8_t n);				//short value for the menu's value column
	void (*get_full)(char *out, uint8_t n);			//editor prefill / full info text (NULL: get)

	//FK_TEXT, FK_NUMBER
	uint8_t (*set_text)(const char *s);				//0: accepted, 1: invalid
	uint8_t max_len;
	uint8_t modes;									//allowed entry modes (TM() bitmask)
	text_entry_t first_mode;

	//FK_OPTION
	uint8_t (*opt_num)(void);
	void (*opt_name)(uint8_t i, char *out, uint8_t n);
	uint8_t (*opt_get)(void);						//currently selected option
	void (*opt_set)(uint8_t i);
	void (*opt_preview)(uint8_t i, uint8_t active);	//live preview while scrolling, active=0: end it

	//FK_ACTION
	void (*action)(void);

	uint8_t save;									//store the settings after a change
} field_t;

extern const disp_t displays[DISP_NUM_ITEMS];

const field_t *fieldGet(edit_set_t e);
void fieldValue(edit_set_t e, char *out, uint8_t n);	//short value ("" if none)
void fieldFullText(edit_set_t e, char *out, uint8_t n);	//prefill / full value

uint8_t isStaticMenu(disp_state_t s);
void applyDeviceSettings(void);							//backlight, contrast, keyboard

#endif
