/**
  ******************************************************************************
  * @file    spi1.h
  * @brief   Register-level SPI1 master (no HAL): the bus shared by the W5500
  *          and the W25Q32. SCK PB3, MISO PB4, MOSI PB5, mode 0, MSB first,
  *          8-bit, 80 MHz / 8 = 10 MHz. Chip selects are driven by each driver.
  *
  *          Reference: RM0368 (STM32F401 reference manual)
  *            ch. 6  RCC  - clock enable bits
  *            ch. 8  GPIO - MODER / OTYPER / OSPEEDR / PUPDR / AFR
  *            ch. 20 SPI  - CR1 / SR / DR
  ******************************************************************************
  */
#ifndef __SPI1_H
#define __SPI1_H

#include "main.h"

/* Clocks, pins and SPI1 registers. Call once at start-up before any transfer. */
void spi1_init(void);

/* One full-duplex byte: sends tx, returns what came back on MISO. */
uint8_t spi1_xfer(uint8_t tx);

/* Sends len bytes (whatever comes back is thrown away). */
void spi1_write(const uint8_t *buf, uint32_t len);

/* Receives len bytes (sends 0xFF while reading). */
void spi1_read(uint8_t *buf, uint32_t len);

/* Waits until the last bit has left the shift register; call before raising CS. */
void spi1_wait_idle(void);

#endif /* __SPI1_H */
