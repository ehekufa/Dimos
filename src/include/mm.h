#ifndef DIMAOS_MM_H
#define DIMAOS_MM_H

#include "types.h"
#include "limine.h"

#define PAGE_SIZE 4096

/* Page Table Flags */
#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_USER     (1ULL << 2)
#define PTE_NX       (1ULL << 63)

/* Physical Memory Manager (PMM) */
void     MmInitPmm(struct limine_memmap_response* memmap, uint64_t hhdm_offset);
uint64_t MmAllocPage(void);
void     MmFreePage(uint64_t phys_addr);
uint64_t MmGetTotalMemory(void);
uint64_t MmGetFreeMemory(void);
uint64_t MmGetUsedMemory(void);
uint64_t MmGetHhdmOffset(void);

/* Virtual Memory Manager (VMM) */
void     MmInitVmm(void);
uint64_t* MmGetKernelPml4(void);
bool     MmMapPage(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags);
void     MmUnmapPage(uint64_t* pml4, uint64_t virt);
uint64_t MmGetPhysAddr(uint64_t* pml4, uint64_t virt);

/* Kernel Heap (Pool Allocator) */
void  MmInitPool(void);
void* ExAllocatePool(size_t size);
void  ExFreePool(void* ptr);
void* ExReallocatePool(void* ptr, size_t new_size);

#endif /* DIMAOS_MM_H */
