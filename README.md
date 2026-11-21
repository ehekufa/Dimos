# DimaOS (x86_64)

A 64-bit `x86_64` freestanding operating system built on **Limine v8.x** + **LVGL v8.3**, combining an NT-style higher-half kernel with the desktop shell, hardware drivers, FAT12 persistence, and applications inspired by [DimOS-moya](https://github.com/ehekufa/DimOS-moya).

## Features
- **64-bit Higher-Half Kernel**: GDT/TSS, 256-vector IDT, PIC/APIC, 1000 Hz PIT, CMOS RTC clock, ACPI `_S5_` power-off, physical/virtual memory managers, and 16 MB kernel pool.
- **Hardware Drivers**: PS/2 keyboard (`0xE0`/`0xE1` + polling fallback), IntelliMouse wheel & 8042 retry, PC Speaker sequencer (`0x61`), ATA PIO LBA28 IDE disk, 32-bit Limine framebuffer.
- **Storage & Persistence**: RAM-disk (`C:\`), writable **FAT12** on IDE drive, and checksummed **`CONFIG.CFG`** for theme, brightness, and audio preferences.
- **LVGL Desktop & 10 Apps**: Lock Screen, Start Menu with Live Tiles, Control Center, 4 themes (`Blue`, `Dark`, `Green`, `Orange`), plus **Files**, **Notepad**, **Terminal**, **Calculator**, **Settings**, **Paint**, **Snake**, **Mines**, **Music**, and **Cheesy Balls**.

## Build
```bash
make deps
make -j$(nproc)
make iso
```

See [ARCHITECTURE.md](ARCHITECTURE.md) for full architecture and QEMU instructions.
