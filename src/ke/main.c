#include "../include/types.h"
#include "../include/hal.h"
#include "../include/mm.h"
#include "../include/ob.h"
#include "../include/ps.h"
#include "../include/fs.h"
#include "../include/drivers.h"
#include "../include/gui.h"
#include "limine.h"

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(2);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request g_fb_req = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request g_memmap_req = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request g_hhdm_req = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request g_rsdp_req = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

void KeMain(void) {
    HalCli();
    HalSerialInit();
    HalPrintf("\n========================================\n");
    HalPrintf("  DimaOS v1.0.0\n");
    HalPrintf("  64-bit x86_64 NT-style Hybrid Kernel\n");
    HalPrintf("========================================\n");

    if (!LIMINE_BASE_REVISION_SUPPORTED) {
        HalPrintf("[KE] Limine base revision not supported!\n");
        for (;;) HalHlt();
    }

    if (!g_fb_req.response || g_fb_req.response->framebuffer_count < 1 ||
        !g_memmap_req.response || !g_hhdm_req.response) {
        HalPrintf("[KE] Missing Limine bootloader responses!\n");
        for (;;) HalHlt();
    }

    uint64_t hhdm_offset = g_hhdm_req.response->offset;
    uint64_t rsdp_addr = g_rsdp_req.response ? (uint64_t)g_rsdp_req.response->address : 0;

    /* 1. HAL: GDT, IDT, PIC, APIC, ACPI, RTC */
    HalInitGdt();
    HalInitIdt();
    HalInitPic();
    HalInitApic(hhdm_offset);
    HalAcpiInit(rsdp_addr, hhdm_offset);
    HalClockInit();

    /* 2. Memory Manager: PMM, VMM, Kernel Pool */
    MmInitPmm(g_memmap_req.response, hhdm_offset);
    MmInitVmm();
    MmInitPool();

    /* 3. Object, Process/Thread, File Systems & Persistent Config */
    ObInit();
    PsInit();
    FsInit();

    /* 4. Drivers: Framebuffer, Keyboard, Mouse, Sound, ATA IDE */
    struct limine_framebuffer* fb = g_fb_req.response->framebuffers[0];
    DrvFbInit(fb);
    DrvKeyboardInit();
    DrvMouseInit((int32_t)fb->width, (int32_t)fb->height);
    DrvSoundInit();
    DrvAtaInit();
    VolInit();
    CfgInit();

    /* 5. System Timer (1000 Hz) & Enable Interrupts */
    HalInitTimer(1000);
    HalSti();

    /* 6. Graphical User Interface (LVGL) */
    GuiInit();
    DrvSoundPlayStartup();

    HalPrintf("[KE] DimaOS boot complete. Entering GUI loop.\n");
    for (;;) {
        DrvInputPoll();
        GuiTask();
        HalHlt();
    }
}
