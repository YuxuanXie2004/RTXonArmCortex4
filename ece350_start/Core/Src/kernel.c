#include "main.h"
#include "k_task.h"
#include <limits.h>
#include "common.h"
#include <stdio.h>
#include "k_mem.h"

TCB task_list[MAX_TASKS];
kernel_state = -1;
U32* MSP_INIT_VAL; //beginning of stack 
U32* PSP_INIT_VAL; //beginning of PSP stack
U32 halt_TID; 
task_t current_tid;
// U32 systick_active = 0;
// U32 continue_schedule = 0;
// U32 resume_schedule = 0;

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

void osKernelInit(void){
    kernel_state = READY;
    HAL_SuspendTick();
    k_mem_init();
    halt_TID = -1;
    MSP_INIT_VAL = *(U32**)0x0;
    PSP_INIT_VAL = MSP_INIT_VAL - MAIN_STACK_SIZE; //MSP Size: 0x400

    task_list[0].ptask = NULL;
    task_list[0].stack_high = (U32*)((U32)k_mem_alloc(STACK_SIZE) + STACK_SIZE);
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
    // halt_TID = -1;
    // k_mem_init();
//    k_mem_state = 1; // Set k_mem_state to RUNNING after memory initialization
    // k_mem_init();
    current_tid = TID_NULL;
    SHPR3 = (SHPR3 & ~(0xFFU << 24)) | (0xF0U << 24);//SysTick is lowest priority (highest number)
    SHPR3 = (SHPR3 & ~(0xFFU << 16)) | (0xE0U << 16);//PendSV is in the middle
    SHPR2 = (SHPR2 & ~(0xFFU << 24)) | (0xD0U << 24);//SVC is highest priority (lowest number)
};

int osCreateTask(TCB* task){
    __disable_irq();
    if (task == NULL || task->stack_size < STACK_SIZE || k_mem_state != RUNNING || task->stack_size > (heap_end - heap_start)) {
        return RTX_ERR;
    }
    // Align stack size to 8 bytes
    if (task->stack_size % 8 != 0) {
        task->stack_size = 8 * ((task->stack_size / 8) + 1);
    }
    // Find a free slot in the task list
    int free_task = -1;
    for (int i = 1; i < MAX_TASKS; ++i) {
        if (task_list[i].state == NULL || task_list[i].state == DORMANT) {
            free_task = i;
            break;
        }
    }
    //no free task
    if (free_task == -1) {
        __enable_irq();
        return RTX_ERR;
    }
    // Temporarily set curr_id so k_mem_alloc tags ownership correctly
    int saved_tid = current_tid;
    current_tid = free_task;
    void* mem = k_mem_alloc(task->stack_size);
    __disable_irq();
    current_tid = saved_tid;
    if (mem == NULL) {
        __enable_irq();
        return RTX_ERR;
    }
    // Fill out TCB
    task->tid = free_task;
    task->stack_size = task->stack_size;
    task->stack_high = (U32*) ((U32)mem + (U32)task->stack_size);
    task->psp = task->stack_high;
    task->deadline = 5;
    task->time_remaining = 5;  // Used for remaining time slice
    task->state = READY;
    // Initialize stack frame
    U32* sp = task->stack_high;
    *(--sp) = 1 << 24;                 // xPSR
    *(--sp) = (U32) task->ptask;      // PC
    *(--sp) = 0xFFFFFFFD;             // LR
    for (int i = 0; i < 13; ++i) {
        *(--sp) = 0xA;                // R0–R12
    }
    for (int i = 0; i < 16; ++i) {
        *(--task->psp);               // Padding (if needed)
    }
    task->psp = sp;
    // Register task
    task_list[free_task] = *task;
    // Preemption check (EDF logic)
    if (kernel_state == RUNNING && task_list[current_tid].time_remaining > task_list[free_task].time_remaining) {
        // Preempt current task
        osYield_SaveTime();
        __enable_irq();
    }
    return RTX_OK;
}

int osKernelStart(void){
    if(kernel_state == RUNNING) {
        return RTX_ERR;
    }
    if(kernel_state != READY) {
        return RTX_ERR;
    }
    HAL_ResumeTick();

    U32 ready_task = -1;
    U32 min_deadline = -1;
    U32 found = 0;

    //find closest deadline task
    for (U32 i = 1; i < MAX_TASKS; ++i) {
        //skip dormant or non-ready tasks
        if (task_list[i].state != READY || i == halt_TID) {
            continue;
        }

        //first deadline
        if (min_deadline == -1){
            min_deadline = task_list[i].time_remaining;
            ready_task = i; //store the task ID of the task with the earliest deadline
            found = 1;
        }else if (task_list[i].time_remaining < min_deadline) { //check if the task has the shortest deadline so far
            min_deadline = task_list[i].time_remaining;
            ready_task = i;
            found = 1;
        }else if (task_list[i].time_remaining == min_deadline) { //in case of a tie, choose the task with the lower TID
            if (task_list[i].tid < task_list[ready_task].tid) {
                ready_task = i;
            }
        }
    }

    if(ready_task == -1) {
        ready_task = 0;
        __set_PSP(task_list[ready_task].psp);
        __enable_irq();
        __asm("SVC #18");
        // TRY
        task_list[ready_task].psp =__get_PSP();
        return RTX_OK;
    }

    kernel_state = RUNNING;
    task_list[ready_task].state = RUNNING;
    current_tid = task_list[ready_task].tid;
    // systick_active = 1;
    __set_PSP(task_list[ready_task].psp);
    __enable_irq();
    __asm("SVC #18");
    // TRY
    task_list[ready_task].psp =__get_PSP();
    return RTX_OK;
}

void osYield(){
    __disable_irq();
    //find current running task
    U32 running_task = -1;
    // for(U32  i = 1; i < MAX_TASKS; ++ i) {
    //     if (task_list[i].state == RUNNING) {
    //         running_task = i;
    //         break;
    //     }
    // }
    task_list[current_tid].state = READY;
    halt_TID = task_list[current_tid].tid; //save halt tid
    task_list[current_tid].time_remaining = task_list[current_tid].deadline; //reset deadline

    //context switching
    task_list[current_tid].psp =__get_PSP(); //save halt tid psp
    __enable_irq();
    // systick_active = 0; 
    // continue_schedule = 1;
    // while(resume_schedule != 1){}
    // systick_active = 1;
    // continue_schedule = 0;
    __asm("SVC #17");
    // TRY
    // for(U32  i = 1; i < MAX_TASKS; ++ i) {
    //     if (task_list[i].state == RUNNING) {
    //         running_task = i;
    //         break;
    //     }
    // }
    task_list[current_tid].psp =__get_PSP();
    // current_tid = running_task;
    return;
}

void osYield_SaveTime(){
    __disable_irq();
    //find current running task
    U32 running_task = -1;
    // for(U32  i = 1; i < MAX_TASKS; ++ i) {
    //     if (task_list[i].state == RUNNING) {
    //         running_task = i;
    //         break;
    //     }
    // }
    task_list[current_tid].state = READY;
    halt_TID = task_list[current_tid].tid; //save halt tid
    // task_list[current_tid].time_remaining = task_list[current_tid].deadline; //reset deadline

    //context switching
    task_list[current_tid].psp =__get_PSP(); //save halt tid psp
    __enable_irq();
    // systick_active = 0; 
    // continue_schedule = 1;
    // while(resume_schedule != 1){}
    // systick_active = 1;
    // continue_schedule = 0;
    __asm("SVC #17");
    // TRY
    // for(U32  i = 1; i < MAX_TASKS; ++ i) {
    //     if (task_list[i].state == RUNNING) {
    //         running_task = i;
    //         break;
    //     }
    // }
    task_list[current_tid].psp =__get_PSP();
    // current_tid = running_task;
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
        task_copy->deadline = task_list[TID].deadline;
        task_copy->time_remaining = task_list[TID].time_remaining;
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

    return 0;
}

int osTaskExit(){
    __disable_irq();
    U32 running_task = current_tid;

    if (running_task == 0 || task_list[running_task].state != RUNNING) {
        __enable_irq();
        return RTX_ERR;
    }

    // Calculate the original mem pointer from stack_high and stack_size
    void* mem_ptr = (void*)((U32)task_list[running_task].stack_high - task_list[running_task].stack_size);
    int result = k_mem_dealloc(mem_ptr);
    if (result != RTX_OK) {
        __enable_irq();
        return RTX_ERR;
    }

    // Mark task as DORMANT and clear stack info
    task_list[running_task].state = DORMANT;
    task_list[running_task].stack_high = NULL;
    task_list[running_task].stack_size = 0;
    halt_TID = running_task;

    // Save PSP and trigger scheduler
    task_list[running_task].psp = (U32*)__get_PSP();
    __enable_irq();
    __asm("SVC #17");  // Trigger context switch via PendSV

    return RTX_OK;
}

// void RR_Scheduler(){
//    task_list[halt_TID].psp = (U32*) __get_PSP(); //curr of saved task
//    U32 ready_task = halt_TID+1;
//    U32 found = -1;

//    if (ready_task == MAX_TASKS){
//        ready_task = 1;
//    }

//    for (U32 i = 0; i < MAX_TASKS-1; ++i){
//        if (task_list[ready_task].state == READY){
//            found = 0;
//            break;
//        }else {
//            ready_task++;
//            if (ready_task == MAX_TASKS){
//                ready_task = 1;
//            }
//        }
//    }

//    //no ready tasks
//    if (found == -1){
//         __set_PSP(task_list[halt_TID].psp);
//         task_list[halt_TID].state = RUNNING;
//         current_tid = task_list[halt_TID].tid;
//    }else {
//         __set_PSP(task_list[ready_task].psp);
//        task_list[ready_task].state = RUNNING;
//        current_tid = task_list[ready_task].tid;
//    }
// }

void EDF_Scheduler() {
    __disable_irq();
    task_list[halt_TID].psp = (U32*) __get_PSP(); // Save current PSP

    U32 ready_task = -1;
    U32 min_deadline = -1;

    // Find task with earliest deadline
    for (U32 i = 1; i < MAX_TASKS; ++i) {
        if (task_list[i].state != READY || i == halt_TID) continue;
        if (task_list[i].time_remaining < min_deadline || min_deadline == -1) {
            min_deadline = task_list[i].time_remaining;
            ready_task = i;
        }
    }

    if (ready_task != -1) {
        __set_PSP(task_list[ready_task].psp);
        task_list[ready_task].state = RUNNING;
        current_tid = ready_task;
    } else {
        // Fallback to idle task
        __set_PSP(task_list[0].psp);
        current_tid = 0;
    }

    // Reset halt_TID to allow future scheduling of this task
    halt_TID = -1;  // Add this line

    __enable_irq();
}



int osSetDeadline(int deadline, task_t TID) {
    __disable_irq();
    if (deadline <= 0) {
        __enable_irq();
        return RTX_ERR;
    }

    //find task
    U32 change_task = -1;
    for (U32 i = 1; i < MAX_TASKS; i++) {
        if (task_list[i].tid == TID) {
            change_task = i;
            break;
        }
    }

    if (change_task == -1 || task_list[change_task].state != READY){
        __enable_irq();
        return RTX_ERR;
    }

    task_list[change_task].deadline = deadline;
    task_list[change_task].time_remaining = deadline;

    __enable_irq();
    // if (task_list[change_task].time_remaining < task_list[current_tid].time_remaining){
    //     __disable_irq();
    //     task_list[current_tid].state = READY;
    //     halt_TID = task_list[current_tid].tid; //save halt tid
    //     task_list[current_tid].time_remaining = task_list[current_tid].deadline; //reset deadline

    //     //context switching
    //     task_list[current_tid].psp =__get_PSP(); //save halt tid psp
    //     __enable_irq();
    //     __asm("SVC #17");
    //     task_list[current_tid].psp =__get_PSP();
    // }
    osYield_SaveTime();

    return RTX_OK;
}

int osCreateDeadlineTask(int deadline, TCB* task) {
    __disable_irq();
    if (task == NULL || deadline <= 0 || task->stack_size < STACK_SIZE 
            || k_mem_state != RUNNING || task->stack_size > heap_end - heap_start) {
        return RTX_ERR;
    }
    // Align stack size to 8 bytes
    if (task->stack_size % 8 != 0) {
        task->stack_size = 8 * ((task->stack_size / 8) + 1);
    }
    // Find a free slot in the task list
    int free_task = -1;
    for (int i = 1; i < MAX_TASKS; ++i) {
        if (task_list[i].state == NULL || task_list[i].state == DORMANT) {
            free_task = i;
            break;
        }
    }
    if (free_task == -1) {
        __enable_irq();
        return RTX_ERR;
    }
    // Temporarily set curr_id so k_mem_alloc tags ownership correctly
    int saved_tid = current_tid;
    current_tid = free_task;
    void* mem = k_mem_alloc(task->stack_size);
    __disable_irq();
    current_tid = saved_tid;
    if (mem == NULL) {
        __enable_irq();
        return RTX_ERR;
    }
    // Fill out TCB
    task->tid = free_task;
    task->stack_size = task->stack_size;
    task->stack_high = (U32*) ((U32)mem + (U32)task->stack_size);
    task->psp = task->stack_high;
    task->deadline = deadline;
    task->time_remaining = deadline;  // Used for remaining time slice
    task->state = READY;
    // Initialize stack frame
    U32* sp = task->stack_high;
    *(--sp) = 1 << 24;                 // xPSR
    *(--sp) = (U32) task->ptask;      // PC
    *(--sp) = 0xFFFFFFFD;             // LR
    for (int i = 0; i < 13; ++i) {
        *(--sp) = 0xA;                // R0–R12
    }
    for (int i = 0; i < 16; ++i) {
        *(--task->psp);               // Padding (if needed)
    }
    task->psp = sp;
    // Register task
    task_list[free_task] = *task;
    // Preemption check (EDF logic)
    if (kernel_state == RUNNING && task_list[current_tid].time_remaining > deadline) {
        // Preempt current task
        osYield_SaveTime();
        __enable_irq();
        // osYield();
    }
    return RTX_OK;
}

void osSleep(int timeInMs)
{ 
    __disable_irq();
    U32 running_task = -1;
    task_list[current_tid].state = SLEEPING;
    task_list[current_tid].time_remaining = timeInMs;
    halt_TID = current_tid;

    task_list[current_tid].psp =__get_PSP(); //save halt tid psp
    __enable_irq();
    // systick_active = 0; 
    // continue_schedule = 1;
    // while(resume_schedule != 1){}
    // systick_active = 1;
    // continue_schedule = 0;
    __asm("SVC #17");

    // TRY
    // for(U32  i = 1; i < MAX_TASKS; ++ i) {
    //     if (task_list[i].state == RUNNING) {
    //         running_task = i;
    //         break;
    //     }
    // }
    task_list[current_tid].psp =__get_PSP();
    // current_tid = running_task;
    return;
}

void osPeriodYield()
{
    __disable_irq();
    U32 running_task = -1;
    task_list[current_tid].state = SLEEPING;
    // task_list[current_tid].time_remaining = timeInMs;
    halt_TID = current_tid;

    task_list[current_tid].psp =__get_PSP(); //save halt tid psp
    __enable_irq();
    // systick_active = 0; 
    // continue_schedule = 1;
    // while(resume_schedule != 1){}
    // systick_active = 1;
    // continue_schedule = 0;
    __asm("SVC #17");

    // TRY
    // for(U32  i = 1; i < MAX_TASKS; ++ i) {
    //     if (task_list[i].state == RUNNING) {
    //         running_task = i;
    //         break;
    //     }
    // }
    task_list[current_tid].psp =__get_PSP();
    // current_tid = running_task;
    return;
    // task_list[current_tid].psp = (U32*)__get_PSP(); 
    // task_list[current_tid].state = SLEEPING;

    // // Automatically calculate remaining time until period ends
    // task_list[current_tid].time_remaining = task_list[current_tid].deadline; 
    // halt_TID = current_tid;
    // __enable_irq();
    // __asm("SVC #17");
}
