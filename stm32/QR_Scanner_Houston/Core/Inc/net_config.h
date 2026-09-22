/*
 * net_config.h
 *
 * Persists the W5500 network info (MAC/IP/Subnet/Gateway) in the last
 * internal flash sector so it survives power cycles, and can be updated
 * at runtime from the HTTP settings page instead of being hardcoded.
 */
#ifndef NET_CONFIG_H
#define NET_CONFIG_H

#include "wizchip_conf.h"

/* Load the stored network config into *out.
 * Returns 1 if a valid config was found in flash, 0 otherwise
 * (caller should fall back to defaults and call NetConfig_Save). */
uint8_t NetConfig_Load(wiz_NetInfo *out);

/* Erase the config flash sector and store *net there. */
void NetConfig_Save(const wiz_NetInfo *net);

#endif /* NET_CONFIG_H */
