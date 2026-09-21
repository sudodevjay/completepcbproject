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
extern char Floor_ID,Number_of_Display,Total_Zone;

unsigned char Master_rx[20] = {}; // this is buffer to recive request packet from Master controller
#define ZONE_RX_BUF_SIZE   256
#define MASTER_TX_SIZE    5000

unsigned char Zone_rx[ZONE_RX_BUF_SIZE];// this buffer is to recive data packet from Zone controller
unsigned char Master_tx[MASTER_TX_SIZE]; //this is a buffer which hold all zone data to send master
uint8_t Master_response[5000]={0};
volatile uint16_t Master_tx_idx = 0;
volatile uint16_t Current_Zone = 0;
static uint16_t idx;
/* Floor totals */
volatile uint32_t Floor_Total_Vacant  = 0;
volatile uint32_t Floor_Total_Vacant_Copy  = 0;
volatile uint32_t Floor_Total_Engaged = 0;
volatile uint32_t Floor_Total_Faulty  = 0;
volatile uint32_t Floor_Total_NoComm  = 0;
char timer1=0,timer2=0;
static uint32_t last_update = 0;
char flg=0;
//char flg=0;

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

void LCD_Show(uint8_t TZ,uint8_t TD,uint8_t TV,uint8_t TO,uint8_t EN,uint8_t DS)
{
    char buf[21];

    if (DWT_ELAPSED(last_update) > 1000) // 1s update
    {
        if (flg == 0)
        {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("   HOUSTON SYSTEM   ");
            lcd.setCursor(0, 1);
            lcd.print("********************");
            flg = 1;
        }
    }

    /* -------- Line 3 -------- */
    lcd.setCursor(0, 2);
    snprintf(buf, sizeof(buf),
             "TZ=%02d TD=%02d TV=%02d ",
             TZ, TD, TV);
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
	if(Floor_Total_Vacant_Copy!=Floor_Total_Vacant)
	{
			Floor_Total_Vacant_Copy=Floor_Total_Vacant;
		char txbuf[64];
		char clearbuf[16];

		for (uint8_t d = 0; d < Number_of_Display; d++)
		{
			/* ---------- SELECT MODE STRING ---------- */
			const char *mode_str;
			switch (Displays[d].mode)
			{
				case 1: mode_str = "#u#"; break;
				case 2: mode_str = "#d#"; break;
				case 3: mode_str = "#r#"; break;
				case 4: mode_str = "#l#"; break;
				default: mode_str = "#u#"; break;
			}

			/* ---------- SELECT COLOR ---------- */
			char color_char = 'R';
			if (Displays[d].color == 2) color_char = 'G';
			else if (Displays[d].color == 3) color_char = 'B';

			/* ---------- CLEAR DISPLAY ---------- */
			snprintf(clearbuf, sizeof(clearbuf),
					 "|C|%d|6|",
					 Displays[d].id);

			HAL_UART_Transmit(&huart2,
							  (uint8_t *)clearbuf,
							  strlen(clearbuf),
							  0xFFFF);

			HAL_Delay(300);   // display clear delay

			/* ---------- BUILD DISPLAY COMMAND ---------- */
			snprintf(txbuf, sizeof(txbuf),
					 "|C|%d|4|1|28-0-%s%c%lu|",
					 Displays[d].id,
					 mode_str,
					 color_char,
					 (unsigned long)Floor_Total_Vacant_Copy);

			/* ---------- SEND TO DISPLAY ---------- */
			HAL_UART_Transmit(&huart2,
							  (uint8_t *)txbuf,
							  strlen(txbuf),
							  0xFFFF);

			HAL_Delay(20);   // gap between displays
		}
	}
}

void Zone_Request_Task(void)
{

    if (FL.Flag.Zone_request == 1)
    {
        uint8_t tx_frame[7];

        tx_frame[0] = 0xAA;
        tx_frame[1] = Current_Zone;
        tx_frame[2] = 0x80;
        tx_frame[3] = 0xA0;
        tx_frame[4] = 0x00;
        tx_frame[5] = 0x00;
        tx_frame[6] = 0x55;

        HAL_Delay(10);   // ✅ small inter-request gap
        HAL_UART_Transmit(&huart4, tx_frame, 7, 100);

        /* Clear request flag */
        FL.Flag.Zone_request = 0;
        timer1=0;
        /* Increment zone */
//        Current_Zone++;
//        if (Current_Zone > Total_Zone)
//        {
//            Current_Zone = 1;
//        }
    }
}
void Flush_All_Data(void)
{
    memset(Master_tx, 0, MASTER_TX_SIZE);

    Floor_Total_Vacant  = 0;
    Floor_Total_Engaged = 0;
    Floor_Total_Faulty  = 0;
    Floor_Total_NoComm  = 0;
    Master_tx_idx=0;
    FL.Flag.Display_request = 0;
}

void Build_Master_Response(void)
{
	memset(Master_response, 0, sizeof(Master_response));
	idx=0;
    Master_response[idx++] = 0xDE;
    Master_response[idx++] = Floor_ID;
    Master_response[idx++] = Total_Zone;

    /* Copy entire Master_tx buffer */
    memcpy(&Master_response[idx], Master_tx, Master_tx_idx);
    idx += Master_tx_idx;

    /* Append totals (little-endian, 16-bit) */
    Master_response[idx++] = (uint8_t)(Floor_Total_Vacant);
    Master_response[idx++] = (uint8_t)(Floor_Total_Vacant >> 8);

    Master_response[idx++] = (uint8_t)(Floor_Total_Engaged);
    Master_response[idx++] = (uint8_t)(Floor_Total_Engaged >> 8);

    Master_response[idx++] = (uint8_t)(Floor_Total_Faulty);
    Master_response[idx++] = (uint8_t)(Floor_Total_Faulty >> 8);

    Master_response[idx++] = (uint8_t)(Floor_Total_NoComm);
    Master_response[idx++] = (uint8_t)(Floor_Total_NoComm >> 8);

    Master_response[idx++] = 0xE9;

    /* idx now holds total response length */
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
  //UART 4 is for Zone // uart 2 for display // uart 1 for Master controller//
  HAL_UARTEx_ReceiveToIdle_IT(&huart4,Zone_rx,ZONE_RX_BUF_SIZE);
  HAL_UARTEx_ReceiveToIdle_IT(&huart1,Master_rx,20);
  HAL_TIM_Base_Start_IT(&htim3);
  LCD_ShowInitScreen();
  for(uint8_t i = 1; i <= 6; i++)
  {
      LCD_InitDots(i);
      HAL_Delay(700);   // 👈 slow increase (adjust speed)
  }
  Read_data();
//  Current_Zone = 1;
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
	 			case SET_Floor_ID:
	 				setFloorID();
	 				break;
	 			case  SET_NUMBER_OF_DISPLAY:
	 				set_no_Display();
	 				break;
	 			case SET_DISPLAY_CONFIG:
	 				SetDisplay_Config();
	 				break;
	 			case SET_TOTAL_Zone:
	 				SetTotal_Zone();
	 				break;
	 	 	  }
	  if (!FL.Flag.Display_request)
	  {
	      Zone_Request_Task();
	  }
	  else
	  {
//		  LCD_Show(uint8_t TZ,uint8_t TD,uint8_t TV,uint8_t TO,uint8_t EN,uint8_t DS)
		  LCD_Show(Total_Zone, Number_of_Display, Floor_Total_Vacant, Floor_Total_Engaged, Floor_Total_Faulty+Floor_Total_NoComm,Floor_Total_Vacant+Floor_Total_Faulty+Floor_Total_NoComm);
	      Update_All_Displays();
	      Build_Master_Response();
	      Flush_All_Data();
	  }
	if( FL.Flag.Master_request)
	{
		HAL_UART_Transmit(&huart1,(uint8_t *)Master_response,idx,0xFFFF);
		 FL.Flag.Master_request=0;
	}
//	  if(flag1)
//	  {
//		  send_scan_to_all_devices();
//		  timer1=0;
//		  flag1=0;
//	  }
//		  if((timer2>1)&&(flag2))
//		  {
//
//			  Update_All_Displays();
//			  total = eng + diseng + error + nocom;
//			  LCD_Show( Total_Zone,Number_of_Display, diseng, eng, error+nocom, diseng+error+nocom);
//				if(FL.Flag.DATA_REQUEST)
//				{
//					Send_Zone_Reply(&huart1,
//							Floor_ID,
//							total,
//							sensors,diseng,eng,error,nocom);
//					FL.Flag.DATA_REQUEST=0;
//				}
//
//					flag1=0;			// to trigger send request
//					timer1=0;
//					timer2=0;
//					flag2=0;
//					rx6_wr = 0;
//					sensor_index=0;			//total get sensor
//					memset(sensors, 0, sizeof(sensors));
//					memset(Zone_rx6, 0, MAX_DATA_LEN);
//					eng = 0;
//					diseng = 0;
//					error = 0;
//					nocom = 0;
//					total=0;
//		  }
//	  cnt++;
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

	if (huart->Instance == UART4)
	    {
	        uint16_t len = Size;

	        if ((len > 8) &&
	            (Zone_rx[0] == 0xAA) &&
			     Zone_rx[1]==Current_Zone&&
	            (Zone_rx[len - 1] == 0x55))
	        {
	            /* Copy entire valid response */
	            memcpy(&Master_tx[Master_tx_idx], Zone_rx, len);
	            Master_tx_idx += len;

	            /* Extract floor totals */
	            uint8_t total_sensor = Zone_rx[2];
	            uint16_t idx = 3 + total_sensor;

	            Floor_Total_Vacant  += Zone_rx[idx];
	            Floor_Total_Engaged += Zone_rx[idx + 1];
	            Floor_Total_Faulty  += Zone_rx[idx + 2];
	            Floor_Total_NoComm  += Zone_rx[idx + 3];
	            FL.Flag.Zone_request=1;
	            timer1=0;
	            Current_Zone++;
				if (Current_Zone > Total_Zone){
					Current_Zone = 1;
					FL.Flag.Display_request=1;
				}
	        }

	        /* Clear RX buffer */
	        memset(Zone_rx, 0, sizeof(Zone_rx));
	        /* Restart RX */
	        HAL_UARTEx_ReceiveToIdle_IT(&huart4, Zone_rx, ZONE_RX_BUF_SIZE);
	    }
	if (huart->Instance == USART1)
	{
	    uint16_t len = Size;

	    /* Validate packet length */
	    if ((len == 7 && Master_rx[0] == 0xDE && Master_rx[1] == Floor_ID && Master_rx[2] == 0x80 && Master_rx[3] == 0xA0 && Master_rx[6] == 0xE9))
	        FL.Flag.Master_request = 1;

	    /* Restart RX */
	    HAL_UARTEx_ReceiveToIdle_IT(&huart1,Master_rx,20);
	}
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
	//	timer2++;
	if(!FL.Flag.Zone_request)timer1++;
	if (timer1>10)			//Every 1 sec intrrupt////
	{
		timer1 = 0;
		Current_Zone++;
		if (Current_Zone > Total_Zone){
			Current_Zone = 1;
			FL.Flag.Display_request=1;
		}
		FL.Flag.Zone_request=1; //time out zone request
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
