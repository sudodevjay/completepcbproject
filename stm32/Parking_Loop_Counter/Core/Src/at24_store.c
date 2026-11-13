/**
  ******************************************************************************
  * @file    at24_store.c
  * @brief   Power-loss safe storage of parking counters in AT24 EEPROM.
  *
  * Slot layout (8 bytes):
  *   [0]    magic 0xA6
  *   [1..2] sequence number (newest record = highest sequence)
  *   [3..4] car count
  *   [5..6] max full
  *   [7]    checksum (~sum of bytes 0..6)
  ******************************************************************************
  */
#include "at24_store.h"

#define AT24_MAGIC          0xA6   /* changed from 0xA5 so old records (wrong max full) are ignored */
#define AT24_WRITE_TIME_MS  20
#define AT24_RETRY          3

static uint8_t  cur_slot = AT24_SLOT_COUNT - 1;  /* last slot written */
static uint16_t cur_seq  = 0;

static uint8_t calc_checksum(const uint8_t *buf)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < AT24_SLOT_SIZE - 1; i++)
    {
        sum += buf[i];
    }
    return (uint8_t)~sum;
}

static uint16_t slot_addr(uint8_t slot)
{
    return AT24_BASE_ADDR + (uint16_t)slot * AT24_SLOT_SIZE;
}

/* Wait until the EEPROM finished its internal write cycle (ACK polling) */
static void wait_ready(I2C_HandleTypeDef *hi2c)
{
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < AT24_WRITE_TIME_MS)
    {
        if (HAL_I2C_IsDeviceReady(hi2c, AT24_I2C_ADDR, 1, 2) == HAL_OK)
        {
            return;
        }
    }
}

/* Returns 1 if a valid record was found and copied into pd */
uint8_t AT24_Load(I2C_HandleTypeDef *hi2c, ParkData *pd)
{
    uint8_t buf[AT24_SLOT_SIZE];
    uint8_t found = 0;

    for (uint8_t slot = 0; slot < AT24_SLOT_COUNT; slot++)
    {
        if (HAL_I2C_Mem_Read(hi2c, AT24_I2C_ADDR, slot_addr(slot), AT24_MEM_ADDR_SIZE,
                             buf, AT24_SLOT_SIZE, 50) != HAL_OK)
        {
            continue;
        }
        if (buf[0] != AT24_MAGIC || buf[7] != calc_checksum(buf))
        {
            continue;
        }

        uint16_t seq = (uint16_t)(buf[1] | (buf[2] << 8));

        /* wrap-around safe "newer than" compare */
        if (!found || (int16_t)(seq - cur_seq) > 0)
        {
            found = 1;
            cur_seq  = seq;
            cur_slot = slot;
            pd->car      = (uint16_t)(buf[3] | (buf[4] << 8));
            pd->max_full = (uint16_t)(buf[5] | (buf[6] << 8));
        }
    }
    return found;
}

/* Returns 1 if the record was written and verified */
uint8_t AT24_Save(I2C_HandleTypeDef *hi2c, const ParkData *pd)
{
    uint8_t buf[AT24_SLOT_SIZE];
    uint8_t chk[AT24_SLOT_SIZE];

    /* always move to the next slot, never overwrite the last good record */
    cur_slot = (uint8_t)((cur_slot + 1) % AT24_SLOT_COUNT);
    cur_seq++;

    buf[0] = AT24_MAGIC;
    buf[1] = (uint8_t)(cur_seq & 0xFF);
    buf[2] = (uint8_t)(cur_seq >> 8);
    buf[3] = (uint8_t)(pd->car & 0xFF);
    buf[4] = (uint8_t)(pd->car >> 8);
    buf[5] = (uint8_t)(pd->max_full & 0xFF);
    buf[6] = (uint8_t)(pd->max_full >> 8);
    buf[7] = calc_checksum(buf);

    for (uint8_t retry = 0; retry < AT24_RETRY; retry++)
    {
        if (HAL_I2C_Mem_Write(hi2c, AT24_I2C_ADDR, slot_addr(cur_slot), AT24_MEM_ADDR_SIZE,
                              buf, AT24_SLOT_SIZE, 50) == HAL_OK)
        {
            wait_ready(hi2c);
            if (HAL_I2C_Mem_Read(hi2c, AT24_I2C_ADDR, slot_addr(cur_slot), AT24_MEM_ADDR_SIZE,
                                 chk, AT24_SLOT_SIZE, 50) == HAL_OK)
            {
                uint8_t same = 1;
                for (uint8_t i = 0; i < AT24_SLOT_SIZE; i++)
                {
                    if (chk[i] != buf[i]) same = 0;
                }
                if (same) return 1;
            }
        }
        else
        {
            wait_ready(hi2c);
        }
    }
    return 0;
}
