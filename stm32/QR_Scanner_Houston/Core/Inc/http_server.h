/*
 * http_server.h
 *
 * Minimal HTTP/1.0 server running on its own W5500 socket, used to serve
 * a password-protected web page for viewing/editing the device's
 * IP address, subnet mask and gateway (previously hardcoded in main.c).
 */
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stdint.h>

#define HTTP_SOCK   1
#define HTTP_PORT   80

/* Opens the listening socket. Call once after wizchip_setnetinfo(). */
void HTTPServer_Init(void);

/* Non-blocking; call every iteration of the main loop. */
void HTTPServer_Poll(void);

#endif /* HTTP_SERVER_H */
