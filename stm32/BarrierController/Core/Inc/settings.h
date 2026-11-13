/**
  ******************************************************************************
  * @file    settings.h
  * @brief   Network settings kept in the AT24C128 (address 0x0000), with a
  *          magic word and CRC so a blank or corrupt chip falls back to the
  *          factory defaults from main.h.
  ******************************************************************************
  */
#ifndef __SETTINGS_H
#define __SETTINGS_H

#include "main.h"

typedef struct
{
    uint32_t magic;
    uint8_t  ip[4];
    uint8_t  subnet[4];
    uint8_t  gateway[4];
    uint16_t port;          /* TCP command port */
    uint8_t  gate;          /* GATE_ENTRY / GATE_EXIT */
    uint8_t  reserved;
    uint16_t crc;           /* CRC-16/CCITT over everything above */
} settings_t;

/* The settings in use (loaded at start-up). */
extern settings_t g_settings;

/* MAC in use: CFG_MAC_PREFIX + gate byte, set by settings_load() */
extern uint8_t g_mac[6];

/* Fills g_settings from the EEPROM, or with the defaults.
 * Returns true if the EEPROM held valid settings. */
bool settings_load(void);

/* Writes s to the EEPROM and reads it back. Does not touch g_settings. */
bool settings_save(const settings_t *s);

void settings_defaults(settings_t *s);

/* What a gate type means: open command ("OPENEN"/"OPENEX"), name, MAC. */
const char *settings_open_cmd(uint8_t gate);
const char *settings_gate_name(uint8_t gate);
void        settings_mac(uint8_t gate, uint8_t mac[6]);

#endif /* __SETTINGS_H */
