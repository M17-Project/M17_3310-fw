#include "settings.h"

codeplug_t codeplug =
{
	"MyCodeplug",
	2,
	{
		{
			RF_MODE_DIG,		//channel mode (digital or analog)
			RF_BW_12K5,			//channel bandwidth
			RF_PWR_LOW,			//RF power
			"MyChannel 1",		//channel name
			433475000,			//RX frequency
			433475000,			//TX frequency
			"@ALL",				//DST
			0					//CAN
		},
		{
			RF_MODE_DIG,		//channel mode (digital or analog)
			RF_BW_12K5,			//channel bandwidth
			RF_PWR_LOW,			//RF power
			"MyChannel 2",		//channel name
			439425000,			//RX frequency
			431812500,			//TX frequency
			"@ALL",				//DST
			0					//CAN
		}
	}
};
