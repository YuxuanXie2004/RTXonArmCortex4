/*
 * k_mem.h
 *
 *  Created on: Jan 5, 2024
 *      Author: nexususer
 *
 *      NOTE: any C functions you write must go into a corresponding c file that you create in the Core->Src folder
 */
#include "main.h"
#include "common.h"

#ifndef INC_K_MEM_H_
#define INC_K_MEM_H_

#define FREE 0 
#define ALLOCATED 1
#define META_SIZE 20

typedef struct memory_block{
    struct memory_block* next_block; 
    struct memory_block* prev_block;
    U32 block_high;
    U32 block_size; 
    U8 state; 
    U32 block_tid; 
}MEMBLOCK;

extern int k_mem_state;
extern U32 heap_start;
extern U32 heap_end;
extern MEMBLOCK* freelist_head; 
extern MEMBLOCK* allocatedlist_head;

int k_mem_init();
void* k_mem_alloc(size_t size);
int k_mem_dealloc(void* ptr);
int k_mem_count_extfrag(size_t size);

#endif /* INC_K_MEM_H_ */
