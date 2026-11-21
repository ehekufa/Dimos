#ifndef DIMAOS_HAL_H
#define DIMAOS_HAL_H

#include "types.h"

/* CPU Interrupt Control */
static inline void HalCli(void) {
    __asm__ volatile ("cli");
}

static inline void HalSti(void) {
    __asm__ volatile ("sti");
}

static inline void HalHlt(void) {
    __asm__ volatile ("hlt");
}

/* Port I/O */
uint8_t  HalInPort8(uint16_t port);
void     HalOutPort8(uint16_t port, uint8_t value);
uint16_t HalInPort16(uint16_t port);
void     HalOutPort16(uint16_t port, uint16_t value);
uint32_t HalInPort32(uint16_t port);
void     HalOutPort32(uint16_t port, uint32_t value);
void     HalIoWait(void);

/* COM1 Serial Debug (0x3F8) */
void HalSerialInit(void);
void HalSerialWriteChar(char c);
void HalSerialWrite(const char* str);
void HalPrintf(const char* fmt, ...);

/* GDT & TSS */
void HalInitGdt(void);
void HalSetKernelStack(uint64_t stack);

/* IDT & Interrupts */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed)) KTRAP_FRAME;

typedef void (*IRQ_HANDLER)(KTRAP_FRAME* frame);

void HalInitIdt(void);
void HalRegisterIrqHandler(uint8_t irq, IRQ_HANDLER handler);

/* PIC & APIC */
void HalInitPic(void);
void HalDisablePic(void);
void HalPicSendEoi(uint8_t irq);
void HalInitApic(uint64_t hhdm_offset);
void HalApicSendEoi(void);
bool HalIsApicEnabled(void);

/* System Timer (1000 Hz) */
void     HalInitTimer(uint32_t frequency_hz);
uint64_t HalGetTickCount(void);

/* CMOS Real-Time Clock */
typedef struct {
    uint8_t  seconds;
    uint8_t  minutes;
    uint8_t  hours;
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
} DIMA_TIME;

void     HalClockInit(void);
void     HalClockGetTime(DIMA_TIME* out);
uint8_t  HalClockSeconds(void);
uint8_t  HalClockMinutes(void);
uint8_t  HalClockHours(void);
uint8_t  HalClockDay(void);
uint8_t  HalClockMonth(void);
uint16_t HalClockYear(void);

/* ACPI & Power Management */
void HalAcpiInit(uint64_t rsdp_addr, uint64_t hhdm_offset);
bool HalAcpiPowerOff(void);
void HalReboot(void);
void HalShutdown(void);

#endif /* DIMAOS_HAL_H */
