/**
  ******************************************************************************
  * @file    i2c3.c
  * @brief   Register-level I2C3 master (no HAL). See i2c3.h.
  *
  * Master sequence on the STM32F4 I2C (RM0368 18.3.3):
  *   START -> wait SR1.SB, write address to DR -> wait SR1.ADDR (or AF = NACK),
  *   clear ADDR by reading SR1 then SR2 -> bytes through DR -> STOP.
  * Receiving: ACK must be cleared and STOP set before the last byte comes in,
  * so the slave gets a NACK on it and releases the bus.
  ******************************************************************************
  */
#include "i2c3.h"

#define PIN_SCL     8u          /* PA8 */
#define PIN_SDA     9u          /* PC9 */
#define AF4_I2C3    4u

#define I2C_TIMEOUT_MS  10u

/* ---- pins ------------------------------------------------------------------ */

static void pin_mode(GPIO_TypeDef *port, uint32_t n, uint32_t mode)
{
    port->MODER = (port->MODER & ~(3u << (n * 2))) | (mode << (n * 2));
}

static void pins_od_setup(void)
{
    /* open-drain, pull-up (the board has external ones too), medium speed */
    GPIOA->OTYPER |= (1u << PIN_SCL);
    GPIOC->OTYPER |= (1u << PIN_SDA);
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(3u << (PIN_SCL * 2))) | (1u << (PIN_SCL * 2));
    GPIOC->PUPDR = (GPIOC->PUPDR & ~(3u << (PIN_SDA * 2))) | (1u << (PIN_SDA * 2));
    GPIOA->OSPEEDR = (GPIOA->OSPEEDR & ~(3u << (PIN_SCL * 2))) | (1u << (PIN_SCL * 2));
    GPIOC->OSPEEDR = (GPIOC->OSPEEDR & ~(3u << (PIN_SDA * 2))) | (1u << (PIN_SDA * 2));

    /* AFR[1] = pins 8..15 */
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~(0xFu << ((PIN_SCL - 8u) * 4))) | (AF4_I2C3 << ((PIN_SCL - 8u) * 4));
    GPIOC->AFR[1] = (GPIOC->AFR[1] & ~(0xFu << ((PIN_SDA - 8u) * 4))) | (AF4_I2C3 << ((PIN_SDA - 8u) * 4));
}

static void bit_delay(void)
{
    /* ~5 us at 80 MHz: half a 100 kHz clock period */
    for (volatile uint32_t i = 0; i < 100u; i++)
    {
    }
}

/* A slave reset in the middle of a read can hold SDA low forever. Clocking SCL
 * by hand until it lets go of SDA, then a STOP, frees the bus (UM10204 3.1.16). */
static void bus_recover(void)
{
    GPIOA->BSRR = (1u << PIN_SCL);
    GPIOC->BSRR = (1u << PIN_SDA);
    pin_mode(GPIOA, PIN_SCL, 1u);       /* 01 = output (open-drain) */
    pin_mode(GPIOC, PIN_SDA, 1u);

    for (uint32_t i = 0; i < 9u && !(GPIOC->IDR & (1u << PIN_SDA)); i++)
    {
        GPIOA->BSRR = (1u << PIN_SCL) << 16;
        bit_delay();
        GPIOA->BSRR = (1u << PIN_SCL);
        bit_delay();
    }

    /* STOP: SDA low -> high while SCL is high */
    GPIOA->BSRR = (1u << PIN_SCL) << 16;
    bit_delay();
    GPIOC->BSRR = (1u << PIN_SDA) << 16;
    bit_delay();
    GPIOA->BSRR = (1u << PIN_SCL);
    bit_delay();
    GPIOC->BSRR = (1u << PIN_SDA);
    bit_delay();

    pin_mode(GPIOA, PIN_SCL, 2u);       /* 10 = alternate function */
    pin_mode(GPIOC, PIN_SDA, 2u);
}

/* ---- peripheral ------------------------------------------------------------ */

void i2c3_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C3EN;
    (void)RCC->APB1ENR;

    I2C3->CR1 = 0;                      /* PE = 0 before the pins move */
    pins_od_setup();
    bus_recover();

    /* software reset clears a BUSY flag left over from a stuck bus */
    I2C3->CR1 = I2C_CR1_SWRST;
    I2C3->CR1 = 0;

    /* PCLK1 = 40 MHz (clock_init in main.c)
     *   CR2.FREQ = 40                          (MHz)
     *   CCR      = F/S, DUTY 0 (Tlow = 2 Thigh):
     *              40 MHz / (3 * 400 kHz) = 34 -> 392 kHz fast mode.
     *              Fast, so an AT24C128 transfer fits the window in which the
     *              MAC chip is silenced (eeprom.c).
     *   TRISE    = 300 ns * 40 MHz + 1  = 13      max rise time in fast mode */
    I2C3->CR2   = 40u;
    I2C3->CCR   = I2C_CCR_FS | 34u;
    I2C3->TRISE = 13u;
    I2C3->CR1   = I2C_CR1_PE;
}

/* waits until (SR1 & flag); false on timeout or NACK */
static bool wait_sr1(uint32_t flag)
{
    uint32_t start = millis();

    while (!(I2C3->SR1 & flag))
    {
        if (I2C3->SR1 & I2C_SR1_AF)
        {
            I2C3->SR1 = ~I2C_SR1_AF;    /* rc_w0: write 0 to clear */
            return false;
        }
        if ((millis() - start) > I2C_TIMEOUT_MS)
        {
            return false;
        }
    }
    return true;
}

static void stop(void)
{
    uint32_t start = millis();

    I2C3->CR1 |= I2C_CR1_STOP;
    while ((I2C3->CR1 & I2C_CR1_STOP) && (millis() - start) <= I2C_TIMEOUT_MS)
    {
    }
}

/* START + address. dir: 0 = write, 1 = read. Leaves ADDR set (not cleared). */
static bool start_addr(uint8_t addr, uint8_t dir)
{
    I2C3->CR1 |= I2C_CR1_START;
    if (!wait_sr1(I2C_SR1_SB))
    {
        return false;
    }
    I2C3->DR = (uint32_t)((addr << 1) | dir);
    return wait_sr1(I2C_SR1_ADDR);
}

static inline void clear_addr(void)
{
    (void)I2C3->SR1;
    (void)I2C3->SR2;
}

static bool send_bytes(const uint8_t *p, uint16_t len)
{
    while (len--)
    {
        if (!wait_sr1(I2C_SR1_TXE))
        {
            return false;
        }
        I2C3->DR = *p++;
    }
    return true;
}

/* any failure: STOP, and if the bus is still busy, start over */
static bool fail(void)
{
    stop();
    if (I2C3->SR2 & I2C_SR2_BUSY)
    {
        i2c3_init();
    }
    return false;
}

bool i2c3_probe(uint8_t addr)
{
    if (!start_addr(addr, 0))
    {
        return fail();
    }
    clear_addr();
    stop();
    return true;
}

bool i2c3_write(uint8_t addr, const uint8_t *hdr, uint8_t hlen,
                const uint8_t *data, uint16_t len)
{
    if (!start_addr(addr, 0))
    {
        return fail();
    }
    clear_addr();
    if (!send_bytes(hdr, hlen) || !send_bytes(data, len) || !wait_sr1(I2C_SR1_BTF))
    {
        return fail();
    }
    stop();
    return true;
}

bool i2c3_read(uint8_t addr, const uint8_t *hdr, uint8_t hlen,
               uint8_t *data, uint16_t len)
{
    if (len == 0)
    {
        return true;
    }

    if (hlen > 0)
    {
        if (!start_addr(addr, 0))
        {
            return fail();
        }
        clear_addr();
        if (!send_bytes(hdr, hlen) || !wait_sr1(I2C_SR1_BTF))
        {
            return fail();
        }
    }

    /* repeated START */
    if (!start_addr(addr, 1))
    {
        return fail();
    }

    if (len == 1)
    {
        /* single byte: NACK and STOP are set up while ADDR is being cleared */
        I2C3->CR1 &= ~I2C_CR1_ACK;
        clear_addr();
        I2C3->CR1 |= I2C_CR1_STOP;
        if (!wait_sr1(I2C_SR1_RXNE))
        {
            return fail();
        }
        *data = (uint8_t)I2C3->DR;
    }
    else
    {
        I2C3->CR1 |= I2C_CR1_ACK;
        clear_addr();
        while (len)
        {
            if (len == 1u)
            {
                /* the last byte is on the wire now: NACK it, then STOP */
                I2C3->CR1 &= ~I2C_CR1_ACK;
                I2C3->CR1 |= I2C_CR1_STOP;
            }
            if (!wait_sr1(I2C_SR1_RXNE))
            {
                return fail();
            }
            *data++ = (uint8_t)I2C3->DR;
            len--;
        }
    }

    /* STOP is sent by hardware after the last byte; wait for it to finish */
    {
        uint32_t start = millis();
        while ((I2C3->CR1 & I2C_CR1_STOP) && (millis() - start) <= I2C_TIMEOUT_MS)
        {
        }
    }
    I2C3->CR1 |= I2C_CR1_ACK;
    return true;
}
