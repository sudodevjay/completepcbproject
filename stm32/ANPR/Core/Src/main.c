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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "socket.h"
#include "wizchip_conf.h"
#include "loopback.h"
#include "w5500.h"
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
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
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim3;

/* Definitions for Server_Task */
osThreadId_t Server_TaskHandle;
const osThreadAttr_t Server_Task_attributes = {
  .name = "Server_Task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for Loop_task */
osThreadId_t Loop_taskHandle;
const osThreadAttr_t Loop_task_attributes = {
  .name = "Loop_task",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* USER CODE BEGIN PV */
uint8_t socket_buf[100];     						// Buffer to receive data
uint8_t rcvBuf[20],bufSize[]={2,2,2,2};
int32_t len;										//Len of data recived
wiz_NetInfo getInfo ={ .mac={0},  					//Mac address
	  		  	  	   .ip ={0}, 					// IP address
	    			   .sn ={0},					//Subnet mask
	    			   .gw ={0}};					//Gateway address

uint8_t dest_ip[4] = {192, 168, 1, 43};
uint16_t dest_port = 5000;
#define SOCK_TCPC 0         							     // Socket 0
#define SRC_PORT 5000     								     // Listening port
int temp = 0;
#define RX_BUF_SIZE 1024
uint8_t rx_buf[RX_BUF_SIZE];
#define DEBOUNCE_DELAY   50// milliseconds
volatile uint8_t loop1_flag = 0;
volatile uint8_t loop2_flag = 0;

volatile uint8_t entry_Busy = 0;
volatile uint8_t exit_Busy = 0;
char tmr=0;
char relay_signal=0;
#define RESET_LOOP 5

char json_body[256];
int content_length;
char http_post[512];

char relaystt=0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM3_Init(void);
void handle_server(void *argument);
void Loop_Scanning(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void cs_sel(){
	 HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
}
void cs_desel(){
	HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
}
uint8_t spi_rb(void){
	uint8_t rbuf;
	HAL_SPI_Receive(&hspi1,&rbuf,1,100);
	return rbuf;
}
void spi_wb(uint8_t b){
	HAL_SPI_Transmit(&hspi1,&b,1,100);
}
void check_inputs(void)
{
    static uint32_t last_inc_time = 0;
    static uint32_t last_dec_time = 0;
    static uint8_t inc_flag = 0;
    static uint8_t dec_flag = 0;
    GPIO_PinState inc_pin = HAL_GPIO_ReadPin(GPIOC, Loop1_Pin);
    GPIO_PinState dec_pin = HAL_GPIO_ReadPin(GPIOC, Loop2_Pin);
    uint32_t now = HAL_GetTick();
    if (inc_pin == GPIO_PIN_RESET) // active low Loop
    {
        if (!inc_flag && (now - last_inc_time) > DEBOUNCE_DELAY)
        {
            inc_flag = 1; // mark as pressed
            last_inc_time = now;
//            HAL_Delay(100);
            if(exit_Busy)
            {
            	exit_Busy=0;
            }
            else
            {
//                entry_Busy=1;
//                if(!loop1_flag)loop1_flag=1;
                if(!entry_Busy)
                	{
                	entry_Busy=1;
                	if(!loop1_flag)loop1_flag=1;
                	HAL_GPIO_WritePin(RL_GPIO_Port, RL_Pin, GPIO_PIN_RESET);
                	relay_signal=0;
                	tmr=0;
                	}
            }

//            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
//            HAL_GPIO_WritePin(RL_GPIO_Port, RL_Pin, GPIO_PIN_RESET);
        }
    }
    else
    {
        // Loop released
        inc_flag = 0;
    }

    if (dec_pin == GPIO_PIN_RESET) // active low Loop
    {
        if (!dec_flag && (now - last_dec_time) > DEBOUNCE_DELAY)
        {
            dec_flag = 1; // mark as pressed
            last_dec_time = now;
//            HAL_Delay(100);
            if(entry_Busy)
            {
            	entry_Busy=0;

            }
            else
            {
//            	 if(!loop2_flag) {loop2_flag=1;}
//            	 exit_Busy=1;
            	 if(!exit_Busy)
					{
            		 exit_Busy=1;
					if(!loop2_flag)loop2_flag=1;
					HAL_GPIO_WritePin(RL_GPIO_Port, RL_Pin, GPIO_PIN_RESET);
					relay_signal=0;
					tmr=0;
					}
            }


//            HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
//            relaystt=1;
        }
    }
    else
    {
        // Loop released
        dec_flag = 0;
//        if(relaystt)
//        	{
//        	relaystt=0;
//        		HAL_GPIO_WritePin(RL_GPIO_Port, RL_Pin, GPIO_PIN_SET);
//        	}
    }
}
//void send_post_request(void)
//{
//	if(loop1_flag)
//	{
//		loop1_flag=0;
//	    int8_t sock = socket(SOCK_TCPC, Sn_MR_TCP, SRC_PORT, 0);
//	    if (sock != SOCK_TCPC) {
//	        printf("Socket open failed\n");
//	    }
//	    if (connect(SOCK_TCPC, dest_ip, dest_port) != SOCK_OK) {
//	        printf("Socket connect failed\n");
//	        close(SOCK_TCPC);
//	    }
//	    snprintf(json_body, sizeof(json_body),
//	             "{\"data\":\"Entry\"}");
//	     content_length = strlen(json_body);
//	    snprintf(http_post, sizeof(http_post),
//	        "POST /sensor_data HTTP/1.1\r\n"
//	        "Host: 192.168.1.42:5000\r\n"
//	        "Content-Type: application/json\r\n"
//	        "Content-Length: %d\r\n"
//			"Connection: keep-alive\r\n"
//	        "\r\n"
//	        "%s",
//	        content_length, json_body);
//	    int sent = send(SOCK_TCPC, (uint8_t*)http_post, strlen(http_post));
//	    if (sent <= 0) {
//	        printf("Send failed\n");
//	    } else {
//	    	 HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
//	        printf("POST sent\n");
//	    }
////	    uint8_t rx_buff[1024] = {0};
////	    int32_t recv_len = recv(SOCK_TCPC, rx_buff, sizeof(rx_buff));
////	    int32_t len = recv(SOCK_TCPC, rx_buf, RX_BUF_SIZE );  // Leave space for null terminator
//	    close(SOCK_TCPC);
//
//	}
//	if(loop2_flag)
//	{
//		 loop2_flag=0;
//		int8_t sock = socket(SOCK_TCPC, Sn_MR_TCP, SRC_PORT, 0);
//		if (sock != SOCK_TCPC) {
//			printf("Socket open failed\n");
//		}
//		if (connect(SOCK_TCPC, dest_ip, dest_port) != SOCK_OK) {
//			printf("Socket connect failed\n");
//			close(SOCK_TCPC);
//		}
//		    snprintf(json_body, sizeof(json_body),
//		             "{\"data\":\"Exit\"}");
//		content_length = strlen(json_body);
//		snprintf(http_post, sizeof(http_post),
//			"POST /sensor_data HTTP/1.1\r\n"
//			"Host: 192.168.1.42:5000\r\n"
//			"Content-Type: application/json\r\n"
//			"Content-Length: %d\r\n"
//			"Connection: keep-alive\r\n"
//			"\r\n"
//			"%s",
//			content_length, json_body);
//		int sent = send(SOCK_TCPC, (uint8_t*)http_post, strlen(http_post));
//		if (sent <= 0) {
//			printf("Send failed\n");
//		} else {
//			 HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
//			printf("POST sent\n");
//		}
////	 Optionally receive response
////		    uint8_t rx_buff[1024] = {0};
////		    int32_t recv_len = recv(SOCK_TCPC, rx_buff, sizeof(rx_buff));
////		    int32_t len = recv(SOCK_TCPC, rx_buf, RX_BUF_SIZE );  // Leave space for null terminator
//		    close(SOCK_TCPC);
//
//		}
//}
int send_post_request(const char *data)
{
    uint32_t start;
    int8_t sock;

    /* Open socket */
    sock = socket(SOCK_TCPC, Sn_MR_TCP, SRC_PORT, 0);
    if (sock != SOCK_TCPC)
    {
        printf("Socket open failed\n");
        return -1;
    }

    /* Connect */
    connect(SOCK_TCPC, dest_ip, dest_port);

    start = xTaskGetTickCount();
    while (getSn_SR(SOCK_TCPC) != SOCK_ESTABLISHED)
    {
        if (getSn_SR(SOCK_TCPC) == SOCK_CLOSED)
        {
            printf("Connect failed\n");
            close(SOCK_TCPC);
            return -2;
        }

        if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(300))
        {
            printf("Connect timeout\n");
            close(SOCK_TCPC);
            return -3;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Build POST */
//    char json_body[64];
//    char http_post[256];

    snprintf(json_body, sizeof(json_body),
             "{\"data\":\"%s\"}", data);

    int content_length = strlen(json_body);

    snprintf(http_post, sizeof(http_post),
        "POST /sensor_data HTTP/1.1\r\n"
        "Host: 192.168.1.43:5000\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        content_length, json_body);

    /* Send */
    int ret = send(SOCK_TCPC, (uint8_t *)http_post, strlen(http_post));
    if (ret <= 0)
    {
        printf("Send failed\n");
        close(SOCK_TCPC);
        return -4;
    }

    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    printf("POST sent: %s\n", data);

    /* Close */
    disconnect(SOCK_TCPC);
    close(SOCK_TCPC);

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
  MX_SPI1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  reg_wizchip_cs_cbfunc(cs_sel,cs_desel);
  reg_wizchip_spi_cbfunc(spi_rb,spi_wb);
  wizchip_init(bufSize,bufSize);
  wiz_NetTimeout net_timeout;
  net_timeout.retry_cnt  = 3;      // retry 3 times
  net_timeout.time_100us = 2500;  // 1 second per retry
  wizchip_settimeout(&net_timeout);
  wiz_NetInfo netInfo ={ .mac={0x00,0x08,0xdc,0xab,0xcd,0xef}, //Mac address
					  .ip ={192,168,1,113}, 					// IP address
					  .sn ={255,255,255,0},					//Subnet mask
					  .gw ={192,168,1,1}};					//Gateway address
  HAL_Delay(500);
  wizchip_setnetinfo(&netInfo);
  HAL_Delay(500);
  wizchip_getnetinfo(&getInfo);
  HAL_TIM_Base_Start_IT(&htim3);
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of Server_Task */
  Server_TaskHandle = osThreadNew(handle_server, NULL, &Server_Task_attributes);

  /* creation of Loop_task */
  Loop_taskHandle = osThreadNew(Loop_Scanning, NULL, &Loop_task_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
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
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
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
  htim3.Init.Prescaler = 1343;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 62499;
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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(RL_GPIO_Port, RL_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : Loop2_Pin Loop1_Pin */
  GPIO_InitStruct.Pin = Loop2_Pin|Loop1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : RL_Pin */
  GPIO_InitStruct.Pin = RL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CS_Pin */
  GPIO_InitStruct.Pin = CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_handle_server */
/**
  * @brief  Function implementing the Server_Task thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_handle_server */
void handle_server(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
//	  uint8_t phy = getPHYCFGR();
//	 	 	 	  if ((phy & 0x01) == 0) {
//	 	 	 	// Link is down
//	 	 	 		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
//	 	 	 	   close(SOCK_TCPC);
////	 	 	 	   socket(SOCK_TCPC, Sn_MR_TCP, SRC_PORT, 0);
////	 	 	 	   listen(SOCK_TCPC);
//	 	 	 	  }
//	 	 	 	  else
//	 	 	 	  {
//	 	 	 		  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
//	 	 	 		send_post_request();
//	 	 	 	  }

	  /* Check PHY link */
	  if (wizphy_getphylink() == PHY_LINK_OFF)
		 {
			 HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

			 if (getSn_SR(SOCK_TCPC) != SOCK_CLOSED)
			 {
				 close(SOCK_TCPC);
			 }

			 vTaskDelay(pdMS_TO_TICKS(500));
			 continue;
		 }
//
		 HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
//
//		 /* Process requests */
		 if (loop1_flag)
		 {
			 loop1_flag = 0;
			 send_post_request("Entry");
		 }
//
		 if (loop2_flag)
		 {
			 loop2_flag = 0;
			 send_post_request("Exit");
		 }

		 vTaskDelay(pdMS_TO_TICKS(50));

//    osDelay(1);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_Loop_Scanning */
/**
* @brief Function implementing the Loop_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Loop_Scanning */
void Loop_Scanning(void *argument)
{
  /* USER CODE BEGIN Loop_Scanning */
  /* Infinite loop */
  for(;;)
  {

	 	 	 	  check_inputs();
    osDelay(1);
  }
  /* USER CODE END Loop_Scanning */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM10 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM10)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */
  if (htim->Instance == TIM3)
  {
	  tmr++;
	  relay_signal++;
	  if(relay_signal>2)
		  {
		  	  relay_signal=0;
		  	HAL_GPIO_WritePin(RL_GPIO_Port, RL_Pin, GPIO_PIN_SET);
		  }
	  if(tmr>RESET_LOOP)
	  {
		  entry_Busy=0;
		  exit_Busy=0;
		  loop1_flag=0;
		  loop2_flag=0;
	  }

  }
  /* USER CODE END Callback 1 */
}

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
