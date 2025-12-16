#include "debug.h"

void dbg_print(const char* fmt, ...)
{
    // usb not connected - nothing to do
	if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
    	return;

	char str[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(str, sizeof(str), fmt, ap);
	va_end(ap);

	uint32_t t_end = HAL_GetTick() + 2; //when to give up
	while (CDC_Transmit_FS((uint8_t*)str, strlen(str)) != USBD_OK)
	{
		if (HAL_GetTick() >= t_end)
			return;
	}
}
