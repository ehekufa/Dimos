#include "../include/gui.h"
#include "../include/hal.h"
#include "../include/mm.h"
#include "../include/ps.h"
#include "../include/fs.h"
#include "../include/rtl.h"

#define TASKBAR_HEIGHT 44

static const DIMA_PALETTE g_palettes[THEME_COUNT] = {
    /* THEME_DARK (Fluent Dark) */
    {
        .bg_desktop = 0x0F172A,
        .surface    = 0x1D1F27,
        .surface_2  = 0x23262F,
        .surface_3  = 0x2B2E38,
        .border     = 0x343846,
        .accent     = 0x4F8CFF,
        .text       = 0xF1F3F8,
        .text_dim   = 0xB4B9C8,
        .danger     = 0xFF6464,
        .good       = 0x3DDC84,
        .warn       = 0xF59E0B
    },
    /* THEME_GREEN (Green Phosphor from DimOS-moya) */
    {
        .bg_desktop = 0x071A0E,
        .surface    = 0x0D2616,
        .surface_2  = 0x12341F,
        .surface_3  = 0x19472B,
        .border     = 0x286B41,
        .accent     = 0x3DDC84,
        .text       = 0xD7FFE5,
        .text_dim   = 0x7ED49E,
        .danger     = 0xFF6B6B,
        .good       = 0x52FF9A,
        .warn       = 0xD9F99D
    },
    /* THEME_AMBER (Amber Phosphor from DimOS-moya) */
    {
        .bg_desktop = 0x1C1204,
        .surface    = 0x281A07,
        .surface_2  = 0x36230B,
        .surface_3  = 0x472F10,
        .border     = 0x734D1B,
        .accent     = 0xFFB020,
        .text       = 0xFFF1D0,
        .text_dim   = 0xD4A85C,
        .danger     = 0xFF5C5C,
        .good       = 0xFACC15,
        .warn       = 0xFB923C
    },
    /* THEME_MONO (Monochrome from DimOS-moya) */
    {
        .bg_desktop = 0x111111,
        .surface    = 0x1E1E1E,
        .surface_2  = 0x282828,
        .surface_3  = 0x363636,
        .border     = 0x525252,
        .accent     = 0xE5E5E5,
        .text       = 0xFAFAFA,
        .text_dim   = 0xA3A3A3,
        .danger     = 0xB0B0B0,
        .good       = 0xD4D4D4,
        .warn       = 0xCCCCCC
    }
};

static const char* g_theme_names[THEME_COUNT] = {
    "Dark", "Green", "Amber", "Mono"
};

static DIMA_THEME g_theme = THEME_DARK;
static uint8_t    g_brightness = 20;
static uint8_t    g_volume = 10;

static lv_disp_draw_buf_t g_draw_buf;
static lv_disp_drv_t      g_disp_drv;
static lv_indev_drv_t     g_mouse_drv;
static lv_indev_drv_t     g_kbd_drv;
static lv_group_t*        g_kbd_group = NULL;

static lv_obj_t* g_desktop = NULL;
static lv_obj_t* g_taskbar = NULL;
static lv_obj_t* g_taskbar_apps = NULL;
static lv_obj_t* g_tray_clock = NULL;
static lv_obj_t* g_start_menu = NULL;
static lv_obj_t* g_start_btn = NULL;
static lv_obj_t* g_cc_panel = NULL;
static lv_obj_t* g_cc_btn = NULL;
static lv_obj_t* g_cc_theme_lbl = NULL;
static lv_obj_t* g_cc_sound_lbl = NULL;
static lv_obj_t* g_cc_bright_slider = NULL;
static lv_obj_t* g_dim_overlay = NULL;
static lv_obj_t* g_lock_screen = NULL;
static lv_obj_t* g_lock_clock = NULL;
static lv_obj_t* g_lock_date = NULL;

static lv_obj_t* g_desktop_icons[APP_COUNT];

typedef struct {
    bool       open;
    bool       minimized;
    bool       maximized;
    lv_obj_t*  win;
    lv_obj_t*  titlebar;
    lv_obj_t*  content;
    lv_obj_t*  task_btn;
    lv_coord_t saved_x, saved_y, saved_w, saved_h;
} DIMA_WINDOW;

static DIMA_WINDOW g_windows[APP_COUNT];
static int         g_active_app = -1;
static bool        g_locked = false;

static void disp_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
    DrvFbBlitRect((uint32_t)area->x1, (uint32_t)area->y1, w, h, (const uint32_t*)color_p);
    lv_disp_flush_ready(drv);
}

static void mouse_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    UNUSED(drv);
    MOUSE_STATE ms;
    DrvMouseGetState(&ms);
    data->point.x = (lv_coord_t)ms.x;
    data->point.y = (lv_coord_t)ms.y;
    data->state = ms.left_button ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void kbd_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    UNUSED(drv);
    KEY_EVENT ev;
    if (DrvKeyboardGetEvent(&ev)) {
        if (ev.pressed) {
            if (g_locked) {
                if (ev.ascii == '\n' || ev.ascii == ' ') {
                    GuiUnlock();
                }
            } else {
                if (ev.ascii == 27) {
                    if (g_start_menu) lv_obj_add_flag(g_start_menu, LV_OBJ_FLAG_HIDDEN);
                    if (g_cc_panel) lv_obj_add_flag(g_cc_panel, LV_OBJ_FLAG_HIDDEN);
                }
                AppHandleKey(&ev);
            }
        }

        data->state = ev.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        if (ev.ascii == '\n') data->key = LV_KEY_ENTER;
        else if (ev.ascii == '\b') data->key = LV_KEY_BACKSPACE;
        else if (ev.ascii == '\t') data->key = LV_KEY_NEXT;
        else if (ev.ascii == 27) data->key = LV_KEY_ESC;
        else if (ev.keycode == DIMA_KEY_UP) data->key = LV_KEY_UP;
        else if (ev.keycode == DIMA_KEY_DOWN) data->key = LV_KEY_DOWN;
        else if (ev.keycode == DIMA_KEY_LEFT) data->key = LV_KEY_LEFT;
        else if (ev.keycode == DIMA_KEY_RIGHT) data->key = LV_KEY_RIGHT;
        else if (ev.keycode == DIMA_KEY_DEL) data->key = LV_KEY_DEL;
        else if (ev.keycode == DIMA_KEY_HOME) data->key = LV_KEY_HOME;
        else if (ev.keycode == DIMA_KEY_END) data->key = LV_KEY_END;
        else if (ev.keycode == DIMA_KEY_PGUP) data->key = LV_KEY_PREV;
        else if (ev.keycode == DIMA_KEY_PGDN) data->key = LV_KEY_NEXT;
        else data->key = (uint32_t)(uint8_t)ev.ascii;

        data->continue_reading = DrvKeyboardHasChar();
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        data->continue_reading = false;
    }
}

static void win_activate(int id) {
    if (id < 0 || id >= APP_COUNT || !g_windows[id].win) return;
    g_active_app = id;
    lv_obj_move_foreground(g_windows[id].win);
    if (g_start_menu && !lv_obj_has_flag(g_start_menu, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_move_foreground(g_start_menu);
    }
    if (g_cc_panel && !lv_obj_has_flag(g_cc_panel, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_move_foreground(g_cc_panel);
    }
}

static void win_drag_cb(lv_event_t* e) {
    int id = (int)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t* win = g_windows[id].win;
    if (!win) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);
    lv_coord_t nx = lv_obj_get_x(win) + vect.x;
    lv_coord_t ny = lv_obj_get_y(win) + vect.y;
    if (ny < 0) ny = 0;
    FB_INFO* fb = DrvFbGetInfo();
    if (ny > (lv_coord_t)(fb->height - TASKBAR_HEIGHT - 34)) {
        ny = (lv_coord_t)(fb->height - TASKBAR_HEIGHT - 34);
    }
    lv_obj_set_pos(win, nx, ny);
    win_activate(id);
}

static void win_focus_cb(lv_event_t* e) {
    int id = (int)(uintptr_t)lv_event_get_user_data(e);
    win_activate(id);
}

void GuiCloseApp(DIMA_APP_ID app_id) {
    int id = (int)app_id;
    if (id < 0 || id >= APP_COUNT) return;
    AppOnWindowClosed(app_id);
    if (g_windows[id].win) {
        lv_obj_del(g_windows[id].win);
        g_windows[id].win = NULL;
    }
    if (g_windows[id].task_btn) {
        lv_obj_del(g_windows[id].task_btn);
        g_windows[id].task_btn = NULL;
    }
    g_windows[id].titlebar = NULL;
    g_windows[id].content = NULL;
    g_windows[id].open = false;
    g_windows[id].minimized = false;
    g_windows[id].maximized = false;
    if (g_active_app == id) {
        g_active_app = -1;
        for (int i = 0; i < APP_COUNT; i++) {
            if (g_windows[i].open && !g_windows[i].minimized) {
                g_active_app = i;
                break;
            }
        }
    }
}

static void win_close_cb(lv_event_t* e) {
    int id = (int)(uintptr_t)lv_event_get_user_data(e);
    GuiCloseApp((DIMA_APP_ID)id);
}

static void win_min_cb(lv_event_t* e) {
    int id = (int)(uintptr_t)lv_event_get_user_data(e);
    if (g_windows[id].win) {
        lv_obj_add_flag(g_windows[id].win, LV_OBJ_FLAG_HIDDEN);
        g_windows[id].minimized = true;
        if (g_active_app == id) g_active_app = -1;
    }
}

static void win_max_cb(lv_event_t* e) {
    int id = (int)(uintptr_t)lv_event_get_user_data(e);
    DIMA_WINDOW* w = &g_windows[id];
    if (!w->win) return;
    FB_INFO* fb = DrvFbGetInfo();
    if (!w->maximized) {
        w->saved_x = lv_obj_get_x(w->win); w->saved_y = lv_obj_get_y(w->win);
        w->saved_w = lv_obj_get_width(w->win); w->saved_h = lv_obj_get_height(w->win);
        lv_obj_set_pos(w->win, 0, 0);
        lv_obj_set_size(w->win, (lv_coord_t)fb->width, (lv_coord_t)(fb->height - TASKBAR_HEIGHT));
        if (w->content) {
            lv_obj_set_size(w->content, lv_pct(100), (lv_coord_t)(fb->height - TASKBAR_HEIGHT - 36));
        }
        w->maximized = true;
    } else {
        lv_obj_set_pos(w->win, w->saved_x, w->saved_y);
        lv_obj_set_size(w->win, w->saved_w, w->saved_h);
        if (w->content) {
            lv_obj_set_size(w->content, lv_pct(100), w->saved_h - 36);
        }
        w->maximized = false;
    }
    win_activate(id);
}

static void taskbar_btn_cb(lv_event_t* e) {
    int id = (int)(uintptr_t)lv_event_get_user_data(e);
    DIMA_WINDOW* w = &g_windows[id];
    if (!w->win) return;
    if (w->minimized) {
        lv_obj_clear_flag(w->win, LV_OBJ_FLAG_HIDDEN);
        w->minimized = false;
    }
    win_activate(id);
}

lv_obj_t* GuiCreateWindowFrame(DIMA_APP_ID id, const char* title,
                               lv_coord_t x, lv_coord_t y,
                               lv_coord_t w, lv_coord_t h) {
    DIMA_WINDOW* dw = &g_windows[id];
    if (dw->open && dw->win) {
        lv_obj_clear_flag(dw->win, LV_OBJ_FLAG_HIDDEN);
        dw->minimized = false;
        win_activate((int)id);
        return NULL;
    }

    const DIMA_PALETTE* pal = GuiGetPalette();
    FB_INFO* fb = DrvFbGetInfo();
    if (w > (lv_coord_t)fb->width - 20) w = (lv_coord_t)fb->width - 20;
    if (h > (lv_coord_t)(fb->height - TASKBAR_HEIGHT - 16)) {
        h = (lv_coord_t)(fb->height - TASKBAR_HEIGHT - 16);
    }
    if (x + w > (lv_coord_t)fb->width) x = (lv_coord_t)fb->width - w - 10;
    if (y + h > (lv_coord_t)(fb->height - TASKBAR_HEIGHT)) {
        y = (lv_coord_t)(fb->height - TASKBAR_HEIGHT - h - 8);
    }
    if (x < 0) x = 8;
    if (y < 0) y = 8;

    lv_obj_t* win = lv_obj_create(g_desktop);
    lv_obj_set_pos(win, x, y);
    lv_obj_set_size(win, w, h);
    lv_obj_set_style_pad_all(win, 0, 0);
    lv_obj_set_style_radius(win, 10, 0);
    lv_obj_set_style_bg_color(win, lv_color_hex(pal->surface), 0);
    lv_obj_set_style_border_width(win, 1, 0);
    lv_obj_set_style_border_color(win, lv_color_hex(pal->border), 0);
    lv_obj_clear_flag(win, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(win, win_focus_cb, LV_EVENT_PRESSED, (void*)(uintptr_t)id);

    /* Title Bar */
    lv_obj_t* titlebar = lv_obj_create(win);
    lv_obj_set_size(titlebar, lv_pct(100), 34);
    lv_obj_set_pos(titlebar, 0, 0);
    lv_obj_set_style_bg_color(titlebar, lv_color_hex(pal->surface_2), 0);
    lv_obj_set_style_radius(titlebar, 0, 0);
    lv_obj_set_style_border_width(titlebar, 0, 0);
    lv_obj_set_style_pad_all(titlebar, 6, 0);
    lv_obj_clear_flag(titlebar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(titlebar, win_drag_cb, LV_EVENT_PRESSING, (void*)(uintptr_t)id);

    lv_obj_t* lbl = lv_label_create(titlebar);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_color_hex(pal->text), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 6, 0);

    /* Close (X) */
    lv_obj_t* btn_close = lv_btn_create(titlebar);
    lv_obj_set_size(btn_close, 26, 22);
    lv_obj_align(btn_close, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(pal->danger), 0);
    lv_obj_set_style_radius(btn_close, 6, 0);
    lv_obj_add_event_cb(btn_close, win_close_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)id);
    lv_obj_t* lx = lv_label_create(btn_close);
    lv_label_set_text(lx, "X");
    lv_obj_set_style_text_color(lx, lv_color_hex(pal->text), 0);
    lv_obj_center(lx);

    /* Maximize */
    lv_obj_t* btn_max = lv_btn_create(titlebar);
    lv_obj_set_size(btn_max, 26, 22);
    lv_obj_align(btn_max, LV_ALIGN_RIGHT_MID, -32, 0);
    lv_obj_set_style_bg_color(btn_max, lv_color_hex(pal->surface_3), 0);
    lv_obj_set_style_radius(btn_max, 6, 0);
    lv_obj_add_event_cb(btn_max, win_max_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)id);
    lv_obj_t* lmax = lv_label_create(btn_max);
    lv_label_set_text(lmax, "[]");
    lv_obj_set_style_text_color(lmax, lv_color_hex(pal->text), 0);
    lv_obj_center(lmax);

    /* Minimize (_) */
    lv_obj_t* btn_min = lv_btn_create(titlebar);
    lv_obj_set_size(btn_min, 26, 22);
    lv_obj_align(btn_min, LV_ALIGN_RIGHT_MID, -62, 0);
    lv_obj_set_style_bg_color(btn_min, lv_color_hex(pal->surface_3), 0);
    lv_obj_set_style_radius(btn_min, 6, 0);
    lv_obj_add_event_cb(btn_min, win_min_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)id);
    lv_obj_t* lmin = lv_label_create(btn_min);
    lv_label_set_text(lmin, "_");
    lv_obj_set_style_text_color(lmin, lv_color_hex(pal->text), 0);
    lv_obj_center(lmin);

    /* Content area */
    lv_obj_t* content = lv_obj_create(win);
    lv_obj_set_pos(content, 0, 34);
    lv_obj_set_size(content, lv_pct(100), h - 36);
    lv_obj_set_style_bg_color(content, lv_color_hex(pal->surface), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 0, 0);
    lv_obj_set_style_pad_all(content, 10, 0);
    lv_obj_add_event_cb(content, win_focus_cb, LV_EVENT_PRESSED, (void*)(uintptr_t)id);

    /* Taskbar button */
    lv_obj_t* tbtn = lv_btn_create(g_taskbar_apps);
    lv_obj_set_height(tbtn, 32);
    lv_obj_set_style_bg_color(tbtn, lv_color_hex(pal->surface_3), 0);
    lv_obj_set_style_radius(tbtn, 6, 0);
    lv_obj_add_event_cb(tbtn, taskbar_btn_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)id);
    lv_obj_t* tlbl = lv_label_create(tbtn);
    lv_label_set_text(tlbl, title);
    lv_obj_set_style_text_color(tlbl, lv_color_hex(pal->text), 0);
    lv_obj_center(tlbl);

    dw->open = true;
    dw->minimized = false;
    dw->maximized = false;
    dw->win = win;
    dw->titlebar = titlebar;
    dw->content = content;
    dw->task_btn = tbtn;
    win_activate((int)id);
    return content;
}

void GuiFocusObject(lv_obj_t* obj) {
    if (g_kbd_group && obj) {
        lv_group_add_obj(g_kbd_group, obj);
        lv_group_focus_obj(obj);
    }
}

bool GuiIsWindowOpen(DIMA_APP_ID id) {
    return (id < APP_COUNT) && g_windows[id].open;
}

bool GuiIsWindowActive(DIMA_APP_ID id) {
    return (g_active_app == (int)id) && g_windows[id].open && !g_windows[id].minimized;
}

void GuiOpenApp(DIMA_APP_ID app_id) {
    if (g_start_menu) lv_obj_add_flag(g_start_menu, LV_OBJ_FLAG_HIDDEN);
    if (g_cc_panel) lv_obj_add_flag(g_cc_panel, LV_OBJ_FLAG_HIDDEN);

    switch (app_id) {
        case APP_MY_COMPUTER: AppOpenFiles(); break;
        case APP_NOTEPAD:     AppOpenNotepad(); break;
        case APP_CMD:         AppOpenTerminal(); break;
        case APP_CALCULATOR:  AppOpenCalculator(); break;
        case APP_PAINT:       AppOpenPaint(); break;
        case APP_SNAKE:       AppOpenSnake(); break;
        case APP_MINES:       AppOpenMines(); break;
        case APP_MUSIC:       AppOpenMusic(); break;
        case APP_CHEESY:      AppOpenCheesy(); break;
        case APP_ABOUT:       AppOpenSettings(); break;
        default: break;
    }
}

const DIMA_PALETTE* GuiGetPalette(void) {
    return &g_palettes[g_theme];
}

DIMA_THEME GuiGetTheme(void) {
    return g_theme;
}

const char* GuiGetThemeName(DIMA_THEME theme) {
    if (theme >= THEME_COUNT) return "Dark";
    return g_theme_names[theme];
}

void GuiSelectTheme(DIMA_THEME theme) {
    if (theme >= THEME_COUNT) theme = THEME_DARK;
    g_theme = theme;
    const DIMA_PALETTE* pal = &g_palettes[g_theme];

    if (g_desktop) lv_obj_set_style_bg_color(g_desktop, lv_color_hex(pal->bg_desktop), 0);
    if (g_taskbar) {
        lv_obj_set_style_bg_color(g_taskbar, lv_color_hex(pal->surface), 0);
        lv_obj_set_style_border_color(g_taskbar, lv_color_hex(pal->border), 0);
    }
    if (g_start_btn) lv_obj_set_style_bg_color(g_start_btn, lv_color_hex(pal->accent), 0);
    if (g_cc_btn) lv_obj_set_style_bg_color(g_cc_btn, lv_color_hex(pal->surface_3), 0);
    if (g_tray_clock) lv_obj_set_style_text_color(g_tray_clock, lv_color_hex(pal->text), 0);
    if (g_start_menu) {
        lv_obj_set_style_bg_color(g_start_menu, lv_color_hex(pal->surface), 0);
        lv_obj_set_style_border_color(g_start_menu, lv_color_hex(pal->border), 0);
    }
    if (g_cc_panel) {
        lv_obj_set_style_bg_color(g_cc_panel, lv_color_hex(pal->surface), 0);
        lv_obj_set_style_border_color(g_cc_panel, lv_color_hex(pal->border), 0);
    }
    if (g_cc_theme_lbl) {
        lv_label_set_text_fmt(g_cc_theme_lbl, "Theme: %s", GuiGetThemeName(g_theme));
    }

    for (int i = 0; i < APP_COUNT; i++) {
        if (g_desktop_icons[i]) {
            lv_obj_set_style_bg_color(g_desktop_icons[i], lv_color_hex(pal->surface_2), 0);
            lv_obj_set_style_border_color(g_desktop_icons[i], lv_color_hex(pal->border), 0);
        }
        if (g_windows[i].win) {
            lv_obj_set_style_bg_color(g_windows[i].win, lv_color_hex(pal->surface), 0);
            lv_obj_set_style_border_color(g_windows[i].win, lv_color_hex(pal->border), 0);
        }
        if (g_windows[i].titlebar) {
            lv_obj_set_style_bg_color(g_windows[i].titlebar, lv_color_hex(pal->surface_2), 0);
        }
        if (g_windows[i].content && i != APP_CMD && i != APP_MUSIC) {
            lv_obj_set_style_bg_color(g_windows[i].content, lv_color_hex(pal->surface), 0);
        }
        if (g_windows[i].task_btn) {
            lv_obj_set_style_bg_color(g_windows[i].task_btn, lv_color_hex(pal->surface_3), 0);
        }
    }
    CfgSetCurrent((uint8_t)g_theme, g_brightness, g_volume);
}

void GuiSetBrightness(uint8_t level) {
    if (level < 4) level = 4;
    if (level > 20) level = 20;
    g_brightness = level;
    if (g_dim_overlay) {
        /* 20 -> 0 opa (brightest), 4 -> ~176 opa (dim) */
        lv_opa_t opa = (lv_opa_t)((20U - g_brightness) * 11U);
        lv_obj_set_style_bg_opa(g_dim_overlay, opa, 0);
    }
    if (g_cc_bright_slider) {
        lv_slider_set_value(g_cc_bright_slider, g_brightness, LV_ANIM_OFF);
    }
    CfgSetCurrent((uint8_t)g_theme, g_brightness, g_volume);
}

uint8_t GuiGetBrightness(void) {
    return g_brightness;
}

void GuiSetVolume(uint8_t level) {
    if (level > 20) level = 20;
    g_volume = level;
    DrvSoundSetEnabled(level > 0);
    if (g_cc_sound_lbl) {
        lv_label_set_text_fmt(g_cc_sound_lbl, "Sound: %s", (level > 0) ? "ON" : "MUTE");
    }
    CfgSetCurrent((uint8_t)g_theme, g_brightness, g_volume);
}

uint8_t GuiGetVolume(void) {
    return g_volume;
}

void GuiLock(void) {
    if (g_start_menu) lv_obj_add_flag(g_start_menu, LV_OBJ_FLAG_HIDDEN);
    if (g_cc_panel) lv_obj_add_flag(g_cc_panel, LV_OBJ_FLAG_HIDDEN);
    if (g_lock_screen) {
        g_locked = true;
        lv_obj_clear_flag(g_lock_screen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_lock_screen);
    }
}

void GuiUnlock(void) {
    if (g_lock_screen) {
        g_locked = false;
        lv_obj_add_flag(g_lock_screen, LV_OBJ_FLAG_HIDDEN);
    }
}

bool GuiIsLocked(void) {
    return g_locked;
}

static void desktop_icon_cb(lv_event_t* e) {
    DIMA_APP_ID id = (DIMA_APP_ID)(uintptr_t)lv_event_get_user_data(e);
    GuiOpenApp(id);
}

static void start_btn_cb(lv_event_t* e) {
    UNUSED(e);
    if (g_cc_panel) lv_obj_add_flag(g_cc_panel, LV_OBJ_FLAG_HIDDEN);
    if (lv_obj_has_flag(g_start_menu, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(g_start_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_start_menu);
    } else {
        lv_obj_add_flag(g_start_menu, LV_OBJ_FLAG_HIDDEN);
    }
}

static void cc_btn_cb(lv_event_t* e) {
    UNUSED(e);
    if (g_start_menu) lv_obj_add_flag(g_start_menu, LV_OBJ_FLAG_HIDDEN);
    if (lv_obj_has_flag(g_cc_panel, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(g_cc_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_cc_panel);
    } else {
        lv_obj_add_flag(g_cc_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void cc_theme_cb(lv_event_t* e) {
    UNUSED(e);
    GuiSelectTheme((DIMA_THEME)((g_theme + 1) % THEME_COUNT));
    CfgSave((uint8_t)g_theme, g_brightness, g_volume);
}

static void cc_sound_cb(lv_event_t* e) {
    UNUSED(e);
    GuiSetVolume(g_volume > 0 ? 0 : 10);
    if (g_volume > 0) DrvSoundBeep();
    CfgSave((uint8_t)g_theme, g_brightness, g_volume);
}

static void cc_bright_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    GuiSetBrightness((uint8_t)val);
    CfgSave((uint8_t)g_theme, g_brightness, g_volume);
}

static void lock_btn_cb(lv_event_t* e) {
    UNUSED(e);
    GuiLock();
}

static void unlock_click_cb(lv_event_t* e) {
    UNUSED(e);
    GuiUnlock();
}

static void reboot_cb(lv_event_t* e) { UNUSED(e); HalReboot(); }
static void shutdown_cb(lv_event_t* e) { UNUSED(e); HalShutdown(); }

static lv_obj_t* create_desktop_icon(const char* text, lv_coord_t x, lv_coord_t y,
                                     DIMA_APP_ID id, uint32_t badge_col) {
    const DIMA_PALETTE* pal = GuiGetPalette();
    lv_obj_t* btn = lv_btn_create(g_desktop);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, 102, 72);
    lv_obj_set_style_bg_opa(btn, LV_OPA_70, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(pal->surface_2), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(pal->border), 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_add_event_cb(btn, desktop_icon_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)id);

    lv_obj_t* dot = lv_obj_create(btn);
    lv_obj_set_size(dot, 18, 18);
    lv_obj_align(dot, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_bg_color(dot, lv_color_hex(badge_col), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_radius(dot, 6, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(pal->text), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -2);
    return btn;
}

void GuiInit(void) {
    lv_init();

    FB_INFO* fb = DrvFbGetInfo();
    uint32_t buf_pixels = fb->width * 64;
    lv_color_t* buf1 = (lv_color_t*)ExAllocatePool(buf_pixels * sizeof(lv_color_t));
    lv_color_t* buf2 = (lv_color_t*)ExAllocatePool(buf_pixels * sizeof(lv_color_t));
    lv_disp_draw_buf_init(&g_draw_buf, buf1, buf2, buf_pixels);

    lv_disp_drv_init(&g_disp_drv);
    g_disp_drv.hor_res  = (lv_coord_t)fb->width;
    g_disp_drv.ver_res  = (lv_coord_t)fb->height;
    g_disp_drv.flush_cb = disp_flush_cb;
    g_disp_drv.draw_buf = &g_draw_buf;
    lv_disp_drv_register(&g_disp_drv);

    lv_indev_drv_init(&g_mouse_drv);
    g_mouse_drv.type    = LV_INDEV_TYPE_POINTER;
    g_mouse_drv.read_cb = mouse_read_cb;
    lv_indev_t* mouse_indev = lv_indev_drv_register(&g_mouse_drv);

    lv_obj_t* cursor = lv_obj_create(lv_layer_sys());
    lv_obj_set_size(cursor, 12, 12);
    lv_obj_set_style_bg_color(cursor, lv_color_hex(0x4F8CFF), 0);
    lv_obj_set_style_border_color(cursor, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(cursor, 2, 0);
    lv_obj_set_style_radius(cursor, 6, 0);
    lv_indev_set_cursor(mouse_indev, cursor);

    g_kbd_group = lv_group_create();
    lv_indev_drv_init(&g_kbd_drv);
    g_kbd_drv.type    = LV_INDEV_TYPE_KEYPAD;
    g_kbd_drv.read_cb = kbd_read_cb;
    lv_indev_t* kbd_indev = lv_indev_drv_register(&g_kbd_drv);
    lv_indev_set_group(kbd_indev, g_kbd_group);

    /* Apply persisted config before constructing UI */
    g_theme      = (DIMA_THEME)CfgGetTheme();
    g_brightness = CfgGetBrightness();
    g_volume     = CfgGetVolume();
    DrvSoundSetEnabled(g_volume > 0);
    const DIMA_PALETTE* pal = GuiGetPalette();

    /* Desktop */
    g_desktop = lv_scr_act();
    lv_obj_set_style_bg_color(g_desktop, lv_color_hex(pal->bg_desktop), 0);
    lv_obj_clear_flag(g_desktop, LV_OBJ_FLAG_SCROLLABLE);

    static const char* icon_labels[APP_COUNT] = {
        "Files", "Notepad", "Terminal", "Calc",
        "Paint", "Snake", "Mines", "Music", "Cheesy", "Settings"
    };
    static const uint32_t tile_colors[APP_COUNT] = {
        0x2563EB, 0x059669, 0x374151, 0xD97706,
        0x9333EA, 0x16A34A, 0x4F46E5, 0xEC4899, 0xEAB308, 0x64748B
    };

    /* 2 columns of 5 icons */
    for (int i = 0; i < APP_COUNT; i++) {
        int col = i / 5;
        int row = i % 5;
        g_desktop_icons[i] = create_desktop_icon(
            icon_labels[i],
            (lv_coord_t)(20 + col * 114),
            (lv_coord_t)(20 + row * 84),
            (DIMA_APP_ID)i,
            tile_colors[i]);
    }

    /* Taskbar */
    g_taskbar = lv_obj_create(g_desktop);
    lv_obj_set_pos(g_taskbar, 0, (lv_coord_t)(fb->height - TASKBAR_HEIGHT));
    lv_obj_set_size(g_taskbar, (lv_coord_t)fb->width, TASKBAR_HEIGHT);
    lv_obj_set_style_bg_color(g_taskbar, lv_color_hex(pal->surface), 0);
    lv_obj_set_style_border_width(g_taskbar, 1, 0);
    lv_obj_set_style_border_color(g_taskbar, lv_color_hex(pal->border), 0);
    lv_obj_set_style_radius(g_taskbar, 0, 0);
    lv_obj_set_style_pad_all(g_taskbar, 5, 0);
    lv_obj_clear_flag(g_taskbar, LV_OBJ_FLAG_SCROLLABLE);

    g_start_btn = lv_btn_create(g_taskbar);
    lv_obj_set_size(g_start_btn, 80, 32);
    lv_obj_align(g_start_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(g_start_btn, lv_color_hex(pal->accent), 0);
    lv_obj_set_style_radius(g_start_btn, 8, 0);
    lv_obj_add_event_cb(g_start_btn, start_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* slbl = lv_label_create(g_start_btn);
    lv_label_set_text(slbl, "Start");
    lv_obj_set_style_text_color(slbl, lv_color_hex(pal->text), 0);
    lv_obj_center(slbl);

    g_taskbar_apps = lv_obj_create(g_taskbar);
    lv_obj_set_pos(g_taskbar_apps, 88, 0);
    lv_obj_set_size(g_taskbar_apps, (lv_coord_t)(fb->width - 235), 32);
    lv_obj_set_style_bg_opa(g_taskbar_apps, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_taskbar_apps, 0, 0);
    lv_obj_set_style_pad_all(g_taskbar_apps, 0, 0);
    lv_obj_set_flex_flow(g_taskbar_apps, LV_FLEX_FLOW_ROW);

    g_cc_btn = lv_btn_create(g_taskbar);
    lv_obj_set_size(g_cc_btn, 64, 30);
    lv_obj_align(g_cc_btn, LV_ALIGN_RIGHT_MID, -68, 0);
    lv_obj_set_style_bg_color(g_cc_btn, lv_color_hex(pal->surface_3), 0);
    lv_obj_set_style_radius(g_cc_btn, 6, 0);
    lv_obj_add_event_cb(g_cc_btn, cc_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* cclbl = lv_label_create(g_cc_btn);
    lv_label_set_text(cclbl, "Ctrl");
    lv_obj_set_style_text_color(cclbl, lv_color_hex(pal->text), 0);
    lv_obj_center(cclbl);

    g_tray_clock = lv_label_create(g_taskbar);
    lv_obj_align(g_tray_clock, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_text_color(g_tray_clock, lv_color_hex(pal->text), 0);
    lv_label_set_text_fmt(g_tray_clock, "%02u:%02u", HalClockHours(), HalClockMinutes());

    /* Start Menu (App List + Windows 10 Live Tiles) */
    lv_coord_t sm_h = 432;
    if (sm_h > (lv_coord_t)(fb->height - TASKBAR_HEIGHT - 12)) {
        sm_h = (lv_coord_t)(fb->height - TASKBAR_HEIGHT - 12);
    }
    g_start_menu = lv_obj_create(g_desktop);
    lv_obj_set_pos(g_start_menu, 0, (lv_coord_t)(fb->height - TASKBAR_HEIGHT - sm_h));
    lv_obj_set_size(g_start_menu, 456, sm_h);
    lv_obj_set_style_bg_color(g_start_menu, lv_color_hex(pal->surface), 0);
    lv_obj_set_style_border_width(g_start_menu, 1, 0);
    lv_obj_set_style_border_color(g_start_menu, lv_color_hex(pal->border), 0);
    lv_obj_set_style_radius(g_start_menu, 8, 0);
    lv_obj_set_style_pad_all(g_start_menu, 10, 0);
    lv_obj_clear_flag(g_start_menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_start_menu, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* sm_left = lv_obj_create(g_start_menu);
    lv_obj_set_pos(sm_left, 0, 0);
    lv_obj_set_size(sm_left, 192, sm_h - 22);
    lv_obj_set_style_bg_opa(sm_left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sm_left, 0, 0);
    lv_obj_set_style_pad_all(sm_left, 2, 0);
    lv_obj_set_flex_flow(sm_left, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* sm_hdr = lv_label_create(sm_left);
    lv_label_set_text(sm_hdr, "DimaOS 64-bit");
    lv_obj_set_style_text_color(sm_hdr, lv_color_hex(pal->accent), 0);

    static const char* full_names[APP_COUNT] = {
        "Files (C:\\)", "Notepad", "Terminal", "Calculator",
        "Paint", "Snake", "Minesweeper", "Music", "Cheesy Balls", "Settings"
    };
    for (int i = 0; i < APP_COUNT; i++) {
        lv_obj_t* b = lv_btn_create(sm_left);
        lv_obj_set_size(b, lv_pct(100), 28);
        lv_obj_set_style_bg_color(b, lv_color_hex(pal->surface_2), 0);
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_add_event_cb(b, desktop_icon_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, full_names[i]);
        lv_obj_set_style_text_color(l, lv_color_hex(pal->text), 0);
        lv_obj_center(l);
    }

    lv_obj_t* b_lock = lv_btn_create(sm_left);
    lv_obj_set_size(b_lock, lv_pct(100), 26);
    lv_obj_set_style_bg_color(b_lock, lv_color_hex(pal->surface_3), 0);
    lv_obj_add_event_cb(b_lock, lock_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l_lock = lv_label_create(b_lock);
    lv_label_set_text(l_lock, "Lock");
    lv_obj_center(l_lock);

    lv_obj_t* b_reb = lv_btn_create(sm_left);
    lv_obj_set_size(b_reb, lv_pct(100), 26);
    lv_obj_set_style_bg_color(b_reb, lv_color_hex(pal->surface_3), 0);
    lv_obj_add_event_cb(b_reb, reboot_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l_reb = lv_label_create(b_reb);
    lv_label_set_text(l_reb, "Reboot");
    lv_obj_center(l_reb);

    lv_obj_t* b_shut = lv_btn_create(sm_left);
    lv_obj_set_size(b_shut, lv_pct(100), 26);
    lv_obj_set_style_bg_color(b_shut, lv_color_hex(pal->danger), 0);
    lv_obj_add_event_cb(b_shut, shutdown_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l_shut = lv_label_create(b_shut);
    lv_label_set_text(l_shut, "Shutdown");
    lv_obj_center(l_shut);

    /* Right column: Windows 10 Live Tiles */
    lv_obj_t* sm_right = lv_obj_create(g_start_menu);
    lv_obj_set_pos(sm_right, 202, 0);
    lv_obj_set_size(sm_right, 232, sm_h - 22);
    lv_obj_set_style_bg_opa(sm_right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(sm_right, 0, 0);
    lv_obj_set_style_pad_all(sm_right, 4, 0);

    for (int i = 0; i < APP_COUNT; i++) {
        int row = i / 2;
        int col = i % 2;
        lv_obj_t* tile = lv_btn_create(sm_right);
        lv_obj_set_pos(tile, col * 108, row * 78);
        lv_obj_set_size(tile, 100, 72);
        lv_obj_set_style_bg_color(tile, lv_color_hex(tile_colors[i]), 0);
        lv_obj_set_style_radius(tile, 6, 0);
        lv_obj_add_event_cb(tile, desktop_icon_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        lv_obj_t* tl = lv_label_create(tile);
        lv_label_set_text(tl, icon_labels[i]);
        lv_obj_set_style_text_color(tl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(tl);
    }

    /* Control Center Popup (DimOS-moya style) */
    g_cc_panel = lv_obj_create(g_desktop);
    lv_obj_set_size(g_cc_panel, 250, 236);
    lv_obj_set_pos(g_cc_panel, (lv_coord_t)(fb->width - 258),
                   (lv_coord_t)(fb->height - TASKBAR_HEIGHT - 242));
    lv_obj_set_style_bg_color(g_cc_panel, lv_color_hex(pal->surface), 0);
    lv_obj_set_style_border_width(g_cc_panel, 1, 0);
    lv_obj_set_style_border_color(g_cc_panel, lv_color_hex(pal->border), 0);
    lv_obj_set_style_radius(g_cc_panel, 10, 0);
    lv_obj_set_style_pad_all(g_cc_panel, 12, 0);
    lv_obj_clear_flag(g_cc_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_cc_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* cc_title = lv_label_create(g_cc_panel);
    lv_label_set_text(cc_title, "Control Center");
    lv_obj_set_style_text_color(cc_title, lv_color_hex(pal->text), 0);
    lv_obj_set_pos(cc_title, 0, 0);

    lv_obj_t* cc_theme_btn = lv_btn_create(g_cc_panel);
    lv_obj_set_pos(cc_theme_btn, 0, 28);
    lv_obj_set_size(cc_theme_btn, 224, 34);
    lv_obj_set_style_bg_color(cc_theme_btn, lv_color_hex(pal->surface_3), 0);
    lv_obj_add_event_cb(cc_theme_btn, cc_theme_cb, LV_EVENT_CLICKED, NULL);
    g_cc_theme_lbl = lv_label_create(cc_theme_btn);
    lv_label_set_text_fmt(g_cc_theme_lbl, "Theme: %s", GuiGetThemeName(g_theme));
    lv_obj_center(g_cc_theme_lbl);

    lv_obj_t* cc_sound_btn = lv_btn_create(g_cc_panel);
    lv_obj_set_pos(cc_sound_btn, 0, 68);
    lv_obj_set_size(cc_sound_btn, 224, 34);
    lv_obj_set_style_bg_color(cc_sound_btn, lv_color_hex(pal->surface_3), 0);
    lv_obj_add_event_cb(cc_sound_btn, cc_sound_cb, LV_EVENT_CLICKED, NULL);
    g_cc_sound_lbl = lv_label_create(cc_sound_btn);
    lv_label_set_text_fmt(g_cc_sound_lbl, "Sound: %s", (g_volume > 0) ? "ON" : "MUTE");
    lv_obj_center(g_cc_sound_lbl);

    lv_obj_t* br_lbl = lv_label_create(g_cc_panel);
    lv_label_set_text(br_lbl, "Brightness");
    lv_obj_set_style_text_color(br_lbl, lv_color_hex(pal->text_dim), 0);
    lv_obj_set_pos(br_lbl, 0, 110);

    g_cc_bright_slider = lv_slider_create(g_cc_panel);
    lv_obj_set_pos(g_cc_bright_slider, 4, 134);
    lv_obj_set_size(g_cc_bright_slider, 216, 14);
    lv_slider_set_range(g_cc_bright_slider, 4, 20);
    lv_slider_set_value(g_cc_bright_slider, g_brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(g_cc_bright_slider, cc_bright_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t* cc_lock_btn = lv_btn_create(g_cc_panel);
    lv_obj_set_pos(cc_lock_btn, 0, 168);
    lv_obj_set_size(cc_lock_btn, 108, 34);
    lv_obj_set_style_bg_color(cc_lock_btn, lv_color_hex(pal->accent), 0);
    lv_obj_add_event_cb(cc_lock_btn, lock_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ccl = lv_label_create(cc_lock_btn);
    lv_label_set_text(ccl, "Lock");
    lv_obj_center(ccl);

    lv_obj_t* cc_reb_btn = lv_btn_create(g_cc_panel);
    lv_obj_set_pos(cc_reb_btn, 116, 168);
    lv_obj_set_size(cc_reb_btn, 108, 34);
    lv_obj_set_style_bg_color(cc_reb_btn, lv_color_hex(pal->danger), 0);
    lv_obj_add_event_cb(cc_reb_btn, reboot_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ccr = lv_label_create(cc_reb_btn);
    lv_label_set_text(ccr, "Reboot");
    lv_obj_center(ccr);

    /* Brightness dim overlay (non-clickable, sits on top layer) */
    g_dim_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(g_dim_overlay, 0, 0);
    lv_obj_set_size(g_dim_overlay, (lv_coord_t)fb->width, (lv_coord_t)fb->height);
    lv_obj_set_style_bg_color(g_dim_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(g_dim_overlay, 0, 0);
    lv_obj_set_style_radius(g_dim_overlay, 0, 0);
    lv_obj_clear_flag(g_dim_overlay, LV_OBJ_FLAG_CLICKABLE);
    GuiSetBrightness(g_brightness);

    /* Lock Screen overlay (DimOS-moya style) */
    g_lock_screen = lv_obj_create(lv_layer_top());
    lv_obj_set_pos(g_lock_screen, 0, 0);
    lv_obj_set_size(g_lock_screen, (lv_coord_t)fb->width, (lv_coord_t)fb->height);
    lv_obj_set_style_bg_color(g_lock_screen, lv_color_hex(0x090D16), 0);
    lv_obj_set_style_bg_opa(g_lock_screen, LV_OPA_90, 0);
    lv_obj_set_style_border_width(g_lock_screen, 0, 0);
    lv_obj_set_style_radius(g_lock_screen, 0, 0);
    lv_obj_clear_flag(g_lock_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_lock_screen, unlock_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(g_lock_screen, LV_OBJ_FLAG_HIDDEN);

    g_lock_clock = lv_label_create(g_lock_screen);
    lv_obj_set_style_text_font(g_lock_clock, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(g_lock_clock, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text_fmt(g_lock_clock, "%02u:%02u", HalClockHours(), HalClockMinutes());
    lv_obj_align(g_lock_clock, LV_ALIGN_CENTER, 0, -40);

    g_lock_date = lv_label_create(g_lock_screen);
    lv_obj_set_style_text_font(g_lock_date, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_lock_date, lv_color_hex(0x94A3B8), 0);
    lv_label_set_text_fmt(g_lock_date, "%04u-%02u-%02u  |  DimaOS Locked",
                          HalClockYear(), HalClockMonth(), HalClockDay());
    lv_obj_align(g_lock_date, LV_ALIGN_CENTER, 0, 4);

    lv_obj_t* lock_hint = lv_label_create(g_lock_screen);
    lv_label_set_text(lock_hint, "Click anywhere or press Enter to unlock");
    lv_obj_set_style_text_color(lock_hint, lv_color_hex(0x4F8CFF), 0);
    lv_obj_align(lock_hint, LV_ALIGN_BOTTOM_MID, 0, -36);

    HalPrintf("[GUI] DimaOS Fluent Desktop & DimOS-moya apps initialized\n");
}

void GuiTask(void) {
    static uint64_t last_sec = (uint64_t)-1;
    uint64_t now_ms = HalGetTickCount();
    uint64_t sec = now_ms / 1000ULL;
    if (sec != last_sec) {
        last_sec = sec;
        DIMA_TIME tm;
        HalClockGetTime(&tm);
        if (g_tray_clock) {
            lv_label_set_text_fmt(g_tray_clock, "%02u:%02u", tm.hours, tm.minutes);
        }
        if (g_locked && g_lock_clock) {
            lv_label_set_text_fmt(g_lock_clock, "%02u:%02u", tm.hours, tm.minutes);
        }
    }
    if (!g_locked) {
        AppTick(now_ms);
    }
    lv_timer_handler();
}
