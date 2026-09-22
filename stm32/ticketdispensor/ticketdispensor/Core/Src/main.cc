/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "socket.h"
#include "wizchip_conf.h"
#include "loopback.h"
#include "w5500.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stm32f4xx_hal.h"
#include "sntp.h"
//#include "libescpos.h"

#include "Adafruit_Thermal_STM32.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
RTC_TimeTypeDef gTime;
RTC_DateTypeDef gDate;
#define SOCK_TCPS 0         							     // Socket 0
	#define PORT_TCPS 6000     								     // Listening port
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c3;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart5;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */
uint8_t socket_buf[100];     						// Buffer to receive data
uint8_t rcvBuf[20],bufSize[]={2,2,2,2};
int32_t len;										//Len of data recived
wiz_NetInfo getInfo ={ .mac={0},  					//Mac address
	  		  	  	   .ip ={0}, 					// IP address
	    			   .sn ={0},					//Subnet mask
	    			   .gw ={0}};					//Gateway address


uint8_t send_flag = 0;

uint8_t ntp_buf[48];
datetime now;

uint8_t ntp_server[4] = {129, 6, 15, 28}; // time.nist.gov
uint8_t time_synced = 0;


Adafruit_Thermal_t printer;

uint8_t RxData[10]={0};
uint8_t RxData2[50]={0};

//DIaplay data setting //
union {
    struct {
        unsigned Line_1_CLR:1;
        unsigned Line_2_CLR:1;
        unsigned Line_1_DEF:1;
        unsigned Line_2_DEF:1;
        unsigned Line_1_INZ:1;
        unsigned Line_2_INZ:1;
        unsigned Line_1_FIN:1;
        unsigned Line_2_FIN:1;
        unsigned PRNT_ON:1;
        unsigned PRNT_OFF:1;
        unsigned SER_ON:1;
        unsigned SER_OFF:1;
        unsigned TIME_UPDATE:1;
        unsigned BOOTH_UPDATE:1;
        unsigned VERSION_UPDATE:1;
        unsigned INVALID_CMD:1;
    } Flag;
    unsigned int Flags;
}Disp;

union {
    struct {
        unsigned LOOP_ENG:1;
        unsigned BUTTON_PRESSED:1;
        unsigned CARD_PUNCH:1;
        unsigned VALIED_CARD:1;
        unsigned INVALIED_CARD:1;
        unsigned TICKET_GENERATE:1;
        unsigned WAIT:1;
//        unsigned Line_2_DEF:1;
    } Flag;
    unsigned int Flags;
}Flg;

char def1[32]={0};
char inz1[32]={0};
char fin1[32]={0};
char def2[32]={0};
char inz2[32]={0};
char fin2[32]={0};

char Tik1[40]={0};
char Tik2[40]={0};
char Tik3[40]={0};
char Tik4[40]={0};
char Tik5[40]={0};
char Tik6[40]={0};
char Booth[40]={0};


static uint8_t prev_state_loop = 1; // assume initially HIGH
static uint8_t prev_state_Button =1;

RTC_TimeTypeDef Ticket_Time;
RTC_DateTypeDef Ticket_Date;

uint32_t Ticket_number;
static uint32_t delay = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C3_Init(void);
static void MX_SPI1_Init(void);
static void MX_UART4_Init(void);
static void MX_UART5_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void cs_sel(){
	 HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET);
}
void cs_desel(){
	HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);
}
uint8_t spi_rb(void){
	uint8_t rbuf;
	HAL_SPI_Receive(&hspi1,&rbuf,1,0xFFFFFFFF);
	return rbuf;
}
void spi_wb(uint8_t b){
	HAL_SPI_Transmit(&hspi1,&b,1,0xFFFFFFFF);
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
        uint8_t data;
        HAL_UART_Receive_IT(huart, &data, 1);
        Adafruit_Thermal_RxCallback(&printer, data);
    }
}

void PrintTicket(Adafruit_Thermal_t *printer,
                 uint32_t ticket_no,
                 RTC_TimeTypeDef *gTime,
                 RTC_DateTypeDef *gDate)
{
    Adafruit_Thermal_Feed(printer, 1);

    // ==========================================
    // HEADER
    // ==========================================
    Adafruit_Thermal_Justify(printer, 'C');

    Adafruit_Thermal_BoldOn(printer);

    Adafruit_Thermal_SetSize(printer, 'L');
    Adafruit_Thermal_Println(printer, Tik1);

    Adafruit_Thermal_SetSize(printer, 'L');
    Adafruit_Thermal_Println(printer, Tik2);

    Adafruit_Thermal_SetSize(printer, 'S');

    Adafruit_Thermal_Println(printer, "");0

    Adafruit_Thermal_BoldOff(printer);

    Adafruit_Thermal_SetFont(printer, 'A');

    Adafruit_Thermal_Println(printer, Tik3);
    Adafruit_Thermal_Println(printer, Tik4);
    Adafruit_Thermal_Println(printer, Tik5);
    Adafruit_Thermal_Println(printer, Tik6);

    Adafruit_Thermal_Println(printer, "");


    // ==========================================
    // CREATE BARCODE DATA
    // FORMAT:
    // BOOTH + YYMMDDHHMMSS + TICKET_NO
    // Example:
    // 0001240511103056000123
    // ==========================================

    char qr_data[64];

    sprintf(qr_data,
            "%s%02d%02d%02d%02d%02d%02d%06lu",
            Booth,
            gDate->Year,
            gDate->Month,
            gDate->Date,
            gTime->Hours,
            gTime->Minutes,
            gTime->Seconds,
            ticket_no);

    // ==========================================
    // PRINT QR
    // ==========================================

    Adafruit_Thermal_BoldOn(printer);

    Adafruit_Thermal_PrintQRCodeWithSize(printer,
                                         qr_data,
                                         7);

    Adafruit_Thermal_Println(printer, "");

//    Adafruit_Thermal_Println(printer, "");

    Adafruit_Thermal_Justify(printer, 'C');

    Adafruit_Thermal_Println(printer, qr_data);

    Adafruit_Thermal_Println(printer, "");


    Adafruit_Thermal_BoldOff(printer);


    // ==========================================
    // PRINT TICKET DETAILS
    // ==========================================

    char buffer[64];

    sprintf(buffer,
            "Ticket No : %06lu",
            ticket_no);

    Adafruit_Thermal_Println(printer, buffer);


    sprintf(buffer,
            "Date : %02d-%02d-20%02d",gDate->Date,gDate->Month,gDate->Year);
    Adafruit_Thermal_Println(printer, buffer);
    sprintf(buffer,"Entry Time : %02d:%02d:%02d",gTime->Hours,gTime->Minutes,gTime->Seconds);
    Adafruit_Thermal_Println(printer, buffer);
        Adafruit_Thermal_Feed(printer, 3);
        Adafruit_Thermal_Println(printer, "");
        Adafruit_Thermal_FullCut(printer);
}
//void PrintTicket(Adafruit_Thermal_t *printer,
//                           uint32_t ticket_no,
//                           const char* date,
//                           const char* entry_time,
//                           const char* barcode_data) {
//	 Adafruit_Thermal_Feed(printer, 1);
//    // Center everything
//    Adafruit_Thermal_Justify(printer, 'C');
//    Adafruit_Thermal_BoldOn(printer);
//    // Print ticket content
//    Adafruit_Thermal_SetSize(printer, 'L');
////    Adafruit_Thermal_Println(printer, "HOUSYS PARKING");
//    Adafruit_Thermal_Println(printer,Tik1);
//    Adafruit_Thermal_SetSize(printer, 'L');
////    Adafruit_Thermal_Println(printer, "WELCOME");
//    Adafruit_Thermal_Println(printer, Tik2);
//    Adafruit_Thermal_SetSize(printer, 'S');
//    Adafruit_Thermal_Println(printer, "");
//    Adafruit_Thermal_BoldOff(printer);
//    Adafruit_Thermal_SetFont(printer, 'A');
////    Adafruit_Thermal_Println(printer, "Please Keep this ticket before exit.");
//    Adafruit_Thermal_Println(printer, Tik3);
////    Adafruit_Thermal_Println(printer, "Lost ticket may result in penalty");
//    Adafruit_Thermal_Println(printer, Tik4);
////    Adafruit_Thermal_Println(printer, "");
////    Adafruit_Thermal_Println(printer, "For assistance, call: 0123456789");
//    Adafruit_Thermal_Println(printer, Tik5);
////    Adafruit_Thermal_Println(printer, "Enjoy your stay.");
//    Adafruit_Thermal_Println(printer,Tik6);
//    Adafruit_Thermal_Println(printer, "");
//    Adafruit_Thermal_BoldOn(printer);
//    char qr_data[100];
//	sprintf(qr_data,barcode_data );
//	Adafruit_Thermal_PrintQRCodeWithSize(printer, qr_data, 5);
//    Adafruit_Thermal_Println(printer, "");
//    Adafruit_Thermal_BoldOff(printer);
//    Adafruit_Thermal_SetFont(printer, 'A');
//
//    char buffer[50];
//    sprintf(buffer, "Ticket No    %lu", ticket_no);
//    Adafruit_Thermal_Println(printer, buffer);
//    sprintf(buffer, "Date    %s", date);
//    Adafruit_Thermal_Println(printer, buffer);
//    sprintf(buffer, "Entry Time    %s", entry_time);
//    Adafruit_Thermal_Println(printer, buffer);
//    Adafruit_Thermal_Println(printer, "");
//}
void EEPROM_WriteByte(uint16_t addr, uint8_t data)
{
    HAL_I2C_Mem_Write(&hi2c3,
                      0xA2,
                      addr,
                      I2C_MEMADD_SIZE_16BIT,
                      &data,
                      1,
                      100);

    HAL_Delay(5);
}
uint8_t EEPROM_ReadByte(uint16_t addr)
{
    uint8_t data = 0;

    HAL_I2C_Mem_Read(&hi2c3,
                     0xA2,
                     addr,
                     I2C_MEMADD_SIZE_16BIT,
                     &data,
                     1,
                     100);

    return data;
}
// Write string in format: [Length] + [String] + [NULL]
// len = length of string (max 64)
void EEPROM_WriteString(uint16_t startAddr, uint8_t len, const char* str)
{
	char data[] = " SAVED ";
					send(SOCK_TCPS,(uint8_t*)data,strlen(data));
    // Write length byte
    EEPROM_WriteByte(startAddr, len);

    // Write string characters
    for(uint8_t i = 0; i < len; i++)
    {
        EEPROM_WriteByte(startAddr + 1 + i, str[i]);
    }

    // Write NULL terminator
    EEPROM_WriteByte(startAddr + 1 + len, '\0');
}

// Read string from EEPROM
// Returns: length of string read
uint8_t EEPROM_ReadString(uint16_t startAddr, char* outputBuffer)
{
    uint8_t len;

    // Read the length byte
    len = EEPROM_ReadByte(startAddr);

//     Check if length is valid (max 64)
    if(len > 64)
    {
        outputBuffer[0] = '\0';
        return 0; // Error
    }

    // Read the string
    for(uint8_t i = 0; i < len; i++)
    {
        outputBuffer[i] = EEPROM_ReadByte(startAddr + 1 + i);
    }

    // Add NULL terminator to output buffer
    outputBuffer[len] = '\0';

    return len; // Return actual length
}
// Function to extract text between "LCD LINE X: " and "%END"
bool ExtractLCDText(char* socket_buf, uint8_t lineNum, char* outputBuffer, uint8_t maxLen)
{
    char searchPattern[20];
    char* startPtr = NULL;
    char* endPtr = NULL;

    // Create search pattern based on line number
    sprintf(searchPattern, "LCD LINE %d: ", lineNum);

    // Find the pattern in buffer
    startPtr = strstr(socket_buf, searchPattern);
    if(startPtr == NULL)
    {
        return false; // Pattern not found
    }

    startPtr += strlen(searchPattern); // Skip the pattern

    // Find "%END" marker
    endPtr = strstr(startPtr, "%END");
    if(endPtr == NULL)
    {
        return false; // %END not found
    }

    // Calculate length of text
    uint8_t len = endPtr - startPtr;
    if(len > maxLen - 1) len = maxLen - 1;

    // Copy text to output buffer
    uint8_t cleanLen = 0;
    for(uint8_t i = 0; i < len; i++)
    {
        // Skip any special characters
        if(startPtr[i] != '\r' && startPtr[i] != '\n')
        {
            outputBuffer[cleanLen++] = startPtr[i];
        }
    }
    outputBuffer[cleanLen] = '\0';

    return true;
}
typedef enum {
    IDLE,
    PROCESSING_DEFAULT,
    PROCESSING_INITIAL,
    PROCESSING_FINAL
} ParserState_t;

ParserState_t parserState = IDLE;

void ProcessSocketBuffer(char* socket_buf)
{
    char extractedText[65];

    // Auto-detect message type
    if(strstr(socket_buf, "LCD Default:") != NULL)
    {
        parserState = PROCESSING_DEFAULT;
    }
    else if(strstr(socket_buf, "Initial State:") != NULL)
    {
        parserState = PROCESSING_INITIAL;
    }
    else if(strstr(socket_buf, "Final State:") != NULL)
    {
        parserState = PROCESSING_FINAL;
    }

    // Process based on state
    switch(parserState)
    {
        case PROCESSING_DEFAULT:
            if(ExtractLCDText(socket_buf, 1, extractedText, 65))
            {
                EEPROM_WriteString(0x0000, strlen(extractedText), extractedText);
                printf("Saved Default Line 1: %s\n", extractedText);
            }
            if(ExtractLCDText(socket_buf, 2, extractedText, 65))
            {
                EEPROM_WriteString(0x0020, strlen(extractedText), extractedText);
                printf("Saved Default Line 2: %s\n", extractedText);
                parserState = IDLE; // Reset after LINE 2
            }
            break;

        case PROCESSING_INITIAL:
            if(ExtractLCDText(socket_buf, 1, extractedText, 65))
            {
                EEPROM_WriteString(0x0040, strlen(extractedText), extractedText);
                printf("Saved Initial Line 1: %s\n", extractedText);
            }
            if(ExtractLCDText(socket_buf, 2, extractedText, 65))
            {
                EEPROM_WriteString(0x0060, strlen(extractedText), extractedText);
                printf("Saved Initial Line 2: %s\n", extractedText);
                parserState = IDLE;
            }
            break;

        case PROCESSING_FINAL:
            if(ExtractLCDText(socket_buf, 1, extractedText, 65))
            {
                EEPROM_WriteString(0x0080, strlen(extractedText), extractedText);
                printf("Saved Final Line 1: %s\n", extractedText);
            }
            if(ExtractLCDText(socket_buf, 2, extractedText, 65))
            {
                EEPROM_WriteString(0x00A0, strlen(extractedText), extractedText);
                printf("Saved Final Line 2: %s\n", extractedText);
                parserState = IDLE;
            }
            break;

        default:
            break;
    }
}
#define ADDR_LINE1   0x00C8
#define ADDR_LINE2   0x00F0
#define ADDR_LINE3   0x0118
#define ADDR_LINE4   0x0140
#define ADDR_LINE5   0x0168
#define ADDR_LINE6   0x0190
#define ADDR_BOOTH   0x01B8
void process_Ticket_buffer(char *socket_buf)
{
    char *ptr = socket_buf;

    while ((ptr = strstr(ptr, "%END")) != NULL)
    {
        char frame[160] = {0};
        char extractedText[160] = {0};

        // -----------------------------------
        // Find frame start
        char *start = ptr;

        while (start > socket_buf && *(start - 1) != '\n')
            start--;

        int len = ptr - start;

        if (len <= 0 || len >= sizeof(frame))
        {
            ptr += 4;
            continue;
        }

        strncpy(frame, start, len);
        frame[len] = '\0';

        ptr += 4; // skip %END

        // ===================================
        // TICKET FRAME
        // ===================================
        if (strncmp(frame, "TICKET:", 7) == 0)
        {
            char *p = frame + 7;

            while (*p == ' ') p++;

            // check LINE
            if (strncmp(p, "LINE", 4) == 0)
            {
                p += 4;

                while (*p == ' ') p++;

                // Get line number
                int lineNo = atoi(p);

                // move to :
                while (*p && *p != ':')
                    p++;

                if (*p == ':')
                    p++;

                while (*p == ' ')
                    p++;

                strcpy(extractedText, p);

                // -----------------------------------
                // SAVE DIFFERENT LINE TO DIFFERENT ADDRESS
                switch (lineNo)
                {
                    case 1:
                        EEPROM_WriteString(ADDR_LINE1,
                                           strlen(extractedText),
                                           extractedText);
                        break;

                    case 2:
                        EEPROM_WriteString(ADDR_LINE2,
                                           strlen(extractedText),
                                           extractedText);
                        break;

                    case 3:
                        EEPROM_WriteString(ADDR_LINE3,
                                           strlen(extractedText),
                                           extractedText);
                        break;

                    case 4:
                        EEPROM_WriteString(ADDR_LINE4,
                                           strlen(extractedText),
                                           extractedText);
                        break;

                    case 5:
                        EEPROM_WriteString(ADDR_LINE5,
                                           strlen(extractedText),
                                           extractedText);
                        break;

                    case 6:
                        EEPROM_WriteString(ADDR_LINE6,
                                           strlen(extractedText),
                                           extractedText);
                        break;

                    default:
                        break;
                }
            }
        }

        // ===================================
        // BOOTH FRAME
        // ===================================
        else if (strncmp(frame, "BOOTH:", 6) == 0)
        {
            char *p = frame + 6;

            while (*p == ' ')
                p++;

            strcpy(extractedText, p);

            EEPROM_WriteString(ADDR_BOOTH,
                               strlen(extractedText),
                               extractedText);
        }
    }
}
void read_dis_settings(void)
{
	uint8_t length=0;
	 length = EEPROM_ReadString(0x0000, def1);
	 length = EEPROM_ReadString(0x0020, def2);
	 length = EEPROM_ReadString(0x0040, inz1);
	 length = EEPROM_ReadString(0x0060, inz2);
	 length = EEPROM_ReadString(0x0080, fin1);
	 length = EEPROM_ReadString(0x00A0, fin2);

	 length = EEPROM_ReadString(ADDR_LINE1, Tik1);
	 length = EEPROM_ReadString(ADDR_LINE2, Tik2);
	 length = EEPROM_ReadString(ADDR_LINE3, Tik3);
	 length = EEPROM_ReadString(ADDR_LINE4, Tik4);
	 length = EEPROM_ReadString(ADDR_LINE5, Tik5);
	 length = EEPROM_ReadString(ADDR_LINE6, Tik6);
	 length = EEPROM_ReadString(ADDR_BOOTH, Booth);

}
void Display_Task(void)
{
	if(Disp.Flag.Line_1_CLR)
	{
		Disp.Flag.Line_1_CLR=0;
		const uint8_t line1clr[]={0x5A,0xA5,0x17,0x82,0x10,0x00,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20};
		HAL_UART_Transmit(&huart3,line1clr,26,HAL_MAX_DELAY);
	}
	if(Disp.Flag.Line_2_CLR)
	{
		Disp.Flag.Line_2_CLR=0;
		const uint8_t line2clr[]={0x5A,0xA5,0x17,0x82,0x20,0x00,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20};
		HAL_UART_Transmit(&huart3,line2clr,26,HAL_MAX_DELAY);
	}
	if(Disp.Flag.Line_1_DEF)
	{
		Disp.Flag.Line_1_DEF=0;
		char len=strlen(def1);
		char total = 20;
		char leftPad = (total - len) / 2;
		char rightPad = total - len - leftPad;
		uint8_t txBuf[38] = {
		    0x5A,0xA5,0x17,0x82,0x10,0x00
		};
		int i = 6;
		// left spaces
		for(int j = 0; j < leftPad; j++)txBuf[i++] = 0x20;
		// string
		for(int j = 0; j < len; j++)txBuf[i++] = def1[j];
		// right spaces
		for(int j = 0; j < rightPad; j++)txBuf[i++] = 0x20;
		HAL_UART_Transmit(&huart3, txBuf, 26, HAL_MAX_DELAY);
	}
	if(Disp.Flag.Line_2_DEF)
		{
			Disp.Flag.Line_2_DEF=0;
			char len=strlen(def2);
			char total = 20;
			char leftPad = (total - len) / 2;
			char rightPad = total - len - leftPad;
			uint8_t txBuf[38] = {
			    0x5A,0xA5,0x17,0x82,0x20,0x00
			};
			int i = 6;
			// left spaces
			for(int j = 0; j < leftPad; j++)txBuf[i++] = 0x20;
			// string
			for(int j = 0; j < len; j++)txBuf[i++] = def2[j];
			// right spaces
			for(int j = 0; j < rightPad; j++)txBuf[i++] = 0x20;
			HAL_UART_Transmit(&huart3, txBuf, 26, HAL_MAX_DELAY);
		}
	if(Disp.Flag.Line_1_INZ)
	{
		Disp.Flag.Line_1_INZ=0;
		char len=strlen(inz1);
		char total = 20;
		char leftPad = (total - len) / 2;
		char rightPad = total - len - leftPad;
		uint8_t txBuf[38] = {
		    0x5A,0xA5,0x17,0x82,0x10,0x00
		};
		int i = 6;
		// left spaces
		for(int j = 0; j < leftPad; j++)txBuf[i++] = 0x20;
		// string
		for(int j = 0; j < len; j++)txBuf[i++] = inz1[j];
		// right spaces
		for(int j = 0; j < rightPad; j++)txBuf[i++] = 0x20;
		HAL_UART_Transmit(&huart3, txBuf, 26, HAL_MAX_DELAY);
	}
	if(Disp.Flag.Line_2_INZ)
		{
			Disp.Flag.Line_2_INZ=0;
			char len=strlen(inz2);
			char total = 20;
			char leftPad = (total - len) / 2;
			char rightPad = total - len - leftPad;
			uint8_t txBuf[38] = {
			    0x5A,0xA5,0x17,0x82,0x20,0x00
			};
			int i = 6;
			// left spaces
			for(int j = 0; j < leftPad; j++)txBuf[i++] = 0x20;
			// string
			for(int j = 0; j < len; j++)txBuf[i++] = inz2[j];
			// right spaces
			for(int j = 0; j < rightPad; j++)txBuf[i++] = 0x20;
			HAL_UART_Transmit(&huart3, txBuf, 26, HAL_MAX_DELAY);
		}
	if(Disp.Flag.Line_1_FIN)
	{
		Disp.Flag.Line_1_FIN=0;
		char len=strlen(fin1);
		char total = 20;
		char leftPad = (total - len) / 2;
		char rightPad = total - len - leftPad;
		uint8_t txBuf[38] = {
		    0x5A,0xA5,0x17,0x82,0x10,0x00
		};
		int i = 6;
		// left spaces
		for(int j = 0; j < leftPad; j++)txBuf[i++] = 0x20;
		// string
		for(int j = 0; j < len; j++)txBuf[i++] = fin1[j];
		// right spaces
		for(int j = 0; j < rightPad; j++)txBuf[i++] = 0x20;
		HAL_UART_Transmit(&huart3, txBuf, 26, HAL_MAX_DELAY);
	}
	if(Disp.Flag.Line_2_FIN)
		{
			Disp.Flag.Line_2_FIN=0;
			char len=strlen(fin2);
			char total = 20;
			char leftPad = (total - len) / 2;
			char rightPad = total - len - leftPad;
			uint8_t txBuf[38] = {
			    0x5A,0xA5,0x17,0x82,0x20,0x00
			};
			int i = 6;
			// left spaces
			for(int j = 0; j < leftPad; j++)txBuf[i++] = 0x20;
			// string
			for(int j = 0; j < len; j++)txBuf[i++] = fin2[j];
			// right spaces
			for(int j = 0; j < rightPad; j++)txBuf[i++] = 0x20;
			HAL_UART_Transmit(&huart3, txBuf, 26, HAL_MAX_DELAY);
		}
	if(Disp.Flag.PRNT_ON)
	{
		Disp.Flag.PRNT_ON=0;
		uint8_t txBuf[10]={0x5A,0xA5,0x05,0x82,0x30,0x00,0x00,0x01};
		HAL_UART_Transmit(&huart3, txBuf, 8, HAL_MAX_DELAY);
	}
	if(Disp.Flag.PRNT_OFF)
	{
		Disp.Flag.PRNT_OFF=0;
		uint8_t txBuf[10]={0x5A,0xA5,0x05,0x82,0x30,0x00,0x00,0x00};
		HAL_UART_Transmit(&huart3, txBuf, 8, HAL_MAX_DELAY);
	}
	if(Disp.Flag.SER_ON)
	{
		Disp.Flag.SER_ON=0;
		uint8_t txBuf[10]={0x5A,0xA5,0x05,0x82,0x31,0x00,0x00,0x01};
		HAL_UART_Transmit(&huart3, txBuf, 8, HAL_MAX_DELAY);
	}
	if(Disp.Flag.SER_OFF)
	{
		Disp.Flag.SER_OFF=0;
		uint8_t txBuf[10]={0x5A,0xA5,0x05,0x82,0x31,0x00,0x00,0x00};
		HAL_UART_Transmit(&huart3, txBuf, 8, HAL_MAX_DELAY);
	}
	if(Disp.Flag.BOOTH_UPDATE)
	{
		Disp.Flag.BOOTH_UPDATE=0;
		uint8_t txBuf[15]={0x5A,0xA5,0x08,0x82,0x40,0x00,0x34,0x30,0x30,0x33,0x2E};
		HAL_UART_Transmit(&huart3, txBuf, 11, HAL_MAX_DELAY);
	}
	if(Disp.Flag.VERSION_UPDATE)
	{
		Disp.Flag.VERSION_UPDATE=0;
		uint8_t txBuf[15]={0x5A,0xA5,0x08,0x82,0x50,0x00,0x30,0x2E,0x30,0x2E,0x33};
		HAL_UART_Transmit(&huart3, txBuf, 11, HAL_MAX_DELAY);
	}
//	const uint8_t date_clr[]={0x5A,0xA5,0x17,0x82,0x45,0x00,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20};
//	HAL_UART_Transmit(&huart3,date_clr,26,HAL_MAX_DELAY);
	static uint8_t prevSecond = 255;
	 HAL_RTC_GetTime(&hrtc, &gTime, RTC_FORMAT_BIN);
     HAL_RTC_GetDate(&hrtc, &gDate, RTC_FORMAT_BIN);
	if (gTime.Seconds != prevSecond)
	{
	    prevSecond = gTime.Seconds;
	    uint8_t time[26] = {0x5A,0xA5,0x17,0x82,0x45,0x00};
	    sprintf((char *)&time[6],"20%02d/%02d/%02d %02d:%02d:%02d ",gDate.Year,gDate.Month,gDate.Date,gTime.Hours,gTime.Minutes,gTime.Seconds);
	    HAL_UART_Transmit(&huart3, time, 26,HAL_MAX_DELAY);
	}
}
// Function 1: Debounced GPIO read using structure
typedef struct
{
    uint32_t lastTime;
    GPIO_PinState stableState;
    GPIO_PinState lastReading;
    uint16_t debounceTime;
} GPIO_Debounce_t;
GPIO_PinState ReadPinDebounced(GPIO_TypeDef* GPIOx,uint16_t GPIO_Pin,GPIO_Debounce_t* deb)
{
    GPIO_PinState reading =HAL_GPIO_ReadPin(GPIOx, GPIO_Pin);
    uint32_t now = HAL_GetTick();
    if (reading != deb->lastReading)
    {
        deb->lastTime = now;
        deb->lastReading = reading;
    }
    if ((now - deb->lastTime) >= deb->debounceTime)
    {
        deb->stableState = reading;
    }
    return deb->stableState;
}
GPIO_Debounce_t di2 = {
    .lastTime     = 0,
    .stableState  = GPIO_PIN_SET,
    .lastReading  = GPIO_PIN_SET,
    .debounceTime = 500
};

GPIO_Debounce_t di3 = {
    .lastTime     = 0,
    .stableState  = GPIO_PIN_SET,
    .lastReading  = GPIO_PIN_SET,
    .debounceTime = 200
};
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C3_Init();
  MX_SPI1_Init();
  MX_UART4_Init();
  MX_UART5_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_USART6_UART_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
  reg_wizchip_cs_cbfunc(cs_sel,cs_desel);
  reg_wizchip_spi_cbfunc(spi_rb,spi_wb);
  wizchip_init(bufSize,bufSize);
  wiz_NetInfo netInfo ={ .mac={0x0a,0x09,0xdc,0xab,0xcd,0xef}, //Mac address
		  	  	  	 	  .ip ={192,168,1,132}, 					// IP address
  						  .sn ={255,255,255,0},					//Subnet mask
  						  .gw ={192,168,1,1}};					//Gateway address
  HAL_Delay(500);
  wizchip_setnetinfo(&netInfo);
  HAL_Delay(500);
  wizchip_getnetinfo(&getInfo);
  SNTP_init(2, ntp_server, 34, ntp_buf);   // socket 2, India timezone
  read_dis_settings();
  // Initialize printer
      HAL_UART_Receive_IT(&huart6,RxData,10);
      HAL_UARTEx_ReceiveToIdle_IT(&huart2,RxData2,50);
      Adafruit_Thermal_Init(&printer, &huart6, NULL, 0); // No DTR pin
      // Start printer (firmware version 2.68)
      Adafruit_Thermal_Begin(&printer, 268);
      Adafruit_Thermal_SetTimes(&printer, 0, 0);
      Adafruit_Thermal_SetDefault(&printer);
      HAL_Delay(1000);
//      / Example ticket data
      uint32_t ticket_number = 512;
//      char date_str[] = "06/04/2026";
//      char time_str[] = "17:10";
//      char barcode_str[] = "TICKET:0000512|BOOTH:0001|DATE:06/04/2026|TIME:17:10";
      HAL_Delay(2000);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

//  char myString[] = "Vehicleinfo:UP12345678|TICKET:0000005|DATE:24/02/2026|TIME:14:22|AMOUNT:150";
//      char readBuffer[200];


	socket(SOCK_TCPS, Sn_MR_TCP, PORT_TCPS, 0); 				 // Open TCP socket
	listen(SOCK_TCPS);


//	HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0);
	Ticket_number = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0);

	Disp.Flag.Line_1_CLR=1;
	Disp.Flag.Line_2_CLR=1;
	Disp.Flag.Line_1_DEF=1;
	Disp.Flag.Line_2_DEF=1;
//	Disp.Flag.Line_1_INZ=1;
//	Disp.Flag.Line_2_INZ=1;
//	Disp.Flag.Line_1_FIN=1;
//	Disp.Flag.Line_2_FIN=1;
	Disp.Flag.BOOTH_UPDATE=1;
	Disp.Flag.VERSION_UPDATE=1;
	Disp.Flag.PRNT_ON=1;
	Disp.Flag.SER_ON=1;
//#define SOCK_TCPC  1
//#define PORT_TCPC  5000   // local port (can be anything)
//
//uint8_t destip[4] = {192, 168, 1, 34};  // server IP
//uint16_t destport = 4000;                // server port
  while (1)
  {
	  // Read inputs and control output
//	  HAL_RTC_GetTime(&hrtc, &gTime, RTC_FORMAT_BIN);
//	  HAL_RTC_GetDate(&hrtc, &gDate, RTC_FORMAT_BIN);
	  if (!time_synced)
	  {
	      if (SNTP_run(&now))
	      {
	          time_synced = 1;
	          printf("Time Synced!\r\n");
	          printf("%02d-%02d-%04d %02d:%02d:%02d\r\n",now.dd,now.mo,now.yy,now.hh,now.mm,now.ss);
	          RTC_TimeTypeDef sTime = {0};
	          RTC_DateTypeDef sDate = {0};
	          // TIME
	          sTime.Hours   = now.hh;
	          sTime.Minutes = now.mm;
	          sTime.Seconds = now.ss;
	          HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
	          // DATE
	          sDate.Date  = now.dd;
	          sDate.Month = now.mo;
	          // RTC stores only last 2 digits
	          sDate.Year = now.yy - 2000;
	          // optional weekday
	          sDate.WeekDay = RTC_WEEKDAY_MONDAY;
	          HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
	          printf("RTC Updated!\r\n");
	      }
	  }
	   uint8_t phy = getPHYCFGR();
	   if ((phy & 0x01) == 0) {
		   // Link is down
		   close(SOCK_TCPS);
		   socket(SOCK_TCPS, Sn_MR_TCP, PORT_TCPS, 0);
		   listen(SOCK_TCPS);
	   }
	  switch (getSn_SR(SOCK_TCPS))
	  {
		  case SOCK_ESTABLISHED:
		  {
			  if (getSn_IR(SOCK_TCPS) & Sn_IR_CON)
			  {
				  setSn_IR(SOCK_TCPS, Sn_IR_CON); 			// Clear connection interrupt
				  printf("Client connected\r\n");
			  }
			  // Check for received data
			  if ((len = getSn_RX_RSR(SOCK_TCPS)) > 0) {
				  len = recv(SOCK_TCPS, socket_buf, len);  // Receive data
				  socket_buf[len] = 0; // Null terminate
				  printf("Received: %s\r\n", socket_buf);
				  //procesz received data
				    ProcessSocketBuffer((char*)socket_buf);
				    // Clear buffer after processing
				    process_Ticket_buffer((char*)socket_buf);
				    if( Flg.Flag.LOOP_ENG)
				    {
						if (strstr((char*)socket_buf, "OPEN_GATE") != NULL)
						{
							Flg.Flag.VALIED_CARD= 1;
							Flg.Flag.TICKET_GENERATE=1;
//							Flg.Flag.CARD_PUNCH=0;
						}
						else if (strstr((char*)socket_buf, "INVALID_CARD") != NULL)
						{
							Flg.Flag.INVALIED_CARD = 1;
//							Flg.Flag.CARD_PUNCH=0;
							Flg.Flag.WAIT=0;
						}
				    }
				    memset(socket_buf, 0, sizeof(socket_buf));
//				  send(SOCK_TCPS, socket_buf, len);
			  }
			  break;
		  }
		  case SOCK_CLOSE_WAIT:
		  {
			  close(SOCK_TCPS);
			  socket(SOCK_TCPS, Sn_MR_TCP, PORT_TCPS, 0);
			  listen(SOCK_TCPS);
			  break;
		  }

		  case SOCK_CLOSED:
		  {
			  socket(SOCK_TCPS, Sn_MR_TCP, PORT_TCPS, 0);
			  listen(SOCK_TCPS);
			  break;
		  }
		  default:
		  {
			  break;
		  }
	  }
//
//	  switch (getSn_SR(SOCK_TCPC))
//	  {
//	      case SOCK_CLOSED:
//	      {
//	          socket(SOCK_TCPC, Sn_MR_TCP, PORT_TCPC, 0);
//	          break;
//	      }
//	      case SOCK_INIT:
//	      {
//	          connect(SOCK_TCPC, destip, destport);
//	          break;
//	      }
//	      case SOCK_ESTABLISHED:
//	      {
//	          if (getSn_IR(SOCK_TCPC) & Sn_IR_CON)
//	          {
//	              setSn_IR(SOCK_TCPC, Sn_IR_CON);
//	              printf("Connected to server\r\n");
//	          }
//
//	          // Send data (example)
//	          char msg[] = "Hello from client";
//	          send(SOCK_TCPC, (uint8_t*)msg, sizeof(msg) - 1);
//
//	          // Receive data
//	          if (getSn_RX_RSR(SOCK_TCPC) > 0)
//	          {
//	              int len = recv(SOCK_TCPC, socket_buf, sizeof(socket_buf));
//	              socket_buf[len] = 0;
//	              printf("Server says: %s\r\n", socket_buf);
//	          }
//	          break;
//	      }
//	      case SOCK_CLOSE_WAIT:
//	      {
//	          close(SOCK_TCPC);
//	          break;
//	      }
//	      default:
//	      {
//	          break;
//	      }
//	  }
//	  if (!time_synced)
//	  {
//	      if (SNTP_run(&now))
//	      {
//	          time_synced = 1;
//	          printf("Time Synced!\r\n");
//	          printf("%02d-%02d-%04d %02d:%02d:%02d\r\n",now.dd, now.mo, now.yy,now.hh, now.mm, now.ss);
//	      }
//	  }

	   GPIO_PinState current_loop = ReadPinDebounced(DI2_GPIO_Port, DI2_Pin, &di2);
	  // HIGH -> LOW
	    if (current_loop == GPIO_PIN_RESET && prev_state_loop == GPIO_PIN_SET)
	    {
	        char data[] = " Loop engaged ";
	        send(SOCK_TCPS,(uint8_t*)data,strlen(data));
	        Disp.Flag.Line_1_INZ = 1;
	        Disp.Flag.Line_2_INZ = 1;
	        Flg.Flag.LOOP_ENG=1;
	    }
	    // LOW -> HIGH
	    else if (current_loop == GPIO_PIN_SET && prev_state_loop == GPIO_PIN_RESET)
	    {
	        Disp.Flag.Line_1_DEF = 1;
	        Disp.Flag.Line_2_DEF = 1;
	        Flg.Flag.LOOP_ENG=0;
	    }
	    prev_state_loop = current_loop;

		if( Flg.Flag.LOOP_ENG)
		{
		   GPIO_PinState current_Button = ReadPinDebounced(DI3_GPIO_Port, DI3_Pin, &di3);
		  // HIGH -> LOW
				if (current_Button == GPIO_PIN_RESET && prev_state_Button == GPIO_PIN_SET)
				{
					char data[] = " BUTTON PRESSED ";
					send(SOCK_TCPS,(uint8_t*)data,strlen(data));
					if(!Flg.Flag.WAIT)
						{	Flg.Flag.BUTTON_PRESSED=1;
							Flg.Flag.WAIT=1;
						}
				}
				// LOW -> HIGH
				else if (current_Button == GPIO_PIN_SET && prev_state_Button == GPIO_PIN_RESET)
				{

				}
				prev_state_Button = current_Button;
			}
		if(Flg.Flag.LOOP_ENG && Flg.Flag.BUTTON_PRESSED )
		{
			if(Flg.Flag.TICKET_GENERATE==0)Flg.Flag.TICKET_GENERATE=1;

		}
		if(Flg.Flag.LOOP_ENG && Flg.Flag.CARD_PUNCH )
		{
			 Flg.Flag.CARD_PUNCH=0;
			 char data[64];
			 sprintf(data,"CARD: %d",RxData2);
			 send(SOCK_TCPS,(uint8_t*)data,strlen(data));
			  memset(RxData2, 0, sizeof(RxData2));
		}
		if(Flg.Flag.TICKET_GENERATE)
			{
	// Print actual ticket with return logic
			  Ticket_number++;
			  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, Ticket_number);
			  HAL_RTC_GetTime(&hrtc, &gTime, RTC_FORMAT_BIN);
			  HAL_RTC_GetDate(&hrtc, &gDate, RTC_FORMAT_BIN);
			  Ticket_Time=gTime;
			  Ticket_Date=gDate;
			  PrintTicket(&printer,Ticket_number ,&Ticket_Time,&Ticket_Date);
			  char data[64];
			  sprintf(data,
			             "ENTRY: %s%02d%02d%02d%02d%02d%02d%06lu ",
			             Booth,
						 Ticket_Date.Year,
						 Ticket_Date.Month,
						 Ticket_Date.Date,
						 Ticket_Time.Hours,
						 Ticket_Time.Minutes,
						 Ticket_Time.Seconds,
						 Ticket_number);
			  send(SOCK_TCPS,(uint8_t*)data,strlen(data));
				Disp.Flag.Line_1_FIN=1;
				Disp.Flag.Line_2_FIN=1;
				 HAL_GPIO_WritePin(RL5_GPIO_Port, RL5_Pin, GPIO_PIN_SET);
				 delay = HAL_GetTick();   // start timer
				char data1[] = " GATE OPEN ";
				send(SOCK_TCPS,(uint8_t*)data1,strlen(data1));
				Flg.Flag.TICKET_GENERATE=0;
				Flg.Flag.BUTTON_PRESSED=0;
				Flg.Flag.VALIED_CARD=0;
				Flg.Flag.WAIT=0;
	}
		if (delay && (HAL_GetTick() - delay) >= 2000)
		{
			HAL_GPIO_WritePin(RL5_GPIO_Port, RL5_Pin, GPIO_PIN_RESET);

//			char data[] = "GATE CLOSED";
//			send(SOCK_TCPS, (uint8_t*)data, strlen(data));

			delay = 0; // reset
		}
	    Display_Task();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 360;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLRCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.ClockSpeed = 400000;
  hi2c3.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x10;
  sTime.Minutes = 0x10;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_TUESDAY;
  sDate.Month = RTC_MONTH_MAY;
  sDate.Date = 0x5;
  sDate.Year = 0x26;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

}

/**
  * @brief UART5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART5_Init(void)
{

  /* USER CODE BEGIN UART5_Init 0 */

  /* USER CODE END UART5_Init 0 */

  /* USER CODE BEGIN UART5_Init 1 */

  /* USER CODE END UART5_Init 1 */
  huart5.Instance = UART5;
  huart5.Init.BaudRate = 115200;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART5_Init 2 */

  /* USER CODE END UART5_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 38400;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, XA_Pin|XB_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(RL5_GPIO_Port, RL5_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : DI1_Pin DI2_Pin */
  GPIO_InitStruct.Pin = DI1_Pin|DI2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : DI3_Pin */
  GPIO_InitStruct.Pin = DI3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(DI3_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SPI1_CS_Pin */
  GPIO_InitStruct.Pin = SPI1_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SPI1_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SPI1_INT_Pin */
  GPIO_InitStruct.Pin = SPI1_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SPI1_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : XA_Pin XB_Pin */
  GPIO_InitStruct.Pin = XA_Pin|XB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : RL5_Pin */
  GPIO_InitStruct.Pin = RL5_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RL5_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{

	if(huart->Instance==USART2)
	{
		uint8_t RxLen = Size;
		if(RxLen>4 && Flg.Flag.LOOP_ENG && !Flg.Flag.WAIT)
			{
			Flg.Flag.CARD_PUNCH=1;
			Flg.Flag.WAIT=1;
			}
		HAL_UARTEx_ReceiveToIdle_IT(&huart2,RxData2,50);
	}

}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
