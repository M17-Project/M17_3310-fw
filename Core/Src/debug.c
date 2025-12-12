#include "debug.h"

void dbg_print(const char* fmt, ...)
{
	char str[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(str, sizeof(str), fmt, ap);
	va_end(ap);

	while (CDC_Transmit_FS((uint8_t*)str, strlen(str)) != USBD_OK);
}
