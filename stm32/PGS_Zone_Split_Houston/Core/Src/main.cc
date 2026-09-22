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
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <cstring>
#include "LiquidCrystal_I2C_STM32.h"
#include "Read_key.h"
#include "Settings.h"
#include "AT24C512.h"
#include "HWprofile.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c3;
DMA_HandleTypeDef hdma_i2c1_tx;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
char buf[20];
static uint32_t last_update = 0;
int cnt=0;

unsigned char  txbuffer[4];

#define MAX_SENSORS 99			//sensor max
#define MAX_DATA_LEN 99*4		//because each sensor have 4 byte length 50*4=200 +50 for noise data

typedef struct {
    unsigned char device_id;
    unsigned char data;
} SensorData;
//struct typedef for device id and data
extern char Zone_ID,Number_of_display,Total_Sensor;

SensorData sensors[MAX_SENSORS]; 	// array to save device id with data
unsigned char RxData6[MAX_DATA_LEN] = {};
unsigned char RxData[20] = {};
unsigned char Zone_rx[20] = {};
unsigned char Zone_tx[250] = {};
volatile uint16_t rx6_wr  = 0;   // write index (rolls)
volatile uint16_t rx6_len = 0;   // last packet length

char device_id=0,sensor_index=0;
char timer1=0,flag1=0,timer2=0,flag2=0;
int eng = 0;
int diseng = 0;
int error = 0;
int nocom = 0;
int total=0;

char flg=0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C3_Init(void);
static void MX_TIM3_Init(void);
static void MX_UART4_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
LiquidCrystal_I2C lcd(&hi2c1, 0x27, 20, 4);
#define US_TO_CYC(us) ((us) * (SystemCoreClock / 1000000))
#define DWT_NOW()     (DWT->CYCCNT)
#define DWT_ELAPSED(start) (DWT->CYCCNT - (start))

void DWT_Init(void)
{
    // Enable TRC
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    // Reset the cycle counter
    DWT->CYCCNT = 0;
    // Enable the cycle counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
uint16_t calculate_diseng(uint8_t start, uint8_t stop)
{
    uint16_t cnt = 0;

    for (uint8_t i = start; i <= stop; i++)
    {
        if (sensors[i].data == 0x02)  // DISENG
            cnt++;
    }
    return cnt;
}

void build_tx_packet(uint8_t device_id)
{
    uint8_t sum = 3;        // initial sum for XOR checksum
    txbuffer[0] = 0xFA;     // Fixed header
    txbuffer[1] = device_id; // Device ID
    txbuffer[2] = 0x01;     // Fixed command

    // Calculate checksum: sum XOR txbuffer bytes
    sum ^= txbuffer[0];
    sum ^= txbuffer[1];
    sum ^= txbuffer[2];

    txbuffer[3] = sum;      // Checksum as END byte
}

/*This Function will send request to max sensor */
void send_scan_to_all_devices()
{
	uint8_t txbuf1[] = {0xAA, 0x81, 0x16, 0x00, 0x3E};
	uint8_t txbuf2[] = {0xAA, 0x82, 0x16, 0x00, 0x3D};
	uint8_t txbuf3[] = {0xAA, 0x83, 0x16, 0x00, 0x3C};
	uint8_t txbuf4[] = {0xAA, 0x80, 0x16, 0x00, 0x3F};

	while (1)
	{
	    HAL_UART_Transmit(&huart4, txbuf1, sizeof(txbuf1), 0xFFFF);
	    HAL_Delay(25);

	    HAL_UART_Transmit(&huart4, txbuf2, sizeof(txbuf2), 0xFFFF);
	    HAL_Delay(25);

	    HAL_UART_Transmit(&huart4, txbuf3, sizeof(txbuf3), 0xFFFF);
	    HAL_Delay(25);

	    HAL_UART_Transmit(&huart4, txbuf4, sizeof(txbuf4), 0xFFFF);
	    HAL_Delay(25);

	    HAL_UART_Transmit(&huart4, txbuf1, sizeof(txbuf1), 0xFFFF);
		HAL_Delay(25);

		HAL_UART_Transmit(&huart4, txbuf2, sizeof(txbuf2), 0xFFFF);
		HAL_Delay(25);

		HAL_UART_Transmit(&huart4, txbuf3, sizeof(txbuf3), 0xFFFF);
		HAL_Delay(25);

		HAL_UART_Transmit(&huart4, txbuf4, sizeof(txbuf4), 0xFFFF);
		HAL_Delay(25);
	    break;
	}

    for ( device_id = 1; device_id <= Total_Sensor; device_id++)
    {
        build_tx_packet(device_id);
        HAL_UART_Transmit(&huart4, txbuffer, sizeof(txbuffer),0xFFFF);
        HAL_Delay(25);  // small delay to avoid UART overlapping (adjust as needed)
    }
   timer2=0;
   flag2=1;
}



//void LCD_Show(uint8_t TS, uint8_t TO,uint8_t TV, uint8_t TE, uint8_t NC)
//{
//    char buf[21];
//    if (DWT_ELAPSED(last_update) > 1000)// 5000us = 5ms
//	  {
//		  if( flg==0)
//		  {
//			  lcd.clear();
//			  lcd.setCursor(0,0);
//			  lcd.print("   HOUSTON SYSTEM   ");
//			  lcd.setCursor(0,1);
//			  lcd.print("********************");
//			  flg=1;
//			  }
//	  }
//    // Line 3: Disengaged + Error
//    lcd.setCursor(0, 2);
//    snprintf(buf, sizeof(buf), "TS=%02d TO=%02d TV=%02d   ", TS, TO, TV);
//    lcd.print(buf);
//
//    // Line 4: Total + Engaged + NoComm
//    lcd.setCursor(0, 3);
//    snprintf(buf, sizeof(buf), "    TE=%02d  NC=%02d   ", TE, NC);
//    lcd.print(buf);
//}
void LCD_Show(uint8_t TS,uint8_t TD,uint8_t TV,uint8_t TO,uint8_t EN,uint8_t DS)
{
    char buf[21];

    if (DWT_ELAPSED(last_update) > 1000) // 1s update
    {
        if (flg == 0)
        {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  HOUSTON SYSTEMS   ");
            lcd.setCursor(0, 1);
            lcd.print("********************");
            flg = 1;
        }
    }

    /* -------- Line 3 -------- */
    lcd.setCursor(0, 2);
    snprintf(buf, sizeof(buf),
             "TS=%02d TD=%02d TV=%02d ",
             TS, TD, TV);
    lcd.print(buf);

    /* -------- Line 4 -------- */
    lcd.setCursor(0, 3);
    snprintf(buf, sizeof(buf),
             "TO=%02d E/N=%02d DS=%02d",
             TO, EN, DS);
    lcd.print(buf);
}

void Update_All_Displays(void)
{
    char txbuf[64];

    for (uint8_t d = 0; d < Number_of_display; d++)
    {
        uint16_t eng = 0, diseng = 0, error = 0, nocom = 0;

        /* Safety check */
        if (Displays[d].sensor_stop > Total_Sensor)
            continue;

        /* Count sensors for this display */
        for (uint8_t i = Displays[d].sensor_start-1;
             i <= Displays[d].sensor_stop-1;
             i++)
        {
            switch (sensors[i].data)
            {
                case 0x01:
                    eng++;
                    break;

                case 0x02:
                    diseng++;
                    break;

                case 0x03:
                    error++;
                    break;

                default:
                    nocom++;
                    break;
            }
        }

        /* Select mode string */
        const char *mode_str;
        switch (Displays[d].mode)
        {
            case 1: mode_str = "#u#"; break;
            case 2: mode_str = "#d#"; break;
            case 3: mode_str = "#r#"; break;
            case 4: mode_str = "#l#"; break;
            default: mode_str = "#u#"; break;
        }

        /* Select color char */
        char color_char = 'R';
        if (Displays[d].color == 2) color_char = 'G';
        else if (Displays[d].color == 3) color_char = 'B';


        char clearbuf[16];

        /* ---------- CLEAR DISPLAY ---------- */
        snprintf(clearbuf, sizeof(clearbuf),
                 "|C|%d|6|",
                 Displays[d].id);

        HAL_UART_Transmit(&huart2,
                          (uint8_t *)clearbuf,
                          strlen(clearbuf),
                          0xFFFF);

        HAL_Delay(300);   // small clear delay

        /* Build display command (DISENG only) */
        snprintf(txbuf, sizeof(txbuf),
                 "|C|%d|4|1|28-0-%s%c%d|",
                 Displays[d].id,
                 mode_str,
                 color_char,
                 diseng);

        /* Send to display UART */
        HAL_UART_Transmit(&huart2,
                          (uint8_t *)txbuf,
                          strlen(txbuf),
                          0xFFFF);

        HAL_Delay(20);   // small gap between displays
    }
}
void Send_Zone_Reply(UART_HandleTypeDef *huart,uint8_t zone_id,uint8_t total,SensorData *sensors,uint8_t diseng,uint8_t eng,uint8_t error,uint8_t nocom )
{
    uint8_t buff[64];
    uint8_t idx = 0;

    /* Header */
    buff[idx++] = 0xAA;
    buff[idx++] = zone_id;
    buff[idx++] = total;

    /* DATA: one byte per sensor ID */
    for (uint8_t expected_id = 1; expected_id <= total; expected_id++)
    {
        uint8_t found = 0;

        for (uint8_t i = 0; i < total; i++)
        {
            if (sensors[i].device_id == expected_id)
            {
                buff[idx++] = sensors[i].data;  // ✅ send actual data
                found = 1;
                break;
            }
        }

        /* If ID skipped → send NOCOM (4) */
        if (!found)
        {
            buff[idx++] = 4;
        }
    }

    /* Footer */
    buff[idx++] = diseng;
    buff[idx++] = eng;
    buff[idx++] = error;
    buff[idx++] = nocom;
    buff[idx++] = 0x55;

    /* Transmit */
    HAL_UART_Transmit(huart, buff, idx, 0xFFFF);
}

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
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_I2C3_Init();
  MX_TIM3_Init();
  MX_UART4_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  DWT_Init();
  lcd.begin(20, 4);
  lcd.backlight();
  lcd.clear();
  //UART 4 is for sensor // uart 2 for display // uart 1 for main controller//
//  HAL_UART_Receive_IT(&huart4,RxData,MAX_DATA_LEN);

  HAL_TIM_Base_Start_IT(&htim3);
  LCD_ShowInitScreen();
  for(uint8_t i = 1; i <= 6; i++)
  {
      LCD_InitDots(i);
      HAL_Delay(700);   // 👈 slow increase (adjust speed)
  }
  Read_data();
  HAL_UARTEx_ReceiveToIdle_IT(&huart4,RxData,20);
  HAL_UARTEx_ReceiveToIdle_IT(&huart1,Zone_rx,20);
  flag1=1;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
	  keyprocess();
	  switch(DisMenu)
	 	 	  {
	 			case MIAN_PAGE:
//	 				LCD_Show(total, eng, diseng, error, nocom);
	 			    break;
	 			case ENTER_PASSWORD:
	 				SetPSW();
	 				break;
	 			case SET_ZONE_ID:
	 				setZoneID();
	 				break;
	 			case  SET_NUMBER_OF_DISPLAY:
	 				SetNumber_of_Display();
	 				break;
	 			case SET_DISPLAY_CONFIG:
	 				SetDisplay_Config();
	 				break;
	 			case SET_TOTAL_SENSOR:
	 				SetTotal_Sensor();
	 				break;
	 	 	  }
	  if(flag1)
	  {
		  send_scan_to_all_devices();
		  timer1=0;
		  flag1=0;
	  }
		  if((timer2>1)&&(flag2))
		  {
			  for (int i = 0; i <= Total_Sensor*4 - 4; )
			  {
					  if (RxData6[i] == 0xF5)
					  {
						  unsigned char device_id = RxData6[i + 1];
						  unsigned char data = RxData6[i + 2];
						  unsigned char end_byte = RxData6[i + 3];
						  unsigned char checksum=0x3^0xF5;
						  checksum ^= device_id;
						  checksum^=data;
	 //		              if (end_byte == (0xF4 + device_id))
						  if (end_byte ==checksum)
						  {
							  // Valid frame found
							  if (sensor_index <= Total_Sensor)
							  {
								  sensors[sensor_index].device_id = device_id;
								  sensors[sensor_index].data = data;
								  sensor_index++;
							  }
							  i += 4;  // Skip past the full frame
						  }
						  else
						  {
							  i++;  // Invalid frame, move one byte
						  }
					  }
					  else
					  {
						  i++;  // Not a frame start, move one byte
					  }
				  }
			  Update_All_Displays();
				  for (int i = 0; i < Total_Sensor; i++)
				  {
					  switch (sensors[i].data) {
						  case 0x01:
							  eng++;
							  break;
						  case 0x02:
							  diseng++;
							  break;
						  case 0x03:
							  error++;
							  break;
						  default:
							  nocom++;
							  break;
					  }
				  }
//					char maintx[100];
//					char lcdtx[100];
//					char clear[20];
					 total = eng + diseng + error + nocom;
//					 void LCD_Show(uint8_t TS,uint8_t TD,uint8_t TV,uint8_t TO,uint8_t EN,uint8_t DS)
					LCD_Show( Total_Sensor,Number_of_display, diseng, eng, error+nocom, diseng+error+nocom);
//					snprintf(maintx, sizeof(maintx), "AA01%02d%02d%02d%02d%02dFF", total, eng, diseng, error, nocom);
//					snprintf(lcdtx, sizeof(lcdtx), "|AA|01|%02d|%02d|%02d|%02d|55|", total, eng, diseng, error); //|C|1|4|1|30-0-#u#R9|
//					snprintf(clear, sizeof(clear), "|C|1|6|"); //|C|1|4|1|30-0-#u#R9|
//					snprintf(lcdtx, sizeof(lcdtx), "|C|1|4|1|28-0-#u#G%d|", diseng); //|C|1|4|1|30-0-#u#R9|
//
//
//					HAL_UART_Transmit(&huart1, (uint8_t*)maintx, strlen(maintx),0xFFFF);
//					HAL_UART_Transmit(&huart2, (uint8_t*)clear, strlen(clear),0xFFFF);
//					HAL_Delay(250);
//					HAL_UART_Transmit(&huart2, (uint8_t*)lcdtx, strlen(lcdtx),0xFFFF);

					if(FL.Flag.DATA_REQUEST)
					{
						Send_Zone_Reply(&huart1,
								Zone_ID,
								total,
								sensors,diseng,eng,error,nocom);
						FL.Flag.DATA_REQUEST=0;
					}



					flag1=0;			// to trigger send request
					timer1=0;
					timer2=0;
					flag2=0;
					rx6_wr = 0;
					sensor_index=0;			//total get sensor
					memset(sensors, 0, sizeof(sensors));
					memset(RxData6, 0, MAX_DATA_LEN);
					eng = 0;
					diseng = 0;
					error = 0;
					nocom = 0;
					total=0;
		  }
	  cnt++;
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 12;
  RCC_OscInitStruct.PLL.PLLN = 240;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
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
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 62499;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1439;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

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
  huart4.Init.BaudRate = 9600;
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
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  huart2.Init.BaudRate = 9600;
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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn);

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : DN_KEY_Pin */
  GPIO_InitStruct.Pin = DN_KEY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(DN_KEY_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : UP_KEY_Pin ENT_KEY_Pin SFT_KEY_Pin */
  GPIO_InitStruct.Pin = UP_KEY_Pin|ENT_KEY_Pin|SFT_KEY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
//void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{

//    if(huart->Instance==UART4)
//   	{
////    	HAL_UARTEx_ReceiveToIdle_IT(&huart4,RxData,10);
//    	HAL_UART_Receive_IT(&huart4, RxData6, MAX_DATA_LEN);
////    	HAL_UART_Transmit(&huart4, RxData6,6,0xFFFF);
//
//   	}
	if (huart->Instance == UART4)
	    {
	        uint16_t len = Size;

	        /* Validate IDLE packet */
	        if ((len >= 4) && (RxData[0] == 0xF5))
	        {
	            for (uint16_t i = 0; i < len; i++)
	            {
	                RxData6[rx6_wr++] = RxData[i];

	                if (rx6_wr >= MAX_DATA_LEN)
	                    rx6_wr = 0;   // 🔁 rollover using YOUR buffer
	            }

	            rx6_len = len;   // store packet length
	        }

	        /* Restart RX */
	        HAL_UARTEx_ReceiveToIdle_IT(&huart4, RxData, 20);
	    }
	if (huart->Instance == USART1)
	{
	    uint16_t len = Size;

	    /* Validate packet length */
	    if (len >4)
	    {
	        /* Validate frame structure */
	        if ( Zone_rx[0] == 0xAA &&
	        	 Zone_rx[1] == Zone_ID &&
				 Zone_rx[2] == 0x80 &&
				 Zone_rx[3] == 0xA0 &&
				 Zone_rx[4] == 0x00 &&
				 Zone_rx[5] == 0x00 &&
				 Zone_rx[6] == 0x55 )
	        {
	            /* Packet is valid */
	        	FL.Flag.DATA_REQUEST=1;
	        }
	    }

	    /* Restart RX */
	    HAL_UARTEx_ReceiveToIdle_IT(&huart1,Zone_rx,20);
	}
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
	timer1++;
	timer2++;
	if (timer1>2)			//Every 30 sec timer1 will reset and send max sensor requests////
	{
		flag1=1;
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
