#ifndef INC_PLATFORM_H_
#define INC_PLATFORM_H_

#include <stdint.h>
#include "macros.h"
#include "main.h"
#include <math.h>

extern volatile uint16_t batt_adc;

//timers used
extern TIM_HandleTypeDef htim1;		//TIM1 - buzzer timer (by default at 1kHz)
extern TIM_HandleTypeDef htim2;		//TIM2 - LED backlight PWM timer
extern TIM_HandleTypeDef htim14;	//TIM14 - display backlight timeout timer

uint16_t getBattVoltage(void);
void setBacklight(uint8_t level);
void setBacklightTimer(uint8_t t);
void startBacklightTimer(void);
void actVibr(uint8_t period);
void playBeep(float freq, uint16_t duration);

#endif /* INC_PLATFORM_H_ */
