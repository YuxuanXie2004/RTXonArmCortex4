#include "main.h"
#include <stdio.h>
#include "common.h"
#include "k_task.h"
#include "k_mem.h"

#define  ARM_CM_DEMCR      (*(uint32_t *)0xE000EDFC)
#define  ARM_CM_DWT_CTRL   (*(uint32_t *)0xE0001000)
#define  ARM_CM_DWT_CYCCNT (*(uint32_t *)0xE0001004)

int i_test = 0;

int i_test2 = 0;

void Task1(void *) {
	while(1){
	  printf("1\r\n");
	  for (int i_cnt = 0; i_cnt < 5000; i_cnt++);
	  osYield();
	}
 }
 
 
 void Task2(void *) {
	while(1){
	  printf("2\r\n");
	  for (int i_cnt = 0; i_cnt < 5000; i_cnt++);
	  osYield();
	}
 }
 
 
 void Task3(void *) {
	while(1){
	  printf("3\r\n");
	  for (int i_cnt = 0; i_cnt < 5000; i_cnt++);
	  osYield();
	}
 }

int main(void)
{

  /* MCU Configuration: Don't change this or the whole chip won't work!*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* MCU Configuration is now complete. Start writing your code below this line */

  osKernelInit();
  k_mem_init();

  TCB st_mytask;
  st_mytask.stack_size = STACK_SIZE;
  st_mytask.ptask = &Task1;
  osCreateTask(&st_mytask);


  st_mytask.ptask = &Task2;
  osCreateTask(&st_mytask);


  st_mytask.ptask = &Task3;
  osCreateTask(&st_mytask);

  osKernelStart();
  while (1) {
  }
 }
