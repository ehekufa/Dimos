#include "../include/hal.h"
#include "../include/rtl.h"

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) IDT_ENTRY;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) IDTR;

static IDT_ENTRY g_idt[256];
static IRQ_HANDLER g_irq_handlers[16];

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

static void set_idt_gate(uint8_t vec, uint64_t handler, uint8_t attr) {
    g_idt[vec].offset_low  = (uint16_t)(handler & 0xFFFF);
    g_idt[vec].selector    = 0x08;
    g_idt[vec].ist         = 0;
    g_idt[vec].type_attr   = attr;
    g_idt[vec].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    g_idt[vec].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    g_idt[vec].reserved    = 0;
}

void HalRegisterIrqHandler(uint8_t irq, IRQ_HANDLER handler) {
    if (irq < 16) {
        g_irq_handlers[irq] = handler;
    }
}

void HalInterruptDispatch(KTRAP_FRAME* frame) {
    if (frame->int_no < 32) {
        uint64_t cr2 = 0;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        HalPrintf("\n[EXCEPTION #%llu] err=0x%llx rip=0x%llx rsp=0x%llx cr2=0x%llx\n",
                  frame->int_no, frame->err_code, frame->rip, frame->rsp, cr2);
        for (;;) {
            HalCli();
            HalHlt();
        }
    } else if (frame->int_no >= 32 && frame->int_no < 48) {
        uint8_t irq = (uint8_t)(frame->int_no - 32);
        if (g_irq_handlers[irq]) {
            g_irq_handlers[irq](frame);
        }
        HalPicSendEoi(irq);
    }
}

void HalInitIdt(void) {
    memset(g_idt, 0, sizeof(g_idt));
    memset(g_irq_handlers, 0, sizeof(g_irq_handlers));

    void* isrs[32] = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };
    for (int i = 0; i < 32; i++) {
        set_idt_gate((uint8_t)i, (uint64_t)isrs[i], 0x8E);
    }

    void* irqs[16] = {
        irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
        irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
    };
    for (int i = 0; i < 16; i++) {
        set_idt_gate((uint8_t)(32 + i), (uint64_t)irqs[i], 0x8E);
    }

    IDTR idtr;
    idtr.limit = sizeof(g_idt) - 1;
    idtr.base  = (uint64_t)&g_idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr));

    HalPrintf("[HAL] IDT initialized (256 vectors)\n");
}
