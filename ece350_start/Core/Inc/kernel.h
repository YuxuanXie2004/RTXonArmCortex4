#include "main.h"
#include "common.h"

void osIdleTaskRun();

void osKernelInit();
int osCreateTask(TCB* task);
int osKernelStart();
void osYield();
int osTaskInfo(task_t TID, TCB* task_copy);
task_t osGetTID ();
int osTaskExit();