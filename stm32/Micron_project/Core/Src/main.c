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

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DEBOUNCE_TIME_MS      100U    /* software debounce time for both inputs */
#define INPUT_WINDOW_MS       3000U  /* max allowed gap between INPUT1 and INPUT2 (3 s) */
#define RELAY_ON_TIME_MS      1000U  /* relay ON duration: 1000U = 1 s, 2000U = 2 s */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
typedef struct
{
  GPIO_TypeDef *port;
  uint16_t      pin;
  GPIO_PinState stableState;
  GPIO_PinState lastRawState;
  uint32_t      lastChangeTick;
} Debounce_t;

static Debounce_t input1 = {INPUT1_GPIO_Port, INPUT1_Pin, GPIO_PIN_SET, GPIO_PIN_SET, 0};
static Debounce_t input2 = {INPUT2_GPIO_Port, INPUT2_Pin, GPIO_PIN_SET, GPIO_PIN_SET, 0};

static uint8_t  input1_pending = 0;
static uint8_t  input2_pending = 0;
static uint32_t input1_trigger_tick = 0;
static uint32_t input2_trigger_tick = 0;

static uint8_t  relay_active = 0;
static uint32_t relay_on_tick = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
static uint8_t Input_CheckPressed(Debounce_t *input);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
  * @brief  Samples a pulled-up input, debounces it, and reports a fresh
  *         active-low press edge.
  * @retval 1 once when the input becomes active (pressed), 0 otherwise
  */
static uint8_t Input_CheckPressed(Debounce_t *input)
{
  GPIO_PinState raw = HAL_GPIO_ReadPin(input->port, input->pin);
  uint32_t now = HAL_GetTick();

  if (raw != input->lastRawState)
  {
    input->lastRawState = raw;
    input->lastChangeTick = now;
  }

  if ((now - input->lastChangeTick) >= DEBOUNCE_TIME_MS)
  {
    if (input->stableState != input->lastRawState)
    {
      input->stableState = input->lastRawState;
      if (input->stableState == GPIO_PIN_RESET)
      {
        return 1;
      }
    }
  }

  return 0;
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
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    {
//      uint8_t  in1_pressed = Input_CheckPressed(&input1);
//      uint8_t  in2_pressed = Input_CheckPressed(&input2);
//      uint32_t now = HAL_GetTick();
//
//      if (in1_pressed)
//      {
//        input1_pending = 1;
//        input1_trigger_tick = now;
//      }
//      if (in2_pressed)
//      {
//        input2_pending = 1;
//        input2_trigger_tick = now;
//      }
//
//      if (!relay_active)
//      {
//        if (input1_pending && input2_pending)
//        {
//          uint32_t diff = (input1_trigger_tick > input2_trigger_tick)
//                           ? (input1_trigger_tick - input2_trigger_tick)
//                           : (input2_trigger_tick - input1_trigger_tick);
//
//          if (diff <= INPUT_WINDOW_MS)
//          {
//            HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, GPIO_PIN_SET);
//            relay_active = 1;
//            relay_on_tick = now;
//          }
//
//          input1_pending = 0;
//          input2_pending = 0;
//        }
//        else if (input1_pending && ((now - input1_trigger_tick) > INPUT_WINDOW_MS))
//        {
//          input1_pending = 0; /* input2 did not arrive in time */
//        }
//        else if (input2_pending && ((now - input2_trigger_tick) > INPUT_WINDOW_MS))
//        {
//          input2_pending = 0; /* input1 did not arrive in time */
//        }
//      }
//      else if ((now - relay_on_tick) >= RELAY_ON_TIME_MS)
//      {
//        HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, GPIO_PIN_RESET);
//        relay_active = 0;
//      }
//    }
    	uint8_t in1_pressed = Input_CheckPressed(&input1);
    	uint8_t in2_pressed = Input_CheckPressed(&input2);
    	uint32_t now = HAL_GetTick();

    	if (!relay_active)
    	{
    	    /* First input must be INPUT1 */
    	    if (!input1_pending && in1_pressed)
    	    {
    	        input1_pending = 1;
    	        input1_trigger_tick = now;
    	    }

    	    /* Second input must be INPUT2 */
    	    if (input1_pending && in2_pressed)
    	    {
    	        if ((now - input1_trigger_tick) <= INPUT_WINDOW_MS)
    	        {
    	            HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, GPIO_PIN_SET);

    	            relay_active = 1;
    	            relay_on_tick = now;
    	        }

    	        input1_pending = 0;
    	    }

    	    /* INPUT2 came first -> ignore it */
    	    if (in2_pressed && !input1_pending)
    	    {
    	        /* Do nothing */
    	    }

    	    /* INPUT1 timed out waiting for INPUT2 */
    	    if (input1_pending &&
    	        ((now - input1_trigger_tick) > INPUT_WINDOW_MS))
    	    {
    	        input1_pending = 0;
    	    }
    	}
    	else if ((now - relay_on_tick) >= RELAY_ON_TIME_MS)
    	{
    	    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, GPIO_PIN_RESET);
    	    relay_active = 0;
    	}
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
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
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA0 PA1 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : RELAY_Pin */
  GPIO_InitStruct.Pin = RELAY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RELAY_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
