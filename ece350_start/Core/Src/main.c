#include "common.h"
#include "k_task.h"
#include "main.h"
#include <stdio.h>

// Task Function Prototypes
void Task1(void *);
void Task2(void *);

int main(void) {
    /* MCU Configuration: Don't change this or the whole chip won't work! */

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();

    /* Initialize the RTOS Kernel */
    osKernelInit();

    /* Create Task 1 */
    TCB st_mytask1;
    st_mytask1.stack_size = 0x400;
    st_mytask1.ptask = &Task1;
    osCreateTask(&st_mytask1);

    /* Create Task 2 */
    TCB st_mytask2;
    st_mytask2.stack_size = 0x400;
    st_mytask2.ptask = &Task2;
    osCreateTask(&st_mytask2);

    /* Start the Kernel (starts running tasks) */
    osKernelStart();

    /* Infinite loop (should never reach here) */
    while (1);
}

/* Task 1: Stores "hello!" in stack memory */
void Task1(void *arg) {
    char stack_msg[] = "hello!"; // Stored on stack

    while (1) {
        printf("task-1: %s\r\n", stack_msg); // Verify if stack data is intact
        osYield(); // Yield to Task2
    }
}

/* Task 2: Stores "world!" in stack memory */
void Task2(void *arg) {
    char stack_msg[] = "world!"; // Stored on stack

    while (1) {
        printf("task-2: %s\r\n", stack_msg); // Verify if stack data is intact
        osYield(); // Yield to Task1
    }
}
