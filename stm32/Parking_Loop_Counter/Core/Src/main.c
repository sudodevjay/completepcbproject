/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Parking loop counter
  *                   PA0 (pin 14) entry loop  -> car +1
  *                   PA1 (pin 15) exit loop   -> car -1
  *                   AT24 EEPROM on I2C3 (SDA PC9, SCL PA8) keeps the count
  *                   over power failure.
  *                   Display on USART2 (PA2 TX -> display RX, PA3 RX <- display TX)
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
#include <stdio.h>
#include <ctype.h>
#include "at24_store.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       stable;    /* 1 = vehicle on loop (debounced) */
    uint8_t       last_raw;
    uint32_t      change_tick;
} LoopInput;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LOOP_DEBOUNCE_MS     100   /* loop signal must be stable this long */
#define DISPLAY_BOOT_MS      5000  /* wait for display to power up */
#define DISPLAY_GAP_MS       300   /* gap between clear and value message */
#define DEFAULT_MAX_FULL     101   /* used only when EEPROM is blank */
#define MAX_FULL_LIMIT       999   /* display shows only 3 digits */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c3;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
ParkData park;                 /* car count + max full, mirrored in EEPROM */
char lcdtx[64];                /* display transmit buffer */
uint8_t RxData2[32];           /* UART receive buffer */
char RxCmd[33];                /* copy of last received command */
volatile uint8_t flag = 0;     /* set when new UART message received */

LoopInput loop_in  = { LOOP_IN_GPIO_Port,  LOOP_IN_Pin  };
LoopInput loop_out = { LOOP_OUT_GPIO_Port, LOOP_OUT_Pin };

uint8_t  disp_pending = 1;     /* refresh display once after boot */
uint8_t  disp_state = 0;       /* 0 = idle, 1 = clear sent, waiting gap */
uint32_t disp_tick = 0;
uint16_t disp_shown = 0xFFFF;  /* vacancy last sent to display (0xFFFF = none yet) */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C3_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
static void I2C3_BusRecover(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* After a power cut in the middle of an I2C transfer the AT24 can hold SDA
 * low. Clock SCL until it releases the bus, then send a STOP. */
static void I2C3_BusRecover(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);   /* SCL */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET);   /* SDA */

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Pin = GPIO_PIN_8;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    HAL_Delay(1);

    for (uint8_t i = 0; i < 9 && HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9) == GPIO_PIN_RESET; i++)
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
        HAL_Delay(1);
    }

    /* STOP condition */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_Delay(1);
}

/* Returns 1 once when a vehicle arrives on the loop (active low input) */
static uint8_t loop_triggered(LoopInput *in)
{
    uint8_t raw = (HAL_GPIO_ReadPin(in->port, in->pin) == GPIO_PIN_RESET);
    uint32_t now = HAL_GetTick();

    if (raw != in->last_raw)
    {
        in->last_raw = raw;
        in->change_tick = now;
        return 0;
    }
    if (raw != in->stable && (now - in->change_tick) >= LOOP_DEBOUNCE_MS)
    {
        in->stable = raw;
        return raw;
    }
    return 0;
}

/* Take the current pin level as the starting state, so a vehicle standing
 * on the loop while power comes back is not counted again. */
static void loop_init(LoopInput *in)
{
    in->last_raw = (HAL_GPIO_ReadPin(in->port, in->pin) == GPIO_PIN_RESET);
    in->stable = in->last_raw;
    in->change_tick = HAL_GetTick();
}

static uint16_t vacancy(void)
{
    return (park.car >= park.max_full) ? 0 : (uint16_t)(park.max_full - park.car);
}

static void save_and_show(void)
{
    if (park.car > park.max_full)
    {
        park.car = park.max_full;
    }
    AT24_Save(&hi2c3, &park);
    if (vacancy() != disp_shown)
    {
        disp_pending = 1;   // send to display only when vacancy really changed
    }
}

void check_inputs(void)
{
    if (loop_triggered(&loop_in))
    {
        if (park.car < park.max_full)
        {
            park.car++;
            save_and_show();
        }
    }

    if (loop_triggered(&loop_out))
    {
        if (park.car > 0)
        {
            park.car--;
            save_and_show();
        }
    }
}

/* Reads 4 digits followed by '|' */
static uint8_t parse4(const char *ptr, uint16_t *value)
{
    if (isdigit((uint8_t)ptr[0]) && isdigit((uint8_t)ptr[1]) &&
        isdigit((uint8_t)ptr[2]) && isdigit((uint8_t)ptr[3]) && ptr[4] == '|')
    {
        *value = (uint16_t)((ptr[0] - '0') * 1000 + (ptr[1] - '0') * 100 +
                            (ptr[2] - '0') * 10 + (ptr[3] - '0'));
        return 1;
    }
    return 0;
}

void process_uart_data(void)
{
    if (flag)   // only process if flag set
    {
        flag = 0; // clear after processing
        uint16_t value;

        // --- Case 1: |SET|=#0001|  -> set car count
        char *ptr = strstr(RxCmd, "|SET|=#");
        if (ptr != NULL)
        {
            if (parse4(ptr + 7, &value))
            {
                park.car = value;
                save_and_show();
            }
            return;
        }

        // --- Case 2: |SETFULL|=#0100|  -> set capacity
        ptr = strstr(RxCmd, "|SETFULL|=#");
        if (ptr != NULL)
        {
            if (parse4(ptr + 11, &value) && value > 0 && value <= MAX_FULL_LIMIT)
            {
                park.max_full = value;
                save_and_show();
            }
            return;
        }
    }
}

static void display_format(void)
{
    uint16_t avail = vacancy();

    if (avail == 0)
    {
        sprintf(lcdtx, "|C|1|4|1|33-0-#G000|");
    }
    else                   // always three digits: 1 -> 001, 25 -> 025
    {
        sprintf(lcdtx, "|C|1|4|1|33-0-#G%03u|", avail);
    }
}

/* Non-blocking display refresh: clear, wait DISPLAY_GAP_MS, send value.
 * Sends only once after power up and whenever the vacancy changes. */
void display_task(void)
{
    uint32_t now = HAL_GetTick();

    if (now < DISPLAY_BOOT_MS)
    {
        return;
    }

    if (disp_state == 0 && disp_pending)
    {
        disp_pending = 0;
        if (vacancy() == disp_shown)
        {
            return;   // value went back to what display already shows
        }
        sprintf(lcdtx, "|C|1|6|");
        HAL_UART_Transmit(&huart2, (uint8_t*)lcdtx, strlen(lcdtx), 3000);
        disp_tick = now;
        disp_state = 1;
    }
    else if (disp_state == 1 && (now - disp_tick) >= DISPLAY_GAP_MS)
    {
        display_format();
        HAL_UART_Transmit(&huart2, (uint8_t*)lcdtx, strlen(lcdtx), 3000);
        disp_shown = vacancy();
        disp_state = 0;
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
  I2C3_BusRecover();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C3_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(10);   // let EEPROM settle after power up

  if (!AT24_Load(&hi2c3, &park))
  {
      // blank EEPROM: start with defaults and store them
      park.car = 0;
      park.max_full = DEFAULT_MAX_FULL;
      AT24_Save(&hi2c3, &park);
  }
  if (park.max_full == 0 || park.max_full > MAX_FULL_LIMIT)
  {
      park.max_full = DEFAULT_MAX_FULL;
      AT24_Save(&hi2c3, &park);
  }
  if (park.car > park.max_full)
  {
      park.car = park.max_full;
  }

  loop_init(&loop_in);
  loop_init(&loop_out);

  HAL_UARTEx_ReceiveToIdle_IT(&huart2, RxData2, sizeof(RxData2));
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  process_uart_data();
	  check_inputs();
	  display_task();
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
  hi2c3.Init.ClockSpeed = 100000;
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
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pins : LOOP_IN_Pin LOOP_OUT_Pin */
  GPIO_InitStruct.Pin = LOOP_IN_Pin|LOOP_OUT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART2)
    {
        if (Size > sizeof(RxData2)) Size = sizeof(RxData2);
        memcpy(RxCmd, RxData2, Size);
        RxCmd[Size] = '\0';
        flag = 1;
        HAL_UARTEx_ReceiveToIdle_IT(&huart2, RxData2, sizeof(RxData2));
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        // noise / overrun on the line: restart reception
        HAL_UARTEx_ReceiveToIdle_IT(&huart2, RxData2, sizeof(RxData2));
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
