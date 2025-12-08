#ifndef INC_DEBUG_H_
#define INC_DEBUG_H_

#include <stdarg.h>
#include <stdio.h>
#include "usbd_cdc_if.h"

void dbg_print(const char* fmt, ...);

#endif /* INC_DEBUG_H_ */
