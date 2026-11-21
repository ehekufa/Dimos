#include "../include/hal.h"
#include "../include/rtl.h"

#define COM1_PORT 0x3F8

uint8_t HalInPort8(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void HalOutPort8(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

uint16_t HalInPort16(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void HalOutPort16(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

uint32_t HalInPort32(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void HalOutPort32(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

void HalIoWait(void) {
    HalOutPort8(0x80, 0);
}

void HalSerialInit(void) {
    HalOutPort8(COM1_PORT + 1, 0x00); /* Disable all interrupts */
    HalOutPort8(COM1_PORT + 3, 0x80); /* Enable DLAB */
    HalOutPort8(COM1_PORT + 0, 0x03); /* 38400 baud (low) */
    HalOutPort8(COM1_PORT + 1, 0x00); /* 38400 baud (high) */
    HalOutPort8(COM1_PORT + 3, 0x03); /* 8 bits, no parity, one stop bit */
    HalOutPort8(COM1_PORT + 2, 0xC7); /* Enable FIFO */
    HalOutPort8(COM1_PORT + 4, 0x0B); /* IRQs enabled, RTS/DSR set */
}

static bool serial_transmit_empty(void) {
    return (HalInPort8(COM1_PORT + 5) & 0x20) != 0;
}

void HalSerialWriteChar(char c) {
    if (c == '\n') {
        while (!serial_transmit_empty());
        HalOutPort8(COM1_PORT, '\r');
    }
    while (!serial_transmit_empty());
    HalOutPort8(COM1_PORT, (uint8_t)c);
}

void HalSerialWrite(const char* str) {
    while (*str) {
        HalSerialWriteChar(*str++);
    }
}

void HalPrintf(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    HalSerialWrite(buf);
}

void HalReboot(void) {
    HalCli();
    /* Pulse CPU reset line via 8042 keyboard controller */
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = HalInPort8(0x64);
    }
    HalOutPort8(0x64, 0xFE);
    for (;;) {
        HalHlt();
    }
}

void HalShutdown(void) {
    HalCli();
    /* Try real ACPI S5 power-off first, then QEMU / Bochs / VirtualBox ports */
    HalAcpiPowerOff();
    HalOutPort16(0x604, 0x2000);
    HalOutPort16(0xB004, 0x2000);
    HalOutPort16(0x4004, 0x3400);
    for (;;) {
        HalHlt();
    }
}
