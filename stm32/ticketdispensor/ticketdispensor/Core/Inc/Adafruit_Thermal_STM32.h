/*!
 * @file Adafruit_Thermal_STM32.h
 * @brief STM32CubeIDE port - No Arduino dependencies
 */

#ifndef ADAFRUIT_THERMAL_STM32_H
#define ADAFRUIT_THERMAL_STM32_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

// Internal character sets used with ESC R n
#define CHARSET_USA 0
#define CHARSET_FRANCE 1
#define CHARSET_GERMANY 2
#define CHARSET_UK 3
#define CHARSET_DENMARK1 4
#define CHARSET_SWEDEN 5
#define CHARSET_ITALY 6
#define CHARSET_SPAIN1 7
#define CHARSET_JAPAN 8
#define CHARSET_NORWAY 9
#define CHARSET_DENMARK2 10
#define CHARSET_SPAIN2 11
#define CHARSET_LATINAMERICA 12
#define CHARSET_KOREA 13
#define CHARSET_SLOVENIA 14
#define CHARSET_CROATIA 14
#define CHARSET_CHINA 15

// Character code tables used with ESC t n
#define CODEPAGE_CP437 0
#define CODEPAGE_KATAKANA 1
#define CODEPAGE_CP850 2
#define CODEPAGE_CP860 3
#define CODEPAGE_CP863 4
#define CODEPAGE_CP865 5
#define CODEPAGE_WCP1251 6
#define CODEPAGE_CP866 7
#define CODEPAGE_MIK 8
#define CODEPAGE_CP755 9
#define CODEPAGE_IRAN 10
#define CODEPAGE_CP862 15
#define CODEPAGE_WCP1252 16
#define CODEPAGE_WCP1253 17
#define CODEPAGE_CP852 18
#define CODEPAGE_CP858 19
#define CODEPAGE_IRAN2 20
#define CODEPAGE_LATVIAN 21
#define CODEPAGE_CP864 22
#define CODEPAGE_ISO_8859_1 23
#define CODEPAGE_CP737 24
#define CODEPAGE_WCP1257 25
#define CODEPAGE_THAI 26
#define CODEPAGE_CP720 27
#define CODEPAGE_CP855 28
#define CODEPAGE_CP857 29
#define CODEPAGE_WCP1250 30
#define CODEPAGE_CP775 31
#define CODEPAGE_WCP1254 32
#define CODEPAGE_WCP1255 33
#define CODEPAGE_WCP1256 34
#define CODEPAGE_WCP1258 35
#define CODEPAGE_ISO_8859_2 36
#define CODEPAGE_ISO_8859_3 37
#define CODEPAGE_ISO_8859_4 38
#define CODEPAGE_ISO_8859_5 39
#define CODEPAGE_ISO_8859_6 40
#define CODEPAGE_ISO_8859_7 41
#define CODEPAGE_ISO_8859_8 42
#define CODEPAGE_ISO_8859_9 43
#define CODEPAGE_ISO_8859_15 44
#define CODEPAGE_THAI2 45
#define CODEPAGE_CP856 46
#define CODEPAGE_CP874 47

// Barcode types
typedef enum {
    BARCODE_UPC_A = 0,
    BARCODE_UPC_E = 1,
    BARCODE_EAN13 = 2,
    BARCODE_EAN8 = 3,
    BARCODE_CODE39 = 4,
    BARCODE_ITF = 5,
    BARCODE_CODABAR = 6,
    BARCODE_CODE93 = 7,
    BARCODE_CODE128 = 8
} barcode_type_t;

// Thermal Printer Structure
typedef struct {
    // UART Handle
    UART_HandleTypeDef *huart;

    // DTR Pin (optional)
    GPIO_TypeDef *dtr_port;
    uint16_t dtr_pin;
    uint8_t dtr_enabled;

    // Printer state
    uint16_t firmware;
    uint8_t print_mode;
    uint8_t prev_byte;
    uint8_t column;
    uint8_t max_column;
    uint8_t char_height;
    uint8_t line_spacing;
    uint8_t barcode_height;
    uint8_t max_chunk_height;

    // Timing
    uint64_t resume_time;
    uint32_t dot_print_time;
    uint32_t dot_feed_time;

    // RX buffer for paper status
    uint8_t rx_buffer;
    uint8_t rx_available;
} Adafruit_Thermal_t;

// Initialize printer
void Adafruit_Thermal_Init(Adafruit_Thermal_t *printer,
                           UART_HandleTypeDef *huart,
                           GPIO_TypeDef *dtr_port,
                           uint16_t dtr_pin);

// Main control functions
void Adafruit_Thermal_Begin(Adafruit_Thermal_t *printer, uint16_t firmware_version);
void Adafruit_Thermal_Reset(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_SetDefault(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_Wake(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_Sleep(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_Offline(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_Online(Adafruit_Thermal_t *printer);

// Text printing
void Adafruit_Thermal_Write(Adafruit_Thermal_t *printer, uint8_t c);
void Adafruit_Thermal_Print(Adafruit_Thermal_t *printer, const char *str);
void Adafruit_Thermal_Println(Adafruit_Thermal_t *printer, const char *str);
void Adafruit_Thermal_PrintInt(Adafruit_Thermal_t *printer, int32_t num);
void Adafruit_Thermal_PrintlnInt(Adafruit_Thermal_t *printer, int32_t num);

// Paper feeding
void Adafruit_Thermal_Feed(Adafruit_Thermal_t *printer, uint8_t lines);
void Adafruit_Thermal_FeedRows(Adafruit_Thermal_t *printer, uint8_t rows);
void Adafruit_Thermal_Flush(Adafruit_Thermal_t *printer);

// Text formatting
void Adafruit_Thermal_Justify(Adafruit_Thermal_t *printer, char value);
void Adafruit_Thermal_BoldOn(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_BoldOff(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_UnderlineOn(Adafruit_Thermal_t *printer, uint8_t weight);
void Adafruit_Thermal_UnderlineOff(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_InverseOn(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_InverseOff(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_DoubleHeightOn(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_DoubleHeightOff(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_DoubleWidthOn(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_DoubleWidthOff(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_Normal(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_SetSize(Adafruit_Thermal_t *printer, char value);
void Adafruit_Thermal_SetFont(Adafruit_Thermal_t *printer, char font);
void Adafruit_Thermal_SetLineHeight(Adafruit_Thermal_t *printer, int height);
void Adafruit_Thermal_SetCharSpacing(Adafruit_Thermal_t *printer, int spacing);
void Adafruit_Thermal_Tab(Adafruit_Thermal_t *printer);

// Barcode
void Adafruit_Thermal_SetBarcodeHeight(Adafruit_Thermal_t *printer, uint8_t height);
void Adafruit_Thermal_PrintBarcode(Adafruit_Thermal_t *printer, const char *text, uint8_t type);

// Configuration
void Adafruit_Thermal_SetHeatConfig(Adafruit_Thermal_t *printer, uint8_t dots, uint8_t time, uint8_t interval);
void Adafruit_Thermal_SetPrintDensity(Adafruit_Thermal_t *printer, uint8_t density, uint8_t breakTime);
void Adafruit_Thermal_SetCharset(Adafruit_Thermal_t *printer, uint8_t val);
void Adafruit_Thermal_SetCodePage(Adafruit_Thermal_t *printer, uint8_t val);
void Adafruit_Thermal_SetTimes(Adafruit_Thermal_t *printer, uint32_t print_time, uint32_t feed_time);
void Adafruit_Thermal_SetMaxChunkHeight(Adafruit_Thermal_t *printer, int val);

// Status
bool Adafruit_Thermal_HasPaper(Adafruit_Thermal_t *printer);

// Test functions
void Adafruit_Thermal_Test(Adafruit_Thermal_t *printer);
void Adafruit_Thermal_TestPage(Adafruit_Thermal_t *printer);

// Call this in UART RX callback
void Adafruit_Thermal_RxCallback(Adafruit_Thermal_t *printer, uint8_t data);

void Adafruit_Thermal_SetLeftMargin(Adafruit_Thermal_t *printer, uint8_t margin);
void Adafruit_Thermal_PrintAndReturn(Adafruit_Thermal_t *printer, uint8_t n) ;
void Adafruit_Thermal_PrintAndFeed(Adafruit_Thermal_t *printer, uint8_t n);
void Adafruit_Thermal_FullCut(Adafruit_Thermal_t *printer) ;
void Adafruit_Thermal_HalfCut(Adafruit_Thermal_t *printer);

void Adafruit_Thermal_PrintQRCode(Adafruit_Thermal_t *printer, const char *data);
void Adafruit_Thermal_PrintQRCodeWithSize(Adafruit_Thermal_t *printer, const char *data, uint8_t moduleSize);
#endif // ADAFRUIT_THERMAL_STM32_H
