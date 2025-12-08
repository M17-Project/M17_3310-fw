#include "debug.h"

void dbg_print(const char* fmt, ...)
{
	char str[1024];
	va_list ap;

	va_start(ap, fmt);
	vsprintf(str, fmt, ap);
	va_end(ap);

	CDC_Transmit_FS((uint8_t*)str, strlen(str));
}
