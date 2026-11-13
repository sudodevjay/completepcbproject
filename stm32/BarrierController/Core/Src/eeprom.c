/**
  ******************************************************************************
  * @file    eeprom.c
  * @brief   AT24C128 settings EEPROM on I2C3, next to a 24AA02E48 MAC chip
  *          (the MAC chip is not read: the MAC is fixed in main.h).
  *
  * The problem on this board: the 24AA02E48 does not use its A0..A2 pins, it
  * answers on EVERY address 0x50..0x57, so it also answers on the AT24C128's
  * address. Both then drive SDA together and the data is corrupted
  * (open-drain: a 0 from either chip wins).
  *
  * The fix: an EEPROM in its internal write cycle (up to 5 ms) ignores the
  * bus completely. Before each AT24C128 access the MAC chip is given a
  * one-byte write (0xFF to 0x7F, an unused byte of its free lower half; the
  * upper half with the EUI-48 is write-protected in the chip). While it is
  * busy writing, the AT24C128 is alone on the bus. Transfers are kept short
  * (8 bytes at 400 kHz, well under 1 ms) so they finish inside that window.
  *
  * The dummy write is "[0x7F][0xFF]" on the MAC chip's address. The AT24C128
  * never sees it (different address), and even if it did, two bytes are only a
  * memory address to it: nothing gets written.
  *
  * If there is no AT24C128, the settings go to the MAC chip's free lower
  * half (128 bytes, 1-byte address, 8-byte pages) instead.
  ******************************************************************************
  */
#include "eeprom.h"
#include "i2c3.h"
#include <string.h>

#define DUMMY_MEM        0x7F       /* MAC chip byte sacrificed to the dummy writes */
#define WRITE_CYCLE_MS   10u        /* ack-polling limit, datasheet max 5 ms */
#define AT24_TWR_MS      6u         /* fixed wait after an AT24C128 write */
#define CHUNK            8u         /* bytes per AT24C128 transfer (fits the quiet window) */
#define RETRIES          3u

#define MACCHIP_SIZE     128u
#define MACCHIP_PAGE     8u

static uint8_t  s_addr;             /* settings storage */
static bool     s_wide;             /* true = AT24C128 (2-byte memory address) */
static uint16_t s_size;
static uint16_t s_page;
static bool     s_have_mac;         /* a 24AA02E48 that can be silenced */
static uint8_t  s_mac_addr;         /* an address where only the MAC chip answers */

static void wait_ms(uint32_t ms)
{
    uint32_t start = millis();

    /* +1: millis() may tick right after start was taken */
    while ((millis() - start) < ms + 1u)
    {
    }
}

/* MAC chip busy writing -> it ignores the bus for a few ms. true if it went quiet. */
static bool mac_silence(void)
{
    static const uint8_t dummy[2] = { DUMMY_MEM, 0xFF };

    if (!s_have_mac)
    {
        return true;
    }
    if (!i2c3_write(s_mac_addr, dummy, 2, NULL, 0))
    {
        return false;
    }
    return !i2c3_probe(s_mac_addr);
}

bool eeprom_init(void)
{
    static const uint8_t cand[2] = { 0x57, MAC_CHIP_I2C_ADDR };
    static const uint8_t dummy[2] = { DUMMY_MEM, 0xFF };

    s_addr = 0;
    s_wide = false;
    s_have_mac = false;

    /* Write the dummy byte on a candidate address. If that address then goes
     * silent, it was the MAC chip alone (busy now): whatever still answers on
     * 0x50..0x57 during its write cycle is the AT24C128. */
    for (uint8_t c = 0; c < 2u && !s_have_mac; c++)
    {
        for (uint8_t n = 0; n < RETRIES && !s_have_mac; n++)
        {
            if (!i2c3_probe(cand[c]) || !i2c3_write(cand[c], dummy, 2, NULL, 0) || i2c3_probe(cand[c]))
            {
                continue;
            }
            s_have_mac = true;
            s_mac_addr = cand[c];
            for (uint8_t a = 0x50; a <= 0x57u; a++)
            {
                if (a != s_mac_addr && i2c3_probe(a))
                {
                    s_addr = a;
                    s_wide = true;
                    break;
                }
            }
            wait_ms(AT24_TWR_MS);
        }
    }

    if (!s_have_mac)
    {
        /* no 24AA02E48: a plain bus, the first chip that answers is the AT24C128 */
        for (uint8_t a = 0x50; a <= 0x57u; a++)
        {
            if (i2c3_probe(a))
            {
                s_addr = a;
                s_wide = true;
                break;
            }
        }
    }

    if (s_wide)
    {
        s_size = EEPROM_SIZE;
        s_page = EEPROM_PAGE;
        return true;
    }
    if (s_have_mac)
    {
        /* no AT24C128: the free lower half of the MAC chip, nothing collides */
        s_addr = s_mac_addr;
        s_size = MACCHIP_SIZE;
        s_page = MACCHIP_PAGE;
        return true;
    }
    return false;
}

uint8_t eeprom_addr(void)
{
    return s_addr;
}

const char *eeprom_name(void)
{
    if (s_addr == 0)
    {
        return "none";
    }
    return s_wide ? "AT24C128" : "24AA02E48 (128 B)";
}

/* the chip ignores its address (NACK) until the internal write is done */
static bool wait_write_done(void)
{
    uint32_t start = millis();

    while (!i2c3_probe(s_addr))
    {
        if ((millis() - start) > WRITE_CYCLE_MS)
        {
            return false;
        }
    }
    return true;
}

bool eeprom_read(uint16_t mem, void *buf, uint16_t len)
{
    uint8_t *p = (uint8_t *)buf;

    if (s_addr == 0 || (uint32_t)mem + len > s_size)
    {
        return false;
    }

    if (!s_wide)
    {
        uint8_t hdr = (uint8_t)mem;
        return i2c3_read(s_addr, &hdr, 1, p, len);
    }

    while (len > 0)
    {
        uint16_t chunk = (len < CHUNK) ? len : CHUNK;
        uint8_t hdr[2] = { (uint8_t)(mem >> 8), (uint8_t)mem };
        bool ok = false;

        for (uint8_t n = 0; n < RETRIES && !ok; n++)
        {
            ok = mac_silence() && i2c3_read(s_addr, hdr, 2, p, chunk);
        }
        if (!ok)
        {
            return false;
        }
        wait_ms(AT24_TWR_MS);   /* let the MAC chip finish before the next dummy */
        mem = (uint16_t)(mem + chunk);
        p += chunk;
        len = (uint16_t)(len - chunk);
    }
    return true;
}

bool eeprom_write(uint16_t mem, const void *buf, uint16_t len)
{
    const uint8_t *p = (const uint8_t *)buf;

    if (s_addr == 0 || (uint32_t)mem + len > s_size)
    {
        return false;
    }

    while (len > 0)
    {
        /* a page write wraps inside its page: never cross one */
        uint16_t room  = (uint16_t)(s_page - (mem % s_page));
        uint16_t chunk = (len < room) ? len : room;
        bool ok = false;

        if (chunk > CHUNK)
        {
            chunk = CHUNK;
        }

        if (s_wide)
        {
            uint8_t hdr[2] = { (uint8_t)(mem >> 8), (uint8_t)mem };

            for (uint8_t n = 0; n < RETRIES && !ok; n++)
            {
                ok = mac_silence() && i2c3_write(s_addr, hdr, 2, p, chunk);
            }
            /* no ack polling: once the MAC chip is done it would ACK this
             * address too. Both chips finish within the fixed wait. */
            wait_ms(AT24_TWR_MS);
        }
        else
        {
            uint8_t hdr = (uint8_t)mem;
            ok = i2c3_write(s_addr, &hdr, 1, p, chunk) && wait_write_done();
        }
        if (!ok)
        {
            return false;
        }
        mem = (uint16_t)(mem + chunk);
        p += chunk;
        len = (uint16_t)(len - chunk);
    }
    return true;
}
