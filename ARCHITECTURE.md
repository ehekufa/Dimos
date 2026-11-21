# DimaOS (Дима ОС / Димос) — Архитектура и сборка

**DimaOS** — 64-битная операционная система для архитектуры `x86_64` (Long Mode, Limine v8.x) на языке C11 (`-ffreestanding`), объединяющая 64-битное ядро в стиле NT с графической оболочкой LVGL v8.3 и полным набором системных подсистем и приложений в духе [DimOS-moya](https://github.com/ehekufa/DimOS-moya).

## Что реализовано

### Ядро и HAL (`src/ke/`, `src/hal/`, `src/mm/`)
- **Higher-Half x86_64 ядро** (`0xffffffff80000000`), загрузка через **Limine v8.x** (Framebuffer, HHDM, Memmap, RSDP).
- **GDT/TSS, 256-векторная IDT, 8259 PIC / Local APIC, PIT 1000 Гц**.
- **CMOS RTC (`src/hal/rtc.c`)**: реальное время и дата для панели задач, экрана блокировки и команды `time`.
- **ACPI Power-off (`src/hal/acpi.c`)**: поиск RSDP/RSDT/XSDT/FADT и разбор DSDT `_S5_` для корректного выключения питания (+ QEMU/Bochs/VirtualBox порты).
- **Управление памятью**: физический битовый аллокатор (`pmm.c`), 4-уровневый пейджинг (`vmm.c`), куча ядра 16 МБ (`pool.c`).

### Драйверы и хранилище (`src/drivers/`, `src/fs/`)
- **Клавиатура PS/2 (`keyboard.c`)**: Set-1 + расширенные коды `0xE0` (стрелки, Home/End/PgUp/PgDn/Delete) и `0xE1`, Shift/CapsLock/Ctrl/Alt, polling-фолбэк для веб-эмуляторов.
- **Мышь PS/2 / IntelliMouse (`mouse.c`)**: 3- и 4-байтовые пакеты с колесом прокрутки, проверка переполнения и знака X/Y, таймаут синхронизации 40 мс, повтор `0xFE` (resend) при инициализации 8042.
- **PC Speaker (`sound.c`)**: генератор звука на PIT Ch2 + порт `0x61`, 100 Гц секвенсор нот (Startup Chime, Tiny March, Spring Waltz, Nyam, Burp, Beep/Alert).
- **ATA PIO LBA28 (`ata.c`)**: автодетект Primary Master/Slave с чтением модели и размера диска.
- **Файловые системы (`ramfs.c`, `volume.c`, `config.c`)**: RAM-диск `C:\`, записываемая **FAT12** на IDE-диске (`VolInit`, `VolFormat`, `VolReadFile`, `VolWriteFile`, `VolDeleteFile`) и файл настроек **`CONFIG.CFG`** с контрольной суммой (сохранение темы, яркости, громкости).

### Графическая оболочка и 10 приложений (`src/gui/`)
- **Оболочка LVGL v8.3 (`desktop.c`)**: экран блокировки (часы, дата, `Slide / Click to unlock`), рабочий стол с 10 иконками, панель задач, меню **Start** с Live-плитками, **Control Center** (регулятор яркости 5..15, тумблер звука, кнопка Lock, Power Off), 4 темы оформления (`Blue`, `Dark`, `Green`, `Orange`), экранный оверлей яркости.
- **Системные приложения (`apps_system.c`)**: `Files` (просмотр RAM-диска `C:\` и FAT12, форматирование/удаление), `Notepad` (редактор с сохранением в `C:\NOTES.TXT` и на FAT12), `Terminal` (`help`, `ls`, `cat`, `write`, `rm`, `format`, `time`, `beep`, `clear`, `mem`, `sysinfo`, `theme`, `reboot`, `poweroff`), `Calculator`, `Settings` (выбор 4 тем, яркости, звука, сохранение `CONFIG.CFG`, информация о CPU/RAM/ATA).
- **Развлечения и утилиты (`apps_fun.c`)**: `Paint` (16 цветов, 3 кисти, сохранение `C:\PAINT.RAW`), `Snake` (42x25 поле, стрелки/WASD/D-pad/клик), `Mines` (9x9, 10 мин, правый клик и режим флагов), `Music` (PC Speaker плеер), `Cheesy Balls` (миска с 9 сырными шариками, брызги кетчупа, счётчик отрыжки на 12 шариках).

## Структура проекта

```text
.
├── .github/workflows/build.yml  # CI/CD: сборка build/dimaos.elf и dimaos.iso
├── boot/limine.conf             # Конфигурация Limine 8.x
├── src/
│   ├── include/                 # types.h, rtl.h, hal.h, mm.h, ob.h, ps.h, fs.h, drivers.h, gui.h, lv_conf.h
│   ├── hal/                     # port.c, gdt.c, idt.c, isr.S, apic.c, rtc.c, acpi.c
│   ├── mm/                      # pmm.c, vmm.c, pool.c
│   ├── ob/                      # object.c
│   ├── ps/                      # process.c
│   ├── fs/                      # ramfs.c, volume.c (FAT12), config.c (CONFIG.CFG)
│   ├── drivers/                 # framebuffer.c, keyboard.c, mouse.c, sound.c, ata.c
│   ├── gui/                     # desktop.c, apps_system.c, apps_fun.c
│   └── ke/                      # main.c, rtl.c
├── vendor/                      # limine/ и lvgl/ (через make deps)
├── linker.ld                    # Higher-half линкерный скрипт (0xffffffff80000000)
└── Makefile                     # Сборка ELF и ISO
```

## Сборка и запуск

```bash
make deps          # Скачать Limine v8.x и LVGL v8.3
make -j$(nproc)    # Собрать build/dimaos.elf
make iso           # Собрать загрузочный dimaos.iso (требуется xorriso)
```

Запуск в QEMU (с IDE-диском для сохранения `CONFIG.CFG` и файлов FAT12):
```bash
qemu-img create -f raw disk.img 8M
qemu-system-x86_64 -cdrom dimaos.iso -hda disk.img -m 512M -audiodev pa,id=snd0 -machine pcspk-audiodev=snd0 -serial stdio
```
