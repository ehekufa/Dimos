#include "../include/hal.h"
#include "../include/rtl.h"

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) GDT_ENTRY;

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) GDT_TSS_ENTRY;

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed)) TSS64;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) GDTR;

static struct {
    GDT_ENTRY     null;
    GDT_ENTRY     kernel_code;
    GDT_ENTRY     kernel_data;
    GDT_ENTRY     user_data;
    GDT_ENTRY     user_code;
    GDT_TSS_ENTRY tss;
} __attribute__((packed)) g_gdt;

static TSS64 g_tss;
static uint8_t g_tss_stack[8192] __attribute__((aligned(16)));

static void set_gdt_entry(GDT_ENTRY* e, uint8_t access, uint8_t gran) {
    e->limit_low   = 0xFFFF;
    e->base_low    = 0;
    e->base_mid    = 0;
    e->access      = access;
    e->granularity = gran;
    e->base_high   = 0;
}

static void set_tss_entry(GDT_TSS_ENTRY* e, uint64_t base, uint32_t limit) {
    e->limit_low   = (uint16_t)(limit & 0xFFFF);
    e->base_low    = (uint16_t)(base & 0xFFFF);
    e->base_mid    = (uint8_t)((base >> 16) & 0xFF);
    e->access      = 0x89; /* Present, 64-bit TSS (Available) */
    e->granularity = (uint8_t)((limit >> 16) & 0x0F);
    e->base_high   = (uint8_t)((base >> 24) & 0xFF);
    e->base_upper  = (uint32_t)(base >> 32);
    e->reserved    = 0;
}

void HalSetKernelStack(uint64_t stack) {
    g_tss.rsp0 = stack;
}

void HalInitGdt(void) {
    memset(&g_gdt, 0, sizeof(g_gdt));
    memset(&g_tss, 0, sizeof(g_tss));

    g_tss.rsp0 = (uint64_t)(g_tss_stack + sizeof(g_tss_stack));
    g_tss.iomap_base = sizeof(TSS64);

    set_gdt_entry(&g_gdt.kernel_code, 0x9A, 0x20); /* Ring 0 Code, L=1 */
    set_gdt_entry(&g_gdt.kernel_data, 0x92, 0x00); /* Ring 0 Data */
    set_gdt_entry(&g_gdt.user_data,   0xF2, 0x00); /* Ring 3 Data */
    set_gdt_entry(&g_gdt.user_code,   0xFA, 0x20); /* Ring 3 Code, L=1 */
    set_tss_entry(&g_gdt.tss, (uint64_t)&g_tss, sizeof(TSS64) - 1);

    GDTR gdtr;
    gdtr.limit = sizeof(g_gdt) - 1;
    gdtr.base  = (uint64_t)&g_gdt;

    __asm__ volatile (
        "lgdt %0\n\t"
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1:\n\t"
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        "movw $0x28, %%ax\n\t"
        "ltr %%ax\n\t"
        : : "m"(gdtr) : "rax", "memory"
    );

    HalPrintf("[HAL] GDT & TSS initialized (64-bit Long Mode)\n");
}
