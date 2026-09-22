/*
 * LiquidCrystal_I2C_STM32.h
 * Created on: Nov 20, 2025
 * Author: Houston
 */

#ifndef __LIQUIDCRYSTAL_I2C_STM32_H__
#define __LIQUIDCRYSTAL_I2C_STM32_H__

#include <stdint.h>
#include <stddef.h>

/* ---- C HAL Includes ---- */
#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_i2c.h"

#ifdef __cplusplus
}
#endif

/* -------------------------------------------------
   LCD COMMANDS & FLAGS
--------------------------------------------------*/

#define LCD_CLEARDISPLAY    0x01
#define LCD_RETURNHOME      0x02
#define LCD_ENTRYMODESET    0x04
#define LCD_DISPLAYCONTROL  0x08
#define LCD_CURSORSHIFT     0x10
#define LCD_FUNCTIONSET     0x20
#define LCD_SETCGRAMADDR    0x40
#define LCD_SETDDRAMADDR    0x80

// Entry mode flags
#define LCD_ENTRYRIGHT          0x00
#define LCD_ENTRYLEFT           0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// Display control flags
#define LCD_DISPLAYON   0x04
#define LCD_DISPLAYOFF  0x00
#define LCD_CURSORON    0x02
#define LCD_CURSOROFF   0x00
#define LCD_BLINKON     0x01
#define LCD_BLINKOFF    0x00

// Display shift flags
#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE  0x00
#define LCD_MOVERIGHT   0x04
#define LCD_MOVELEFT    0x00

// Function set flags
#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE    0x08
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS  0x00

// Backlight
#define LCD_BACKLIGHT    0x08
#define LCD_NOBACKLIGHT  0x00

// PCF8574 bits
#define En 0x04   // Enable bit
#define Rw 0x02   // Read/Write
#define Rs 0x01   // Register select


/* -------------------------------------------------
   C++ CLASS
--------------------------------------------------*/

#ifdef __cplusplus

class LiquidCrystal_I2C {
public:
    LiquidCrystal_I2C(I2C_HandleTypeDef* hi2c, uint8_t lcd_Addr, uint8_t lcd_cols, uint8_t lcd_rows);

    void begin(uint8_t cols, uint8_t rows, uint8_t charsize = LCD_5x8DOTS);

    void clear();
    void home();
    void setCursor(uint8_t col, uint8_t row);

    void noDisplay();
    void display();
    void noCursor();
    void cursor();
    void noBlink();
    void blink();

    void backlight();
    void noBacklight();

    void createChar(uint8_t location, uint8_t charmap[]);

    void printstr(const char *str);
    void print(uint16_t value);
    void print(int value);
    void print(const char *str);
    size_t write(uint8_t value);
    uint8_t getRows(void);

private:
    void init_priv();
    void send(uint8_t value, uint8_t mode);
    void write4bits(uint8_t nibble);
    void expanderWrite(uint8_t data);
    void pulseEnable(uint8_t data);

    I2C_HandleTypeDef* _hi2c;
    uint8_t _Addr;
    uint8_t _displayfunction;
    uint8_t _displaycontrol;
    uint8_t _displaymode;
    uint8_t _numlines;
    uint8_t _cols;
    uint8_t _rows;
    uint8_t _backlightval = LCD_BACKLIGHT;
    uint32_t _i2c_timeout = 100; // Default timeout in ms


};


#endif /* __cplusplus */

#endif /* __LIQUIDCRYSTAL_I2C_STM32_H__ */
