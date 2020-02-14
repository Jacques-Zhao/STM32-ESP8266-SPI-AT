/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */     
#include <string.h>
#include <stdio.h>
#include "task.h"
#include "stream_buffer.h"
#include "semphr.h"
#include "usart.h"
#include "gpio.h"
#include "spi.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
 #define RXBUFFERSIZE 256
char RxBuffer[RXBUFFERSIZE];
char RxBufferBackup[RXBUFFERSIZE];
uint8_t aRxBuffer;
uint32_t Uart1_Rx_Cnt = 0;
uint32_t Uart1_Rx_Cnt_Backup = 0;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* List of commands */
#define CMD_WRITE_STATUS            0x01
#define CMD_WRITE_DATA              0x02
#define CMD_READ_DATA               0x03
#define CMD_READ_STATUS             0x04
#define SPI_BUFFER_MAX_SIZE         512

typedef enum {
    SPI_NULL = 0,
    SPI_WRITE,
    SPI_READ
} spi_master_mode_t;

typedef enum {false = 0,true = 1} bool;

static spi_master_mode_t intr_trans_mode = SPI_NULL;
static bool wait_recv_data = false;
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t rxdata[66], txdata[66];       // 1Byte cmd + 1Byte address + 64Byte data
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static StreamBufferHandle_t spi_master_send_ring_buf = NULL;
static StreamBufferHandle_t spi_master_recv_ring_buf = NULL;
static uint32_t transmit_len = 0;
SemaphoreHandle_t DataBinarySem01Handle;
static SemaphoreHandle_t UartBinarySemHandle;
/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void transmit_Task(void * argument);
static int8_t at_spi_load_data(uint8_t* buf, int32_t len);
/* USER CODE END FunctionPrototypes */

void uart_Task(void * argument);
void recv_task(void* arg);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];
  
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}                   
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* definition and creation of DataBinarySem01 */
  //osSemaphoreDef(DataBinarySem01);
  //DataBinarySem01Handle = osSemaphoreCreate(osSemaphore(DataBinarySem01), 1);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  //osSemaphoreDef(UartBinarySem);
  //UartBinarySemHandle = osSemaphoreCreate(osSemaphore(UartBinarySem), 1);
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  //osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  //defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
	/*
  osThreadDef(transmitTask, transmit_Task, osPriorityNormal, 0, 256);
  transmitTaskHandle = osThreadCreate(osThread(transmitTask), NULL);
	*/
	DataBinarySem01Handle = xSemaphoreCreateBinary();
	UartBinarySemHandle = xSemaphoreCreateBinary();
	spi_master_send_ring_buf = xStreamBufferCreate(SPI_BUFFER_MAX_SIZE, 256);
  spi_master_recv_ring_buf = xStreamBufferCreate(SPI_BUFFER_MAX_SIZE, 1);

	xTaskCreate(transmit_Task, "transmit_Task", 128, NULL, 6, NULL);
  xTaskCreate(uart_Task, "uart_Task", 64, NULL, 5, NULL);
	xTaskCreate(recv_task, "recv_Task", 64, NULL, 5, NULL);
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used 
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void uart_Task(void * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
	xSemaphoreTake(UartBinarySemHandle,0);
	// wakeup uart receive intr
	HAL_UART_Receive_IT(&huart1, (uint8_t *)&aRxBuffer, 1);
	printf("Test UART send\r\n");
  /* Infinite loop */
  for(;;)
  {
		xSemaphoreTake(UartBinarySemHandle,portMAX_DELAY);
		//printf("UART len: %d\r\n", Uart1_Rx_Cnt_Backup);
		RxBufferBackup[Uart1_Rx_Cnt_Backup] = '\0';
		//printf("UART DATA: %s", RxBufferBackup);
    at_spi_load_data((uint8_t*)RxBufferBackup, Uart1_Rx_Cnt_Backup);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

void spi_transmit_data(uint32_t len) 
{
		CS_low();
	  HAL_SPI_TransmitReceive(&hspi1, txdata, rxdata, len, 100);  
		CS_high();
}

void set_trans_len(uint32_t len) 
{
    /* Write status */
    txdata[0] = CMD_WRITE_STATUS;
    txdata[1] = (len >>  0) & 0xFF;
    txdata[2] = (len >>  8) & 0xFF;
    txdata[3] = (len >> 16) & 0xFF;
    txdata[4] = (len >> 24) & 0xFF;

    spi_transmit_data(5);
}

static uint32_t get_trans_len(void) 
{
    uint32_t len;

    /* Read status */
    txdata[0] = CMD_READ_STATUS;

    spi_transmit_data(5);

    len = rxdata[1] | rxdata[2] << 8 | rxdata[3] << 16 | rxdata[4] << 24;
    //printf("Data length for receive: %d bytes\r\n", (int)len);

    return len;
}

static void spi_send_data(char *str, uint32_t len)
{
	  uint32_t i;

    txdata[0] = CMD_WRITE_DATA;
    txdata[1] = 0x00;
    for (i = 0; i < len; ++i) {
        txdata[2 + i] = str[i];
    }

    spi_transmit_data(len + 2);
}

static void receive_data(uint32_t len) {
    if(len <= 0){
			printf("Len is error\r\n");
			return;
		}
    txdata[0] = CMD_READ_DATA;
    txdata[1] = 0x00;
    spi_transmit_data(2 + len);
    //printf("Data received: %.*s\r\n", (int)len, &rxdata[2]);
}

// SPI master sent to slave function
static int8_t at_spi_load_data(uint8_t* buf, int32_t len)
{
    if (len > SPI_BUFFER_MAX_SIZE) {
        printf("Send length %d is too large\r\n", len);
        return -1;
    }

    size_t xBytesSent = xStreamBufferSend(spi_master_send_ring_buf, (void*) buf, len, 100);

    if (xBytesSent != len) {
        printf("Send error, len:%d\r\n", xBytesSent);
        return -1;
    }

    if (intr_trans_mode == SPI_NULL) {
        xSemaphoreGive(DataBinarySem01Handle);
    }

    return 0;
}

void recv_task(void* arg)
{
    size_t xReceivedBytes;
    uint8_t read_data[1024 + 1];

    while (1) {
        xReceivedBytes = xStreamBufferReceive(spi_master_recv_ring_buf, read_data, 1024, 2000 / portTICK_RATE_MS);

        if (xReceivedBytes != 0) {
            read_data[xReceivedBytes] = '\0';
            printf("%s", read_data);
            fflush(stdout);    //Force to print even if have not '\n'
        }

        // steam buffer full
        if (wait_recv_data) {
            if (xStreamBufferBytesAvailable(spi_master_recv_ring_buf) > 64) {
							  wait_recv_data = false;
                xSemaphoreGive(DataBinarySem01Handle);
            }
        }
    }
}


void transmit_Task(void * argument)
{
	char trans_data[64];
  uint32_t read_len = 0;
  uint32_t recv_actual_len = 0;

	xSemaphoreTake(DataBinarySem01Handle,0);
	CS_high();
	printf("Start SPI thansmit\r\n");

  /* Infinite loop */
  for(;;)
  {
		xSemaphoreTake(DataBinarySem01Handle,portMAX_DELAY);
    if (intr_trans_mode == SPI_NULL) {       // Some data need to read or write???
        // have some data need to send ???
        if (xStreamBufferIsEmpty(spi_master_send_ring_buf) == pdFALSE) {
            intr_trans_mode = SPI_WRITE;
            transmit_len = xStreamBufferBytesAvailable(spi_master_send_ring_buf);
					  set_trans_len(transmit_len);
            continue;
        }

        // Check if there is any data to receive
        transmit_len  = get_trans_len();

        if (transmit_len > 0) {
            //printf("Receive data len: %d\n", transmit_len);
            intr_trans_mode = SPI_READ;
            continue;
        } else {
            printf("Nothing to do");
            continue;
        }
    }
		
		read_len =  transmit_len > 64 ? 64 : transmit_len;

    // SPI slave have some data want to transmit, read it
    if (intr_trans_mode == SPI_READ) {
        if (xStreamBufferSpacesAvailable(spi_master_recv_ring_buf) >= 64) {    // Stream buffer not full, can be read agian
					  receive_data(read_len);
            recv_actual_len = xStreamBufferSend(spi_master_recv_ring_buf, (void*) (rxdata + 2), read_len, 1000);
            if(recv_actual_len != read_len){
							printf("Receive len error\r\n");
							continue;
						}
            transmit_len -= read_len;

            if (transmit_len == 0) {
                intr_trans_mode = SPI_NULL;

                /* When SPI slave sending data , maybe MCU also have some date wait for send */
                if (xStreamBufferIsEmpty(spi_master_send_ring_buf) == pdFALSE) {
                    xSemaphoreGive(DataBinarySem01Handle);
                }
            }
        } else {   // stream buffer full, wait to be tacken out
            printf("Stream buffer full\r\n");
					  wait_recv_data = true;
        }


        // MCU want to send data to ESP8266
    } else if (intr_trans_mode == SPI_WRITE) {
        if (read_len > 0) {
            recv_actual_len = xStreamBufferReceive(spi_master_send_ring_buf,
                              (void*)trans_data,
                              read_len,
                              1000);

            if (recv_actual_len != read_len) {
                printf("Expect to send %d bytes, but only %d bytes\r\n", read_len, recv_actual_len);
                continue;
            }

						spi_send_data(trans_data, recv_actual_len);
            transmit_len -= read_len;
        } else {
            intr_trans_mode = SPI_NULL;

            if (xStreamBufferIsEmpty(spi_master_send_ring_buf) == pdFALSE) {
                xSemaphoreGive(DataBinarySem01Handle);
            } else {
                // if ring buffer is empty, send status=0 tell slave send done
                set_trans_len(0);
            }
        }
    }
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  /* NOTE: This function Should not be modified, when the callback is needed,
           the HAL_UART_TxCpltCallback could be implemented in the user file
   */
	BaseType_t xHigherPriorityTaskWoken;
	
	if(Uart1_Rx_Cnt >= 254)  // copy before over flow
	{
		Uart1_Rx_Cnt_Backup = Uart1_Rx_Cnt;
		memcpy(RxBufferBackup, RxBuffer, Uart1_Rx_Cnt);
		xSemaphoreGiveFromISR(UartBinarySemHandle,&xHigherPriorityTaskWoken);	
		  
		Uart1_Rx_Cnt = 0;
		memset(RxBuffer,0x00,sizeof(RxBuffer));	
        
	} else {
		RxBuffer[Uart1_Rx_Cnt++] = aRxBuffer;   //
	
		if((RxBuffer[Uart1_Rx_Cnt-1] == 0x0A)&&(RxBuffer[Uart1_Rx_Cnt-2] == 0x0D)) 
		{
      Uart1_Rx_Cnt_Backup = Uart1_Rx_Cnt;
		  memcpy(RxBufferBackup, RxBuffer, Uart1_Rx_Cnt);
			xSemaphoreGiveFromISR(UartBinarySemHandle,&xHigherPriorityTaskWoken);
			Uart1_Rx_Cnt = 0;
			memset(RxBuffer,0x00,sizeof(RxBuffer)); 
		}
	}
	
	HAL_UART_Receive_IT(&huart1, (uint8_t *)&aRxBuffer, 1); 
	
	if(xHigherPriorityTaskWoken) {
	  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
	
}

/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
