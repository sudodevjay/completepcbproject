/*
 * LiquidCrystal_I2C_STM32.cpp
 *
 *  Created on: Nov 20, 2025
 *      Author: Houston
 */


#include "LiquidCrystal_I2C_STM32.h"
#include <string.h>
#include <stdio.h> // for snprintf
#include <stdlib.h>

extern I2C_HandleTypeDef hi2c1;
void delay_us(uint32_t us)
{
//    uint32_t start = DWT->CYCCNT;
//    uint32_t ticks = us * (SystemCoreClock / 1000000);
//    while(DWT->CYCCNT - start < ticks);
	HAL_Delay(1);
}

LiquidCrystal_I2C::LiquidCrystal_I2C(I2C_HandleTypeDef* hi2c, uint8_t lcd_Addr, uint8_t lcd_cols, uint8_t lcd_rows)
{
  _hi2c = hi2c;
  _Addr = lcd_Addr & 0x7F; // keep 7-bit
  _cols = lcd_cols;
  _rows = lcd_rows;
  _backlightval = LCD_BACKLIGHT;
  _i2c_timeout = 100; // default timeout ms
  _displayfunction = LCD_4BITMODE | LCD_5x8DOTS;
}

void LiquidCrystal_I2C::begin(uint8_t cols, uint8_t rows, uint8_t charsize)
{
  _cols = cols;
  _rows = rows;
  if (rows > 1) _displayfunction |= LCD_2LINE;
  init_priv();
}
//for(volatile int i=0; i<10; i++); // ~ few microseconds

void LiquidCrystal_I2C::init_priv()
{
  // Wait for LCD to power up
  HAL_Delay(50);

  // Initialize expander (backlight on)
  expanderWrite(_backlightval);
  HAL_Delay(100);

  // According to datasheet init sequence for 4-bit mode via PCF8574
  // send 0x03 three times
  write4bits(0x03 << 4);
  HAL_Delay(5);
  write4bits(0x03 << 4);
  HAL_Delay(5);
  write4bits(0x03 << 4);
  HAL_Delay(1);

  // then set to 4-bit mode
  write4bits(0x02 << 4);
  HAL_Delay(1);

  // function set
  uint8_t cmd = LCD_FUNCTIONSET | _displayfunction;
  send(cmd, 0);

  // display control - turn on display, no cursor, no blink
  _displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
  display();

  clear();

  // entry mode set - left to right
  _displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
  send(LCD_ENTRYMODESET | _displaymode, 0);

  home();
}

void LiquidCrystal_I2C::clear()
{
  send(LCD_CLEARDISPLAY, 0);
  HAL_Delay(2); // clear needs >1.52ms
}

void LiquidCrystal_I2C::home()
{
  send(LCD_RETURNHOME, 0);
  HAL_Delay(2);
}

void LiquidCrystal_I2C::setCursor(uint8_t col, uint8_t row)
{
  static const uint8_t row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
  if (row >= _rows) row = _rows - 1;
  send(LCD_SETDDRAMADDR | (col + row_offsets[row]), 0);
}

void LiquidCrystal_I2C::noDisplay()
{
  _displaycontrol &= ~LCD_DISPLAYON;
  send(LCD_DISPLAYCONTROL | _displaycontrol, 0);
}

void LiquidCrystal_I2C::display()
{
  _displaycontrol |= LCD_DISPLAYON;
  send(LCD_DISPLAYCONTROL | _displaycontrol, 0);
}

void LiquidCrystal_I2C::noCursor()
{
  _displaycontrol &= ~LCD_CURSORON;
  send(LCD_DISPLAYCONTROL | _displaycontrol, 0);
}

void LiquidCrystal_I2C::cursor()
{
  _displaycontrol |= LCD_CURSORON;
  send(LCD_DISPLAYCONTROL | _displaycontrol, 0);
}

void LiquidCrystal_I2C::noBlink()
{
  _displaycontrol &= ~LCD_BLINKON;
  send(LCD_DISPLAYCONTROL | _displaycontrol, 0);
}

void LiquidCrystal_I2C::blink()
{
  _displaycontrol |= LCD_BLINKON;
  send(LCD_DISPLAYCONTROL | _displaycontrol, 0);
}

void LiquidCrystal_I2C::backlight() {
    _backlightval = LCD_BACKLIGHT;
    expanderWrite(_backlightval);
}
void LiquidCrystal_I2C::noBacklight() {
    _backlightval = LCD_NOBACKLIGHT;
    expanderWrite(_backlightval);
}

void LiquidCrystal_I2C::createChar(uint8_t location, uint8_t charmap[])
{
  location &= 0x7; // only 8 locations 0-7
  send(LCD_SETCGRAMADDR | (location << 3), 0);
  for (int i = 0; i < 8; i++) {
    write(charmap[i]);
  }
}

void LiquidCrystal_I2C::send(uint8_t value, uint8_t mode)
{
  // mode = Rs bit (0 = command, Rs=1 => data)
  uint8_t highnib = value & 0xF0;
  uint8_t lownib = (value << 4) & 0xF0;
  write4bits(highnib | mode);
  write4bits(lownib | mode);
}

void LiquidCrystal_I2C::write4bits(uint8_t data)
{
  expanderWrite(data | _backlightval);
  pulseEnable(data | _backlightval);
}

void LiquidCrystal_I2C::expanderWrite(uint8_t data)
{
  uint8_t buf[1];
  buf[0] = data;
  // HAL expects 8-bit address (7-bit << 1)
  HAL_I2C_Master_Transmit(_hi2c, (uint16_t)(_Addr << 1), buf, 1,_i2c_timeout);//
//  HAL_I2C_Master_Transmit_IT(_hi2c, (uint16_t)(_Addr << 1), buf, 1);
}

void LiquidCrystal_I2C::pulseEnable(uint8_t data)
{
  expanderWrite(data | En);
  // enable pulse must be >450ns, so a tiny delay
  // HAL_Delay(1) is too big; use short micro delay
  // If DWT cycle counter is not available, use simple small loop
  // Minimal safe delay here:
//  HAL_Delay(1); // safe and portable (ms). If you want faster, replace with microsecond delay.
//  for(volatile int i=0; i<10; i++);
  delay_us(1);
  expanderWrite(data & ~En);
//  HAL_Delay(1);
//  for(volatile int i=0; i<10; i++);
  delay_us(1);
}

// print helpers
void LiquidCrystal_I2C::printstr(const char *str)
{
  while (*str) {
    write((uint8_t)*str++);
  }
}

void LiquidCrystal_I2C::print(const char *str)
{
  printstr(str);
}

void LiquidCrystal_I2C::print(uint16_t value)
{
  char buf[12];
  snprintf(buf, sizeof(buf), "%u", (unsigned int)value);
  printstr(buf);
}

void LiquidCrystal_I2C::print(int value)
{
  char buf[12];
  snprintf(buf, sizeof(buf), "%d", value);
  printstr(buf);
}

size_t LiquidCrystal_I2C::write(uint8_t value)
{
  send(value, Rs);
  return 1;
}
uint8_t LiquidCrystal_I2C::getRows(void)
{
    return _rows;
}

