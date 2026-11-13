/**
  ******************************************************************************
  * @file    w5500.c
  * @brief   Minimal W5500 driver: one TCP server socket, polled (no INT pin).
  *
  * SPI frame (variable data length mode):
  *   [addr hi][addr lo][control = BSB<<3 | RW<<2 | 00][data ...]
  ******************************************************************************
  */
#include "w5500.h"
#include "spi1.h"

/* common registers (BSB 0) */
#define REG_MR          0x0000
#define REG_GAR         0x0001
#define REG_SUBR        0x0005
#define REG_SHAR        0x0009
#define REG_SIPR        0x000F
#define REG_RTR         0x0019
#define REG_RCR         0x001B
#define REG_PHYCFGR     0x002E
#define REG_VERSIONR    0x0039

/* socket registers */
#define Sn_MR           0x0000
#define Sn_CR           0x0001
#define Sn_IR           0x0002
#define Sn_SR           0x0003
#define Sn_PORT         0x0004
#define Sn_TX_FSR       0x0020
#define Sn_TX_WR        0x0024
#define Sn_RX_RSR       0x0026
#define Sn_RX_RD        0x0028
#define Sn_KPALVTR      0x002F

#define SOCK_CMD_OPEN     0x01
#define SOCK_CMD_LISTEN   0x02
#define SOCK_CMD_DISCON   0x08
#define SOCK_CMD_CLOSE    0x10
#define SOCK_CMD_SEND     0x20
#define SOCK_CMD_RECV     0x40

#define SOCK_CLOSED       0x00
#define SOCK_INIT         0x13
#define SOCK_LISTEN       0x14
#define SOCK_ESTABLISHED  0x17
#define SOCK_CLOSE_WAIT   0x1C

#define Sn_IR_TIMEOUT     0x08
#define Sn_IR_SEND_OK     0x10

#define Sn_RXBUF_SIZE   0x001E
#define Sn_TXBUF_SIZE   0x001F

#define BSB_COMMON        0x00
#define BSB_REG(s)        ((uint8_t)(((s) << 2) + 1))
#define BSB_TX(s)         ((uint8_t)(((s) << 2) + 2))
#define BSB_RX(s)         ((uint8_t)(((s) << 2) + 3))

#define SEND_TIMEOUT_MS   2000
/* 16 KB each way in total, 2 KB per socket (the chip default) */
static const uint8_t s_buf_kb[W5500_SOCKETS] = { 2, 2, 2, 2, 2, 2, 2, 2 };

static w5500_config_t s_cfg;

/* last VERSIONR read at init, for diagnosis over SWD / debugger:
 * 0x04 = W5500 ok, 0xFF = nothing answers (MISO open, CS or 3.3 V missing),
 * 0x00 = MISO held low or chip in reset */
volatile uint8_t w5500_version_seen;

/* ---- chip select ----------------------------------------------------------
 * BSRR (bit set/reset register) changes pins in one write, with no
 * read-modify-write: bits 0..15 set the pin HIGH, bits 16..31 set it LOW.
 * W5500_CS_Pin is a bit mask (GPIO_PIN_6 = 1 << 6), so it fits BSRR directly. */
static inline void cs_low(void)  { W5500_CS_GPIO_Port->BSRR = (uint32_t)W5500_CS_Pin << 16; }
static inline void cs_high(void) { W5500_CS_GPIO_Port->BSRR = W5500_CS_Pin; }

/* ---- time -------------------------------------------------------------------
 * DWT->CYCCNT is a 32-bit counter in the Cortex-M4 core that counts every CPU
 * clock (80 MHz -> 12.5 ns per count, wraps after ~53 s). Unsigned subtraction
 * (now - start) stays correct across a wrap, so timeouts up to ~50 s are safe. */
static void time_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;     /* power the trace/DWT unit */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;                /* start the cycle counter */
}

static inline uint32_t time_now(void)
{
    return DWT->CYCCNT;
}

static inline uint32_t ms_since(uint32_t start)
{
    return (DWT->CYCCNT - start) / (SystemCoreClock / 1000u);
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = time_now();
    while (ms_since(start) < ms)
    {
    }
}

/* ---- SPI frames ---------------------------------------------------------------
 * Every W5500 access is: CS low, 16-bit address, control byte, data, CS high.
 * Control byte = BSB[4:0] (which block: common / socket n regs / TX / RX buffer)
 *                << 3 | RWB (1 = write) << 2 | OM[1:0] (00 = variable length). */
static void spi_write(uint16_t addr, uint8_t bsb, const uint8_t *data, uint16_t len)
{
    cs_low();
    spi1_xfer((uint8_t)(addr >> 8));
    spi1_xfer((uint8_t)addr);
    spi1_xfer((uint8_t)((bsb << 3) | 0x04));
    spi1_write(data, len);
    spi1_wait_idle();
    cs_high();
}

static void spi_read(uint16_t addr, uint8_t bsb, uint8_t *data, uint16_t len)
{
    cs_low();
    spi1_xfer((uint8_t)(addr >> 8));
    spi1_xfer((uint8_t)addr);
    spi1_xfer((uint8_t)(bsb << 3));
    spi1_read(data, len);
    spi1_wait_idle();
    cs_high();
}

static void write8(uint16_t addr, uint8_t bsb, uint8_t v)
{
    spi_write(addr, bsb, &v, 1);
}

static uint8_t read8(uint16_t addr, uint8_t bsb)
{
    uint8_t v = 0;
    spi_read(addr, bsb, &v, 1);
    return v;
}

static void write16(uint16_t addr, uint8_t bsb, uint16_t v)
{
    uint8_t b[2] = { (uint8_t)(v >> 8), (uint8_t)v };
    spi_write(addr, bsb, b, 2);
}

static uint16_t read16(uint16_t addr, uint8_t bsb)
{
    uint8_t b[2];
    spi_read(addr, bsb, b, 2);
    return (uint16_t)((b[0] << 8) | b[1]);
}

/* FSR / RSR change while being read: read until two reads agree */
static uint16_t read16_stable(uint16_t addr, uint8_t bsb)
{
    uint16_t a, b;
    do
    {
        a = read16(addr, bsb);
        b = read16(addr, bsb);
    } while (a != b);
    return a;
}

static void sock_cmd(uint8_t sock, uint8_t cmd)
{
    uint32_t start = time_now();

    write8(Sn_CR, BSB_REG(sock), cmd);
    while (read8(Sn_CR, BSB_REG(sock)) != 0 && ms_since(start) < 100)
    {
    }
}

static uint8_t sock_status(uint8_t sock)
{
    return read8(Sn_SR, BSB_REG(sock));
}

static void sock_listen(uint8_t sock, uint16_t port)
{
    sock_cmd(sock, SOCK_CMD_CLOSE);
    write8(Sn_IR, BSB_REG(sock), 0xFF);
    write8(Sn_MR, BSB_REG(sock), 0x01);                 /* TCP */
    write16(Sn_PORT, BSB_REG(sock), port);
    sock_cmd(sock, SOCK_CMD_OPEN);
    if (sock_status(sock) != SOCK_INIT)
    {
        return;   /* retried on the next poll */
    }
    write8(Sn_KPALVTR, BSB_REG(sock), 2);               /* keep-alive every 10 s */
    sock_cmd(sock, SOCK_CMD_LISTEN);
}

bool w5500_init(const w5500_config_t *cfg)
{
    s_cfg = *cfg;
    time_init();

    cs_high();
#ifdef W5500_RST_Pin
    W5500_RST_GPIO_Port->BSRR = (uint32_t)W5500_RST_Pin << 16;   /* RSTn low >= 500 us */
    delay_ms(2);
    W5500_RST_GPIO_Port->BSRR = W5500_RST_Pin;
#endif
    delay_ms(60);   /* PLL lock after (power-on) reset */

    w5500_version_seen = read8(REG_VERSIONR, BSB_COMMON);
    if (w5500_version_seen != 0x04)
    {
        return false;
    }

    write8(REG_MR, BSB_COMMON, 0x80);                   /* software reset */
    delay_ms(2);

    spi_write(REG_SHAR, BSB_COMMON, s_cfg.mac, 6);
    spi_write(REG_GAR, BSB_COMMON, s_cfg.gateway, 4);
    spi_write(REG_SUBR, BSB_COMMON, s_cfg.subnet, 4);
    spi_write(REG_SIPR, BSB_COMMON, s_cfg.ip, 4);
    write16(REG_RTR, BSB_COMMON, 2000);                 /* 200 ms retry time */
    write8(REG_RCR, BSB_COMMON, 8);

    for (uint8_t sock = 0; sock < W5500_SOCKETS; sock++)
    {
        write8(Sn_RXBUF_SIZE, BSB_REG(sock), s_buf_kb[sock]);
        write8(Sn_TXBUF_SIZE, BSB_REG(sock), s_buf_kb[sock]);
    }
    return true;
}

bool w5500_link_up(void)
{
    return (read8(REG_PHYCFGR, BSB_COMMON) & 0x01) != 0;
}

void w5500_serve(uint8_t sock, uint16_t port)
{
    switch (sock_status(sock))
    {
        case SOCK_LISTEN:
        case SOCK_ESTABLISHED:
            break;

        case SOCK_CLOSE_WAIT:
            sock_cmd(sock, SOCK_CMD_DISCON);
            break;

        case SOCK_CLOSED:
            sock_listen(sock, port);
            break;

        default:
            /* SYNRECV / FIN_WAIT / TIME_WAIT etc. are transient; anything
             * that sticks (e.g. INIT after a failed listen) is reopened */
            if (sock_status(sock) == SOCK_INIT)
            {
                sock_listen(sock, port);
            }
            break;
    }
}

bool w5500_connected(uint8_t sock)
{
    return sock_status(sock) == SOCK_ESTABLISHED;
}

uint16_t w5500_recv(uint8_t sock, uint8_t *buf, uint16_t max)
{
    uint16_t len, ptr;
    uint8_t st = sock_status(sock);

    /* CLOSE_WAIT can still hold the tail of a request */
    if (st != SOCK_ESTABLISHED && st != SOCK_CLOSE_WAIT)
    {
        return 0;
    }

    len = read16_stable(Sn_RX_RSR, BSB_REG(sock));
    if (len == 0)
    {
        return 0;
    }
    if (len > max)
    {
        len = max;
    }

    ptr = read16(Sn_RX_RD, BSB_REG(sock));
    spi_read(ptr, BSB_RX(sock), buf, len);   /* chip wraps the buffer address itself */
    write16(Sn_RX_RD, BSB_REG(sock), (uint16_t)(ptr + len));
    sock_cmd(sock, SOCK_CMD_RECV);
    return len;
}

static bool send_chunk(uint8_t sock, const uint8_t *buf, uint16_t len)
{
    uint32_t start = time_now();
    uint16_t ptr;
    uint8_t ir;

    while (read16_stable(Sn_TX_FSR, BSB_REG(sock)) < len)
    {
        if (sock_status(sock) != SOCK_ESTABLISHED || ms_since(start) > SEND_TIMEOUT_MS)
        {
            return false;
        }
    }

    ptr = read16(Sn_TX_WR, BSB_REG(sock));
    spi_write(ptr, BSB_TX(sock), buf, len);
    write16(Sn_TX_WR, BSB_REG(sock), (uint16_t)(ptr + len));
    sock_cmd(sock, SOCK_CMD_SEND);

    for (;;)
    {
        ir = read8(Sn_IR, BSB_REG(sock));
        if (ir & Sn_IR_SEND_OK)
        {
            write8(Sn_IR, BSB_REG(sock), Sn_IR_SEND_OK);
            return true;
        }
        if ((ir & Sn_IR_TIMEOUT) || sock_status(sock) == SOCK_CLOSED ||
            ms_since(start) > SEND_TIMEOUT_MS)
        {
            write8(Sn_IR, BSB_REG(sock), Sn_IR_TIMEOUT);
            return false;
        }
    }
}

bool w5500_send(uint8_t sock, const uint8_t *buf, uint16_t len)
{
    /* the socket buffer takes at most its own size per SEND */
    uint16_t max = (uint16_t)(s_buf_kb[sock] * 1024u);

    while (len > 0)
    {
        uint16_t chunk = (len > max) ? max : len;
        if (!send_chunk(sock, buf, chunk))
        {
            return false;
        }
        buf += chunk;
        len = (uint16_t)(len - chunk);
    }
    return true;
}

void w5500_disconnect(uint8_t sock)
{
    sock_cmd(sock, SOCK_CMD_DISCON);
}

void w5500_close(uint8_t sock)
{
    sock_cmd(sock, SOCK_CMD_CLOSE);
    write8(Sn_IR, BSB_REG(sock), 0xFF);
}
