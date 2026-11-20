#include "../include/hal.h"
#include "../include/ps.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define PIT_CH0   0x40
#define PIT_CMD   0x43
#define PIT_FREQ  1193182

static volatile uint64_t g_tick_count = 0;
static volatile uint64_t g_lapic_virt = 0;
static bool g_apic_enabled = false;

extern void lv_tick_inc(uint32_t tick_period);

void HalInitPic(void) {
    HalOutPort8(PIC1_CMD, 0x11); HalIoWait();
    HalOutPort8(PIC2_CMD, 0x11); HalIoWait();

    HalOutPort8(PIC1_DATA, 32);   HalIoWait(); /* Master offset 32 */
    HalOutPort8(PIC2_DATA, 40);   HalIoWait(); /* Slave offset 40 */

    HalOutPort8(PIC1_DATA, 0x04); HalIoWait();
    HalOutPort8(PIC2_DATA, 0x02); HalIoWait();

    HalOutPort8(PIC1_DATA, 0x01); HalIoWait();
    HalOutPort8(PIC2_DATA, 0x01); HalIoWait();

    /* Unmask IRQ0 (Timer), IRQ1 (Keyboard), IRQ2 (Cascade), IRQ12 (Mouse) */
    HalOutPort8(PIC1_DATA, 0xF8);
    HalOutPort8(PIC2_DATA, 0xEF);
    HalPrintf("[HAL] 8259 PIC remapped (IRQ 0..15 -> INT 32..47)\n");
}

void HalDisablePic(void) {
    HalOutPort8(PIC1_DATA, 0xFF);
    HalOutPort8(PIC2_DATA, 0xFF);
}

void HalPicSendEoi(uint8_t irq) {
    if (irq >= 8) {
        HalOutPort8(PIC2_CMD, 0x20);
    }
    HalOutPort8(PIC1_CMD, 0x20);
}

void HalInitApic(uint64_t hhdm_offset) {
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x1B));
    uint64_t apic_phys = ((uint64_t)hi << 32) | (lo & 0xFFFFF000);
    if (apic_phys == 0) {
        HalPrintf("[HAL] Local APIC not present, using PIC\n");
        return;
    }
    g_lapic_virt = apic_phys + hhdm_offset;
    /* Enable Local APIC via Spurious Interrupt Vector Register (0xF0) */
    volatile uint32_t* svr = (volatile uint32_t*)(g_lapic_virt + 0xF0);
    *svr = (*svr) | 0x100 | 0xFF;
    g_apic_enabled = true;
    HalPrintf("[HAL] Local APIC enabled at phys=0x%llx\n", apic_phys);
}

void HalApicSendEoi(void) {
    if (g_apic_enabled && g_lapic_virt) {
        volatile uint32_t* eoi = (volatile uint32_t*)(g_lapic_virt + 0xB0);
        *eoi = 0;
    }
}

bool HalIsApicEnabled(void) {
    return g_apic_enabled;
}

static void timer_irq_handler(KTRAP_FRAME* frame) {
    g_tick_count++;
    lv_tick_inc(1);
    PsSchedule(frame);
}

void HalInitTimer(uint32_t frequency_hz) {
    uint32_t divisor = PIT_FREQ / frequency_hz;
    HalOutPort8(PIT_CMD, 0x36);
    HalOutPort8(PIT_CH0, (uint8_t)(divisor & 0xFF));
    HalOutPort8(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));

    HalRegisterIrqHandler(0, timer_irq_handler);
    HalPrintf("[HAL] System Timer initialized at %u Hz\n", frequency_hz);
}

uint64_t HalGetTickCount(void) {
    return g_tick_count;
}
