/**
  ******************************************************************************
  * @file    http.h
  * @brief   Tiny HTTP server for the settings page (W5500 sockets 2 and 3).
  *
  *   GET  /         password page
  *   POST /login    pw=..   right password -> settings form (with a session
  *                          token, valid 10 min), wrong -> password page again
  *   POST /save     token=..&ip=..&sn=..&gw=..&port=..&gate=en|ex
  *                          checks, stores in the AT24C128, reboots
  *
  * Forms are POSTed so the password never shows up in the URL / history.
  * After 5 wrong passwords, logins are refused for 60 s.
  ******************************************************************************
  */
#ifndef __HTTP_H
#define __HTTP_H

#include "main.h"

#define HTTP_FIRST_SOCKET   2u
#define HTTP_SOCKET_COUNT   2u

/* Call every main loop pass while the link is up. */
void http_poll(void);

/* Forget half-received requests (after the link went down). */
void http_reset(void);

#endif /* __HTTP_H */
