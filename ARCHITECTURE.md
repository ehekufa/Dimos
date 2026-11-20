# DimaOS (Дима ОС / Димос) — Архитектура и сборка

**DimaOS** — 64-битная операционная система для архитектуры `x86_64` (Long Mode) на языке C11 (`-ffreestanding`), спроектированная с нуля (Clean Room Design) с гибридным ядром в стиле NT и классическим графическим интерфейсом Windows на базе LVGL v8.3.

## Структура проекта

```text
.
├── .github/workflows/
│   └── build.yml           # CI/CD GitHub Actions (сборка dimaos.iso и загрузка артефакта)
├── boot/
│   └── limine.conf         # Конфигурация загрузчика Limine 8.x
├── src/
│   ├── include/            # Заголовочные файлы ядра, HAL, MM, OB, PS, FS, драйверов и LVGL
│   │   ├── types.h         # Базовые типы и NTSTATUS
│   │   ├── rtl.h           # Clean-room Runtime Library
│   │   ├── hal.h           # Интерфейсы HAL
│   │   ├── mm.h            # PMM, VMM, Pool Allocator
│   │   ├── ob.h            # Object Manager & Handle Table
│   │   ├── ps.h            # Process & Thread Manager
│   │   ├── fs.h            # RAM-диск (C:\)
│   │   ├── drivers.h       # PS/2 клавиатура, мышь, Framebuffer
│   │   ├── gui.h           # Графическая оболочка DimaOS
│   │   └── lv_conf.h       # Конфигурация LVGL v8.3 для bare-metal
│   ├── hal/                # Hardware Abstraction Layer
│   │   ├── port.c          # Порты ввода-вывода, COM1 (0x3F8), Reboot/Shutdown
│   │   ├── gdt.c           # 64-битная GDT (Ring 0 / Ring 3) и TSS
│   │   ├── idt.c           # 64-битная IDT и диспетчер прерываний
│   │   ├── isr.S           # Ассемблерные обработчики исключений и IRQ
│   │   └── apic.c          # 8259 PIC, Local APIC и системный таймер PIT (1000 Гц)
│   ├── mm/                 # Управление памятью
│   │   ├── pmm.c           # Битовый аллокатор физических страниц 4 КБ
│   │   ├── vmm.c           # 4-уровневая страничная адресация (PML4 -> PDPT -> PD -> PT)
│   │   └── pool.c          # Ядерная куча (ExAllocatePool / ExFreePool)
│   ├── ob/
│   │   └── object.c        # Объектная модель и таблица дескрипторов (Handles)
│   ├── ps/
│   │   └── process.c       # Процессы (KPROCESS), потоки (KTHREAD) и планировщик
│   ├── fs/
│   │   └── ramfs.c         # Виртуальная ФС в памяти с адресацией тома C:\
│   ├── drivers/            # Драйверы устройств
│   │   ├── framebuffer.c   # Драйвер 32-bit XRGB8888 фреймбуфера Limine
│   │   ├── keyboard.c      # Драйвер PS/2 клавиатуры (IRQ1)
│   │   └── mouse.c         # Драйвер PS/2 мыши (IRQ12)
│   ├── gui/
│   │   └── desktop.c       # Оболочка DimaOS (Рабочий стол, Пуск, Панель задач, окна)
│   └── ke/                 # Ядро
│       ├── main.c          # Точка входа KeMain и инициализация подсистем
│       └── rtl.c           # Реализация memcpy, memset, snprintf и др.
├── vendor/                 # Внешние зависимости (подтягиваются через make deps)
│   ├── limine/             # Limine v8.x-binary
│   └── lvgl/               # LVGL release/v8.3
├── linker.ld               # Скрипт линковщика (Higher Half Kernel 0xffffffff80000000)
└── Makefile                # Сборка ядра и образа dimaos.iso
```

## Сборка и запуск

### Локальная сборка
```bash
make deps          # Клонирует Limine 8.x и LVGL 8.3 в vendor/
make -j$(nproc)    # Собирает build/dimaos.elf
make iso           # Собирает загрузочный образ dimaos.iso (требуется xorriso)
```

### Запуск в QEMU
```bash
qemu-system-x86_64 -cdrom dimaos.iso -m 512M -serial stdio
```

## CI/CD (GitHub Actions)
При каждом `push` или `pull_request` в ветку `main`/`master` workflow `.github/workflows/build.yml` автоматически устанавливает инструменты сборки на Ubuntu, скачивает Limine и LVGL, собирает `dimaos.iso` и публикует его как артефакт `dimaos-iso`.
