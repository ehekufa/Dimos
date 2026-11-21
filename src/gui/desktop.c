#include "../include/gui.h"
#include "../include/drivers.h"
#include "../include/hal.h"
#include "../include/mm.h"
#include "../include/ps.h"
#include "../include/fs.h"
#include "../include/rtl.h"
#include "lvgl.h"

#define TASKBAR_HEIGHT 44
#define MAX_WINDOWS    5

/* Fluent Dark Palette */
#define COL_BG_DESKTOP 0x0F172A
#define COL_SURFACE    0x1D1F27
#define COL_SURFACE_2  0x23262F
#define COL_SURFACE_3  0x2B2E38
#define COL_BORDER     0x343846
#define COL_ACCENT     0x4F8CFF
#define COL_TEXT       0xF1F3F8
#define COL_TEXT_DIM   0xB4B9C8
#define COL_DANGER     0xFF6464

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

typedef struct {
    bool       open;
    bool       minimized;
    bool       maximized;
    lv_obj_t*  win;
    lv_obj_t*  task_btn;
    lv_coord_t saved_x, saved_y, saved_w, saved_h;
} DIMA_WINDOW;

static DIMA_WINDOW g_windows[MAX_WINDOWS];
static lv_obj_t* g_cmd_output = NULL;
static lv_obj_t* g_cmd_input  = NULL;
static lv_obj_t* g_notepad_ta = NULL;

/* Calculator state */
static lv_obj_t* g_calc_display = NULL;
static int64_t   g_calc_acc = 0;
static int64_t   g_calc_cur = 0;
static char      g_calc_op  = 0;
static bool      g_calc_new = true;

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
        data->state = ev.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        if (ev.ascii == '\n') data->key = LV_KEY_ENTER;
        else if (ev.ascii == '\b') data->key = LV_KEY_BACKSPACE;
        else if (ev.ascii == '\t') data->key = LV_KEY_NEXT;
        else if (ev.ascii == 27) data->key = LV_KEY_ESC;
        else data->key = (uint32_t)ev.ascii;
        data->continue_reading = DrvKeyboardHasChar();
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        data->continue_reading = false;
    }
}

static void win_drag_cb(lv_event_t* e) {
    lv_obj_t* win = (lv_obj_t*)lv_event_get_user_data(e);
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);
    lv_obj_set_pos(win, lv_obj_get_x(win) + vect.x, lv_obj_get_y(win) + vect.y);
    lv_obj_move_foreground(win);
}

static void win_close_cb(lv_event_t* e) {
    int id = (int)(uintptr_t)lv_event_get_user_data(e);
    if (g_windows[id].win) { lv_obj_del(g_windows[id].win); g_windows[id].win = NULL; }
    if (g_windows[id].task_btn) { lv_obj_del(g_windows[id].task_btn); g_windows[id].task_btn = NULL; }
    g_windows[id].open = false;
}

static void win_min_cb(lv_event_t* e) {
    int id = (int)(uintptr_t)lv_event_get_user_data(e);
    if (g_windows[id].win) {
        lv_obj_add_flag(g_windows[id].win, LV_OBJ_FLAG_HIDDEN);
        g_windows[id].minimized = true;
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
        w->maximized = true;
    } else {
        lv_obj_set_pos(w->win, w->saved_x, w->saved_y);
        lv_obj_set_size(w->win, w->saved_w, w->saved_h);
        w->maximized = false;
    }
}

static void taskbar_btn_cb(lv_event_t* e) {
    int id = (int)(uintptr_t)lv_event_get_user_data(e);
    DIMA_WINDOW* w = &g_windows[id];
    if (!w->win) return;
    if (w->minimized) {
        lv_obj_clear_flag(w->win, LV_OBJ_FLAG_HIDDEN);
        w->minimized = false;
    }
    lv_obj_move_foreground(w->win);
}

static lv_obj_t* create_window_frame(DIMA_APP_ID id, const char* title, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    DIMA_WINDOW* dw = &g_windows[id];
    if (dw->open && dw->win) {
        lv_obj_clear_flag(dw->win, LV_OBJ_FLAG_HIDDEN);
        dw->minimized = false;
        lv_obj_move_foreground(dw->win);
        return NULL;
    }

    lv_obj_t* win = lv_obj_create(g_desktop);
    lv_obj_set_pos(win, x, y);
    lv_obj_set_size(win, w, h);
    lv_obj_set_style_pad_all(win, 0, 0);
    lv_obj_set_style_radius(win, 10, 0);
    lv_obj_set_style_bg_color(win, lv_color_hex(COL_SURFACE), 0);
    lv_obj_set_style_border_width(win, 1, 0);
    lv_obj_set_style_border_color(win, lv_color_hex(COL_BORDER), 0);
    lv_obj_clear_flag(win, LV_OBJ_FLAG_SCROLLABLE);

    /* Fluent Title Bar */
    lv_obj_t* titlebar = lv_obj_create(win);
    lv_obj_set_size(titlebar, lv_pct(100), 34);
    lv_obj_set_pos(titlebar, 0, 0);
    lv_obj_set_style_bg_color(titlebar, lv_color_hex(COL_SURFACE_2), 0);
    lv_obj_set_style_radius(titlebar, 0, 0);
    lv_obj_set_style_border_width(titlebar, 0, 0);
    lv_obj_set_style_pad_all(titlebar, 6, 0);
    lv_obj_clear_flag(titlebar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(titlebar, win_drag_cb, LV_EVENT_PRESSING, win);

    lv_obj_t* lbl = lv_label_create(titlebar);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TEXT), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 6, 0);

    /* Close (X) */
    lv_obj_t* btn_close = lv_btn_create(titlebar);
    lv_obj_set_size(btn_close, 26, 22);
    lv_obj_align(btn_close, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(COL_DANGER), 0);
    lv_obj_set_style_radius(btn_close, 6, 0);
    lv_obj_add_event_cb(btn_close, win_close_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)id);
    lv_obj_t* lx = lv_label_create(btn_close);
    lv_label_set_text(lx, "X");
    lv_obj_set_style_text_color(lx, lv_color_hex(COL_TEXT), 0);
    lv_obj_center(lx);

    /* Maximize */
    lv_obj_t* btn_max = lv_btn_create(titlebar);
    lv_obj_set_size(btn_max, 26, 22);
    lv_obj_align(btn_max, LV_ALIGN_RIGHT_MID, -32, 0);
    lv_obj_set_style_bg_color(btn_max, lv_color_hex(COL_SURFACE_3), 0);
    lv_obj_set_style_radius(btn_max, 6, 0);
    lv_obj_add_event_cb(btn_max, win_max_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)id);
    lv_obj_t* lmax = lv_label_create(btn_max);
    lv_label_set_text(lmax, "[]");
    lv_obj_set_style_text_color(lmax, lv_color_hex(COL_TEXT), 0);
    lv_obj_center(lmax);

    /* Minimize (_) */
    lv_obj_t* btn_min = lv_btn_create(titlebar);
    lv_obj_set_size(btn_min, 26, 22);
    lv_obj_align(btn_min, LV_ALIGN_RIGHT_MID, -62, 0);
    lv_obj_set_style_bg_color(btn_min, lv_color_hex(COL_SURFACE_3), 0);
    lv_obj_set_style_radius(btn_min, 6, 0);
    lv_obj_add_event_cb(btn_min, win_min_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)id);
    lv_obj_t* lmin = lv_label_create(btn_min);
    lv_label_set_text(lmin, "_");
    lv_obj_set_style_text_color(lmin, lv_color_hex(COL_TEXT), 0);
    lv_obj_center(lmin);

    /* Content area */
    lv_obj_t* content = lv_obj_create(win);
    lv_obj_set_pos(content, 0, 34);
    lv_obj_set_size(content, lv_pct(100), h - 36);
    lv_obj_set_style_bg_color(content, lv_color_hex(COL_SURFACE), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 0, 0);
    lv_obj_set_style_pad_all(content, 10, 0);

    /* Taskbar button */
    lv_obj_t* tbtn = lv_btn_create(g_taskbar_apps);
    lv_obj_set_height(tbtn, 32);
    lv_obj_set_style_bg_color(tbtn, lv_color_hex(COL_SURFACE_3), 0);
    lv_obj_set_style_radius(tbtn, 6, 0);
    lv_obj_add_event_cb(tbtn, taskbar_btn_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)id);
    lv_obj_t* tlbl = lv_label_create(tbtn);
    lv_label_set_text(tlbl, title);
    lv_obj_set_style_text_color(tlbl, lv_color_hex(COL_TEXT), 0);
    lv_obj_center(tlbl);

    dw->open = true;
    dw->minimized = false;
    dw->maximized = false;
    dw->win = win;
    dw->task_btn = tbtn;
    return content;
}

static void notepad_save_cb(lv_event_t* e) {
    UNUSED(e);
    if (!g_notepad_ta) return;
    const char* txt = lv_textarea_get_text(g_notepad_ta);
    HANDLE h = FsOpen("C:\\NOTES.TXT", true);
    if (h != INVALID_HANDLE_VALUE) {
        size_t written = 0;
        FsWrite(h, txt, strlen(txt), &written);
        FsClose(h);
    }
}

static void cmd_exec_cb(lv_event_t* e) {
    UNUSED(e);
    if (!g_cmd_input || !g_cmd_output) return;
    const char* cmd = lv_textarea_get_text(g_cmd_input);
    if (!cmd || cmd[0] == '\0') return;

    char buf[512];
    if (strcmp(cmd, "help") == 0) {
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nCommands: dir, ver, mem, cls, help\n", cmd);
    } else if (strcmp(cmd, "ver") == 0) {
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nDimaOS (Дима ОС / Димос) v1.0.0 [x86_64]\n", cmd);
    } else if (strcmp(cmd, "mem") == 0) {
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nTotal RAM: %lu MB | Free RAM: %lu MB\n",
                 cmd, (unsigned long)(MmGetTotalMemory() / (1024 * 1024)),
                 (unsigned long)(MmGetFreeMemory() / (1024 * 1024)));
    } else if (strcmp(cmd, "dir") == 0) {
        char list[384];
        FsListDir("C:\\", list, sizeof(list));
        snprintf(buf, sizeof(buf), "\nC:\\>%s\n%s", cmd, list);
    } else if (strcmp(cmd, "cls") == 0) {
        lv_label_set_text(g_cmd_output, "DimaOS Terminal [Version 1.0]\nType 'help' for commands.");
        lv_textarea_set_text(g_cmd_input, "");
        return;
    } else {
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nUnknown command: %s\n", cmd, cmd);
    }

    const char* old = lv_label_get_text(g_cmd_output);
    char* combined = (char*)ExAllocatePool(strlen(old) + strlen(buf) + 1);
    if (combined) {
        strcpy(combined, old);
        strcat(combined, buf);
        lv_label_set_text(g_cmd_output, combined);
        ExFreePool(combined);
    }
    lv_textarea_set_text(g_cmd_input, "");
}

static void calc_btn_cb(lv_event_t* e) {
    const char* txt = (const char*)lv_event_get_user_data(e);
    char ch = txt[0];
    if (ch >= '0' && ch <= '9') {
        if (g_calc_new) { g_calc_cur = 0; g_calc_new = false; }
        g_calc_cur = g_calc_cur * 10 + (ch - '0');
        lv_label_set_text_fmt(g_calc_display, "%ld", (long)g_calc_cur);
    } else if (ch == 'C') {
        g_calc_acc = 0; g_calc_cur = 0; g_calc_op = 0; g_calc_new = true;
        lv_label_set_text(g_calc_display, "0");
    } else if (ch == '=') {
        if (g_calc_op == '+') g_calc_cur = g_calc_acc + g_calc_cur;
        else if (g_calc_op == '-') g_calc_cur = g_calc_acc - g_calc_cur;
        else if (g_calc_op == '*') g_calc_cur = g_calc_acc * g_calc_cur;
        else if (g_calc_op == '/' && g_calc_cur != 0) g_calc_cur = g_calc_acc / g_calc_cur;
        g_calc_op = 0; g_calc_new = true;
        lv_label_set_text_fmt(g_calc_display, "%ld", (long)g_calc_cur);
    } else {
        g_calc_acc = g_calc_cur;
        g_calc_op = ch;
        g_calc_new = true;
    }
}

void GuiOpenApp(DIMA_APP_ID app_id) {
    if (g_start_menu) lv_obj_add_flag(g_start_menu, LV_OBJ_FLAG_HIDDEN);

    if (app_id == APP_MY_COMPUTER) {
        lv_obj_t* c = create_window_frame(APP_MY_COMPUTER, "Проводник (C:\\)", 80, 60, 440, 310);
        if (!c) return;
        char list[512];
        FsListDir("C:\\", list, sizeof(list));
        lv_obj_t* lbl = lv_label_create(c);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TEXT), 0);
        lv_label_set_text_fmt(lbl, "Локальный диск (C:\\)\n--------------------------\n%s", list);
    } else if (app_id == APP_NOTEPAD) {
        lv_obj_t* c = create_window_frame(APP_NOTEPAD, "Блокнот - C:\\NOTES.TXT", 140, 90, 460, 330);
        if (!c) return;
        lv_obj_t* btn_save = lv_btn_create(c);
        lv_obj_set_size(btn_save, 100, 28);
        lv_obj_set_style_bg_color(btn_save, lv_color_hex(COL_ACCENT), 0);
        lv_obj_set_style_radius(btn_save, 6, 0);
        lv_obj_add_event_cb(btn_save, notepad_save_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* slbl = lv_label_create(btn_save);
        lv_label_set_text(slbl, "Сохранить");
        lv_obj_center(slbl);

        g_notepad_ta = lv_textarea_create(c);
        lv_obj_set_pos(g_notepad_ta, 0, 36);
        lv_obj_set_size(g_notepad_ta, lv_pct(100), 235);
        lv_obj_set_style_bg_color(g_notepad_ta, lv_color_hex(COL_SURFACE_2), 0);
        lv_obj_set_style_text_color(g_notepad_ta, lv_color_hex(COL_TEXT), 0);
        char text[512];
        memset(text, 0, sizeof(text));
        HANDLE h = FsOpen("C:\\NOTES.TXT", false);
        if (h != INVALID_HANDLE_VALUE) {
            size_t r = 0;
            FsRead(h, text, sizeof(text) - 1, &r);
            FsClose(h);
        }
        lv_textarea_set_text(g_notepad_ta, text);
        if (g_kbd_group) lv_group_add_obj(g_kbd_group, g_notepad_ta);
    } else if (app_id == APP_CMD) {
        lv_obj_t* c = create_window_frame(APP_CMD, "Терминал", 200, 110, 490, 330);
        if (!c) return;
        lv_obj_set_style_bg_color(c, lv_color_hex(0x0D1117), 0);

        g_cmd_output = lv_label_create(c);
        lv_obj_set_width(g_cmd_output, lv_pct(100));
        lv_obj_set_style_text_color(g_cmd_output, lv_color_hex(0x3DDC84), 0);
        lv_label_set_text(g_cmd_output, "DimaOS Terminal [Version 1.0]\nType 'help' for commands.");

        g_cmd_input = lv_textarea_create(c);
        lv_textarea_set_one_line(g_cmd_input, true);
        lv_obj_set_size(g_cmd_input, 370, 34);
        lv_obj_align(g_cmd_input, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_set_style_bg_color(g_cmd_input, lv_color_hex(COL_SURFACE_2), 0);
        lv_obj_set_style_text_color(g_cmd_input, lv_color_hex(COL_TEXT), 0);
        lv_textarea_set_placeholder_text(g_cmd_input, "Enter command...");
        if (g_kbd_group) lv_group_add_obj(g_kbd_group, g_cmd_input);

        lv_obj_t* run_btn = lv_btn_create(c);
        lv_obj_set_size(run_btn, 85, 34);
        lv_obj_align(run_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
        lv_obj_set_style_bg_color(run_btn, lv_color_hex(COL_ACCENT), 0);
        lv_obj_add_event_cb(run_btn, cmd_exec_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* rlbl = lv_label_create(run_btn);
        lv_label_set_text(rlbl, "Run");
        lv_obj_center(rlbl);
    } else if (app_id == APP_CALCULATOR) {
        lv_obj_t* c = create_window_frame(APP_CALCULATOR, "Калькулятор", 320, 100, 260, 330);
        if (!c) return;
        g_calc_display = lv_label_create(c);
        lv_obj_set_width(g_calc_display, lv_pct(100));
        lv_obj_set_style_text_color(g_calc_display, lv_color_hex(COL_TEXT), 0);
        lv_obj_set_style_text_align(g_calc_display, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(g_calc_display, "0");

        static const char* keys[16] = {
            "7","8","9","/",
            "4","5","6","*",
            "1","2","3","-",
            "C","0","=","+"
        };
        for (int i = 0; i < 16; i++) {
            int row = i / 4;
            int col = i % 4;
            lv_obj_t* b = lv_btn_create(c);
            lv_obj_set_pos(b, col * 58, 36 + row * 56);
            lv_obj_set_size(b, 52, 48);
            lv_obj_set_style_bg_color(b, lv_color_hex((keys[i][0] == '=') ? COL_ACCENT : COL_SURFACE_3), 0);
            lv_obj_set_style_radius(b, 8, 0);
            lv_obj_add_event_cb(b, calc_btn_cb, LV_EVENT_CLICKED, (void*)keys[i]);
            lv_obj_t* l = lv_label_create(b);
            lv_label_set_text(l, keys[i]);
            lv_obj_set_style_text_color(l, lv_color_hex(COL_TEXT), 0);
            lv_obj_center(l);
        }
    } else if (app_id == APP_ABOUT) {
        lv_obj_t* c = create_window_frame(APP_ABOUT, "О DimaOS", 260, 150, 390, 240);
        if (!c) return;
        lv_obj_t* lbl = lv_label_create(c);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TEXT), 0);
        lv_label_set_text_fmt(lbl,
            "DimaOS (Дима ОС / Димос)\n"
            "Version 1.0.0 (x86_64 Long Mode)\n"
            "Architecture: NT-style Hybrid Kernel\n\n"
            "Total RAM: %lu MB\n"
            "Free RAM: %lu MB\n"
            "Processes: %u | Threads: %u\n",
            (unsigned long)(MmGetTotalMemory() / (1024 * 1024)),
            (unsigned long)(MmGetFreeMemory() / (1024 * 1024)),
            PsGetProcessCount(),
            PsGetThreadCount());
    }
}

static void desktop_icon_cb(lv_event_t* e) {
    DIMA_APP_ID id = (DIMA_APP_ID)(uintptr_t)lv_event_get_user_data(e);
    GuiOpenApp(id);
}

static void start_btn_cb(lv_event_t* e) {
    UNUSED(e);
    if (lv_obj_has_flag(g_start_menu, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(g_start_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(g_start_menu);
    } else {
        lv_obj_add_flag(g_start_menu, LV_OBJ_FLAG_HIDDEN);
    }
}

static void reboot_cb(lv_event_t* e) { UNUSED(e); HalReboot(); }
static void shutdown_cb(lv_event_t* e) { UNUSED(e); HalShutdown(); }

static void create_desktop_icon(const char* text, lv_coord_t x, lv_coord_t y, DIMA_APP_ID id) {
    lv_obj_t* btn = lv_btn_create(g_desktop);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, 102, 72);
    lv_obj_set_style_bg_opa(btn, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COL_SURFACE_2), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_add_event_cb(btn, desktop_icon_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)id);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);
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
    lv_obj_set_style_bg_color(cursor, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_border_color(cursor, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_border_width(cursor, 2, 0);
    lv_obj_set_style_radius(cursor, 6, 0);
    lv_indev_set_cursor(mouse_indev, cursor);

    g_kbd_group = lv_group_create();
    lv_indev_drv_init(&g_kbd_drv);
    g_kbd_drv.type    = LV_INDEV_TYPE_KEYPAD;
    g_kbd_drv.read_cb = kbd_read_cb;
    lv_indev_t* kbd_indev = lv_indev_drv_register(&g_kbd_drv);
    lv_indev_set_group(kbd_indev, g_kbd_group);

    /* Desktop */
    g_desktop = lv_scr_act();
    lv_obj_set_style_bg_color(g_desktop, lv_color_hex(COL_BG_DESKTOP), 0);
    lv_obj_clear_flag(g_desktop, LV_OBJ_FLAG_SCROLLABLE);

    create_desktop_icon("Проводник",   20, 20,  APP_MY_COMPUTER);
    create_desktop_icon("Блокнот",     20, 105, APP_NOTEPAD);
    create_desktop_icon("Терминал",    20, 190, APP_CMD);
    create_desktop_icon("Калькулятор", 20, 275, APP_CALCULATOR);
    create_desktop_icon("О DimaOS",    20, 360, APP_ABOUT);

    /* Taskbar */
    g_taskbar = lv_obj_create(g_desktop);
    lv_obj_set_pos(g_taskbar, 0, (lv_coord_t)(fb->height - TASKBAR_HEIGHT));
    lv_obj_set_size(g_taskbar, (lv_coord_t)fb->width, TASKBAR_HEIGHT);
    lv_obj_set_style_bg_color(g_taskbar, lv_color_hex(0x1A1C25), 0);
    lv_obj_set_style_border_width(g_taskbar, 1, 0);
    lv_obj_set_style_border_color(g_taskbar, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_radius(g_taskbar, 0, 0);
    lv_obj_set_style_pad_all(g_taskbar, 5, 0);
    lv_obj_clear_flag(g_taskbar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* start_btn = lv_btn_create(g_taskbar);
    lv_obj_set_size(start_btn, 84, 32);
    lv_obj_align(start_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(start_btn, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_radius(start_btn, 8, 0);
    lv_obj_add_event_cb(start_btn, start_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* slbl = lv_label_create(start_btn);
    lv_label_set_text(slbl, "Пуск");
    lv_obj_set_style_text_color(slbl, lv_color_hex(COL_TEXT), 0);
    lv_obj_center(slbl);

    g_taskbar_apps = lv_obj_create(g_taskbar);
    lv_obj_set_pos(g_taskbar_apps, 94, 0);
    lv_obj_set_size(g_taskbar_apps, (lv_coord_t)(fb->width - 190), 32);
    lv_obj_set_style_bg_opa(g_taskbar_apps, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_taskbar_apps, 0, 0);
    lv_obj_set_style_pad_all(g_taskbar_apps, 0, 0);
    lv_obj_set_flex_flow(g_taskbar_apps, LV_FLEX_FLOW_ROW);

    g_tray_clock = lv_label_create(g_taskbar);
    lv_obj_align(g_tray_clock, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_color(g_tray_clock, lv_color_hex(COL_TEXT), 0);
    lv_label_set_text(g_tray_clock, "00:00");

    /* Start Menu */
    g_start_menu = lv_obj_create(g_desktop);
    lv_obj_set_pos(g_start_menu, 10, (lv_coord_t)(fb->height - TASKBAR_HEIGHT - 300));
    lv_obj_set_size(g_start_menu, 240, 290);
    lv_obj_set_style_bg_color(g_start_menu, lv_color_hex(COL_SURFACE), 0);
    lv_obj_set_style_border_width(g_start_menu, 1, 0);
    lv_obj_set_style_border_color(g_start_menu, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_radius(g_start_menu, 12, 0);
    lv_obj_set_flex_flow(g_start_menu, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(g_start_menu, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* sm_hdr = lv_label_create(g_start_menu);
    lv_label_set_text(sm_hdr, "DimaOS (Дима ОС)");
    lv_obj_set_style_text_color(sm_hdr, lv_color_hex(COL_ACCENT), 0);

    const char* names[MAX_WINDOWS] = {"Проводник", "Блокнот", "Терминал", "Калькулятор", "О DimaOS"};
    for (int i = 0; i < MAX_WINDOWS; i++) {
        lv_obj_t* b = lv_btn_create(g_start_menu);
        lv_obj_set_size(b, lv_pct(100), 32);
        lv_obj_set_style_bg_color(b, lv_color_hex(COL_SURFACE_2), 0);
        lv_obj_set_style_radius(b, 6, 0);
        lv_obj_add_event_cb(b, desktop_icon_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, names[i]);
        lv_obj_set_style_text_color(l, lv_color_hex(COL_TEXT), 0);
        lv_obj_center(l);
    }

    lv_obj_t* b_reb = lv_btn_create(g_start_menu);
    lv_obj_set_size(b_reb, lv_pct(100), 28);
    lv_obj_set_style_bg_color(b_reb, lv_color_hex(COL_SURFACE_3), 0);
    lv_obj_add_event_cb(b_reb, reboot_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l_reb = lv_label_create(b_reb);
    lv_label_set_text(l_reb, "Перезагрузка");
    lv_obj_center(l_reb);

    lv_obj_t* b_shut = lv_btn_create(g_start_menu);
    lv_obj_set_size(b_shut, lv_pct(100), 28);
    lv_obj_set_style_bg_color(b_shut, lv_color_hex(COL_DANGER), 0);
    lv_obj_add_event_cb(b_shut, shutdown_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* l_shut = lv_label_create(b_shut);
    lv_label_set_text(l_shut, "Выключение");
    lv_obj_center(l_shut);

    HalPrintf("[GUI] DimaOS Fluent Dark Desktop initialized\n");
}

void GuiTask(void) {
    static uint64_t last_sec = 0;
    uint64_t sec = HalGetTickCount() / 1000;
    if (sec != last_sec) {
        last_sec = sec;
        uint32_t mins = (uint32_t)(sec / 60);
        uint32_t secs = (uint32_t)(sec % 60);
        lv_label_set_text_fmt(g_tray_clock, "%02u:%02u", mins, secs);
    }
    lv_timer_handler();
}
