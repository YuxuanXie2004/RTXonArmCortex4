/*
 * common.h
 *
 *  Created on: Jan 5, 2024
 *      Author: nexususer
 *
 *      NOTE: If you feel that there are common
 *      C functions corresponding to this
 *      header, then any C functions you write must go into a corresponding c file that you create in the Core->Src folder
 */

#ifndef INC_COMMON_H_
#define INC_COMMON_H_

#define TID_NULL 0 //predefined Task ID for the NULL task
#define MAX_TASKS 16 //maximum number of tasks in the system
#define MAIN_STACK_SIZE 0x400 //main stack: interrupts, setups, OS
#define STACK_SIZE 0x200 //min. size of each task’s stack
#define DORMANT 0 //state of terminated task
#define READY 1 //state of task that can be scheduled but is not running
#define RUNNING 2 //state of running task
#define FRAME_PAGE_SIZE 0x200 //Frame and Page Size
#define MAX_FRAME_COUNT 20
#define MAX_STACK 0x4000
#define MAX_HEAP 0x4000

//error codes 
#define RTX_OK 0
#define RTX_ERR -1

typedef unsigned int U32;
typedef unsigned short U16;
typedef char U8;
typedef unsigned int task_t;

// entry in page table
typedef struct {
	U32* page_addr;
	U8 page_status;
}PAGE_ENTRY;

// entry in frame table
typedef struct {
	U32* frame_addr;
	U32 tid;
}FRAME_ENTRY;

typedef struct task_control_block{
	void (*ptask)(void* args); //entry address
	U32 stack_high; //start starting address (high address) -- top of the task stack(?)
	U32* psp; //availible psp stack top 
	task_t tid; //task ID
	U8 state; //task's state — Check if Running, available, Blocked etc.
	U16 stack_size; //stack size. Must be a multiple of 8
}TCB;

void osIdleTaskRun();
void osKernelInit();
int osCreateTask(TCB* task);
int osKernelStart();
void osYield();
int osTaskInfo(task_t TID, TCB* task_copy);
task_t osGetTID ();
int osTaskExit();
#endif /* INC_COMMON_H_ */
