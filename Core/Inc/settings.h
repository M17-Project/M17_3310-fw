#ifndef INC_SETTINGS_H_
#define INC_SETTINGS_H_

#include "typedefs.h"
#include <stdint.h>

#define CH_MAX_NUM 128

//settings
typedef struct ch_settings
{
	rf_mode_t mode;
	ch_bw_t ch_bw;
	rf_power_t rf_pwr;
	char ch_name[24];
	uint32_t rx_frequency;
	uint32_t tx_frequency;
	char dst[12];
	uint8_t can;
} ch_settings_t;

typedef struct codeplug
{
	char name[24];
	uint8_t num_items;
	ch_settings_t channel[CH_MAX_NUM];
} codeplug_t;

typedef enum tuning_mode
{
	TUNING_VFO,
	TUNING_MEM
} tuning_mode_t;

//settings added after layout 1 - see nvmem.c
//RULE: only ever append fields here, never reorder or remove them
typedef struct __attribute__((aligned(4)))
{
	ch_settings_t vfo;			//VFO state (dev_settings.channel is the active channel)
	uint32_t vfo_step;			//Hz
	uint8_t splash_time;		//seconds, 0 = no splash screen
	uint8_t contrast;			//LCD contrast, 0..127
	uint8_t ringtone;			//index into ringtones[], 0 = off
	uint8_t vibration;			//vibrate on a new message
	uint8_t ukrainian;			//Ukrainian text entry modes enabled
	uint8_t reserved[3];
} dev_settings_ext_t;

typedef struct dev_settings
{
	//--- layout 1 (firmware <= 1.4.x), do not change ---
	char src_callsign[12];
	char welcome_msg[2][24];

	uint8_t backlight_level;
	uint8_t backlight_timer;
	uint8_t backlight_always;
	uint16_t kbd_timeout; //keyboard keypress timeout (for text entry)
	uint16_t kbd_delay; //insensitivity delay after keypress detection
	float freq_corr;

	tuning_mode_t tuning_mode;
	ch_settings_t channel;		//active channel (VFO copy or memory channel)
	uint16_t ch_num;

	char refl_name[12];

	//--- layout 2 ---
	dev_settings_ext_t ext;
} dev_settings_t;

#endif /* INC_SETTINGS_H_ */
