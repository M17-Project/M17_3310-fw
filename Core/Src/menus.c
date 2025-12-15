#include "menus.h"
#include "display.h"
#include "keypad.h"

disp_t displays[13] =
{
	//DISP_NONE - 0
	{
		"",
		0,

		{""},
		{""},
		{DISP_NONE},
		DISP_NONE,

		{EDIT_NONE}
	},

	//DISP_SPLASH - 1
	{
		"",
		0,

		{""},
		{""},
		{DISP_NONE},
		DISP_SPLASH,

		{EDIT_NONE}
	},

	//DISP_MAIN_SCR - 2
	{
		"",
		0,

		{""},
		{""},
		{DISP_MAIN_MENU},
		DISP_MAIN_SCR,

		{EDIT_NONE}
	},

	//DISP_MAIN_MENU - 3
	{
		"Main menu",
		4,

		{"Messaging", "Settings", "Info", "Debug"},
		{"", "", "", ""},
		{DISP_TEXT_MSG_ENTRY, DISP_SETTINGS, DISP_INFO, DISP_DEBUG},
		DISP_MAIN_SCR,

		{EDIT_NONE, EDIT_NONE, EDIT_NONE, EDIT_NONE}
	},

	//4
	{
		"Settings",
		4,

		{"Radio", "Display", "Keyboard", "M17"},
		{"", "", "", ""},
		{DISP_RADIO_SETTINGS, DISP_DISPLAY_SETTINGS, DISP_KEYBOARD_SETTINGS, DISP_M17_SETTINGS},
		DISP_MAIN_MENU,

		{EDIT_NONE, EDIT_NONE, EDIT_NONE, EDIT_NONE}
	},

	//DISP_RADIO_SETTINGS - 5
	{
		"Radio settings",
		2,

		{"Offset", "RF power"},
		{"+0.0ppm", "0.5W"},
		{DISP_TEXT_VALUE_ENTRY, DISP_TEXT_VALUE_ENTRY},
		DISP_SETTINGS,

		{EDIT_RF_PPM, EDIT_RF_PWR}
	},

	//6
	{
		"Display settings",
		3,

		{"Intensity", "Timeout", "Always on"},
		{"160", "5s", "No"},
		{DISP_NONE},
		DISP_SETTINGS,

		{EDIT_NONE}
	},

	//7
	{
		"Keyboard",
		3,

		{"Timeout", "Delay", "Vibration"},
		{"750", "150", "Off"},
		{DISP_NONE, DISP_NONE, DISP_NONE},
		DISP_SETTINGS,

		{EDIT_NONE, EDIT_NONE, EDIT_NONE}
	},

	//DISP_M17_SETTINGS - 8
	{
		"M17 settings",
		3,

		{"Callsign", "Dest.", "CAN"},
		{"N0KIA", "@ALL", "0"},
		{DISP_TEXT_VALUE_ENTRY, DISP_TEXT_VALUE_ENTRY, DISP_TEXT_VALUE_ENTRY},
		DISP_SETTINGS,

		{EDIT_M17_SRC_CALLSIGN, EDIT_M17_DST_CALLSIGN, EDIT_M17_CAN}
	},

	//9
	{
		"Info",
		3,

		{"FW ver.", "Author", "libm17 ver."},
		{FW_VER, "SP5WWP", LIBM17_VERSION},
		{DISP_NONE, DISP_NONE, DISP_NONE},
		DISP_MAIN_MENU,

		{EDIT_NONE}
	},

	//DISP_DEBUG - 10
	{
		"Debug menu",
		1,

		{"Debug test"},
		{""},
		{DISP_NONE},
		DISP_MAIN_MENU,

		{EDIT_NONE}
	},

	//DISP_TEXT_MSG_ENTRY - 11
	{
		"",
		0,

		{""},
		{""},
		{DISP_MAIN_SCR},
		DISP_MAIN_MENU,

		{EDIT_NONE}
	},

	//DISP_TEXT_VALUE_ENTRY - 12
	{
		"",
		0,

		{""},
		{""},
		{DISP_MAIN_SCR},
		DISP_MAIN_SCR,

		{EDIT_NONE}
	},
};

void loadMenuValues(disp_state_t state, dev_settings_t *dev_settings)
{
	switch (state)
	{
		case DISP_SETTINGS:
			;
		break;

		case DISP_RADIO_SETTINGS:
			sprintf(displays[DISP_RADIO_SETTINGS].value[0], "%+d.%dppm",
					(int8_t)(dev_settings->freq_corr),
					(uint8_t)fabsf(dev_settings->freq_corr * 10.0f) % 10);
			snprintf(displays[state].value[1], 24, "%s",
					dev_settings->channel.rf_pwr == RF_PWR_HIGH ? "High" : "Low");
		break;

		case DISP_M17_SETTINGS:
			snprintf(displays[state].value[0], 24, "%s", dev_settings->src_callsign);
			snprintf(displays[state].value[1], 24, "%s", dev_settings->channel.dst);
			snprintf(displays[state].value[2], 24, "%u", dev_settings->channel.can);
		break;

		default:
			break;
	}
}

void enterState(disp_dev_t *disp_dev, disp_state_t state, abc_t *text_entry, dev_settings_t *dev_settings)
{
	switch (state)
	{
		case DISP_MAIN_SCR:
			showMainScreen(disp_dev);
		break;

		case DISP_TEXT_MSG_ENTRY:
			text_entry->buffer[0] = 0;
			text_entry->pos = 0;
			// we keep the last text entry mode intentionally
			showTextMessageEntry(disp_dev, text_entry->mode);
		break;

		case DISP_TEXT_VALUE_ENTRY:
			text_entry->buffer[0] = 0;
			text_entry->pos = 0;
			// we keep the last text entry mode intentionally
			showTextValueEntry(disp_dev, text_entry->mode);
		break;

		default:
			loadMenuValues(state, dev_settings);
			showMenu(disp_dev, &displays[state], 0, 0);
		break;
	}
}

void leaveState(disp_state_t state, char *text_entry, dev_settings_t *dev_settings,
		edit_set_t edit_set, radio_state_t *radio_state)
{
	resetTextEntry();

	if (state != DISP_TEXT_VALUE_ENTRY)
		return;

	// commit edited value
	if (edit_set == EDIT_RF_PPM)
	{
		float val = atof(text_entry);
		if (fabsf(val) <= 50.0f)
		{
			dev_settings->freq_corr = val;
			if (*radio_state == RF_RX)
				setFreqRF(dev_settings->channel.rx_frequency,
						  dev_settings->freq_corr);
		}
	}
	else if (edit_set == EDIT_RF_PWR)
	{
		float val = atof(text_entry);
		if (val > 1.0f)
		{
			dev_settings->channel.rf_pwr = RF_PWR_HIGH;
			HAL_GPIO_WritePin(RF_PWR_GPIO_Port, RF_PWR_Pin, 0);
		}
		else
		{
			dev_settings->channel.rf_pwr = RF_PWR_LOW;
			HAL_GPIO_WritePin(RF_PWR_GPIO_Port, RF_PWR_Pin, 1);
		}
	}
	else if (edit_set == EDIT_M17_SRC_CALLSIGN)
	{
		strcpy(dev_settings->src_callsign, text_entry);
	}
	else if (edit_set == EDIT_M17_DST_CALLSIGN)
	{
		strcpy(dev_settings->channel.dst, text_entry);
	}
	else if (edit_set == EDIT_M17_CAN)
	{
		uint8_t val = atoi(text_entry);
		if (val < 16)
			dev_settings->channel.can = val;
	}

	dispClear(&disp_dev, COL_WHITE);
	setString(&disp_dev, 0, 17, &nokia_big, "Saving...", COL_BLACK, ALIGN_CENTER);

	saveData(dev_settings, sizeof(dev_settings_t));
}
