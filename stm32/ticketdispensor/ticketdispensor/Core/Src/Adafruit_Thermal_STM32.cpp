/*!
 * @file Adafruit_Thermal_STM32.c
 * @brief Implementation for STM32CubeIDE - No Arduino dependencies
 */

#include "Adafruit_Thermal_STM32.h"
#include <cstdio>   // for sprintf
#include <cstring>  // for strlen
#include <cctype>   // for toupper
// ASCII codes used by printer commands
#define ASCII_TAB '\t'
#define ASCII_LF '\n'
#define ASCII_FF '\f'
#define ASCII_CR '\r'
#define ASCII_DC2 18
#define ASCII_ESC 27
#define ASCII_FS 28
#define ASCII_GS 29
// Add these definitions at the top with other ASCII codes
//#define ASCII_ESC 27
//#define ASCII_GS 29
#define ESC_K 75  // 'K' - Print and return
#define ESC_J 74  // 'J' - Print and feed
#define ESC_i 105 // 'i' - Full cut
#define ESC_m 109 // 'm' - Half cut
// Default baud rate and timing
#define BAUDRATE 38400
#define BYTE_TIME (((11L * 1000000L) + (BAUDRATE / 2)) / BAUDRATE)

// Print mode masks
#define FONT_MASK (1 << 0)
#define INVERSE_MASK (1 << 1)
#define UPDOWN_MASK (1 << 2)
#define BOLD_MASK (1 << 3)
#define DOUBLE_HEIGHT_MASK (1 << 4)
#define DOUBLE_WIDTH_MASK (1 << 5)
#define STRIKE_MASK (1 << 6)

// Forward declarations of private functions
static void timeoutSet(Adafruit_Thermal_t *printer, uint32_t us);
static void timeoutWait(Adafruit_Thermal_t *printer);
static void writeByte(Adafruit_Thermal_t *printer, uint8_t a);
static void writeBytes2(Adafruit_Thermal_t *printer, uint8_t a, uint8_t b);
static void writeBytes3(Adafruit_Thermal_t *printer, uint8_t a, uint8_t b, uint8_t c);
static void writeBytes4(Adafruit_Thermal_t *printer, uint8_t a, uint8_t b, uint8_t c, uint8_t d);
static void writeBytes5(Adafruit_Thermal_t *printer, uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e);
static void writePrintMode(Adafruit_Thermal_t *printer);
static void setPrintMode(Adafruit_Thermal_t *printer, uint8_t mask);
static void unsetPrintMode(Adafruit_Thermal_t *printer, uint8_t mask);
static void adjustCharValues(Adafruit_Thermal_t *printer, uint8_t printMode);

// ==================== Private Helper Functions ====================

static void timeoutSet(Adafruit_Thermal_t *printer, uint32_t us) {
    if (!printer->dtr_enabled) {
        printer->resume_time = (uint64_t)HAL_GetTick() * 1000ULL + us;
    }
}

static void timeoutWait(Adafruit_Thermal_t *printer) {
    if (printer->dtr_enabled && printer->dtr_port != NULL) {
        while (HAL_GPIO_ReadPin(printer->dtr_port, printer->dtr_pin) == GPIO_PIN_SET) {
            HAL_Delay(1);
        }
    } else if (!printer->dtr_enabled) {
        while (((uint64_t)HAL_GetTick() * 1000ULL) < printer->resume_time) {
            // Wait
        }
    }
}

static void writeByte(Adafruit_Thermal_t *printer, uint8_t a) {
    timeoutWait(printer);
    HAL_UART_Transmit(printer->huart, &a, 1, HAL_MAX_DELAY);
    timeoutSet(printer, BYTE_TIME);
}

static void writeBytes2(Adafruit_Thermal_t *printer, uint8_t a, uint8_t b) {
    timeoutWait(printer);
    uint8_t data[2] = {a, b};
    HAL_UART_Transmit(printer->huart, data, 2, HAL_MAX_DELAY);
    timeoutSet(printer, 2 * BYTE_TIME);
}

static void writeBytes3(Adafruit_Thermal_t *printer, uint8_t a, uint8_t b, uint8_t c) {
    timeoutWait(printer);
    uint8_t data[3] = {a, b, c};
    HAL_UART_Transmit(printer->huart, data, 3, HAL_MAX_DELAY);
    timeoutSet(printer, 3 * BYTE_TIME);
}

static void writeBytes4(Adafruit_Thermal_t *printer, uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    timeoutWait(printer);
    uint8_t data[4] = {a, b, c, d};
    HAL_UART_Transmit(printer->huart, data, 4, HAL_MAX_DELAY);
    timeoutSet(printer, 4 * BYTE_TIME);
}

static void writeBytes5(Adafruit_Thermal_t *printer, uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e) {
    timeoutWait(printer);
    uint8_t data[5] = {a, b, c, d, e};
    HAL_UART_Transmit(printer->huart, data, 5, HAL_MAX_DELAY);
    timeoutSet(printer, 5 * BYTE_TIME);
}

static void adjustCharValues(Adafruit_Thermal_t *printer, uint8_t printMode) {
    uint8_t charWidth;

    if (printMode & FONT_MASK) {
        printer->char_height = 17;
        charWidth = 9;
    } else {
        printer->char_height = 24;
        charWidth = 12;
    }

    if (printMode & DOUBLE_WIDTH_MASK) {
        printer->max_column /= 2;
        charWidth *= 2;
    }

    if (printMode & DOUBLE_HEIGHT_MASK) {
        printer->char_height *= 2;
    }

    printer->max_column = (384 / charWidth);
}

static void writePrintMode(Adafruit_Thermal_t *printer) {
    writeBytes3(printer, ASCII_ESC, '!', printer->print_mode);
}

static void setPrintMode(Adafruit_Thermal_t *printer, uint8_t mask) {
    printer->print_mode |= mask;
    writePrintMode(printer);
    adjustCharValues(printer, printer->print_mode);
}

static void unsetPrintMode(Adafruit_Thermal_t *printer, uint8_t mask) {
    printer->print_mode &= ~mask;
    writePrintMode(printer);
    adjustCharValues(printer, printer->print_mode);
}

// ==================== Public API Implementation ====================

void Adafruit_Thermal_Init(Adafruit_Thermal_t *printer,
                           UART_HandleTypeDef *huart,
                           GPIO_TypeDef *dtr_port,
                           uint16_t dtr_pin) {
    printer->huart = huart;
    printer->dtr_port = dtr_port;
    printer->dtr_pin = dtr_pin;
    printer->dtr_enabled = (dtr_port != NULL);
    printer->dot_print_time = 30000;
    printer->dot_feed_time = 2100;
    printer->max_chunk_height = 255;
    printer->firmware = 268;
    printer->print_mode = 0;
    printer->prev_byte = '\n';
    printer->column = 0;
    printer->max_column = 32;
    printer->char_height = 24;
    printer->line_spacing = 6;
    printer->barcode_height = 50;
    printer->rx_available = 0;
    printer->rx_buffer = 0;
}

void Adafruit_Thermal_Begin(Adafruit_Thermal_t *printer, uint16_t firmware_version) {
    printer->firmware = firmware_version;
    timeoutSet(printer, 500000L);
    Adafruit_Thermal_Wake(printer);
    Adafruit_Thermal_Reset(printer);
    Adafruit_Thermal_SetHeatConfig(printer, 11, 120, 40);

    // Enable DTR pin if requested
    if (printer->dtr_enabled) {
        writeBytes3(printer, ASCII_GS, 'a', (1 << 5));
    }

    printer->dot_print_time = 30000;
    printer->dot_feed_time = 2100;
    printer->max_chunk_height = 255;
}

void Adafruit_Thermal_Reset(Adafruit_Thermal_t *printer) {
    writeBytes2(printer, ASCII_ESC, '@');
    printer->prev_byte = '\n';
    printer->column = 0;
    printer->max_column = 32;
    printer->char_height = 24;
    printer->line_spacing = 6;
    printer->barcode_height = 50;

    if (printer->firmware >= 264) {
        writeBytes2(printer, ASCII_ESC, 'D');
        writeBytes5(printer, 4, 8, 12, 16, 20);
        writeBytes4(printer, 24, 28, 0, 0);
    }
}

void Adafruit_Thermal_SetDefault(Adafruit_Thermal_t *printer) {
    Adafruit_Thermal_Online(printer);
    Adafruit_Thermal_Justify(printer, 'L');
    Adafruit_Thermal_InverseOff(printer);
    Adafruit_Thermal_DoubleHeightOff(printer);
    Adafruit_Thermal_SetLineHeight(printer, 30);
    Adafruit_Thermal_BoldOff(printer);
    Adafruit_Thermal_UnderlineOff(printer);
    Adafruit_Thermal_SetBarcodeHeight(printer, 50);
    Adafruit_Thermal_SetSize(printer, 'S');
    Adafruit_Thermal_SetCharset(printer, 0);
    Adafruit_Thermal_SetCodePage(printer, 0);
}

void Adafruit_Thermal_Write(Adafruit_Thermal_t *printer, uint8_t c) {
    if (c != 13) { // Strip carriage returns
        timeoutWait(printer);
        HAL_UART_Transmit(printer->huart, &c, 1, HAL_MAX_DELAY);

        uint32_t d = BYTE_TIME;
        if ((c == '\n') || (printer->column == printer->max_column)) {
            d += (printer->prev_byte == '\n') ?
                ((printer->char_height + printer->line_spacing) * printer->dot_feed_time) :
                ((printer->char_height * printer->dot_print_time) +
                 (printer->line_spacing * printer->dot_feed_time));
            printer->column = 0;
            c = '\n';
        } else {
            printer->column++;
        }
        timeoutSet(printer, d);
        printer->prev_byte = c;
    }
}

void Adafruit_Thermal_Print(Adafruit_Thermal_t *printer, const char *str) {
    while (*str) {
        Adafruit_Thermal_Write(printer, (uint8_t)*str++);
    }
}

void Adafruit_Thermal_Println(Adafruit_Thermal_t *printer, const char *str) {
    Adafruit_Thermal_Print(printer, str);
    Adafruit_Thermal_Write(printer, '\n');
}

void Adafruit_Thermal_PrintInt(Adafruit_Thermal_t *printer, int32_t num) {
    char buffer[16];
    sprintf(buffer, "%ld", num);
    Adafruit_Thermal_Print(printer, buffer);
}

void Adafruit_Thermal_PrintlnInt(Adafruit_Thermal_t *printer, int32_t num) {
    char buffer[16];
    sprintf(buffer, "%ld", num);
    Adafruit_Thermal_Println(printer, buffer);
}

void Adafruit_Thermal_Feed(Adafruit_Thermal_t *printer, uint8_t lines) {
    if (printer->firmware >= 264) {
        writeBytes3(printer, ASCII_ESC, 'd', lines);
        timeoutSet(printer, printer->dot_feed_time * printer->char_height);
        printer->prev_byte = '\n';
        printer->column = 0;
    } else {
        while (lines--) {
            Adafruit_Thermal_Write(printer, '\n');
        }
    }
}

void Adafruit_Thermal_FeedRows(Adafruit_Thermal_t *printer, uint8_t rows) {
    writeBytes3(printer, ASCII_ESC, 'J', rows);
    timeoutSet(printer, rows * printer->dot_feed_time);
    printer->prev_byte = '\n';
    printer->column = 0;
}

void Adafruit_Thermal_Flush(Adafruit_Thermal_t *printer) {
    writeByte(printer, ASCII_FF);
}

void Adafruit_Thermal_Justify(Adafruit_Thermal_t *printer, char value) {
    uint8_t pos = 0;
    char upper = toupper(value);

    switch (upper) {
        case 'L': pos = 0; break;
        case 'C': pos = 1; break;
        case 'R': pos = 2; break;
    }

    writeBytes3(printer, ASCII_ESC, 'a', pos);
}

void Adafruit_Thermal_BoldOn(Adafruit_Thermal_t *printer) {
    setPrintMode(printer, BOLD_MASK);
}

void Adafruit_Thermal_BoldOff(Adafruit_Thermal_t *printer) {
    unsetPrintMode(printer, BOLD_MASK);
}

void Adafruit_Thermal_UnderlineOn(Adafruit_Thermal_t *printer, uint8_t weight) {
    if (weight > 2) weight = 2;
    writeBytes3(printer, ASCII_ESC, '-', weight);
}

void Adafruit_Thermal_UnderlineOff(Adafruit_Thermal_t *printer) {
    writeBytes3(printer, ASCII_ESC, '-', 0);
}

void Adafruit_Thermal_InverseOn(Adafruit_Thermal_t *printer) {
    if (printer->firmware >= 268) {
        writeBytes3(printer, ASCII_GS, 'B', 1);
    } else {
        setPrintMode(printer, INVERSE_MASK);
    }
}

void Adafruit_Thermal_InverseOff(Adafruit_Thermal_t *printer) {
    if (printer->firmware >= 268) {
        writeBytes3(printer, ASCII_GS, 'B', 0);
    } else {
        unsetPrintMode(printer, INVERSE_MASK);
    }
}

void Adafruit_Thermal_DoubleHeightOn(Adafruit_Thermal_t *printer) {
    setPrintMode(printer, DOUBLE_HEIGHT_MASK);
}

void Adafruit_Thermal_DoubleHeightOff(Adafruit_Thermal_t *printer) {
    unsetPrintMode(printer, DOUBLE_HEIGHT_MASK);
}

void Adafruit_Thermal_DoubleWidthOn(Adafruit_Thermal_t *printer) {
    setPrintMode(printer, DOUBLE_WIDTH_MASK);
}

void Adafruit_Thermal_DoubleWidthOff(Adafruit_Thermal_t *printer) {
    unsetPrintMode(printer, DOUBLE_WIDTH_MASK);
}

void Adafruit_Thermal_Normal(Adafruit_Thermal_t *printer) {
    printer->print_mode = 0;
    writePrintMode(printer);
}

void Adafruit_Thermal_SetSize(Adafruit_Thermal_t *printer, char value) {
    char upper = toupper(value);
    switch (upper) {
        default:
        case 'S':
            Adafruit_Thermal_DoubleWidthOff(printer);
            Adafruit_Thermal_DoubleHeightOff(printer);
            break;
        case 'M':
            Adafruit_Thermal_DoubleHeightOn(printer);
            Adafruit_Thermal_DoubleWidthOff(printer);
            break;
        case 'L':
            Adafruit_Thermal_DoubleHeightOn(printer);
            Adafruit_Thermal_DoubleWidthOn(printer);
            break;
    }
}

void Adafruit_Thermal_SetFont(Adafruit_Thermal_t *printer, char font) {
    char upper = toupper(font);
    if (upper == 'B') {
        setPrintMode(printer, FONT_MASK);
    } else {
        unsetPrintMode(printer, FONT_MASK);
    }
}

void Adafruit_Thermal_SetLineHeight(Adafruit_Thermal_t *printer, int height) {
    if (height < 24) height = 24;
    printer->line_spacing = height - 24;
    writeBytes3(printer, ASCII_ESC, '3', (uint8_t)height);
}

void Adafruit_Thermal_SetCharSpacing(Adafruit_Thermal_t *printer, int spacing) {
    writeBytes3(printer, ASCII_ESC, ' ', (uint8_t)spacing);
}

void Adafruit_Thermal_Tab(Adafruit_Thermal_t *printer) {
    writeByte(printer, ASCII_TAB);
    printer->column = (printer->column + 4) & 0b11111100;
}

void Adafruit_Thermal_SetBarcodeHeight(Adafruit_Thermal_t *printer, uint8_t height) {
    if (height < 1) height = 1;
    printer->barcode_height = height;
    writeBytes3(printer, ASCII_GS, 'h', height);
}

void Adafruit_Thermal_PrintBarcode(Adafruit_Thermal_t *printer, const char *text, uint8_t type) {
    Adafruit_Thermal_Feed(printer, 1);

    if (printer->firmware >= 264) type += 65;

    writeBytes3(printer, ASCII_GS, 'H', 2);
    writeBytes3(printer, ASCII_GS, 'w', 3);
    writeBytes3(printer, ASCII_GS, 'k', type);

    if (printer->firmware >= 264) {
        int len = strlen(text);
        if (len > 255) len = 255;
        writeByte(printer, (uint8_t)len);
        for (int i = 0; i < len; i++) {
            writeByte(printer, (uint8_t)text[i]);
        }
    } else {
        uint8_t i = 0;
        do {
            writeByte(printer, (uint8_t)text[i]);
        } while (text[i++]);
    }

    timeoutSet(printer, (printer->barcode_height + 40) * printer->dot_print_time);
    printer->prev_byte = '\n';
}

void Adafruit_Thermal_SetHeatConfig(Adafruit_Thermal_t *printer, uint8_t dots, uint8_t time, uint8_t interval) {
    writeBytes5(printer, ASCII_ESC, '7', dots, time, interval);
}

void Adafruit_Thermal_SetPrintDensity(Adafruit_Thermal_t *printer, uint8_t density, uint8_t breakTime) {
    writeBytes3(printer, ASCII_DC2, '#', (density << 5) | breakTime);
}

void Adafruit_Thermal_SetCharset(Adafruit_Thermal_t *printer, uint8_t val) {
    if (val > 15) val = 15;
    writeBytes3(printer, ASCII_ESC, 'R', val);
}

void Adafruit_Thermal_SetCodePage(Adafruit_Thermal_t *printer, uint8_t val) {
    if (val > 47) val = 47;
    writeBytes3(printer, ASCII_ESC, 't', val);
}

void Adafruit_Thermal_Wake(Adafruit_Thermal_t *printer) {
    timeoutSet(printer, 0);
    writeByte(printer, 255);
    HAL_Delay(50);

    if (printer->firmware >= 264) {
        writeBytes4(printer, ASCII_ESC, '8', 0, 0);
    } else {
        for (uint8_t i = 0; i < 10; i++) {
            writeByte(printer, 0);
            timeoutSet(printer, 10000L);
        }
    }
}

void Adafruit_Thermal_Sleep(Adafruit_Thermal_t *printer) {
    if (printer->firmware >= 264) {
        writeBytes4(printer, ASCII_ESC, '8', 1, 0);
    } else {
        writeBytes3(printer, ASCII_ESC, '8', 1);
    }
}

void Adafruit_Thermal_Offline(Adafruit_Thermal_t *printer) {
    writeBytes3(printer, ASCII_ESC, '=', 0);
}

void Adafruit_Thermal_Online(Adafruit_Thermal_t *printer) {
    writeBytes3(printer, ASCII_ESC, '=', 1);
}

bool Adafruit_Thermal_HasPaper(Adafruit_Thermal_t *printer) {
    if (printer->firmware >= 264) {
        writeBytes3(printer, ASCII_ESC, 'v', 0);
    } else {
        writeBytes3(printer, ASCII_GS, 'r', 0);
    }

    int status = -1;
    for (uint8_t i = 0; i < 10; i++) {
        if (printer->rx_available) {
            status = printer->rx_buffer;
            printer->rx_available = 0;
            break;
        }
        HAL_Delay(100);
    }

    return !(status & 0b00000100);
}

void Adafruit_Thermal_SetTimes(Adafruit_Thermal_t *printer, uint32_t print_time, uint32_t feed_time) {
    printer->dot_print_time = print_time;
    printer->dot_feed_time = feed_time;
}

void Adafruit_Thermal_SetMaxChunkHeight(Adafruit_Thermal_t *printer, int val) {
    printer->max_chunk_height = (uint8_t)val;
}

void Adafruit_Thermal_Test(Adafruit_Thermal_t *printer) {
    Adafruit_Thermal_Println(printer, "Hello World!");
    Adafruit_Thermal_Feed(printer, 2);
}

void Adafruit_Thermal_TestPage(Adafruit_Thermal_t *printer) {
    writeBytes2(printer, ASCII_DC2, 'T');
    timeoutSet(printer, printer->dot_print_time * 24 * 26 +
               printer->dot_feed_time * (6 * 26 + 30));
}

void Adafruit_Thermal_RxCallback(Adafruit_Thermal_t *printer, uint8_t data) {
    printer->rx_buffer = data;
    printer->rx_available = 1;
}
// Add this function to set left margin
void Adafruit_Thermal_SetLeftMargin(Adafruit_Thermal_t *printer, uint8_t margin) {
    // ESC 'l' n - Set left margin in dots
    // n is number of dots (max 384)
    writeBytes3(printer, ASCII_ESC, 'l', margin);
}
// Test sequence to find retract command



// Paper Return/Retract function (ESC K n)
void Adafruit_Thermal_PrintAndReturn(Adafruit_Thermal_t *printer, uint8_t n) {
    // ESC K n - Print data in buffer and return n vertical dots
    writeBytes3(printer, ASCII_ESC, ESC_K, n);
    timeoutSet(printer, n * printer->dot_feed_time);
    printer->prev_byte = '\n';
    printer->column = 0;
}

// Paper Feed function (ESC J n) - already have similar but add this version
void Adafruit_Thermal_PrintAndFeed(Adafruit_Thermal_t *printer, uint8_t n) {
    // ESC J n - Print data in buffer and feed n vertical dots
    writeBytes3(printer, ASCII_ESC, ESC_J, n);
    timeoutSet(printer, n * printer->dot_feed_time);
    printer->prev_byte = '\n';
    printer->column = 0;
}

// Full Cut (ESC i)
void Adafruit_Thermal_FullCut(Adafruit_Thermal_t *printer) {
    writeBytes2(printer, ASCII_ESC, ESC_i);
    timeoutSet(printer, 500000L);  // Wait 500ms for cut
}

// Half Cut (ESC m)
void Adafruit_Thermal_HalfCut(Adafruit_Thermal_t *printer) {
    writeBytes2(printer, ASCII_ESC, ESC_m);
    timeoutSet(printer, 500000L);
}

//void Adafruit_Thermal_PrintQRCode(Adafruit_Thermal_t *printer, const char *data) {
//    uint16_t len = strlen(data);
//
//    timeoutWait(printer);
//
//    // Select QR Model 2
//    uint8_t model[] = {0x1D, 0x28, 0x6B, 0x04, 0x00, 0x31, 0x41, 0x32, 0x00};
//    for (int i = 0; i < 9; i++) {
//        HAL_UART_Transmit(printer->huart, &model[i], 1, HAL_MAX_DELAY);
//    }
//
//    // Set QR module size (4 = medium)
//    uint8_t size[] = {0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x43, 0x04};
//    for (int i = 0; i < 8; i++) {
//        HAL_UART_Transmit(printer->huart, &size[i], 1, HAL_MAX_DELAY);
//    }
//
//    // Set error correction level (48 = L)
//    uint8_t error[] = {0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x45, 0x30};
//    for (int i = 0; i < 8; i++) {
//        HAL_UART_Transmit(printer->huart, &error[i], 1, HAL_MAX_DELAY);
//    }
//
//    // Store QR data
//    uint8_t dataLenL = (len + 3) & 0xFF;
//    uint8_t dataLenH = ((len + 3) >> 8) & 0xFF;
//
//    uint8_t store[] = {0x1D, 0x28, 0x6B, dataLenL, dataLenH, 0x31, 0x50, 0x30};
//    for (int i = 0; i < 8; i++) {
//        HAL_UART_Transmit(printer->huart, &store[i], 1, HAL_MAX_DELAY);
//    }
//
//    // Send the actual QR data
//    for (uint16_t i = 0; i < len; i++) {
//        HAL_UART_Transmit(printer->huart, (uint8_t*)&data[i], 1, HAL_MAX_DELAY);
//    }
//
//    // Print QR
//    uint8_t print[] = {0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x51, 0x30};
//    for (int i = 0; i < 8; i++) {
//        HAL_UART_Transmit(printer->huart, &print[i], 1, HAL_MAX_DELAY);
//    }
//
//    timeoutSet(printer, 500000L);  // Wait for QR to print
//    printer->prev_byte = '\n';
//}
void Adafruit_Thermal_PrintQRCodeWithSize(Adafruit_Thermal_t *printer, const char *data, uint8_t moduleSize) {
    uint16_t len = strlen(data);

    // Module size range: 1-9 (1 = smallest, 9 = largest)
    if (moduleSize < 1) moduleSize = 1;
    if (moduleSize > 9) moduleSize = 9;

    timeoutWait(printer);

    // Select QR Model 2
    uint8_t model[] = {0x1D, 0x28, 0x6B, 0x04, 0x00, 0x31, 0x41, 0x32, 0x00};
    for (int i = 0; i < 9; i++) {
        HAL_UART_Transmit(printer->huart, &model[i], 1, HAL_MAX_DELAY);
    }

    // Set QR module size (1-9, higher = bigger)
    uint8_t size[] = {0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x43, moduleSize};
    for (int i = 0; i < 8; i++) {
        HAL_UART_Transmit(printer->huart, &size[i], 1, HAL_MAX_DELAY);
    }

    // Set error correction level (48 = L, 49 = M, 50 = Q, 51 = H)
    uint8_t error[] = {0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x45, 0x30};
    for (int i = 0; i < 8; i++) {
        HAL_UART_Transmit(printer->huart, &error[i], 1, HAL_MAX_DELAY);
    }

    // Store QR data
    uint8_t dataLenL = (len + 3) & 0xFF;
    uint8_t dataLenH = ((len + 3) >> 8) & 0xFF;

    uint8_t store[] = {0x1D, 0x28, 0x6B, dataLenL, dataLenH, 0x31, 0x50, 0x30};
    for (int i = 0; i < 8; i++) {
        HAL_UART_Transmit(printer->huart, &store[i], 1, HAL_MAX_DELAY);
    }

    // Send the actual QR data
    for (uint16_t i = 0; i < len; i++) {
        HAL_UART_Transmit(printer->huart, (uint8_t*)&data[i], 1, HAL_MAX_DELAY);
    }

    // Print QR
    uint8_t print[] = {0x1D, 0x28, 0x6B, 0x03, 0x00, 0x31, 0x51, 0x30};
    for (int i = 0; i < 8; i++) {
        HAL_UART_Transmit(printer->huart, &print[i], 1, HAL_MAX_DELAY);
    }

    timeoutSet(printer, 500000L);
    printer->prev_byte = '\n';
}

// Default size (medium)
void Adafruit_Thermal_PrintQRCode(Adafruit_Thermal_t *printer, const char *data) {
    Adafruit_Thermal_PrintQRCodeWithSize(printer, data, 6);  // Size 6 = medium-large
}
