/**
  ******************************************************************************
  * @file    i2c3.h
  * @brief   Register-level I2C3 master (no HAL), polled, 400 kHz.
  *          SDA PC9, SCL PA8 (AF4), 7-bit addresses (not shifted).
  *          Every call is bounded by a timeout; a stuck bus is recovered
  *          (9 SCL clocks + STOP) and the peripheral is re-initialised.
  *
  *          Reference: RM0368 (STM32F401 reference manual) ch. 18 I2C
  ******************************************************************************
  */
#ifndef __I2C3_H
#define __I2C3_H

#include "main.h"

void i2c3_init(void);

/* true if a device ACKs its address */
bool i2c3_probe(uint8_t addr);

/* START, addr+W, hdr[hlen], data[len], STOP. hdr is the memory address. */
bool i2c3_write(uint8_t addr, const uint8_t *hdr, uint8_t hlen,
                const uint8_t *data, uint16_t len);

/* START, addr+W, hdr[hlen], repeated START, addr+R, data[len], STOP */
bool i2c3_read(uint8_t addr, const uint8_t *hdr, uint8_t hlen,
               uint8_t *data, uint16_t len);

#endif /* __I2C3_H */
