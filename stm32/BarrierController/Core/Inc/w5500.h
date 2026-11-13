/**
  ******************************************************************************
  * @file    w5500.h
  * @brief   Minimal W5500 driver: TCP server sockets, polled (no INT pin).
  *          Each of the 8 sockets has 2 KB TX + 2 KB RX (chip default split).
  *          Several TCP sockets may serve the same port (each takes one client).
  *          Register level, no HAL: SPI through spi1.c (call spi1_init() first),
  *          CS through GPIO BSRR, delays and timeouts from the DWT cycle counter.
  ******************************************************************************
  */
#ifndef __W5500_H
#define __W5500_H

#include "main.h"

#define W5500_SOCKETS   8u

typedef struct
{
    uint8_t  mac[6];
    uint8_t  ip[4];
    uint8_t  subnet[4];
    uint8_t  gateway[4];
} w5500_config_t;

/* Reset + network setup. Returns false if the chip does not answer. */
bool w5500_init(const w5500_config_t *cfg);

bool w5500_link_up(void);

/* Keeps a server socket listening on port; call every main loop pass. */
void w5500_serve(uint8_t sock, uint16_t port);

bool w5500_connected(uint8_t sock);

/* Non-blocking read of whatever arrived, returns bytes copied (0 = nothing). */
uint16_t w5500_recv(uint8_t sock, uint8_t *buf, uint16_t max);

/* Blocking send (bounded by a timeout), any length. */
bool w5500_send(uint8_t sock, const uint8_t *buf, uint16_t len);

/* Drops the current client; the socket goes back to listening. */
void w5500_disconnect(uint8_t sock);

/* Drops the client at once, without the FIN handshake (DISCON waits for the
 * peer's answer, up to the W5500 retry timeout when it is gone).
 * For a peer that is known to be unreachable. Listening again on the next serve. */
void w5500_close(uint8_t sock);

#endif /* __W5500_H */
