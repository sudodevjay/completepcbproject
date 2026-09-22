/*
 * mac_eeprom.h
 *
 * Reads the factory-programmed EUI-48 address out of a Microchip
 * 24AA02E48T I2C EEPROM (on I2C3), so the board's real, globally-unique
 * MAC address can be used instead of a hand-picked one.
 */
#ifndef MAC_EEPROM_H
#define MAC_EEPROM_H

#include "stm32f4xx_hal.h"

/* 7-bit device address is 0b1010<A2><A1><A0>; with the address pins
 * tied low that is 0x50. HAL wants it shifted left one bit (8-bit form).
 * If your board straps A0/A1/A2 differently, update this. */
#define MAC_EEPROM_I2C_ADDR    (0x50 << 1)

/* The EUI-48 lives read-only in the last 6 bytes of the 256-byte array. */
#define MAC_EEPROM_EUI_OFFSET  0xFA

/* Reads the 6-byte EUI-48 into mac[6]. Returns 1 on success, 0 if the
 * device didn't respond / the read failed (caller should fall back to
 * a hardcoded MAC in that case). */
uint8_t MAC_EEPROM_Read(I2C_HandleTypeDef *hi2c, uint8_t mac[6]);

#endif /* MAC_EEPROM_H */
