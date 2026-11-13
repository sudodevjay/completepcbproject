/**
  ******************************************************************************
  * @file    main.c
  * @brief   Barrier controller: W5500 TCP server, relay pulses, web settings.
  *          Pure CMSIS, register level (RM0368 = STM32F401 reference manual).
  *
  * TCP protocol (port from settings, default 6000, e.g. Hercules "TCP Client").
  * A command is framed as |NAME% (no line ending needed, case-insensitive,
  * up to 2 clients at once):
  *   |OPENEN% (entry) or |OPENEX% (exit): gate type set on the web page
  *             -> "|OK%"          pulses the open relay (main.h, default R1 PA12)
  *   |STATUS%  -> "|STATUS <IDLE|ACTIVE> LINK <UP|DOWN>%"
  *   |PING%    -> "|PONG%"
  *   other     -> "|ERR%"
  * Unasked, every CFG_HEARTBEAT_MS (3 s) each connected client gets "|HLT%"
  * so the host can tell the controller is online.
  *
  * Web page on port 80 (http://<ip>/) changes IP, subnet, gateway, the TCP
  * port and the gate type (entry/exit, also picks the MAC); they are stored in the AT24C128 (I2C3) and applied after a reboot.
  ******************************************************************************
  */
#include "main.h"
#include "spi1.h"
#include "w5500.h"
#include "i2c3.h"
#include "eeprom.h"
#include "settings.h"
#include "http.h"
#include <string.h>

#define TCP_FIRST_SOCKET   0u
#define TCP_SOCKET_COUNT   2u
#define CMD_MAX            16u

#define FRAME_START        '|'
#define FRAME_END          '%'

typedef struct
{
    char     buf[CMD_MAX];
    uint8_t  len;
    bool     in_frame;
} cmd_t;

static volatile uint32_t s_ms;
static bool              s_relay_on;
static uint32_t          s_pulse_start;
static bool              s_net_ready;
static cmd_t             s_cmd[TCP_SOCKET_COUNT];

/* ======================================================================= clock */

/* HSI 16 MHz -> PLL -> SYSCLK 80 MHz (RM0368 ch. 6)
 *   f_VCO  = 16 MHz / PLLM(16) * PLLN(160) = 160 MHz
 *   SYSCLK = f_VCO / PLLP(2)               =  80 MHz
 *   USB    = f_VCO / PLLQ(4)               =  40 MHz (unused)
 *   AHB /1 = 80 MHz, APB1 /2 = 40 MHz (max 42, I2C3), APB2 /1 = 80 MHz (SPI1) */
static void clock_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR = (PWR->CR & ~PWR_CR_VOS) | PWR_CR_VOS_1;   /* scale 2: up to 84 MHz */

    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY))
    {
    }

    /* PLL must be off while it is configured */
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY)
    {
    }
    RCC->PLLCFGR = (16u  << RCC_PLLCFGR_PLLM_Pos)
                 | (160u << RCC_PLLCFGR_PLLN_Pos)
                 | (0u   << RCC_PLLCFGR_PLLP_Pos)       /* 00 = /2 */
                 | (4u   << RCC_PLLCFGR_PLLQ_Pos)
                 | RCC_PLLCFGR_PLLSRC_HSI;
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY))
    {
    }

    /* 2 wait states for 64..90 MHz at 2.7..3.6 V (RM0368 table 6), plus caches */
    FLASH->ACR = FLASH_ACR_LATENCY_2WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_2WS)
    {
    }

    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2))
              | RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
    {
    }

    SystemCoreClockUpdate();                            /* -> 80000000 */
    SysTick_Config(SystemCoreClock / 1000u);            /* 1 ms tick */
}

void SysTick_Handler(void)
{
    s_ms++;
}

uint32_t millis(void)
{
    return s_ms;
}

/* ======================================================================= GPIO */

/* One pin as push-pull output, low speed, no pull (pin = bit mask) */
static void gpio_output(GPIO_TypeDef *port, uint32_t pin)
{
    uint32_t n = (uint32_t)__builtin_ctz(pin);

    port->MODER   = (port->MODER & ~(3u << (n * 2))) | (1u << (n * 2));   /* 01 = output */
    port->OTYPER &= ~pin;
    port->OSPEEDR = (port->OSPEEDR & ~(3u << (n * 2))) | (2u << (n * 2)); /* 10 = fast */
    port->PUPDR  &= ~(3u << (n * 2));
}

static inline void pin_write(GPIO_TypeDef *port, uint32_t pin, bool high)
{
    port->BSRR = high ? pin : (pin << 16);
}

static void relay_set(GPIO_TypeDef *port, uint32_t pin, bool on)
{
    pin_write(port, pin, on == (RELAY_ACTIVE_HIGH != 0));
}

static void gpio_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIODEN;
    (void)RCC->AHB1ENR;

    /* outputs to their idle level before they are switched to output mode */
    relay_set(RELAY1_GPIO_Port, RELAY1_Pin, false);
    relay_set(RELAY2_GPIO_Port, RELAY2_Pin, false);
    pin_write(W5500_CS_GPIO_Port, W5500_CS_Pin, true);
    pin_write(LED_GPIO_Port, LED_Pin, false);

    gpio_output(RELAY1_GPIO_Port, RELAY1_Pin);
    gpio_output(RELAY2_GPIO_Port, RELAY2_Pin);
    gpio_output(W5500_CS_GPIO_Port, W5500_CS_Pin);
    gpio_output(LED_GPIO_Port, LED_Pin);
}

/* ===================================================================== relays */

/* OPENEN relay on for CFG_RELAY_PULSE_MS; a new command restarts the pulse */
static void relay_pulse(void)
{
    relay_set(OPENEN_RELAY_GPIO_Port, OPENEN_RELAY_Pin, true);
    s_relay_on = true;
    s_pulse_start = millis();
}

static void relay_poll(void)
{
    if (s_relay_on && (millis() - s_pulse_start) >= CFG_RELAY_PULSE_MS)
    {
        relay_set(OPENEN_RELAY_GPIO_Port, OPENEN_RELAY_Pin, false);
        s_relay_on = false;
    }
}

/* ======================================================================== TCP */

static void send_str(uint8_t sock, const char *s)
{
    (void)w5500_send(sock, (const uint8_t *)s, (uint16_t)strlen(s));
}

static bool cmd_is(const char *cmd, const char *name)
{
    while (*name)
    {
        char c = *cmd++;
        if (c >= 'a' && c <= 'z')
        {
            c = (char)(c - 'a' + 'A');
        }
        if (c != *name++)
        {
            return false;
        }
    }
    return *cmd == '\0';
}

static void handle_cmd(uint8_t sock, const char *cmd)
{
    char reply[48];

    if (cmd_is(cmd, settings_open_cmd(g_settings.gate)))
    {
        relay_pulse();
        send_str(sock, "|OK%");
    }
    else if (cmd_is(cmd, "STATUS"))
    {
        strcpy(reply, "|STATUS ");
        strcat(reply, s_relay_on ? "ACTIVE" : "IDLE");
        strcat(reply, w5500_link_up() ? " LINK UP%" : " LINK DOWN%");
        send_str(sock, reply);
    }
    else if (cmd_is(cmd, "PING"))
    {
        send_str(sock, "|PONG%");
    }
    else
    {
        send_str(sock, "|ERR%");
    }
}

/* |NAME% framing: '|' (re)starts a command, '%' ends it, anything outside a
 * frame (CR/LF from the terminal, noise) is ignored */
static void cmd_feed(uint8_t sock, cmd_t *c, char ch)
{
    if (ch == FRAME_START)
    {
        c->len = 0;
        c->in_frame = true;
    }
    else if (!c->in_frame)
    {
        return;
    }
    else if (ch == FRAME_END)
    {
        c->buf[c->len] = '\0';
        c->in_frame = false;
        handle_cmd(sock, c->buf);
    }
    else if (c->len < CMD_MAX - 1u)
    {
        c->buf[c->len++] = ch;
    }
    else
    {
        c->in_frame = false;   /* too long: drop it, wait for the next '|' */
        send_str(sock, "|ERR%");
    }
}

static void net_init(void)
{
    w5500_config_t cfg;

    memcpy(cfg.mac, g_mac, 6);
    memcpy(cfg.ip, g_settings.ip, 4);
    memcpy(cfg.subnet, g_settings.subnet, 4);
    memcpy(cfg.gateway, g_settings.gateway, 4);

    s_net_ready = w5500_init(&cfg);
}

static void net_poll(void)
{
    static uint32_t hb_at;
    uint8_t rx[64];
    uint16_t n;
    bool heartbeat = (millis() - hb_at) >= CFG_HEARTBEAT_MS;

    if (heartbeat)
    {
        hb_at = millis();
    }

    for (uint8_t i = 0; i < TCP_SOCKET_COUNT; i++)
    {
        uint8_t sock = (uint8_t)(TCP_FIRST_SOCKET + i);
        cmd_t *c = &s_cmd[i];

        w5500_serve(sock, g_settings.port);
        if (!w5500_connected(sock))
        {
            c->len = 0;   /* new client starts with an empty frame */
            c->in_frame = false;
            continue;
        }

        while ((n = w5500_recv(sock, rx, sizeof(rx))) > 0)
        {
            for (uint16_t k = 0; k < n; k++)
            {
                cmd_feed(sock, c, (char)rx[k]);
            }
        }

        if (heartbeat)
        {
            send_str(sock, "|HLT%");
        }
    }

    http_poll();
}

/* ======================================================================== LED */

static void led_poll(bool link)
{
    static uint32_t last;
    uint32_t period;

    if (!s_net_ready)
    {
        period = 125;                       /* 4 Hz: W5500 not answering */
    }
    else if (!link)
    {
        pin_write(LED_GPIO_Port, LED_Pin, true);   /* steady: cable out */
        return;
    }
    else
    {
        period = 500;                       /* 1 Hz: running */
    }

    if ((millis() - last) >= period)
    {
        last = millis();
        LED_GPIO_Port->ODR ^= LED_Pin;
    }
}

/* ======================================================================= main */

int main(void)
{
    bool link = false, link_was = false;
    uint32_t retry_at = 0;

    clock_init();
    gpio_init();
    spi1_init();           /* register level, see spi1.c */
    i2c3_init();           /* register level, see i2c3.c */
    (void)eeprom_init();   /* no EEPROM: runs on the defaults, web page says so */
#ifdef FACTORY_RESET
    /* recovery build (-DFACTORY_RESET): writes the main.h defaults to the
     * EEPROM. Flash the normal build again afterwards. */
    settings_defaults(&g_settings);
    (void)settings_save(&g_settings);
#endif
    (void)settings_load(); /* blank / corrupt EEPROM: defaults from main.h */
    net_init();

    for (;;)
    {
        relay_poll();

        if (!s_net_ready)
        {
            /* chip did not answer (power-up, loose cable to the module): retry every 2 s */
            if ((millis() - retry_at) >= 2000u)
            {
                retry_at = millis();
                net_init();
            }
        }
        else
        {
            link = w5500_link_up();
            if (link_was && !link)
            {
                /* cable pulled: drop the clients at once, they are gone */
                for (uint8_t i = 0; i < TCP_SOCKET_COUNT; i++)
                {
                    w5500_close((uint8_t)(TCP_FIRST_SOCKET + i));
                }
                for (uint8_t i = 0; i < HTTP_SOCKET_COUNT; i++)
                {
                    w5500_close((uint8_t)(HTTP_FIRST_SOCKET + i));
                }
                http_reset();
            }
            link_was = link;
            if (link)
            {
                net_poll();
            }
        }

        led_poll(link);
    }
}
