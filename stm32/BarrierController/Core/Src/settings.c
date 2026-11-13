/**
  ******************************************************************************
  * @file    settings.c
  * @brief   Network settings in the AT24C128. See settings.h.
  ******************************************************************************
  */
#include "settings.h"
#include "eeprom.h"
#include <stddef.h>
#include <string.h>

#define SETTINGS_ADDR    0x0000u
#define SETTINGS_MAGIC   0x42435332u        /* "BCS2"; change when settings_t changes */
#define SETTINGS_MAGIC_1 0x42435331u        /* "BCS1": old layout without the gate */
#define V1_CRC_OFFSET    18u                /* BCS1: crc right after port */

settings_t g_settings;
uint8_t    g_mac[6];

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) */
static uint16_t crc16(const uint8_t *p, uint32_t len)
{
    uint16_t crc = 0xFFFF;

    while (len--)
    {
        crc ^= (uint16_t)(*p++ << 8);
        for (uint8_t b = 0; b < 8; b++)
        {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static uint16_t settings_crc(const settings_t *s)
{
    return crc16((const uint8_t *)s, offsetof(settings_t, crc));
}

const char *settings_open_cmd(uint8_t gate)
{
    return (gate == GATE_EXIT) ? "OPENEX" : "OPENEN";
}

const char *settings_gate_name(uint8_t gate)
{
    return (gate == GATE_EXIT) ? "Exit" : "Entry";
}

void settings_mac(uint8_t gate, uint8_t mac[6])
{
    static const uint8_t prefix[5] = CFG_MAC_PREFIX;

    memcpy(mac, prefix, 5);
    mac[5] = (gate == GATE_EXIT) ? GATE_MAC_EXIT : GATE_MAC_ENTRY;
}

void settings_defaults(settings_t *s)
{
    static const uint8_t ip[4] = CFG_IP_ADDR, sn[4] = CFG_SUBNET_MASK, gw[4] = CFG_GATEWAY;

    memset(s, 0, sizeof(*s));
    s->magic = SETTINGS_MAGIC;
    memcpy(s->ip, ip, 4);
    memcpy(s->subnet, sn, 4);
    memcpy(s->gateway, gw, 4);
    s->port = CFG_TCP_PORT;
    s->gate = EXIT_GATE ? GATE_EXIT : GATE_ENTRY;
    s->crc  = settings_crc(s);
}

/* settings saved by the older firmware (BCS1, no gate): keep IP/port, gate
 * from the build default */
static bool load_v1(const settings_t *raw, settings_t *out)
{
    const uint8_t *b = (const uint8_t *)raw;
    uint16_t crc = (uint16_t)(b[V1_CRC_OFFSET] | (b[V1_CRC_OFFSET + 1u] << 8));

    if (raw->magic != SETTINGS_MAGIC_1 || crc != crc16(b, V1_CRC_OFFSET) || raw->port == 0)
    {
        return false;
    }
    *out = *raw;
    out->magic = SETTINGS_MAGIC;
    out->gate = EXIT_GATE ? GATE_EXIT : GATE_ENTRY;
    out->reserved = 0;
    out->crc = settings_crc(out);
    return true;
}

bool settings_load(void)
{
    settings_t s;
    bool ok = false;

    if (eeprom_read(SETTINGS_ADDR, &s, sizeof(s)))
    {
        if (s.magic == SETTINGS_MAGIC && s.crc == settings_crc(&s) && s.port != 0 &&
            (s.gate == GATE_ENTRY || s.gate == GATE_EXIT))
        {
            g_settings = s;
            ok = true;
        }
        else if (load_v1(&s, &g_settings))
        {
            ok = true;
        }
    }
    if (!ok)
    {
        settings_defaults(&g_settings);
    }
    settings_mac(g_settings.gate, g_mac);
    return ok;
}

bool settings_save(const settings_t *in)
{
    settings_t s = *in, back;

    s.magic = SETTINGS_MAGIC;
    s.reserved = 0;
    s.crc   = settings_crc(&s);

    return eeprom_write(SETTINGS_ADDR, &s, sizeof(s)) &&
           eeprom_read(SETTINGS_ADDR, &back, sizeof(back)) &&
           memcmp(&s, &back, sizeof(s)) == 0;
}
