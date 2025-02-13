#include "main.h"
#include "common.h"
#include <stdio.h>

TCB task_list[MAX_TASKS];
TCB* current_tid;
int kernel_state = -1;
U32* MSP_INIT_VAL; //beginning of stack 
U32* PSP_INIT_VAL; //beginning of PSP stack
U32 halt_TID; 

void osIdleTaskRun(){
    U32 found = -1;
    U32 running_task = -1;
    while(1){
        for (U32 i = 1; i < MAX_TASKS; i++){
            if (task_list[i].state == READY){
                found = 0;
                running_task = i;
                break;
            }
        }
        if (found == 0){
            break;
        }
    }

    task_list[running_task].state = RUNNING;
    __set_PSP(task_list[running_task].psp);
    __asm("SVC #18");
    return;
}

void osKernelInit(){
    MSP_INIT_VAL = *(U32**)0x0;
    PSP_INIT_VAL = MSP_INIT_VAL - MAIN_STACK_SIZE; //MSP Size: 0x400

    task_list[0].ptask = NULL;
    task_list[0].stack_high = PSP_INIT_VAL;
    task_list[0].stack_size = STACK_SIZE;
    task_list[0].state = NULL;
    task_list[0].tid = TID_NULL;

    task_list[0].psp = PSP_INIT_VAL;
    *(--task_list[0].psp) = 1<<24; //This is xPSR, should be Thumb mode
    *(--task_list[0].psp) = (U32)osIdleTaskRun; // PC -- the function name
    *(--task_list[0].psp) = 0xFFFFFFFD; //LR : use the PSP
    for (int i = 0; i < 13; i++){
        *(--task_list[0].psp) = 0xA; //init rest of the regs
    }
    for (int i = 1; i < MAX_TASKS; i++){
        task_list[i].ptask = NULL;
        task_list[i].stack_high = 0;
        task_list[i].stack_size = 0;
        task_list[i].psp = 0;
        task_list[i].state = NULL;
        task_list[i].tid = NULL;
    }
    kernel_state = READY;
    halt_TID = -1;
};

int osCreateTask(TCB* task){
    int found_spot = -1;
    U32 insert_i;
    U32 high_val = 0;
    U32 max_size = MSP_INIT_VAL - MAX_STACK;
    if(task->stack_size < STACK_SIZE || task->stack_size > MAX_STACK) {
        return RTX_ERR;
    }

    if(kernel_state == -1) {
        return RTX_ERR;
    }

    if(task->stack_size%8 != 0){
        U32 div = task->stack_size/8;
        task->stack_size = 8*(div+1);
    }

    for(insert_i = 1; insert_i < MAX_TASKS; ++ insert_i) {
        if(task_list[insert_i].state == NULL){
            if ( ((task_list[insert_i-1].stack_high - task_list[insert_i-1].stack_size) - task->stack_size) >= max_size) {
                task->tid = insert_i;
                task->stack_high = task_list[insert_i-1].stack_high - task_list[insert_i-1].stack_size;
                task->psp = task->stack_high;
                task->state = READY;

                U32* stackptr;

                stackptr = (U32*)task->stack_high;
                *(--stackptr) = 1<<24;
                *(--stackptr) = (U32)task->ptask;
                *(--stackptr) = 0xFFFFFFFD;
                for (int i=0; i<13; i++){
                    *(--stackptr) = 0xA;
                }
                for (int i=0; i<16; i++){
                    *(--task->psp);
                }

                task_list[insert_i] = *task;

                //////
                // printf("creat index:=%d, id=%d, stack_high=%p \r\n",insert_i, task_list[insert_i].tid, task_list[insert_i].stack_high);

                return RTX_OK;
            }else{
                return RTX_ERR;
            }
        }else if(task_list[insert_i].state == DORMANT) {
            if(insert_i == MAX_TASKS - 1 || task_list[insert_i+1].state == NULL) {
                high_val = task_list[insert_i-1].stack_high - task_list[insert_i-1].stack_size;
                found_spot = 0;
                break;
            }else if( task->stack_size <= (task_list[insert_i].stack_high - task_list[insert_i+1].stack_high) ) {
                task->tid = insert_i;
                task->stack_high = task_list[insert_i-1].stack_high - task_list[insert_i-1].stack_size;
                task->psp = task->stack_high;
                task->state = READY;

                U32* stackptr;

                stackptr = (U32*)task->stack_high;
                *(--stackptr) = 1<<24;
                *(--stackptr) = (U32)task->ptask;
                *(--stackptr) = 0xFFFFFFFD;
                for (int i=0; i<13; i++){
                    *(--stackptr) = 0xA;
                }
                for (int i=0; i<16; i++){
                    *(--task->psp);
                }

                task_list[insert_i] = *task;

                //////
                                // printf("creat index:=%d, id=%d, stack_high=%p, size=%p \r\n",insert_i, task_list[insert_i].tid, task_list[insert_i].stack_high, task_list[insert_i].stack_size);


                return RTX_OK;
            }
        }
    }

    if(high_val - task->stack_size < max_size || found_spot == -1) {
        return RTX_ERR;
    }

    task->tid = insert_i;
    task->stack_high = task_list[insert_i-1].stack_high - task_list[insert_i-1].stack_size;
    task->psp = task->stack_high;
    task->state = READY;

    U32* stackptr;

    stackptr = (U32*)task->stack_high;
    *(--stackptr) = 1<<24;
    *(--stackptr) = (U32)task->ptask;
    *(--stackptr) = 0xFFFFFFFD;
    for (int i=0; i<13; i++){
        *(--stackptr) = 0xA;
    }
    for (int i=0; i<16; i++){
        *(--task->psp);
    }

    task_list[insert_i] = *task;
//    printf("create task%d\r\n", insert_i);

    //////
    // printf("creat index:=%d, id=%d, stack_high=%p, size=%p \r\n",insert_i, task_list[insert_i].tid, task_list[insert_i].stack_high, task_list[insert_i].stack_size);

    return RTX_OK;
}

int osKernelStart(){
    if(kernel_state == RUNNING) {
        return RTX_ERR;
    }
    if(kernel_state != READY) {
        return RTX_ERR;
    }
    U32 running_task = -1;
    for(U32  i = 1; i < MAX_TASKS; ++ i) {
        if (task_list[i].state == READY) {
            running_task = i;
            break;
        }
    }
    if(running_task == -1) {
        running_task = 0;
        __set_PSP(task_list[running_task].psp);
        __asm("SVC #18");
        // TRY
        task_list[running_task].psp =__get_PSP();
        return RTX_OK;
    }
    kernel_state = RUNNING;
    task_list[running_task].state = RUNNING;
    __set_PSP(task_list[running_task].psp);
    __asm("SVC #18");
    // TRY
    task_list[running_task].psp =__get_PSP();
    return RTX_OK;
}

void osYield(){
    U32 running_task = -1;
    for(U32  i = 1; i < MAX_TASKS; ++ i) {
        if (task_list[i].state == RUNNING) {
            running_task = i;
            break;
        }
    }
    task_list[running_task].state = READY;
    halt_TID = task_list[running_task].tid;
//    __set_PSP(task_list[running_task].psp);	//??

    task_list[running_task].psp =__get_PSP();
    __asm("SVC #17");
    // TRY
    for(U32  i = 1; i < MAX_TASKS; ++ i) {
        if (task_list[i].state == RUNNING) {
            running_task = i;
            break;
        }
    }
    task_list[running_task].psp =__get_PSP();
    return;
}

int osTaskInfo(task_t TID, TCB* task_copy){
    if ( TID < 0 || TID >= MAX_TASKS || task_list[TID].state == DORMANT ) {
        return RTX_ERR;
    }
    else {
        task_copy->ptask = task_list[TID].ptask;
        task_copy->stack_high = task_list[TID].stack_high;
        task_copy->psp = task_list[TID].psp;
        task_copy->stack_size = task_list[TID].stack_size;
        task_copy->state = task_list[TID].state;
        task_copy->tid = task_list[TID].tid;
        return RTX_OK;
    }
}

task_t osGetTID(){
    if( kernel_state == RUNNING ) {
        for (U32 i = 1; i < MAX_TASKS; ++i){
            if (task_list[i].state == RUNNING){
                return i; 
            }
        }
    }
    else {
        return 0;
    }
}

int osTaskExit(){
    U32 running_task = -1;

    for (U32 i = 1; i < MAX_TASKS; i++){
        if (task_list[i].state == RUNNING){
            running_task = i;
            break;
        }
    }

    //no running task
    if (running_task == -1){
        return RTX_ERR;
    }else{
        halt_TID = running_task;
        task_list[running_task].state = DORMANT;
        __set_PSP(task_list[running_task].psp);
        __asm("SVC #17");
        return RTX_OK;
    }
    
}

void RR_Scheduler(){
   task_list[halt_TID].psp = (U32*) __get_PSP(); //curr of saved task
   U32 ready_task = halt_TID+1;
   U32 found = -1;

   if (ready_task == MAX_TASKS){
       ready_task = 1;
   }

   for (U32 i = 0; i < MAX_TASKS-1; ++i){
       if (task_list[ready_task].state == READY){
           found = 0;
           break;
       }else {
           ready_task++;
           if (ready_task == MAX_TASKS){
               ready_task = 1;
           }
       }
   }

   //no ready tasks
   if (found == -1){
        __set_PSP(task_list[halt_TID].psp);
        task_list[halt_TID].state = RUNNING;
   }else {
        __set_PSP(task_list[ready_task].psp);
       task_list[ready_task].state = RUNNING;
   }
}
