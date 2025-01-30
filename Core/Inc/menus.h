//--------------------------------------------------------------------
// 3310 menus state machine related stuff - menus.h
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 28 January 2025
//--------------------------------------------------------------------
#pragma once

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

typedef struct menu
{
	char title[24];							//title
	uint8_t num_items;						//number of items
	char item[MAX_NUM_ITEMS][24];			//entries
	char value[MAX_NUM_ITEMS][24];			//values
	disp_state_t next_state[MAX_NUM_ITEMS];	//next display state, one per item
	void *next_menu[MAX_NUM_ITEMS];			//next menu, one per display state
	disp_state_t prev_state;				//previous display state
	void *prev_menu;						//previous menu
} menu_t;

menu_t main_scr, main_menu, settings_menu, radio_settings, display_settings, keyboard_settings, m17_settings, info_menu, debug_menu;

menu_t main_scr =
{
	"",
	0,

	{},
	{},
	{DISP_MAIN_MENU},
	{&main_menu},

	DISP_NONE,
	NULL
};

menu_t main_menu =
{
	"Main menu",
	4,

	{"Messaging", "Settings", "Info", "Debug"},
	{},
	{DISP_TEXT_MSG_ENTRY, DISP_SETTINGS, DISP_INFO, DISP_DEBUG},
	{&main_scr, &settings_menu, &info_menu, &debug_menu},

	DISP_MAIN_SCR,
	&main_scr
};

menu_t settings_menu =
{
	"Settings",
	4,

	{"Radio", "Display", "Keyboard", "M17"},
	{},
	{DISP_RADIO_SETTINGS, DISP_DISPLAY_SETTINGS, DISP_KEYBOARD_SETTINGS, DISP_M17_SETTINGS},
	{&radio_settings, &display_settings, &keyboard_settings, &m17_settings},

	DISP_MAIN_MENU,
	&main_menu
};

menu_t radio_settings =
{
	"Radio settings",
	1,

	{"Power"},
	{"0.5W"},
	{DISP_NUM_VALUE_ENTRY},
	{NULL},

	DISP_SETTINGS,
	&settings_menu
};

menu_t display_settings =
{
	"Display settings",
	1,

	{"Backlight"},
	{"160"},
	{DISP_NUM_VALUE_ENTRY},
	{NULL},

	DISP_SETTINGS,
	&settings_menu
};

menu_t keyboard_settings =
{
	"Keyboard settings",
	3,

	{"Timeout", "Delay", "Vibration"},
	{"750", "150", "OFF"},
	{DISP_NUM_VALUE_ENTRY, DISP_NUM_VALUE_ENTRY, DISP_ON_OFF_ENTRY},
	{NULL, NULL, NULL},

	DISP_SETTINGS,
	&settings_menu
};

menu_t m17_settings =
{
	"M17 settings",
	3,

	{"Callsign", "Dest.", "CAN"},
	{"N0KIA", "@ALL", "0"},
	{DISP_TEXT_VALUE_ENTRY, DISP_TEXT_VALUE_ENTRY, DISP_NUM_VALUE_ENTRY},
	{NULL, NULL, NULL},

	DISP_SETTINGS,
	&settings_menu
};

menu_t info_menu =
{
	"Info",
	2,

	{"Version", "Author"},
	{FW_VER, "SP5WWP"},
	{DISP_NONE, DISP_NONE},
	{NULL, NULL},

	DISP_MAIN_MENU,
	&main_menu
};

menu_t debug_menu =
{
	"Debug menu",
	1,

	{"Empty"},
	{},
	{DISP_NONE},
	{NULL},

	DISP_MAIN_MENU,
	&main_menu
};
