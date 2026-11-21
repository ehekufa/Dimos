#ifndef DIMAOS_DRIVERS_H
#define DIMAOS_DRIVERS_H

#include "types.h"
#include "limine.h"

/* Special key codes (above ASCII) */
#define DIMA_KEY_NONE   0x0000
#define DIMA_KEY_UP     0x0100
#define DIMA_KEY_RIGHT  0x0101
#define DIMA_KEY_DOWN   0x0102
#define DIMA_KEY_LEFT   0x0103
#define DIMA_KEY_HOME   0x0104
#define DIMA_KEY_END    0x0105
#define DIMA_KEY_PGUP   0x0106
#define DIMA_KEY_PGDN   0x0107
#define DIMA_KEY_DEL    0x0108

/* PS/2 Keyboard */
typedef struct {
    uint8_t  scancode;
    uint16_t keycode;
    char     ascii;
    bool     pressed;
    bool     shift;
    bool     ctrl;
    bool     alt;
} KEY_EVENT;

void DrvKeyboardInit(void);
bool DrvKeyboardHasChar(void);
char DrvKeyboardGetChar(void);
bool DrvKeyboardGetEvent(KEY_EVENT* ev);
void DrvKeyboardFeedByte(uint8_t scancode);

/* PS/2 Mouse */
typedef struct {
    int32_t x;
    int32_t y;
    int32_t wheel;
    bool    left_button;
    bool    right_button;
    bool    middle_button;
    bool    available;
} MOUSE_STATE;

void    DrvMouseInit(int32_t screen_width, int32_t screen_height);
void    DrvMouseGetState(MOUSE_STATE* state);
int32_t DrvMouseConsumeWheel(void);
bool    DrvMouseIsAvailable(void);
void    DrvMouseFeedByte(uint8_t data);
void    DrvInputPoll(void);

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

/* PC Speaker Sound */
typedef struct {
    uint16_t hertz; /* 0 = rest */
    uint16_t ticks; /* duration in 10 ms units */
} SOUND_NOTE;

void DrvSoundInit(void);
void DrvSoundTick(void); /* called at 100 Hz (every 10 ms) */
void DrvSoundPlay(const SOUND_NOTE* notes, uint16_t count, bool repeat);
void DrvSoundStop(void);
bool DrvSoundIsPlaying(void);
void DrvSoundSetEnabled(bool enabled);
bool DrvSoundIsEnabled(void);
void DrvSoundPlayStartup(void);
void DrvSoundPlayMarch(void);
void DrvSoundPlayWaltz(void);
void DrvSoundPlayNom(void);
void DrvSoundPlayBurp(void);
void DrvSoundBeep(void);
void DrvSoundAlert(void);

/* IDE / ATA PIO Disk (Primary Channel, LBA28) */
bool        DrvAtaInit(void);
bool        DrvAtaPresent(void);
uint32_t    DrvAtaSectors(void);
const char* DrvAtaModel(void);
bool        DrvAtaRead(uint32_t lba, void* buffer, uint16_t count);
bool        DrvAtaWrite(uint32_t lba, const void* buffer, uint16_t count);
bool        DrvAtaFlush(void);

#endif /* DIMAOS_DRIVERS_H */
