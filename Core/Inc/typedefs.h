#ifndef INC_TYPEDEFS_H_
#define INC_TYPEDEFS_H_

#include <stdint.h>

//text entry types
typedef enum
{
	TEXT_LOWERCASE,			//# cycles through the allowed modes in this order
	TEXT_UPPERCASE,
	TEXT_UKR_LOWER,
	TEXT_UKR_UPPER,
	TEXT_T9,
	TEXT_NUMERIC,

	TEXT_NUM_MODES
} text_entry_t;

//bitmask of allowed text entry modes
#define TM(x)			(1U<<(x))
#define TM_LATIN		(TM(TEXT_LOWERCASE) | TM(TEXT_UPPERCASE))
#define TM_UKR			(TM(TEXT_UKR_LOWER) | TM(TEXT_UKR_UPPER))
#define TM_ALL			(TM_LATIN | TM(TEXT_T9) | TM(TEXT_NUMERIC) | TM_UKR)

//keys
typedef enum
{
	KEY_NONE,
	KEY_OK,
	KEY_C,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_1,
	KEY_2,
	KEY_3,
	KEY_4,
	KEY_5,
	KEY_6,
	KEY_7,
	KEY_8,
	KEY_9,
	KEY_ASTERISK,
	KEY_0,
	KEY_HASH
} kbd_key_t;

//radio settings
typedef enum
{
	RF_BW_12K5,
	RF_BW_25K
} ch_bw_t;

typedef enum
{
	RF_MODE_ANA,
	RF_MODE_DIG
} rf_mode_t;

typedef enum
{
	RF_PWR_LOW,
	RF_PWR_HIGH
} rf_power_t;

//radio state
typedef enum
{
	RF_RX,
	RF_TX
} radio_state_t;

//menu fields: editable settings, read-only info values and actions
//(see the fields[] table in menus.c)
typedef enum
{
	EDIT_NONE,

	//M17
	EDIT_M17_SRC_CALLSIGN,
	EDIT_M17_DST_CALLSIGN,
	EDIT_M17_CAN,

	//radio
	EDIT_RF_PPM,
	EDIT_RF_PWR,
	EDIT_TUNING_MODE,
	EDIT_MEM_CHANNEL,
	EDIT_VFO_FREQ,
	EDIT_VFO_STEP,
	EDIT_VFO_SHIFT,
	EDIT_MEM_NUMBER,		//main screen: jump to a memory channel by number

	//display
	EDIT_BL_LEVEL,
	EDIT_BL_TIMER,
	EDIT_BL_ALWAYS,
	EDIT_CONTRAST,
	EDIT_SPLASH_TIME,
	EDIT_SPLASH_LINE1,
	EDIT_SPLASH_LINE2,

	//keyboard
	EDIT_KBD_TIMEOUT,
	EDIT_KBD_DELAY,
	EDIT_KBD_UKRAINIAN,

	//alerts
	EDIT_RINGTONE,
	EDIT_VIBRATION,

	//messages
	EDIT_MSG_WRITE,
	EDIT_MSG_REPLY,
	EDIT_MSG_FORWARD,
	EDIT_MSG_DETAILS,
	EDIT_MSG_SET_DST,
	EDIT_MSG_DELETE,
	EDIT_INBOX_CLEAR,
	EDIT_INFO_INBOX,

	//channels (codeplug)
	EDIT_CH_SELECT,
	EDIT_CH_DELETE,
	EDIT_CH_NAME,
	EDIT_CH_RX,
	EDIT_CH_TX,
	EDIT_CH_DST,
	EDIT_CH_CAN,
	EDIT_CH_PWR,

	//info (read only)
	EDIT_INFO_FW,
	EDIT_INFO_LIBM17,
	EDIT_INFO_RF_FW,
	EDIT_INFO_RF_MODEL,
	EDIT_INFO_BATT,
	EDIT_INFO_AUTHOR,

	EDIT_NUM_ITEMS
} edit_set_t;

//text message
typedef struct
{
	char src[10];
	char dst[10];
	uint16_t type;			//LSF TYPE field of the transmission
	uint8_t meta[14];		//LSF META field (GNSS position, text, extended callsign)
	uint16_t len;
	char text[825-4];		//UTF-8, null-terminated
} msg_t;

#endif /* INC_TYPEDEFS_H_ */
