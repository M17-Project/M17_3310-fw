#ifndef INC_RF_MODULE_H_
#define INC_RF_MODULE_H_

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "typedefs.h"
#include "settings.h"
#include "debug.h"

extern UART_HandleTypeDef huart4; //UART for SA868 control

uint8_t setRegRF(uint8_t reg, uint16_t val);
uint16_t getRegRF(uint8_t reg);
void maskSetRegRF(uint8_t reg, uint16_t mask, uint16_t value);
void reloadRF(void);
void setFreqRF(uint32_t freq, float corr);
void setRF(radio_state_t state);
void chBwRF(ch_bw_t bw);
void setModeRF(rf_mode_t mode);
void initRF(dev_settings_t dev_settings);
void shutdownRF(void);

#endif /* INC_RF_MODULE_H_ */
