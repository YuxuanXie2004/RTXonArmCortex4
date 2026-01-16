#include "main.h"
#include "common.h"

//TCB task_list[MAX_TASKS];
//U32 halt_TID;
//task_t current_tid;

void osIdleTaskRun();

void osKernelInit();
int osCreateTask(TCB* task);
int osKernelStart();
void osYield();
int osTaskInfo(task_t TID, TCB* task_copy);
task_t osGetTID ();
int osTaskExit();
int osSetDeadline(int deadline, task_t TID);
void osSleep(int timeInMs);
void osPeriodYield();
int osCreateDeadlineTask(int deadline, TCB* task);
