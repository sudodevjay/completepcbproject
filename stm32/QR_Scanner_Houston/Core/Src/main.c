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
#include "socket.h"
#include "wizchip_conf.h"
#include "loopback.h"
#include "w5500.h"
#include "dhcp.h"
#include "net_config.h"
#include "http_server.h"
#include "mac_eeprom.h"
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
//#define HLT_CMD "|HLT%"
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c3;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */
uint8_t socket_buf[100];     						// Buffer to receive data
uint8_t rcvBuf[20],bufSize[8]={2,2,2,2,2,2,2,2};
int32_t len;										//Len of data recived
wiz_NetInfo getInfo ={ .mac={0},  					//Mac address
	  		  	  	   .ip ={0}, 					// IP address
	    			   .sn ={0},					//Subnet mask
	    			   .gw ={0}};					//Gateway address

char RxData[100]={0};
char RxLen=0;
char Flag=0;
char TxData[120] = {0};  // New array to hold data to be sent
int TxLen = 0;          // Length of the data to be sent
char timer=0,timer2=0,flag2=0;
char relayFlg=0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_TIM3_Init(void);
static void MX_I2C3_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//// SPI Functions  /////
void cs_sel(){
	 HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
}
void cs_desel(){
	HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);
}
uint8_t spi_rb(void){
	uint8_t rbuf;
	HAL_SPI_Receive(&hspi1,&rbuf,1,0xFFFFFFFF);
	return rbuf;
}
void spi_wb(uint8_t b){
	HAL_SPI_Transmit(&hspi1,&b,1,0xFFFFFFFF);
}


//#define MAX_DHCP_RETRY 5
//#define SOCK_DHCP 1
//
//uint8_t dhcp_buffer[1024];
//wiz_NetInfo netInfo;
//
//void my_ip_assign(void)
//{
//    getIPfromDHCP(netInfo.ip);
//    getGWfromDHCP(netInfo.gw);
//    getSNfromDHCP(netInfo.sn);
//    getDNSfromDHCP(netInfo.dns);
//    netInfo.dhcp = NETINFO_DHCP;
//    setSHAR(netInfo.mac);
//    wizchip_setnetinfo(&netInfo);
//
//    printf("DHCP assigned IP: %d.%d.%d.%d\r\n",
//        netInfo.ip[0], netInfo.ip[1], netInfo.ip[2], netInfo.ip[3]);
//}
//
//void my_ip_conflict(void)
//{
//    printf("IP conflict detected by DHCP!\r\n");
//    // Handle conflict - possibly halt or retry
//}

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
  MX_USART6_UART_Init();
  MX_TIM3_Init();
  MX_I2C3_Init();
  /* USER CODE BEGIN 2 */
   reg_wizchip_cs_cbfunc(cs_sel,cs_desel);
   reg_wizchip_spi_cbfunc(spi_rb,spi_wb);
   wizchip_init(bufSize,bufSize);
   wiz_NetInfo netInfo;
   uint8_t haveFlashConfig = NetConfig_Load(&netInfo);
   if (!haveFlashConfig)
   {
	   wiz_NetInfo defaultNetInfo ={ .mac={0x00,0x09,0xdc,0xab,0xcd,0xef}, //Mac address (fallback if EEPROM read fails)
			   	   	   	   	   	     .ip ={192,168,1,100}, 					// IP address
			   	   	   	   	   	     .sn ={255,255,255,0},					//Subnet mask
			   	   	   	   	   	     .gw ={192,168,1,1}};					//Gateway address
	   netInfo = defaultNetInfo;
   }

   uint8_t eepromMac[6];
   if (MAC_EEPROM_Read(&hi2c3, eepromMac))						// 24AA02E48T on I2C3 - real, unique MAC
   {
	   memcpy(netInfo.mac, eepromMac, 6);
   }

   uint8_t forceDefaultIp = (HAL_GPIO_ReadPin(DI_GPIO_Port, DI_Pin) == GPIO_PIN_RESET); // DI held LOW at power-on -> factory reset network config
   if (forceDefaultIp)
   {
	   uint8_t defIp[4] = {192,168,1,100};
	   uint8_t defSn[4] = {255,255,255,0};
	   uint8_t defGw[4] = {192,168,1,1};
	   memcpy(netInfo.ip, defIp, 4);
	   memcpy(netInfo.sn, defSn, 4);
	   memcpy(netInfo.gw, defGw, 4);
   }

   if (!haveFlashConfig || forceDefaultIp)
   {
	   NetConfig_Save(&netInfo);								// persist first-time defaults / DI factory reset (incl. EEPROM MAC if found)
   }

   HAL_Delay(500);
   wizchip_setnetinfo(&netInfo);
   HAL_Delay(500);
   wizchip_getnetinfo(&getInfo);

   HAL_UARTEx_ReceiveToIdle_IT(&huart6,RxData,50);
   HAL_TIM_Base_Start_IT(&htim3);
//   HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
//   HAL_GPIO_WritePin(R2_GPIO_Port, R2_Pin, GPIO_PIN_SET);
   //// this is for DHCP protocol ////


//   wiz_NetInfo netInfo = {
//       .mac = {0x00, 0x08, 0xdc, 0xab, 0xcd, 0xef},
//       .dhcp = NETINFO_DHCP // Set DHCP mode
//   };
//
//   setSHAR(netInfo.mac);  // Set MAC address
//   wizchip_setnetinfo(&netInfo);
//
//   // Initialize DHCP
//   DHCP_init(SOCK_DHCP, dhcp_buffer);
//   reg_dhcp_cbfunc(my_ip_assign, my_ip_assign, my_ip_conflict);

//   int dhcp_retry = 0;
//   while (1)
//   {
//       int8_t dhcp_status = DHCP_run();
//
//       if (dhcp_status == DHCP_IP_LEASED || dhcp_status == DHCP_IP_CHANGED)
//       {
//           printf("DHCP Success\r\n");
//           break;
//       }
//       else if (dhcp_status == DHCP_FAILED)
//       {
//           dhcp_retry++;
//           if (dhcp_retry > MAX_DHCP_RETRY)
//           {
////               printf("DHCP Failed - Falling back or resetting...\r\n");
////               // Optionally assign static IP or reset
////               while (1); // Halt or reset system
//           }
//       }
//
//       HAL_Delay(1000); // Wait a bit before retrying
//   }

//// END here for DHCP protocol ////

	#define SOCK_TCPS 0         							     // Socket 0
	#define PORT_TCPS 6000     								     // Listening port
	socket(SOCK_TCPS, Sn_MR_TCP, PORT_TCPS, 0); 				 // Open TCP socket
	listen(SOCK_TCPS);

	HTTPServer_Init();											 // Socket 1, port 80 - web-based network settings
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  HTTPServer_Poll();

	  switch (getSn_SR(SOCK_TCPS)) {
	 	 	          case SOCK_ESTABLISHED:
	 	 	              if (getSn_IR(SOCK_TCPS) & Sn_IR_CON)
	 	 	              {
	 	 	                  setSn_IR(SOCK_TCPS, Sn_IR_CON); 			// Clear connection interrupt
	 	 	                  printf("Client connected\r\n");
	 	 	              }
	 	 	             uint8_t phy = getPHYCFGR();
	 					   if ((phy & 0x01) == 0) {
	 						   	   	   	   	   	   	   	   	   	   	   	// Link is down
	 						   close(SOCK_TCPS);
	 						   socket(SOCK_TCPS, Sn_MR_TCP, PORT_TCPS, 0);
	 						   listen(SOCK_TCPS);
	 					   }
	 	 	              // Check for received data
	 	 	              if ((len = getSn_RX_RSR(SOCK_TCPS)) > 0) {
	 	 	                  len = recv(SOCK_TCPS, socket_buf, len);  // Receive data
	 	 	                  socket_buf[len] = 0; // Null terminate
	 	 	                  printf("Received: %s\r\n", socket_buf);


	 	 	                // Check command and respond
	 	 	                    if (strcmp((char*)socket_buf, "|OPENEN%\r\n") == 0) {
	 	 	                    	send(SOCK_TCPS, "|OK%\n", strlen("|OK%\n"));
	 	 	                        HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
	 	 	                      HAL_GPIO_WritePin(R1_GPIO_Port, R2_Pin, GPIO_PIN_SET);
	 	 	                      relayFlg=1;
//	 	 	                        strcpy((char*)socket_buf, "R1ON ok");
//	 	 	                        send(SOCK_TCPS, socket_buf, strlen((char*)socket_buf));
//	 	 	                    }
//	 	 	                        else if (strcmp((char*)socket_buf, "R1OFF") == 0) {
//	 	 	                         HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
//	 	 	                        strcpy((char*)socket_buf, "R1OFF ok");
//	 	 	                        send(SOCK_TCPS, socket_buf, strlen((char*)socket_buf));
	 	 	                    }
//	 	 	                    else {
	 	 	                        // Optional: handle unknown command
//	 	 	                        strcpy((char*)socket_buf, "Unknown command");
//	 	 	                        send(SOCK_TCPS, socket_buf, strlen((char*)socket_buf));
//	 	 	                    }
	 	 	                  // Echo back the received data
//	 	 	                  send(SOCK_TCPS, socket_buf, len);
	 	 	              }
//	 	 	              uint8_t tx_buf[64];


	 	 	              if(flag2)
	 	 	              {
							  flag2=0;
							  timer2=0;
							  send(SOCK_TCPS, "|HLT%\n", strlen("|HLT%\n"));
	 	 	              }

	 	 	            if(Flag)
	 	 	            {
//	 	 	            	 Step 1: Find "QR" and "END"
	 	 	            	    char *start = strstr(RxData, "QR");
	 	 	            	    char *end = strstr(RxData, "END");

	 	 	            	    if (start != NULL && end != NULL && end > start)
	 	 	            	    {
	 	 	            	        start += 2; // move past "QR"

	 	 	            	        int data_len = end - start; // Length of data between QR and END

	 	 	            	        char extracted[50] = {0};
	 	 	            	        strncpy(extracted, start, data_len);
	 	 	            	        extracted[data_len] = '\0';

	 	 	            	        // Step 2: Format as ENDATA-<data>END
	 	 	            	        sprintf(TxData, "|ENQR-%s%%\n", extracted);

	 	 	            	        // Step 3: Calculate length
	 	 	            	        TxLen = strlen(TxData);

	 	 	            	        // Step 4: Send
	 	 	            	        if (Flag) {
	 	 	            	            send(SOCK_TCPS, TxData, TxLen);
	 	 	            	          HAL_GPIO_TogglePin (GPIOD, GPIO_PIN_2);
//
	 	 	            	            Flag = 0;
	 	 	            	        }
	 	 	            	    }
//	 	 	            	 send(SOCK_TCPS, RxData, RxLen);
//	 	 	            	 Flag=0;
	 	 	            }
	 	 	              break;

	 	 	          case SOCK_CLOSE_WAIT:
	 	 	              close(SOCK_TCPS);
	 	 	              socket(SOCK_TCPS, Sn_MR_TCP, PORT_TCPS, 0);
	 	 	              listen(SOCK_TCPS);
	 	 	              break;

	 	 	          case SOCK_CLOSED:
	 	 	              socket(SOCK_TCPS, Sn_MR_TCP, PORT_TCPS, 0);
	 	 	              listen(SOCK_TCPS);
	 	 	              break;

	 	 	          default:
	 	 	              break;
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
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  htim3.Init.Prescaler = 671;
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
  huart6.Init.BaudRate = 115200;
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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, R2_Pin|R1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : DI_Pin */
  GPIO_InitStruct.Pin = DI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DI_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : R2_Pin R1_Pin */
  GPIO_InitStruct.Pin = R2_Pin|R1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
//  HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_SET);
//HAL_GPIO_WritePin(R1_GPIO_Port, R2_Pin, GPIO_PIN_SET);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{

    if(huart->Instance==USART6)
   	{
   		RxLen = Size;
   		Flag=1;
   		HAL_UARTEx_ReceiveToIdle_IT(&huart6,RxData,50);
   	 HAL_GPIO_TogglePin (GPIOD, GPIO_PIN_2);
//    	counter1++;
//   		if(RxLen5>4)
//   		{
//   			FL.Flag.FlgRX5=1;
//   		}
   	}
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
	timer2++;
	if(timer2>6)flag2=1;
	if(relayFlg) timer++;			//500 ms intrrupt
	if(timer>1) 					// after 2 sec relay off
		{
		timer=0;
		relayFlg=0;
		HAL_GPIO_WritePin(R1_GPIO_Port, R1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(R1_GPIO_Port, R2_Pin, GPIO_PIN_RESET);
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
