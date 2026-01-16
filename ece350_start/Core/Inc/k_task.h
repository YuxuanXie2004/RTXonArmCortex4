/*
 * k_task.h
 *
 *  Created on: Jan 5, 2024
 *      Author: nexususer
 *
 *      NOTE: any C functions you write must go into a corresponding c file that you create in the Core->Src folder
 */

#ifndef INC_K_TASK_H_
#define INC_K_TASK_H_
#define SHPR2 (*((volatile U32*)0xE000ED1C))//SVC is bits 31-28
#define SHPR3 (*((volatile U32*)0xE000ED20))//SysTick is bits 31-28, and PendSV is bits 23-20
#define _ICSR *(U32*) 0xE000ED04



#endif /* INC_K_TASK_H_ */
