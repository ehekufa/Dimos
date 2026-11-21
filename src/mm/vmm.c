#include "../include/mm.h"
#include "../include/hal.h"
#include "../include/rtl.h"

static uint64_t* g_kernel_pml4 = NULL;

void MmInitVmm(void) {
    uint64_t cr3 = 0;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    g_kernel_pml4 = (uint64_t*)((cr3 & ~0xFFFULL) + MmGetHhdmOffset());
    HalPrintf("[MM] VMM initialized (Kernel PML4 at %p)\n", g_kernel_pml4);
}

uint64_t* MmGetKernelPml4(void) {
    return g_kernel_pml4;
}

static uint64_t* get_next_level(uint64_t* table, size_t index, bool allocate, uint64_t flags) {
    if (table[index] & PTE_PRESENT) {
        return (uint64_t*)((table[index] & 0x000FFFFFFFFFF000ULL) + MmGetHhdmOffset());
    }
    if (!allocate) {
        return NULL;
    }
    uint64_t new_page = MmAllocPage();
    if (!new_page) {
        return NULL;
    }
    table[index] = new_page | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    return (uint64_t*)(new_page + MmGetHhdmOffset());
}

bool MmMapPage(uint64_t* pml4, uint64_t virt, uint64_t phys, uint64_t flags) {
    if (!pml4) pml4 = g_kernel_pml4;

    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t* pdpt = get_next_level(pml4, pml4_idx, true, flags);
    if (!pdpt) return false;

    uint64_t* pd = get_next_level(pdpt, pdpt_idx, true, flags);
    if (!pd) return false;

    uint64_t* pt = get_next_level(pd, pd_idx, true, flags);
    if (!pt) return false;

    pt[pt_idx] = (phys & 0x000FFFFFFFFFF000ULL) | flags | PTE_PRESENT;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    return true;
}

void MmUnmapPage(uint64_t* pml4, uint64_t virt) {
    if (!pml4) pml4 = g_kernel_pml4;

    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t* pdpt = get_next_level(pml4, pml4_idx, false, 0);
    if (!pdpt) return;

    uint64_t* pd = get_next_level(pdpt, pdpt_idx, false, 0);
    if (!pd) return;

    uint64_t* pt = get_next_level(pd, pd_idx, false, 0);
    if (!pt) return;

    pt[pt_idx] = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

uint64_t MmGetPhysAddr(uint64_t* pml4, uint64_t virt) {
    if (!pml4) pml4 = g_kernel_pml4;

    size_t pml4_idx = (virt >> 39) & 0x1FF;
    size_t pdpt_idx = (virt >> 30) & 0x1FF;
    size_t pd_idx   = (virt >> 21) & 0x1FF;
    size_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t* pdpt = get_next_level(pml4, pml4_idx, false, 0);
    if (!pdpt) return 0;

    uint64_t* pd = get_next_level(pdpt, pdpt_idx, false, 0);
    if (!pd) return 0;

    uint64_t* pt = get_next_level(pd, pd_idx, false, 0);
    if (!pt || !(pt[pt_idx] & PTE_PRESENT)) return 0;

    return (pt[pt_idx] & 0x000FFFFFFFFFF000ULL) | (virt & 0xFFF);
}
