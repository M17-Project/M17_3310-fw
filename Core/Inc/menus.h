//--------------------------------------------------------------------
// 3310 menus/displays state machine related stuff - menus.h
//
// Wojciech Kaczmarski, SP5WWP
// M17 Foundation, 16 June 2025
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

typedef struct disp
{
	char title[18];							//title
	uint8_t num_items;						//number of items
	char item[MAX_NUM_ITEMS][24];			//entries
	char value[MAX_NUM_ITEMS][24];			//values
	disp_state_t next_disp[MAX_NUM_ITEMS];	//next display, one per item
	disp_state_t prev_disp;					//previous display
} disp_t;

disp_t displays[13] =
{
	//DISP_NONE - 0
	{
		"",
		0,

		{""},
		{""},
		{DISP_NONE},

		DISP_NONE
	},

	//DISP_SPLASH - 1
	{
		"",
		0,

		{""},
		{""},
		{DISP_NONE},

		DISP_SPLASH
	},

	//DISP_MAIN_SCR - 2
	{
		"",
		0,

		{""},
		{""},
		{DISP_MAIN_MENU},

		DISP_MAIN_SCR
	},

	//DISP_MAIN_MENU - 3
	{
		"Main menu",
		4,

		{"Messaging", "Settings", "Info", "Debug"},
		{"", "", "", ""},
		{DISP_TEXT_MSG_ENTRY, DISP_SETTINGS, DISP_INFO, DISP_DEBUG},

		DISP_MAIN_SCR
	},

	//4
	{
		"Settings",
		4,

		{"Radio", "Display", "Keyboard", "M17"},
		{"", "", "", ""},
		{DISP_RADIO_SETTINGS, DISP_DISPLAY_SETTINGS, DISP_KEYBOARD_SETTINGS, DISP_M17_SETTINGS},

		DISP_MAIN_MENU
	},

	//DISP_RADIO_SETTINGS - 5
	{
		"Radio settings",
		2,

		{"Offset", "RF power"},
		{"+0.0ppm", "0.5W"},
		{DISP_NONE, DISP_NONE},

		DISP_SETTINGS
	},

	//6
	{
		"Display settings",
		3,

		{"Intensity", "Timeout", "Always on"},
		{"160", "5s", "No"},
		{DISP_NONE},

		DISP_SETTINGS
	},

	//7
	{
		"Keyboard",
		3,

		{"Timeout", "Delay", "Vibration"},
		{"750", "150", "Off"},
		{DISP_NONE, DISP_NONE, DISP_NONE},

		DISP_SETTINGS
	},

	//DISP_M17_SETTINGS - 8
	{
		"M17 settings",
		3,

		{"Callsign", "Dest.", "CAN"},
		{"N0KIA", "@ALL", "0"},
		{DISP_TEXT_VALUE_ENTRY, DISP_TEXT_VALUE_ENTRY, DISP_TEXT_VALUE_ENTRY},

		DISP_SETTINGS
	},

	//9
	{
		"Info",
		3,

		{"FW ver.", "Author", "libm17 ver."},
		{FW_VER, "SP5WWP", LIBM17_VERSION},
		{DISP_NONE, DISP_NONE, DISP_NONE},

		DISP_MAIN_MENU
	},

	//10
	{
		"Debug menu",
		1,

		{"Empty"},
		{""},
		{DISP_NONE},

		DISP_MAIN_MENU
	},

	//DISP_TEXT_MSG_ENTRY - 11
	{
		"",
		0,

		{""},
		{""},
		{DISP_MAIN_SCR},

		DISP_MAIN_MENU
	},

	//DISP_TEXT_VALUE_ENTRY - 12
	{
		"",
		0,

		{""},
		{""},
		{DISP_MAIN_SCR},

		DISP_MAIN_SCR
	},
};

