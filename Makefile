CC      := gcc
LD      := ld
AS      := gcc
CFLAGS  := -std=c11 -ffreestanding -fno-stack-protector -fno-stack-check \
           -fno-lto -fno-pic -fno-pie -m64 -march=x86-64 -mno-80387 -mno-mmx \
           -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel \
           -Wall -Wextra -O2 -DLV_CONF_INCLUDE_SIMPLE \
           -Isrc/include -Ivendor/limine -Ivendor/lvgl -Ivendor/lvgl/src
LDFLAGS := -m elf_x86_64 -nostdlib -static -z max-page-size=0x1000 -T linker.ld

KERNEL_C := $(shell find src -name '*.c')
KERNEL_S := $(shell find src -name '*.S')
LVGL_C   := $(shell find vendor/lvgl/src -name '*.c')

KERNEL_OBJS := $(patsubst src/%.c,build/kernel/%.o,$(KERNEL_C)) \
               $(patsubst src/%.S,build/kernel/%.o,$(KERNEL_S))
LVGL_OBJS   := $(patsubst vendor/lvgl/src/%.c,build/lvgl/%.o,$(LVGL_C))

TARGET_ELF := build/dimaos.elf
TARGET_ISO := dimaos.iso

.PHONY: all deps elf iso clean

all: elf

deps:
	@if [ ! -d vendor/limine ]; then \
		git clone --depth=1 --branch=v8.x-binary https://github.com/limine-bootloader/limine.git vendor/limine; \
	fi
	@if [ ! -d vendor/lvgl ]; then \
		git clone --depth=1 --branch=release/v8.3 https://github.com/lvgl/lvgl.git vendor/lvgl; \
	fi

elf: $(TARGET_ELF)

$(TARGET_ELF): $(KERNEL_OBJS) $(LVGL_OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) $(KERNEL_OBJS) $(LVGL_OBJS) -o $@
	@echo "[LD] Built $(TARGET_ELF)"

build/kernel/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/kernel/%.o: src/%.S
	@mkdir -p $(dir $@)
	$(AS) -c $< -o $@

build/lvgl/%.o: vendor/lvgl/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wno-unused-parameter -Wno-sign-compare -c $< -o $@

iso: $(TARGET_ELF)
	@mkdir -p iso_root/boot/limine iso_root/EFI/BOOT
	cp $(TARGET_ELF) iso_root/boot/dimaos.elf
	cp boot/limine.conf iso_root/boot/limine/limine.conf
	cp vendor/limine/limine-bios.sys iso_root/boot/limine/
	cp vendor/limine/limine-bios-cd.bin iso_root/boot/limine/
	cp vendor/limine/limine-uefi-cd.bin iso_root/boot/limine/
	cp vendor/limine/BOOTX64.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		iso_root -o $(TARGET_ISO)
	@echo "[ISO] Built $(TARGET_ISO)"

clean:
	rm -rf build iso_root $(TARGET_ISO)
