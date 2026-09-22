#include "net_config.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define NETCFG_MAGIC        0xA5C3D9E1U
#define NETCFG_FLASH_ADDR    0x08060000U   /* FLASH_SECTOR_7, last 128KB sector */
#define NETCFG_FLASH_SECTOR  FLASH_SECTOR_7

typedef struct
{
	uint32_t magic;
	wiz_NetInfo net;
	uint32_t checksum;
} NetConfigStore;

static uint32_t NetConfig_Checksum(const wiz_NetInfo *net)
{
	const uint8_t *p = (const uint8_t *)net;
	uint32_t sum = 0;
	for (uint32_t i = 0; i < sizeof(wiz_NetInfo); i++)
	{
		sum += p[i];
	}
	return sum;
}

uint8_t NetConfig_Load(wiz_NetInfo *out)
{
	const NetConfigStore *stored = (const NetConfigStore *)NETCFG_FLASH_ADDR;

	if (stored->magic != NETCFG_MAGIC)
	{
		return 0;
	}
	if (stored->checksum != NetConfig_Checksum(&stored->net))
	{
		return 0;
	}

	memcpy(out, &stored->net, sizeof(wiz_NetInfo));
	return 1;
}

void NetConfig_Save(const wiz_NetInfo *net)
{
	NetConfigStore store;
	store.magic = NETCFG_MAGIC;
	memcpy(&store.net, net, sizeof(wiz_NetInfo));
	store.checksum = NetConfig_Checksum(net);

	HAL_FLASH_Unlock();

	FLASH_EraseInitTypeDef eraseInit;
	uint32_t sectorError = 0;
	eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
	eraseInit.Sector = NETCFG_FLASH_SECTOR;
	eraseInit.NbSectors = 1;
	eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
	HAL_FLASHEx_Erase(&eraseInit, &sectorError);

	const uint8_t *src = (const uint8_t *)&store;
	for (uint32_t i = 0; i < sizeof(NetConfigStore); i++)
	{
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, NETCFG_FLASH_ADDR + i, src[i]);
	}

	HAL_FLASH_Lock();
}
