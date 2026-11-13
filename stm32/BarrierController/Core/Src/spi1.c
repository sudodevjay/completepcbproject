/**
  ******************************************************************************
  * @file    spi1.c
  * @brief   Register-level SPI1 master (no HAL). See spi1.h.
  *
  * How one SPI byte works on the STM32F4 (RM0368 ch. 20.3.5):
  *   1. wait for SR.TXE = 1  (transmit buffer empty)
  *   2. write the byte to DR -> the hardware shifts it out on MOSI and, at the
  *      same time, shifts 8 bits in from MISO (SPI is always full duplex)
  *   3. wait for SR.RXNE = 1 (receive buffer not empty)
  *   4. read DR -> the received byte; reading also clears RXNE
  * Always reading DR after every write keeps the overrun flag (OVR) from ever
  * being set, so no error handling is needed for it.
  ******************************************************************************
  */
#include "spi1.h"

/* PB3 = SCK, PB4 = MISO, PB5 = MOSI */
#define PIN_SCK     3u
#define PIN_MISO    4u
#define PIN_MOSI    5u
#define AF5_SPI1    5u      /* alternate function number of SPI1 on these pins (datasheet table 9) */

void spi1_init(void)
{
    /* 1. Clocks: GPIOB sits on AHB1, SPI1 on APB2. A peripheral's registers
     *    cannot be written until its clock is on. */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;     /* read back: makes sure the clock is on before the next write */

    /* 2. Pins. Each pin has 2 bits in MODER/OSPEEDR/PUPDR (pin n -> bits 2n+1:2n),
     *    1 bit in OTYPER and 4 bits in AFR (AFR[0] = pins 0..7, pin n -> bits 4n+3:4n).
     *    Note: after reset PB3/PB4 belong to JTAG (and PB4 has a pull-up);
     *    SWD only needs PA13/PA14, so taking them over is fine. */
    const uint32_t pins2 = (3u << (PIN_SCK * 2)) | (3u << (PIN_MISO * 2)) | (3u << (PIN_MOSI * 2));
    const uint32_t af2   = (2u << (PIN_SCK * 2)) | (2u << (PIN_MISO * 2)) | (2u << (PIN_MOSI * 2));

    GPIOB->MODER   = (GPIOB->MODER & ~pins2) | af2;     /* 10 = alternate function */
    GPIOB->OTYPER &= ~((1u << PIN_SCK) | (1u << PIN_MISO) | (1u << PIN_MOSI));  /* push-pull */
    GPIOB->OSPEEDR |= pins2;                            /* 11 = very high speed (sharp 10 MHz edges) */
    GPIOB->PUPDR  &= ~pins2;                            /* 00 = no pull-up / pull-down */
    GPIOB->AFR[0]  = (GPIOB->AFR[0] & ~((0xFu << (PIN_SCK * 4)) | (0xFu << (PIN_MISO * 4)) | (0xFu << (PIN_MOSI * 4))))
                   | (AF5_SPI1 << (PIN_SCK * 4)) | (AF5_SPI1 << (PIN_MISO * 4)) | (AF5_SPI1 << (PIN_MOSI * 4));

    /* 3. SPI1 control register 1. Configure with SPE = 0, then enable.
     *    MSTR    = 1    master
     *    BR[2:0] = 010  f_PCLK2 / 8 = 80 MHz / 8 = 10 MHz
     *                   (000 = /2 = 40 MHz ... 111 = /256; the W5500 manages up to 80 MHz,
     *                   the W25Q32 up to 104 MHz, the PCB traces decide the real limit)
     *    CPOL    = 0    clock idles low      } mode 0, what both chips use:
     *    CPHA    = 0    sample on 1st edge   } data valid on the rising edge
     *    DFF     = 0    8-bit frames
     *    LSBFIRST= 0    MSB first
     *    SSM     = 1    software slave management: the NSS pin is not used,
     *    SSI     = 1    and NSS is held "high" internally so the master never
     *                   drops out of master mode (MODF error). CS is a GPIO. */
    SPI1->CR1 = 0;
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_BR_1 | SPI_CR1_SSM | SPI_CR1_SSI;
    SPI1->CR2 = 0;                  /* no interrupts, no DMA */
    SPI1->CR1 |= SPI_CR1_SPE;       /* SPI enable */
}

uint8_t spi1_xfer(uint8_t tx)
{
    while (!(SPI1->SR & SPI_SR_TXE))
    {
    }
    /* DR is accessed as 8 bits: a 32-bit write would also be fine in 8-bit mode,
     * but the byte access matches the frame size exactly. */
    *(volatile uint8_t *)&SPI1->DR = tx;
    while (!(SPI1->SR & SPI_SR_RXNE))
    {
    }
    return *(volatile uint8_t *)&SPI1->DR;
}

void spi1_write(const uint8_t *buf, uint32_t len)
{
    while (len--)
    {
        (void)spi1_xfer(*buf++);
    }
}

void spi1_read(uint8_t *buf, uint32_t len)
{
    while (len--)
    {
        *buf++ = spi1_xfer(0xFF);   /* the slave only sends while the master clocks */
    }
}

void spi1_wait_idle(void)
{
    /* BSY = 1 while a frame is still being shifted. After spi1_xfer() has seen
     * RXNE the byte is complete, but checking BSY is the documented way to know
     * the bus is quiet before deselecting the slave (RM0368 20.3.8). */
    while (SPI1->SR & SPI_SR_BSY)
    {
    }
}
