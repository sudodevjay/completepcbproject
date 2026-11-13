/**
  ******************************************************************************
  * @file    eeprom.h
  * @brief   Settings storage on I2C3: the AT24C128 (or, if it is missing,
  *          the free lower 128 bytes of the 24AA02E48 MAC chip). The MAC
  *          chip's own address is not used: the MAC is fixed in main.h.
  ******************************************************************************
  */
#ifndef __EEPROM_H
#define __EEPROM_H

#include "main.h"

#define EEPROM_SIZE      16384u
#define EEPROM_PAGE      64u

/* Picks the settings storage (see eeprom.c). Returns false if there is none.
 * Call after i2c3_init(). */
bool eeprom_init(void);

/* 7-bit address in use, 0 if none was found */
uint8_t eeprom_addr(void);

/* "AT24C128", "24AA02E48 (128 B)" or "none", for the web page */
const char *eeprom_name(void);

bool eeprom_read(uint16_t mem, void *buf, uint16_t len);

/* Splits on page boundaries and waits out each write cycle (<= 5 ms). */
bool eeprom_write(uint16_t mem, const void *buf, uint16_t len);

#endif /* __EEPROM_H */
