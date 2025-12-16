#ifndef INC_MACROS_H_
#define INC_MACROS_H_

#define FIX_TIMER_TRIGGER(handle_ptr) (__HAL_TIM_CLEAR_FLAG(handle_ptr, TIM_SR_UIF))
#define arrlen(x) (sizeof(x)/sizeof(*x))
#define DEBUG_HALT while(1);

#endif /* INC_MACROS_H_ */
