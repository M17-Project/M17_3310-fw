#ifndef INC_TYPEDEFS_H_
#define INC_TYPEDEFS_H_

//text entry types
typedef enum text_entry
{
    TEXT_LOWERCASE,
	TEXT_UPPERCASE,
	TEXT_T9
} text_entry_t;

//keys
typedef enum key
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
typedef enum ch_bw
{
	RF_BW_12K5,
	RF_BW_25K
} ch_bw_t;

typedef enum rf_mode
{
	RF_MODE_FM,
	RF_MODE_4FSK
} rf_mode_t;

typedef enum rf_power
{
	RF_PWR_LOW,
	RF_PWR_HIGH
} rf_power_t;

//radio state
typedef enum radio_state
{
	RF_RX,
	RF_TX
} radio_state_t;

//editable settings
typedef enum edit_set
{
	EDIT_NONE,
	EDIT_M17_SRC_CALLSIGN,
	EDIT_M17_DST_CALLSIGN,
	EDIT_M17_CAN,
	EDIT_RF_PPM,
	EDIT_RF_PWR
} edit_set_t;

#endif /* INC_TYPEDEFS_H_ */
