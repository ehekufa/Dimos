#include "../include/drivers.h"
#include "../include/hal.h"

static FB_INFO g_fb;

void DrvFbInit(struct limine_framebuffer* fb) {
    if (!fb) return;
    g_fb.address = (uint32_t*)fb->address;
    g_fb.width   = (uint32_t)fb->width;
    g_fb.height  = (uint32_t)fb->height;
    g_fb.pitch   = (uint32_t)(fb->pitch / 4);
    g_fb.bpp     = fb->bpp;

    HalPrintf("[DRV] Framebuffer initialized: %ux%u (%u bpp)\n",
              g_fb.width, g_fb.height, g_fb.bpp);
}

FB_INFO* DrvFbGetInfo(void) {
    return &g_fb;
}

void DrvFbPutPixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x < g_fb.width && y < g_fb.height && g_fb.address) {
        g_fb.address[y * g_fb.pitch + x] = color;
    }
}

void DrvFbBlitRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint32_t* pixels) {
    if (!g_fb.address || !pixels) return;
    for (uint32_t row = 0; row < h; row++) {
        uint32_t dy = y + row;
        if (dy >= g_fb.height) break;
        uint32_t* dst = &g_fb.address[dy * g_fb.pitch + x];
        const uint32_t* src = &pixels[row * w];
        for (uint32_t col = 0; col < w; col++) {
            if (x + col >= g_fb.width) break;
            dst[col] = src[col];
        }
    }
}
