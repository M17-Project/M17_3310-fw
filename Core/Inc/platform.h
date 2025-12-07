#ifndef INC_PLATFORM_H_
#define INC_PLATFORM_H_

#include <stdint.h>
#include "main.h"

//timers used
extern TIM_HandleTypeDef htim1;	//TIM1 - buzzer timer (by default at 1kHz)
extern TIM_HandleTypeDef htim2;	//TIM2 - LED backlight PWM timer

void setBacklight(uint8_t level);
void actVibr(uint8_t period);
void playBeep(uint16_t duration);

#endif /* INC_PLATFORM_H_ */
