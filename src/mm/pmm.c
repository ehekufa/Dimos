#include "../include/mm.h"
#include "../include/hal.h"
#include "../include/rtl.h"

static uint8_t* g_bitmap = NULL;
static uint64_t g_total_pages = 0;
static uint64_t g_used_pages = 0;
static uint64_t g_highest_addr = 0;
static uint64_t g_hhdm_offset = 0;

static inline void bitmap_set(uint64_t bit) {
    g_bitmap[bit / 8] |= (uint8_t)(1 << (bit % 8));
}

static inline void bitmap_clear(uint64_t bit) {
    g_bitmap[bit / 8] &= (uint8_t)~(1 << (bit % 8));
}

static inline bool bitmap_test(uint64_t bit) {
    return (g_bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

void MmInitPmm(struct limine_memmap_response* memmap, uint64_t hhdm_offset) {
    g_hhdm_offset = hhdm_offset;
    g_highest_addr = 0;

    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t top = entry->base + entry->length;
            if (top > g_highest_addr) {
                g_highest_addr = top;
            }
        }
    }

    g_total_pages = g_highest_addr / PAGE_SIZE;
    uint64_t bitmap_size = ALIGN_UP(g_total_pages / 8, PAGE_SIZE);

    /* Find a usable region large enough for the bitmap */
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap_size) {
            g_bitmap = (uint8_t*)(entry->base + g_hhdm_offset);
            break;
        }
    }

    /* Mark all pages as used initially */
    memset(g_bitmap, 0xFF, bitmap_size);
    g_used_pages = g_total_pages;

    /* Free usable regions */
    for (uint64_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uint64_t start_page = ALIGN_UP(entry->base, PAGE_SIZE) / PAGE_SIZE;
            uint64_t end_page = ALIGN_DOWN(entry->base + entry->length, PAGE_SIZE) / PAGE_SIZE;
            for (uint64_t p = start_page; p < end_page; p++) {
                bitmap_clear(p);
                g_used_pages--;
            }
        }
    }

    /* Mark bitmap pages themselves as used */
    uint64_t bitmap_phys = (uint64_t)g_bitmap - g_hhdm_offset;
    uint64_t bitmap_start_page = bitmap_phys / PAGE_SIZE;
    uint64_t bitmap_page_count = bitmap_size / PAGE_SIZE;
    for (uint64_t p = bitmap_start_page; p < bitmap_start_page + bitmap_page_count; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            g_used_pages++;
        }
    }

    /* Never allocate page 0 */
    if (!bitmap_test(0)) {
        bitmap_set(0);
        g_used_pages++;
    }

    HalPrintf("[MM] PMM initialized: Total=%llu MB, Free=%llu MB\n",
              (g_total_pages * PAGE_SIZE) / (1024 * 1024),
              ((g_total_pages - g_used_pages) * PAGE_SIZE) / (1024 * 1024));
}

uint64_t MmAllocPage(void) {
    for (uint64_t i = 1; i < g_total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            g_used_pages++;
            uint64_t phys = i * PAGE_SIZE;
            memset((void*)(phys + g_hhdm_offset), 0, PAGE_SIZE);
            return phys;
        }
    }
    HalPrintf("[MM] CRITICAL: Out of physical memory!\n");
    return 0;
}

void MmFreePage(uint64_t phys_addr) {
    uint64_t page = phys_addr / PAGE_SIZE;
    if (page > 0 && page < g_total_pages && bitmap_test(page)) {
        bitmap_clear(page);
        g_used_pages--;
    }
}

uint64_t MmGetTotalMemory(void) {
    return g_total_pages * PAGE_SIZE;
}

uint64_t MmGetFreeMemory(void) {
    return (g_total_pages - g_used_pages) * PAGE_SIZE;
}

uint64_t MmGetUsedMemory(void) {
    return g_used_pages * PAGE_SIZE;
}

uint64_t MmGetHhdmOffset(void) {
    return g_hhdm_offset;
}
