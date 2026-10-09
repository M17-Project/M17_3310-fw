#ifndef INC_PLATFORM_H_
#define INC_PLATFORM_H_

#include <stdint.h>
#include "macros.h"
#include "main.h"

//automatic power-off when the battery is empty (0 disables it)
#ifndef BATT_CUTOFF_MV
#define BATT_CUTOFF_MV		3300U
#endif
#define BATT_CUTOFF_SECS	10U

extern volatile uint16_t batt_adc;

//timers used
extern TIM_HandleTypeDef htim1;		//TIM1 - buzzer timer
extern TIM_HandleTypeDef htim2;		//TIM2 - LED backlight PWM timer
extern TIM_HandleTypeDef htim14;	//TIM14 - display backlight timeout timer

uint16_t getBattVoltage(void);
uint8_t isCharging(void);
void setBacklight(uint8_t level);
void setBacklightTimer(uint8_t t);
void startBacklightTimer(void);
void actVibr(uint8_t period);
void platformTick(void);
void playBeep(float freq, uint16_t duration);

void wdgInit(void);
void wdgKick(void);
uint8_t battCheckEmpty(void);
void powerOff(void);

#endif /* INC_PLATFORM_H_ */
