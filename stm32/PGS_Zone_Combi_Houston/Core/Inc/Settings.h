/*
 * Settings.h
 *
 *  Created on: Jan 24, 2026
 *      Author: Houston
 */

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include "LiquidCrystal_I2C_STM32.h"
#include "AT24C512.h"
#include "HWprofile.h"

extern LiquidCrystal_I2C lcd;

const char *MenuList[] =
{
	"String no. 1",
	"String no. 2",
	"String no. 3",
	"String no. 4",
	"String no. 5",
	"String no. 6",
	"String no. 7",
    "String no. 8",
    "String no. 9",
    "String no. 10"
};

void LCD_ShowInitScreen(void)
{

    lcd.setCursor(0,0);
    lcd.print("   HOUSTON SYSTEM   ");

    lcd.setCursor(0,1);
    lcd.print("********************");

    lcd.setCursor(0,2);
    lcd.print("PARKING GUIDANCE SYS");

    lcd.setCursor(0,3);
    lcd.print("INITIALIZING      "); // spaces important
}
void LCD_InitDots(uint8_t dots)
{
    lcd.setCursor(12, 3);   // after "INITIALIZING"
    for(uint8_t i = 0; i < dots; i++)
        lcd.print(".");
}
static void ValueToDigits(uint32_t value, uint8_t *d)
{
  value %= 1000000;
  d[0] = value / 100000; value %= 100000;
  d[1] = value / 10000;  value %= 10000;
  d[2] = value / 1000;   value %= 1000;
  d[3] = value / 100;    value %= 100;
  d[4] = value / 10;
  d[5] = value % 10;
}

static uint32_t DigitsToValue(uint8_t *d)
{
  return  d[0]*100000UL +
		  d[1]*10000UL  +
		  d[2]*1000UL   +
		  d[3]*100UL    +
		  d[4]*10UL     +
		  d[5];
}
void LCD_ShowDigits(LiquidCrystal_I2C &lcd, uint8_t *d, uint8_t blink, uint8_t dec_pos)
{
  const uint8_t start_col = 5;
  uint8_t col = start_col;
  uint8_t blink_col = start_col;
  /* clear entire field */
  lcd.setCursor(start_col, 2);
  lcd.print("         ");   // enough for 6 digits + dot
  lcd.setCursor(start_col, 2);
  for(uint8_t i = 0; i < 6; i++)
  {
	  /* save cursor position for blink digit */
	  if(i == blink)
		  blink_col = col;

	  /* print digit */
	  lcd.write((char)(d[i] + '0'));
	  col++;

	  /* print decimal point after correct digit */
	  if(dec_pos && ((5 - i) == dec_pos))
	  {
		  lcd.write('.');
		  col++;
	  }
  }
  /* place blinking cursor on correct digit */
  lcd.setCursor(blink_col, 2);
  lcd.cursor();
  lcd.blink();
  }
uint32_t SetV(uint32_t value,uint8_t disits,uint8_t dec_pos,LiquidCrystal_I2C &lcd)
{
    uint8_t blink_min = 6 - disits; // leftmost editable digit
    uint8_t blink_max = 5;                    // rightmost editable digit
    uint8_t blink     = blink_min;            // start at leftmost
    uint8_t DIS[6];
    ValueToDigits(value, DIS);
//    lcd.clear();

    while(1)
    {
        LCD_ShowDigits(lcd, DIS, blink, dec_pos);
        /* wait for key press */
        do { keystt = ReadKey(keystt); }
        while(keystt == K_RLS);

        switch(keystt)
        {
            case K_INC:
                DIS[blink] = (DIS[blink] + 1) % 10;
                break;

            case K_DEC:
                DIS[blink] = (DIS[blink] == 0) ? 9 : DIS[blink] - 1;
                break;

            case K_ESC:   // SHIFT → move LEFT to RIGHT
                if(blink < blink_max)
                    blink++;
                else
                    blink = blink_min;   // roll over
                break;

            case K_ENT:
                lcd.noBlink();
                lcd.noCursor();
                /* wait for key release */
                do { keystt = ReadKey(keystt); }
                while(keystt != K_RLS);

                return DigitsToValue(DIS);
        }

        /* wait for key release */
        do { keystt = ReadKey(keystt); }
        while(keystt != K_RLS);
    }
}

int LCD_MenuSelect(
    LiquidCrystal_I2C &lcd,
    const char *menu[],
    uint8_t menuCount
)
{
    uint8_t rows = lcd.getRows();

    int topIndex = 0;
    int cursorPos = 0;

    int prevTop = -1;
    int prevCursor = -1;

    unsigned char key = K_RLS;
    unsigned char lastKey = K_RLS;
    unsigned char prevKey = K_RLS;

    lcd.clear();

    while (1)
    {
        /* -------- Draw only if needed -------- */
        if ((topIndex != prevTop) || (cursorPos != prevCursor))
        {
            for (uint8_t i = 0; i < rows; i++)
            {
                lcd.setCursor(0, i);
                int itemIndex = topIndex + i;

                if (itemIndex < menuCount)
                {
                    lcd.print(i == cursorPos ? ">" : " ");
                    lcd.print(menu[itemIndex]);
                }
                else
                {
                    lcd.print("                ");
                }
            }

            prevTop = topIndex;
            prevCursor = cursorPos;
        }

        /* -------- Key EDGE detect -------- */
        key = ReadKey(lastKey);
        lastKey = key;

        if (key == prevKey)
            continue;   // ignore hold

        prevKey = key;

        /* -------- UP -------- */
        if (key == K_INC)
        {
            if (cursorPos > 0)
                cursorPos--;
            else if (topIndex > 0)
                topIndex--;
        }

        /* -------- DOWN -------- */
        else if (key == K_DEC)
        {
            if (cursorPos < (rows - 1) &&
                (topIndex + cursorPos + 1) < menuCount)
                cursorPos++;
            else if ((topIndex + rows) < menuCount)
                topIndex++;
        }

        /* -------- ENTER -------- */
        else if (key == K_ENT)
        {
            return (topIndex + cursorPos);
        }

        /* -------- ESC -------- */
        else if (key == K_ESC)
        {
            return -1;
        }
    }
}

void SetPSW(void)
{
unsigned int psw=0x0000;
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("   ENTER PASSWORD   ");
	psw=SetV(0,4,0,lcd);
	if(psw==PASSWORD) DisMenu=SET_ZONE_ID;
	else{
		DisMenu=MIAN_PAGE;
		lcd.clear();
		LCD_ShowInitScreen();
	}
	lcd.clear();
}

void setZoneID(void)
{
	unsigned char temp=0;
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print("   ENTER ZONE ID   ");
	temp=SetV(Zone_ID,1,0,lcd);
	if(temp<0)temp=0;
	if(temp>9)temp=9;
	if(temp!=Zone_ID)
	{
		Zone_ID=temp;
		AT24C512_WriteByte(0x01,Zone_ID);
	}
	DisMenu=SET_NUMBER_OF_DISPLAY;
	lcd.clear();
}
void SetNumber_of_Display(void)
{
	unsigned char temp=0;
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print(" NUMBER OF DISPLAY  ");
	temp=SetV(Number_of_display,1,0,lcd);
	if(temp<0)temp=0;
	if(temp>9)temp=9;
	if(temp!=Number_of_display)
	{
		Number_of_display=temp;
		AT24C512_WriteByte(0x02,Number_of_display);
	}
	DisMenu=SET_DISPLAY_CONFIG;
	lcd.clear();
}

void SetTotal_Sensor(void)
{
	unsigned char temp=0;
	lcd.clear();
	lcd.setCursor(0,0);
	lcd.print(" TOTAL SENSOR  ");
	temp=SetV(Total_Sensor,2,0,lcd);
	if(temp<0)temp=0;
	if(temp>99)temp=99;
	if(temp!=Total_Sensor)
	{
		Total_Sensor=temp;
		AT24C512_WriteByte(0x3D,Total_Sensor);
	}
	DisMenu=MIAN_PAGE;
	lcd.clear();
	LCD_ShowInitScreen();
}

//void SetDisplay_Config(void)
//{
//    for(uint8_t i = 0; i < Number_of_display; i++)
//    {
//        lcd.clear();
//        lcd.setCursor(0,0);
//        lcd.print("DISPLAY ");
//        lcd.print(i + 1);
//
//        /* ---- ID ---- */
//        lcd.setCursor(0,1);
//        lcd.print("ID:");
//        Displays[i].id = SetV(Displays[i].id, 1, 0, lcd);
//
//        /* ---- MODE ---- */
//        lcd.clear();
//        lcd.print("MODE:");
//        Displays[i].mode = SetV(Displays[i].mode, 1, 0, lcd);
//
//        /* ---- COLOR ---- */
//        lcd.clear();
//        lcd.print("COLOR:");
//        Displays[i].color = SetV(Displays[i].color, 1, 0, lcd);
//
//        /* ---- SENSOR START ---- */
//        lcd.clear();
//        lcd.print("SENSOR START:");
//        Displays[i].sensor_start = SetV(Displays[i].sensor_start, 2, 0, lcd);
//
//        /* ---- SENSOR STOP ---- */
//        lcd.clear();
//        lcd.print("SENSOR STOP:");
//        Displays[i].sensor_stop = SetV(Displays[i].sensor_stop, 2, 0, lcd);
//    }
//
//    /* After last display, go back to main page */
//    DisMenu = MIAN_PAGE;
//    lcd.clear();
//}
void SetDisplay_Config(void)
{
    char buf[21];

    for(uint8_t i = 0; i < Number_of_display; i++)
    {
        /* ---------- DISPLAY ID (0–9) ---------- */
        lcd.clear();
        sprintf(buf, "D%d ID", i + 1);
        lcd.setCursor((20 - strlen(buf)) / 2, 0);
        lcd.print(buf);

        Displays[i].id = SetV(Displays[i].id, 1, 0, lcd);
        if(Displays[i].id > 9) Displays[i].id = 9;
        AT24C512_WriteByte(0x10 + (i * 5) + 0, Displays[i].id);

        /* ---------- MODE (1–4) ---------- */
        lcd.clear();
        sprintf(buf, "D%d MODE", i + 1);
        lcd.setCursor((20 - strlen(buf)) / 2, 0);
        lcd.print(buf);

        Displays[i].mode = SetV(Displays[i].mode, 1, 0, lcd);
        if(Displays[i].mode < 1) Displays[i].mode = 1;
        if(Displays[i].mode > 4) Displays[i].mode = 4;
        AT24C512_WriteByte(0x10 + (i * 5) + 1, Displays[i].mode);

        /* ---------- COLOR (1–3) ---------- */
        lcd.clear();
        sprintf(buf, "D%d COLOR", i + 1);
        lcd.setCursor((20 - strlen(buf)) / 2, 0);
        lcd.print(buf);

        Displays[i].color = SetV(Displays[i].color, 1, 0, lcd);
        if(Displays[i].color < 1) Displays[i].color = 1;
        if(Displays[i].color > 3) Displays[i].color = 3;
        AT24C512_WriteByte(0x10 + (i * 5) + 2, Displays[i].color);

        /* ---------- SENSOR START (0–99) ---------- */
        lcd.clear();
        sprintf(buf, "D%d S-START", i + 1);
        lcd.setCursor((20 - strlen(buf)) / 2, 0);
        lcd.print(buf);

        Displays[i].sensor_start = SetV(Displays[i].sensor_start, 2, 0, lcd);
        if(Displays[i].sensor_start > 99) Displays[i].sensor_start = 99;
        AT24C512_WriteByte(0x10 + (i * 5) + 3, Displays[i].sensor_start);

        /* ---------- SENSOR STOP (0–99) ---------- */
        lcd.clear();
        sprintf(buf, "D%d S-STOP", i + 1);
        lcd.setCursor((20 - strlen(buf)) / 2, 0);
        lcd.print(buf);

        Displays[i].sensor_stop = SetV(Displays[i].sensor_stop, 2, 0, lcd);
        if(Displays[i].sensor_stop > 99) Displays[i].sensor_stop = 99;
        if(Displays[i].sensor_stop < Displays[i].sensor_start)
            Displays[i].sensor_stop = Displays[i].sensor_start;

        AT24C512_WriteByte(0x10 + (i * 5) + 4, Displays[i].sensor_stop);
    }

    lcd.clear();
    DisMenu = SET_TOTAL_SENSOR;
}

void Read_data()
{
	Zone_ID=AT24C512_ReadByte(0x01);
	if(Zone_ID<0)Zone_ID=0;
	if(Zone_ID>9)Zone_ID=9;
	Number_of_display=AT24C512_ReadByte(0x02);
	if(Number_of_display<0)Number_of_display=0;
	if(Number_of_display>9)Number_of_display=9;
	 /* Safety limit */
	    if(Number_of_display > 9)
	        Number_of_display = 9;

	    for(uint8_t i = 0; i < Number_of_display; i++)
	    {
	        Displays[i].id = AT24C512_ReadByte(0x10 + (i * 5) + 0);
	        if(Displays[i].id > 9) Displays[i].id = 9;

	        Displays[i].mode = AT24C512_ReadByte(0x10 + (i * 5) + 1);
	        if(Displays[i].mode < 1) Displays[i].mode = 1;
	        if(Displays[i].mode > 4) Displays[i].mode = 4;

	        Displays[i].color = AT24C512_ReadByte(0x10 + (i * 5) + 2);
	        if(Displays[i].color < 1) Displays[i].color = 1;
	        if(Displays[i].color > 3) Displays[i].color = 3;

	        Displays[i].sensor_start = AT24C512_ReadByte(0x10 + (i * 5) + 3);
	        if(Displays[i].sensor_start > 99) Displays[i].sensor_start = 99;

	        Displays[i].sensor_stop = AT24C512_ReadByte(0x10 + (i * 5) + 4);
	        if(Displays[i].sensor_stop > 99) Displays[i].sensor_stop = 99;

	        /* Safety: stop must be >= start */
	        if(Displays[i].sensor_stop < Displays[i].sensor_start)
	            Displays[i].sensor_stop = Displays[i].sensor_start;
	    }
	    Total_Sensor=AT24C512_ReadByte(0x3D);
	    if(Total_Sensor<0)Total_Sensor=0;
	    if(Total_Sensor>99)Total_Sensor=99;
}

//	  Examples:
//uint32_t setpoint;
//lcd.clear();
//lcd.setCursor(0,0);
//lcd.print("   SET BAUD RATE   ");
//setpoint = SetV(12345,     // initial value
//			 6,         // start blinking from right
//			  0,         // decimal position
//			  lcd);      // your LCD object
//int selected;
//
//selected = LCD_MenuSelect(lcd, MenuList, 10);
//
//if (selected >= 0)
//{
//  lcd.clear();
//  lcd.print("Selected:");
//  lcd.setCursor(0, 1);
//  lcd.print(MenuList[selected]);
//}
#endif /* SETTINGS_H_ */
