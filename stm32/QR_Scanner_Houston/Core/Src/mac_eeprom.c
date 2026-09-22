#include "mac_eeprom.h"

uint8_t MAC_EEPROM_Read(I2C_HandleTypeDef *hi2c, uint8_t mac[6])
{
	uint8_t buf[6];

	if (HAL_I2C_Mem_Read(hi2c, MAC_EEPROM_I2C_ADDR, MAC_EEPROM_EUI_OFFSET,
	                      I2C_MEMADD_SIZE_8BIT, buf, 6, 100) != HAL_OK)
	{
		return 0;
	}

	for (uint8_t i = 0; i < 6; i++)
	{
		mac[i] = buf[i];
	}
	return 1;
}
