/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include <stdlib.h>
#include <string.h>

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
RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint16_t data = 0;  // your counter variable
uint16_t CAR = 0;
char lcdtx[64];    // buffer for transmit
char RxData6[20];
#define DEBOUNCE_DELAY   100// milliseconds
uint8_t flag = 0;       // set when new message received
uint16_t MAX_FULL = 0;   // for |SETFULL|=#

void check_inputs(void)
{
    static uint32_t last_inc_time = 0;
    static uint32_t last_dec_time = 0;
    static uint8_t inc_flag = 0;
    static uint8_t dec_flag = 0;

    GPIO_PinState inc_pin = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9);
    GPIO_PinState dec_pin = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_8);
    uint32_t now = HAL_GetTick();

    // --- Increment on PC9 press ---
    if (inc_pin == GPIO_PIN_RESET) // active low button
    {
        if (!inc_flag && (now - last_inc_time) > DEBOUNCE_DELAY)
        {
            data++;
            inc_flag = 1; // mark as pressed
            last_inc_time = now;
            HAL_Delay(100);
        }
    }
    else
    {
        // button released
        inc_flag = 0;
    }

    // --- Decrement on PC8 press ---
    if (dec_pin == GPIO_PIN_RESET) // active low button
    {
        if (!dec_flag && (now - last_dec_time) > DEBOUNCE_DELAY)
        {
            if (data > 0) data--;
            dec_flag = 1; // mark as pressed
            last_dec_time = now;
            HAL_Delay(100);
        }
    }
    else
    {
        // button released
        dec_flag = 0;
    }
}


//void process_uart_data(void)
//{
//    if (flag)   // process only when flag is set
//    {
//        flag = 0; // clear flag after processing
//
//        // Expected format: |SET|=#0001|
//        char *ptr = strstr(RxData6, "|SET|=#");
//        if (ptr != NULL)
//        {
//            ptr += 7; // move pointer to start of digits
//
//            // Check if next 4 chars are digits and followed by '|'
//            if (isdigit(ptr[0]) && isdigit(ptr[1]) &&
//                isdigit(ptr[2]) && isdigit(ptr[3]) && ptr[4] == '|')
//            {
//                char num_str[5] = {0}; // buffer for "0001" + null
//                strncpy(num_str, ptr, 4);
//
//                data = (uint16_t)atoi(num_str); // convert to integer
//
//                // ✅ "data" now has a valid value (0001–9999)
//            }
//            else
//            {
////                // Invalid message format
////                data = 0;
//            }
//        }
//    }
//}

void process_uart_data(void)
{
    if (flag)   // only process if flag set
    {
        flag = 0; // clear after processing

        // --- Case 1: |SET|=#0001|
        char *ptr = strstr(RxData6, "|SET|=#");
        if (ptr != NULL)
        {
            ptr += 7; // move to digits

            if (isdigit(ptr[0]) && isdigit(ptr[1]) &&
                isdigit(ptr[2]) && isdigit(ptr[3]) && ptr[4] == '|')
            {
                char num_str[5] = {0};
                strncpy(num_str, ptr, 4);
                data = (uint16_t)atoi(num_str);
            }
            return; // processed successfully
        }

        // --- Case 2: |SETFULL|=#0100|
        ptr = strstr(RxData6, "|SETFULL|=#");
        if (ptr != NULL)
        {
            ptr += 11; // move to digits (after "|SETFULL|=#")

            if (isdigit(ptr[0]) && isdigit(ptr[1]) &&
                isdigit(ptr[2]) && isdigit(ptr[3]) && ptr[4] == '|')
            {
                char num_str[5] = {0};
                strncpy(num_str, ptr, 4);
                MAX_FULL = (uint16_t)atoi(num_str);
                HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, MAX_FULL);
            }
            return; // processed successfully
        }
    }
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
  MX_USART6_UART_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
//  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0x10);
	   CAR = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0);
	   data=CAR;
	   MAX_FULL= HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1);
	   HAL_Delay(1000);
	   HAL_Delay(1000);
	   HAL_Delay(1000);
	   sprintf(lcdtx, "|C|1|6|");
	   HAL_UART_Transmit(&huart6, (uint8_t*)lcdtx, strlen(lcdtx),3000);
	   HAL_Delay(1000);
	   HAL_Delay(1000);
	  if(CAR>=MAX_FULL)
	  {
		  sprintf(lcdtx, "|C|1|4|1|10-0-#RF U L L|");
	  }
	  else if (MAX_FULL- CAR < 10)
	   {
		   sprintf(lcdtx, "|C|1|4|1|42-0-#G%u|",MAX_FULL- CAR);
	   }
	  else if (MAX_FULL- CAR < 100)  // two digits (00–99)
	  {
		  sprintf(lcdtx, "|C|1|4|1|37-0-#G%u|", MAX_FULL- CAR);
	  }
	  else if(MAX_FULL- CAR < 9999) // three digits (100–999)
	  {
		  sprintf(lcdtx, "|C|1|4|1|33-0-#G%u|", MAX_FULL- CAR);
	  }
	  HAL_Delay(1000);
   HAL_UART_Transmit(&huart6, (uint8_t*)lcdtx, strlen(lcdtx),3000);
   HAL_UARTEx_ReceiveToIdle_IT(&huart6,RxData6,20);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  process_uart_data();
	  check_inputs();
	  if(CAR!=data)
	  {
		  CAR=data;
		  if (CAR>MAX_FULL)
		  {
			  CAR=MAX_FULL;
			  data=MAX_FULL;
		  }
		  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, CAR);
		  sprintf(lcdtx, "|C|1|6|");
		  HAL_UART_Transmit(&huart6, (uint8_t*)lcdtx, strlen(lcdtx), 3000);
		  if(CAR>=MAX_FULL)
		  {
			  sprintf(lcdtx, "|C|1|4|1|10-0-#RF U L L|");
		  }
		  else if (MAX_FULL- CAR < 10)
		   {
			   sprintf(lcdtx, "|C|1|4|1|42-0-#G%u|",MAX_FULL- CAR);
		   }
		  else if (MAX_FULL- CAR < 100)  // two digits (00–99)
		  {
		      sprintf(lcdtx, "|C|1|4|1|37-0-#G%u|", MAX_FULL- CAR);
		  }
		  else if(MAX_FULL- CAR < 9999) // three digits (100–999)
		  {
		      sprintf(lcdtx, "|C|1|4|1|33-0-#G%u|", MAX_FULL- CAR);
		  }
		  HAL_Delay(300);
		  // Transmit second message
		  HAL_UART_Transmit(&huart6, (uint8_t*)lcdtx, strlen(lcdtx), 3000);
//		  // Format your string into ASCII text
//		      sprintf(lcdtx, "|C|1|4|1|40-0-#%u|", CAR);
//
//		      // Send asynchronously (non-blocking)
//		      HAL_UART_Transmit_IT(&huart6, (uint8_t*)lcdtx, strlen(lcdtx));
	  }
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 160;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
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
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

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
  huart6.Init.BaudRate = 9600;
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
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pins : PC8 PC9 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
//void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if(huart->Instance==USART6)
   	{
    	flag=1;
    	HAL_UARTEx_ReceiveToIdle_IT(&huart6,RxData6,20);
//    	HAL_UART_Receive_IT(&huart1, RxData6, 200);

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
