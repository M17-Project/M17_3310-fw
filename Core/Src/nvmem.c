#include "nvmem.h"

//memory
uint8_t eraseSector(void)
{
	if(HAL_FLASH_Unlock()==HAL_OK)
	{
		FLASH_EraseInitTypeDef erase =
		{
			FLASH_TYPEERASE_SECTORS,	//erase sectors
			FLASH_BANK_1,				//bank 1 (the only available, i guess)
			FLASH_SECTOR_11,			//start at sector 11 (last)
			1,							//1 sector to erase
			FLASH_VOLTAGE_RANGE_3,		//2.7 to 3.6V
		};
		uint32_t sector_error=0;

		HAL_StatusTypeDef ret = HAL_FLASHEx_Erase(&erase, &sector_error);

		if(ret==HAL_OK && sector_error==0xFFFFFFFFU) //successful erase
		{
			dbg_print("[NVMEM] Sector erased.\n");
			HAL_FLASH_Lock();

			return 0;
		}

		dbg_print("[NVMEM] Sector erasure error.\n");

		return 1;
	}

	dbg_print("[NVMEM] Error unlocking Flash memory.\n");

	return 1;
}

uint8_t saveData(const void *data, uint16_t size)
{
	if(eraseSector()==0) //erase ok
	{
		if(HAL_FLASH_Unlock()==HAL_OK) //unlock ok
		{
			for(uint16_t i=0; i<size; i++)
				HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, MEM_START+i, *(((uint8_t*)data)+i));

			HAL_FLASH_Lock();

			dbg_print("[NVMEM] Data saved.\n");

			return 0;
		}

		dbg_print("[NVMEM] Error unlocking Flash memory.\n");

		return 1;
	}

	dbg_print("[NVMEM] Sector erasure error.\n");

	return 1;
}

//device settings
void loadDeviceSettings(dev_settings_t *dev_settings, const dev_settings_t *def_dev_settings)
{
	//if the memory is uninitialized
	if(*((uint32_t*)MEM_START)==0xFFFFFFFFU)
	{
		memcpy((uint8_t*)dev_settings, (uint8_t*)def_dev_settings, sizeof(dev_settings_t));
		uint8_t ret = saveData(def_dev_settings, sizeof(dev_settings_t));

		if(ret==0)
			dbg_print("[NVMEM] Default device settings loaded.\n");
		else
			dbg_print("[NVMEM] Error saving default device settings.\n");
	}

	//if settings are available in the memory
	else
	{
		memcpy((uint8_t*)dev_settings, (uint8_t*)MEM_START, sizeof(dev_settings_t));

		dbg_print("[NVMEM] Device settings loaded.\n");
	}

	//load settings into menus
	//frequency correction
	sprintf(displays[DISP_RADIO_SETTINGS].value[0], "%+d.%dppm",
			(int8_t)(dev_settings->freq_corr), (uint8_t)fabsf(dev_settings->freq_corr * 10.0f) % 10
	);

	//RF power
	if(dev_settings->channel.rf_pwr==RF_PWR_HIGH)
		sprintf(displays[DISP_RADIO_SETTINGS].value[1], "2W");
	else
		sprintf(displays[DISP_RADIO_SETTINGS].value[1], "0.5W");

	//SRC callsign
	strcpy(displays[DISP_M17_SETTINGS].value[0], dev_settings->src_callsign);

	//destination
	strcpy(displays[DISP_M17_SETTINGS].value[1], dev_settings->channel.dst);

	//CAN
	sprintf(displays[DISP_M17_SETTINGS].value[2], "%d", dev_settings->channel.can);

	//backlight timeout
	sprintf(displays[DISP_DISPLAY_SETTINGS].value[1], "%ds", dev_settings->backlight_timer);
}
