#ifndef DIMAOS_GUI_H
#define DIMAOS_GUI_H

#include "types.h"
#include "fs.h"
#include "drivers.h"
#include "lvgl.h"

typedef enum {
    APP_MY_COMPUTER = 0,
    APP_NOTEPAD,
    APP_CMD,
    APP_CALCULATOR,
    APP_PAINT,
    APP_SNAKE,
    APP_MINES,
    APP_MUSIC,
    APP_CHEESY,
    APP_ABOUT,
    APP_COUNT
} DIMA_APP_ID;

/* Theme palette exposed to apps */
typedef struct {
    uint32_t bg_desktop;
    uint32_t surface;
    uint32_t surface_2;
    uint32_t surface_3;
    uint32_t border;
    uint32_t accent;
    uint32_t text;
    uint32_t text_dim;
    uint32_t danger;
    uint32_t good;
    uint32_t warn;
} DIMA_PALETTE;

void GuiInit(void);
void GuiTask(void);
void GuiOpenApp(DIMA_APP_ID app_id);
void GuiCloseApp(DIMA_APP_ID app_id);
void GuiLock(void);
void GuiUnlock(void);
bool GuiIsLocked(void);
void GuiSelectTheme(DIMA_THEME theme);
DIMA_THEME GuiGetTheme(void);
const char* GuiGetThemeName(DIMA_THEME theme);
const DIMA_PALETTE* GuiGetPalette(void);
void GuiSetBrightness(uint8_t level); /* 4..20 */
uint8_t GuiGetBrightness(void);
void GuiSetVolume(uint8_t level);     /* 0..20 */
uint8_t GuiGetVolume(void);

/* Shared window manager & input helpers for modular app files */
lv_obj_t* GuiCreateWindowFrame(DIMA_APP_ID id, const char* title,
                               lv_coord_t x, lv_coord_t y,
                               lv_coord_t w, lv_coord_t h);
void GuiFocusObject(lv_obj_t* obj);
bool GuiIsWindowOpen(DIMA_APP_ID id);
bool GuiIsWindowActive(DIMA_APP_ID id);

/* App constructors / lifecycle hooks */
void AppOpenFiles(void);
void AppRefreshFiles(void);
void AppOpenNotepad(void);
void AppOpenTerminal(void);
void AppOpenCalculator(void);
void AppOpenSettings(void);

void AppOpenPaint(void);
void AppOpenSnake(void);
void AppOpenMines(void);
void AppOpenMusic(void);
void AppOpenCheesy(void);

void AppOnWindowClosed(DIMA_APP_ID id);
void AppTick(uint64_t now_ms);
void AppHandleKey(const KEY_EVENT* ev);

#endif /* DIMAOS_GUI_H */
