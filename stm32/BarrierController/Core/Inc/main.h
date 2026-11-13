/**
  ******************************************************************************
  * @file    main.h
  * @brief   Barrier controller - pin map and site config.
  *          Pure CMSIS (no HAL): every peripheral is set up at register level.
  *          Board: STM32F401RET6 (QR_Scanner_Houston / RFID_Offline_Controller PCB).
  ******************************************************************************
  */
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx.h"
#include <stdint.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------------
 * Pin map. Pins are bit masks (1 << n) so they go straight into BSRR / IDR.
 * ------------------------------------------------------------------------- */

/* Relays (board R1 / R2). Both are driven to idle at start-up. */
#define RELAY1_Pin           (1u << 12)     /* PA12 (R1), LQFP64 pin 45 */
#define RELAY1_GPIO_Port     GPIOA
#define RELAY2_Pin           (1u << 11)     /* PA11 (R2), LQFP64 pin 44 */
#define RELAY2_GPIO_Port     GPIOA
#define RELAY_ACTIVE_HIGH    1              /* 0 if the relay board is active-low */

/* Gate type: entry or exit. Chosen on the web page and kept in the EEPROM;
 * EXIT_GATE is only the factory default for a blank EEPROM.
 *   entry: command |OPENEN%, MAC ends in GATE_MAC_ENTRY (01)
 *   exit : command |OPENEX%, MAC ends in GATE_MAC_EXIT  (02)
 * Different MACs let an entry and an exit controller share one network. */
#ifndef EXIT_GATE
#define EXIT_GATE            0
#endif
#define GATE_ENTRY           0u
#define GATE_EXIT            1u
#define GATE_MAC_ENTRY       0x01
#define GATE_MAC_EXIT        0x02

/* The one relay pulsed by the open command. For R2 use RELAY2_Pin / RELAY2_GPIO_Port. */
#define OPENEN_RELAY_Pin       RELAY1_Pin
#define OPENEN_RELAY_GPIO_Port RELAY1_GPIO_Port

/* Status LED: 1 Hz = running, 4 Hz = W5500 not answering, steady = link down */
#define LED_Pin              (1u << 2)      /* PD2 */
#define LED_GPIO_Port        GPIOD

/* W5500 Ethernet on SPI1 : SCK PB3, MISO PB4, MOSI PB5 (spi1.c), CS PB6.
 * The board has no W5500 reset line: the driver uses the chip's software reset.
 * If a board does wire RSTn to the STM32, define W5500_RST_Pin / W5500_RST_GPIO_Port. */
#define W5500_CS_Pin         (1u << 6)      /* PB6 */
#define W5500_CS_GPIO_Port   GPIOB

/* I2C3 : SDA PC9 (pin 40), SCL PA8 (pin 41) - see i2c3.c. On this bus:
 *   AT24C128 EEPROM    : 16 KB, holds the network settings (0x50 on this board)
 *   24AA02E48 MAC chip : answers on every address 0x50..0x57; not read (the MAC
 *                        is fixed below), only kept quiet (eeprom.c) */
#define MAC_CHIP_I2C_ADDR    0x50

/* ---------------------------------------------------------------------------
 * Factory defaults. The web page (http://<ip>/) changes IP, subnet, gateway,
 * command port and gate type; they are kept in the AT24C128 and used from then on.
 * ------------------------------------------------------------------------- */
/* MAC: these 5 bytes + last byte from the gate type (GATE_MAC_ENTRY / _EXIT).
 * 00:08:DC = WIZnet OUI. */
#define CFG_MAC_PREFIX       {0x00, 0x08, 0xDC, 0x42, 0x41}
#define CFG_IP_ADDR          {192, 168, 1, 159}
#define CFG_SUBNET_MASK      {255, 255, 255, 0}
#define CFG_GATEWAY          {192, 168, 1, 1}
#define CFG_TCP_PORT         6000           /* command port (Hercules: TCP client) */
#define CFG_HTTP_PORT        80             /* settings web page */
#define CFG_WEB_PASSWORD     "Housys@123"   /* asked before the settings form opens */

#define CFG_RELAY_PULSE_MS   500            /* relay on-time per open command */
#define CFG_HEARTBEAT_MS     3000           /* "|HLT%" to every connected client */

/* milliseconds since start-up (SysTick) */
uint32_t millis(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
