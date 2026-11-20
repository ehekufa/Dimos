#include "../include/mm.h"
#include "../include/hal.h"
#include "../include/rtl.h"

#define POOL_INITIAL_PAGES 2048 /* 8 MB initial kernel heap for LVGL & OS */

typedef struct _POOL_BLOCK {
    size_t              size;
    bool                free;
    struct _POOL_BLOCK* next;
} POOL_BLOCK;

static POOL_BLOCK* g_heap_head = NULL;

void MmInitPool(void) {
    uint64_t first_phys = MmAllocContiguousPages(POOL_INITIAL_PAGES);
    if (!first_phys) {
        HalPrintf("[MM] Pool init failed: no memory\n");
        return;
    }

    g_heap_head = (POOL_BLOCK*)(first_phys + MmGetHhdmOffset());
    g_heap_head->size = (POOL_INITIAL_PAGES * PAGE_SIZE) - sizeof(POOL_BLOCK);
    g_heap_head->free = true;
    g_heap_head->next = NULL;

    HalPrintf("[MM] Kernel Pool initialized (%u KB)\n", (uint32_t)(g_heap_head->size / 1024));
}

void* ExAllocatePool(size_t size) {
    if (size == 0) return NULL;
    size = ALIGN_UP(size, 16);

    POOL_BLOCK* curr = g_heap_head;
    while (curr) {
        if (curr->free && curr->size >= size) {
            if (curr->size >= size + sizeof(POOL_BLOCK) + 16) {
                POOL_BLOCK* next_block = (POOL_BLOCK*)((uint8_t*)curr + sizeof(POOL_BLOCK) + size);
                next_block->size = curr->size - size - sizeof(POOL_BLOCK);
                next_block->free = true;
                next_block->next = curr->next;

                curr->size = size;
                curr->next = next_block;
            }
            curr->free = false;
            void* ptr = (void*)((uint8_t*)curr + sizeof(POOL_BLOCK));
            memset(ptr, 0, size);
            return ptr;
        }
        curr = curr->next;
    }

    HalPrintf("[MM] ExAllocatePool(%llu) failed: out of heap!\n", (uint64_t)size);
    return NULL;
}

void ExFreePool(void* ptr) {
    if (!ptr) return;

    POOL_BLOCK* block = (POOL_BLOCK*)((uint8_t*)ptr - sizeof(POOL_BLOCK));
    block->free = true;

    /* Coalesce adjacent free blocks */
    POOL_BLOCK* curr = g_heap_head;
    while (curr) {
        if (curr->free && curr->next && curr->next->free) {
            if ((uint8_t*)curr + sizeof(POOL_BLOCK) + curr->size == (uint8_t*)curr->next) {
                curr->size += sizeof(POOL_BLOCK) + curr->next->size;
                curr->next = curr->next->next;
                continue;
            }
        }
        curr = curr->next;
    }
}

void* ExReallocatePool(void* ptr, size_t new_size) {
    if (!ptr) return ExAllocatePool(new_size);
    if (new_size == 0) {
        ExFreePool(ptr);
        return NULL;
    }

    POOL_BLOCK* block = (POOL_BLOCK*)((uint8_t*)ptr - sizeof(POOL_BLOCK));
    if (block->size >= new_size) {
        return ptr;
    }

    void* new_ptr = ExAllocatePool(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        ExFreePool(ptr);
    }
    return new_ptr;
}
