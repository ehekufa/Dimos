#ifndef DIMAOS_DRIVERS_H
#define DIMAOS_DRIVERS_H

#include "types.h"
#include "limine.h"

/* PS/2 Keyboard */
typedef struct {
    uint8_t scancode;
    char    ascii;
    bool    pressed;
    bool    shift;
    bool    ctrl;
    bool    alt;
} KEY_EVENT;

void DrvKeyboardInit(void);
bool DrvKeyboardHasChar(void);
char DrvKeyboardGetChar(void);
bool DrvKeyboardGetEvent(KEY_EVENT* ev);

/* PS/2 Mouse */
typedef struct {
    int32_t x;
    int32_t y;
    bool    left_button;
    bool    right_button;
    bool    middle_button;
} MOUSE_STATE;

void DrvMouseInit(int32_t screen_width, int32_t screen_height);
void DrvMouseGetState(MOUSE_STATE* state);

/* Framebuffer */
typedef struct {
    uint32_t* address;
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch; /* in pixels */
    uint16_t  bpp;
} FB_INFO;

void     DrvFbInit(struct limine_framebuffer* fb);
FB_INFO* DrvFbGetInfo(void);
void     DrvFbPutPixel(uint32_t x, uint32_t y, uint32_t color);
void     DrvFbBlitRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint32_t* pixels);

#endif /* DIMAOS_DRIVERS_H */
