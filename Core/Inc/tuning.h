#ifndef INC_TUNING_H_
#define INC_TUNING_H_

#include <stdint.h>
#include "settings.h"

//VFO / memory channel handling
//
//dev_settings.channel is always the ACTIVE channel - the one the radio uses
//(m17_tx.c, the main screen etc. read it). Where it comes from:
//  VFO mode:    a copy of dev_settings.ext.vfo, kept in sync on every change
//  memory mode: a copy of codeplug.channel[dev_settings.ch_num]
//Per-channel edits (destination, CAN, RF power) made in memory mode are
//written back to the codeplug in RAM only - it is not stored in Flash yet.

#define VFO_STEP_DEFAULT	12500U

void tuneInit(dev_settings_t *settings);	//build the active channel after loading the settings
void tuneApply(void);						//push the active channel to the RF module (RX)

void tuneStep(int8_t dir);					//+1: up (frequency / channel number), -1: down
uint8_t tuneSetMode(tuning_mode_t mode);	//0 on success
void tuneToggleMode(void);
uint8_t tuneSelectChannel(uint16_t idx);	//switches to memory mode, 0 on success
uint8_t tuneSetVfoFreq(uint32_t rx_hz);		//switches to VFO mode, keeps the TX shift
uint8_t tuneSetVfoShift(int32_t shift_hz);
void tuneChannelEdited(void);				//the active channel's parameters were changed

uint16_t tuneNumChannels(void);
const char *tuneChannelName(uint16_t idx);
uint8_t tuneFreqValid(uint32_t hz);

//text helpers
uint8_t parseFreq(const char *s, uint32_t *hz);			//"433.475" (MHz), "433475" (kHz)
uint8_t parseSignedMHz(const char *s, int32_t *hz);	//"-7.6" -> -7600000
void fmtFreq(char *out, uint8_t n, uint32_t hz);		//"433.4750"
void fmtSignedMHz(char *out, uint8_t n, int32_t hz);	//"-7.6", "+0"

#endif /* INC_TUNING_H_ */
