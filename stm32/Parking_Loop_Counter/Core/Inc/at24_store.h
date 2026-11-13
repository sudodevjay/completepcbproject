/**
  ******************************************************************************
  * @file    at24_store.h
  * @brief   Power-loss safe storage of parking counters in AT24 EEPROM.
  ******************************************************************************
  */
#ifndef __AT24_STORE_H
#define __AT24_STORE_H

#include "main.h"

/* AT24 on I2C3 : SDA = PC9, SCL = PA8, A0/A1/A2 = GND */
#define AT24_I2C_ADDR       0xA0

/* AT24C01 / C02 / C04 / C08 / C16 -> I2C_MEMADD_SIZE_8BIT
 * AT24C32 / C64 / C128 / C256 / C512 -> I2C_MEMADD_SIZE_16BIT */
#define AT24_MEM_ADDR_SIZE  I2C_MEMADD_SIZE_8BIT

/* Records are written round-robin over several slots so that a power cut
 * during a write can only damage the slot being written; the previous
 * slot still holds the last good value. It also spreads EEPROM wear. */
#define AT24_BASE_ADDR      0x0000
#define AT24_SLOT_SIZE      8     /* must divide the EEPROM page size */
#define AT24_SLOT_COUNT     16    /* 16 x 8 = 128 bytes */
/* Fitted chip behaves as 24AA02E48: 256 bytes, upper half 0x80-0xFF is
 * write-protected (factory MAC at 0xFA), so slots must stay below 0x80. */

typedef struct
{
    uint16_t car;       /* cars currently inside */
    uint16_t max_full;  /* parking capacity */
} ParkData;

uint8_t AT24_Load(I2C_HandleTypeDef *hi2c, ParkData *pd);
uint8_t AT24_Save(I2C_HandleTypeDef *hi2c, const ParkData *pd);

#endif /* __AT24_STORE_H */
