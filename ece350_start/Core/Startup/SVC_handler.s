.syntax unified
.cpu cortex-m4
.fpu softvfp
.thumb

.global SVC_Handler
.thumb_func
SVC_Handler:
	TST lr, 4
	ITE EQ
	MRSEQ r0, MSP
	MRSNE r0, PSP
	B SVC_Handler_Main

.global PendSV_Handler
.thumb_func
PendSV_Handler:
	CMP r0, #17
	BEQ ContextSwitching_Handler
	CMP r0, #18
	BEQ Restore_Handler

.global ContextSwitching_Handler
.thumb_func
ContextSwitching_Handler:

	MRS r0, PSP //get the current PSP and move it to r0 
	STMDB r0!, {r4-r11} //Push registers R4-R11 back onto its own stack
  MSR PSP, r0
	BL RR_Scheduler //call the scheduler to find the next available task 
  MRS r1, PSP
  LDMIA r1!, {r4-r11} //Restore R4-R11 from the next task's stack
  MSR PSP, r1 //Set PSP to the next task's stack pointer
  MOV lr, #0xFFFFFFFD //Return to Thread Mode, using PSP
  BX lr //Branch back to the restored task

.global Restore_Handler
.thumb_func
Restore_Handler:
  MRS r0, PSP
  LDMIA r0!, {r4-r11} //manually restore R4-R11 from the stack
  MSR PSP, r0 //Sets PSP to the loaded stack pointer
  MOV lr, #0xFFFFFFFD //ensure task runs in thread mode 
  BX lr //Starts execution in the first task
