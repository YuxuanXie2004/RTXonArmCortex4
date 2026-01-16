#include "main.h"
#include "k_mem.h"
#include <stdio.h>
#include <stddef.h>

extern U32 _img_end;
extern U32 _estack;
extern U32 _Min_Stack_Size;
extern int kernel_state;
extern task_t current_tid;
int k_mem_state = -1;
U32 heap_start;
U32 heap_end;

MEMBLOCK* freelist_head; 
MEMBLOCK* allocatedlist_head;

int k_mem_init(){
    if (kernel_state == -1 || k_mem_state != -1){
        return RTX_ERR;
    }

    //k_mem init 
    k_mem_state = RUNNING;
    heap_start = (U32)&_img_end;
    heap_end = (U32)&_estack - (U32)&_Min_Stack_Size;

    freelist_head = (MEMBLOCK*)(heap_start);
    printf("Freelist Head: %x\r\n", (U32)(freelist_head));
    freelist_head->block_high = (U32)(freelist_head) + META_SIZE;
    printf("Freelist BlockHigh: %x\r\n", freelist_head->block_high);
    freelist_head->block_tid = -1;
    freelist_head->block_size = heap_end - heap_start;
    freelist_head->next_block = NULL;
    freelist_head->prev_block = NULL;
    freelist_head->state = FREE;

    allocatedlist_head = NULL;
    // allocatedlist_head->block_high = 0;
    // allocatedlist_head->block_tid = -1;
    // allocatedlist_head->block_size = 0;
    // allocatedlist_head->next_block = NULL;
    // allocatedlist_head->prev_block = NULL;
    // allocatedlist_head->state = ALLOCATED;

    return RTX_OK;
}

void* k_mem_alloc(size_t size) {
    if (size == 0 || k_mem_state != RUNNING)
        return NULL;

    size_t total_size = size + META_SIZE;
    if(total_size%4 != 0){
        U32 div = total_size/4;
        total_size = 4*(div+1);
    }
    
    MEMBLOCK* prev = NULL;
    MEMBLOCK* curr = freelist_head;

    while (curr != NULL)
{
        if (curr->block_size >= total_size)
        {
            //split block
            if (curr->block_size >= total_size + META_SIZE + 4)
            {
                MEMBLOCK* new_block = (MEMBLOCK*)((U32)curr + (U32)total_size);
                // MEMBLOCK* new_block = (MEMBLOCK*)(curr->block_high + size);
                new_block->block_high = (U32)(new_block) + META_SIZE; //curr->block_high + total_size;
                new_block->block_size = curr->block_size - total_size;
                new_block->block_tid = -1;

                // if (curr->prev_block == NULL){ //free first on the list 
                //     freelist_head = new_block;
                //     freelist_head->prev_block = NULL;
                //     // freelist_head->next_block = curr->next_block;
                // }
                new_block->next_block = curr->next_block;
                new_block->prev_block = curr;
                new_block->state = FREE;

                curr->block_size = total_size;
                curr->next_block = new_block;
                if (new_block->next_block != NULL) {
                    new_block->next_block->prev_block = new_block;
                }
            }

            //mark block as allocated
            curr->state = ALLOCATED;
            curr->block_tid = current_tid;//osGetTID();

            //remove block from freelist
            if (prev == NULL){ //head of freelist
                freelist_head = curr->next_block;
                if (freelist_head != NULL) {
                    freelist_head->prev_block = NULL;
                }
            }else{
                prev->next_block = curr->next_block;
                if (curr->next_block != NULL) {
                    curr->next_block->prev_block = prev;
                }
            }

            //add block to allocated list
            if (allocatedlist_head == NULL){
                allocatedlist_head = curr;
                curr->prev_block = NULL;
                curr->next_block = NULL;
            }else{
                curr->next_block = allocatedlist_head;
                curr->prev_block = NULL;
                curr->next_block->prev_block = curr;
                allocatedlist_head = curr;
            }
            // curr->next_block = allocatedlist_head;
            // allocatedlist_head = curr;

            return (void*)(curr->block_high);
        }

        prev = curr;
        curr = curr->next_block;
    }
    
    return NULL;
}

int k_mem_dealloc(void* ptr) {
    if (ptr == NULL || k_mem_state != RUNNING) {
        return RTX_ERR;
    }

    // Find block in allocated list
    MEMBLOCK* dealloc = allocatedlist_head;
    while (dealloc != NULL && (U32)ptr != dealloc->block_high) {
        dealloc = dealloc->next_block;
    }

    if (dealloc == NULL || dealloc->block_tid != osGetTID()) {
        return RTX_ERR;  // Block not found or wrong owner
    }

    //only the owner can deallocate
    // if (dealloc->block_tid != osGetTID()){
    // // if (current_tid != dealloc->block_tid){
    //     return RTX_ERR;
    // }

    //REMOVE FROM ALLOCATED LIST
    if (dealloc->next_block == NULL){ //no blocks after 
        if (dealloc->prev_block == NULL){ //no blocks before 
            allocatedlist_head = NULL;
        }else{ //there are blocks before
            dealloc->prev_block->next_block = NULL;
        }
    }else{ //there are blocks after 
        if (dealloc->prev_block == NULL){ //no blocks before
            allocatedlist_head = dealloc->next_block;
            allocatedlist_head->prev_block = NULL;
        }else{ //there are blocks before 
            dealloc->prev_block->next_block = dealloc->next_block;
            dealloc->next_block->prev_block = dealloc->prev_block;
        }
    }

    // Prepare for freelist insertion
    dealloc->state = FREE;
    dealloc->block_tid = -1;
    MEMBLOCK* curr = freelist_head;
    MEMBLOCK* prev = NULL;

    // Find insertion point (sorted by MEMBLOCK address)
    while (curr != NULL && (U32)curr < (U32)dealloc) {
        prev = curr;
        curr = curr->next_block;
    }

    // Insert into freelist
    dealloc->prev_block = prev;
    dealloc->next_block = curr;
    if (prev != NULL) {
        prev->next_block = dealloc;
    } else {
        freelist_head = dealloc;  // New head of freelist
    }
    if (curr != NULL) {
        curr->prev_block = dealloc;
    }

    // Merge with previous block if adjacent
    if (prev != NULL && (U32)prev + prev->block_size == (U32)dealloc) {
        prev->block_size += dealloc->block_size;
        prev->next_block = dealloc->next_block;
        if (dealloc->next_block != NULL) {
            dealloc->next_block->prev_block = prev;
        }
        dealloc = prev;  // Work with merged block
    }

    // Merge with next block if adjacent
    MEMBLOCK* next = dealloc->next_block;
    if (next != NULL && (U32)dealloc + dealloc->block_size == (U32)next) {
        dealloc->block_size += next->block_size;
        dealloc->next_block = next->next_block;
        if (next->next_block != NULL) {
            next->next_block->prev_block = dealloc;
        }
    }

    return RTX_OK;
}

int k_mem_count_extfrag(size_t size) {
    MEMBLOCK* curr = freelist_head;
    int count = 0;

    if (curr == NULL){
        return 0; 
    }

    while (curr != NULL){
        if (curr->block_size < size){
            count++;
        }
        curr = curr->next_block;
    }

    return count; 
}

// check
