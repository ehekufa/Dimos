#include "../include/gui.h"
#include "../include/hal.h"
#include "../include/mm.h"
#include "../include/fs.h"
#include "../include/drivers.h"
#include "../include/rtl.h"

/* Forward from apps_system.c */
extern void AppSystemOnClosed(DIMA_APP_ID id);

/* ------------------------------------------------------------------ */
/* Tiny direct-buffer drawing helpers for lv_canvas                   */
/* ------------------------------------------------------------------ */

static void cbuf_fill_rect(lv_color_t* buf, int w, int h,
                           int x, int y, int rw, int rh, uint32_t hex) {
    lv_color_t col = lv_color_hex(hex);
    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = (x + rw > w) ? w : (x + rw);
    int y1 = (y + rh > h) ? h : (y + rh);
    for (int py = y0; py < y1; py++) {
        lv_color_t* row = &buf[py * w];
        for (int px = x0; px < x1; px++) {
            row[px] = col;
        }
    }
}

static void cbuf_fill_circle(lv_color_t* buf, int w, int h,
                             int cx, int cy, int r, uint32_t hex) {
    lv_color_t col = lv_color_hex(hex);
    int r2 = r * r;
    for (int dy = -r; dy <= r; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= h) continue;
        for (int dx = -r; dx <= r; dx++) {
            int px = cx + dx;
            if (px < 0 || px >= w) continue;
            if (dx * dx + dy * dy <= r2) {
                buf[py * w + px] = col;
            }
        }
    }
}

static void cbuf_fill_ellipse(lv_color_t* buf, int w, int h,
                              int cx, int cy, int rx, int ry, uint32_t hex) {
    lv_color_t col = lv_color_hex(hex);
    int64_t rx2 = (int64_t)rx * rx;
    int64_t ry2 = (int64_t)ry * ry;
    int64_t rhs = rx2 * ry2;
    for (int dy = -ry; dy <= ry; dy++) {
        int py = cy + dy;
        if (py < 0 || py >= h) continue;
        for (int dx = -rx; dx <= rx; dx++) {
            int px = cx + dx;
            if (px < 0 || px >= w) continue;
            if ((int64_t)dx * dx * ry2 + (int64_t)dy * dy * rx2 <= rhs) {
                buf[py * w + px] = col;
            }
        }
    }
}

/* ================================================================== */
/* 1. PAINT                                                           */
/* ================================================================== */

#define PAINT_W 390
#define PAINT_H 248

static const uint32_t g_paint_palette[16] = {
    0x000000, 0x4B5563, 0x9CA3AF, 0xFFFFFF,
    0xEF4444, 0xEC4899, 0xF97316, 0xEAB308,
    0x22C55E, 0x14B8A6, 0x38BDF8, 0x2563EB,
    0x4F46E5, 0x9333EA, 0x92400E, 0xD946EF
};

static lv_obj_t*   g_paint_canvas = NULL;
static lv_color_t* g_paint_buf = NULL;
static lv_obj_t*   g_paint_status = NULL;
static uint32_t    g_paint_color = 0x000000;
static uint8_t     g_paint_brush = 4;
static bool        g_paint_drawing = false;
static int         g_paint_lx = -1;
static int         g_paint_ly = -1;

static void paint_clear_buf(void) {
    if (!g_paint_buf) return;
    cbuf_fill_rect(g_paint_buf, PAINT_W, PAINT_H, 0, 0, PAINT_W, PAINT_H, 0xF8FAFC);
    if (g_paint_canvas) lv_obj_invalidate(g_paint_canvas);
}

static void paint_plot(int x, int y) {
    if (!g_paint_buf) return;
    cbuf_fill_rect(g_paint_buf, PAINT_W, PAINT_H,
                   x - g_paint_brush / 2, y - g_paint_brush / 2,
                   g_paint_brush, g_paint_brush, g_paint_color);
}

static void paint_stroke(int x, int y) {
    if (g_paint_lx < 0) {
        paint_plot(x, y);
    } else {
        int dx = x - g_paint_lx;
        int dy = y - g_paint_ly;
        int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
        if (steps == 0) {
            paint_plot(x, y);
        } else {
            for (int s = 0; s <= steps; s++) {
                int px = g_paint_lx + (dx * s) / steps;
                int py = g_paint_ly + (dy * s) / steps;
                paint_plot(px, py);
            }
        }
    }
    g_paint_lx = x;
    g_paint_ly = y;
    if (g_paint_canvas) lv_obj_invalidate(g_paint_canvas);
}

static void paint_canvas_event_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        g_paint_drawing = false;
        g_paint_lx = -1;
        g_paint_ly = -1;
        return;
    }

    lv_indev_t* indev = lv_indev_get_act();
    if (!indev || !g_paint_canvas) return;
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    lv_area_t coords;
    lv_obj_get_coords(g_paint_canvas, &coords);
    int cx = (int)(pt.x - coords.x1);
    int cy = (int)(pt.y - coords.y1);
    if (cx < 0 || cy < 0 || cx >= PAINT_W || cy >= PAINT_H) return;

    if (code == LV_EVENT_PRESSED) {
        g_paint_drawing = true;
        g_paint_lx = -1;
        paint_stroke(cx, cy);
    } else if (code == LV_EVENT_PRESSING && g_paint_drawing) {
        paint_stroke(cx, cy);
    }
}

static void paint_color_cb(lv_event_t* e) {
    uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    g_paint_color = g_paint_palette[idx & 15U];
    if (g_paint_status) lv_label_set_text(g_paint_status, "Color selected");
    DrvSoundBeep();
}

static void paint_brush_cb(lv_event_t* e) {
    g_paint_brush = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (g_paint_status) lv_label_set_text_fmt(g_paint_status, "Brush: %upx", g_paint_brush);
}

static void paint_clear_btn_cb(lv_event_t* e) {
    UNUSED(e);
    paint_clear_buf();
    if (g_paint_status) lv_label_set_text(g_paint_status, "Canvas cleared");
}

static void paint_save_btn_cb(lv_event_t* e) {
    UNUSED(e);
    if (!g_paint_buf) return;
    size_t sz = (size_t)(PAINT_W * PAINT_H * sizeof(lv_color_t));
    HANDLE h = FsOpen("C:\\PAINT.RAW", true);
    if (h != INVALID_HANDLE_VALUE) {
        size_t w = 0;
        FsWrite(h, g_paint_buf, sz, &w);
        FsClose(h);
    }
    if (g_paint_status) lv_label_set_text(g_paint_status, "Saved to C:\\PAINT.RAW");
    DrvSoundBeep();
    AppRefreshFiles();
}

void AppOpenPaint(void) {
    lv_obj_t* c = GuiCreateWindowFrame(APP_PAINT, "Paint", 130, 56, 610, 342);
    if (!c) return;
    const DIMA_PALETTE* pal = GuiGetPalette();

    g_paint_buf = (lv_color_t*)ExAllocatePool((size_t)PAINT_W * PAINT_H * sizeof(lv_color_t));
    if (!g_paint_buf) return;

    g_paint_color = 0x000000;
    g_paint_brush = 4;
    g_paint_drawing = false;
    g_paint_lx = -1;
    g_paint_ly = -1;

    g_paint_canvas = lv_canvas_create(c);
    lv_canvas_set_buffer(g_paint_canvas, g_paint_buf, PAINT_W, PAINT_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(g_paint_canvas, 0, 0);
    lv_obj_add_flag(g_paint_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_paint_canvas, paint_canvas_event_cb, LV_EVENT_ALL, NULL);
    paint_clear_buf();

    g_paint_status = lv_label_create(c);
    lv_obj_set_pos(g_paint_status, 0, PAINT_H + 10);
    lv_obj_set_style_text_color(g_paint_status, lv_color_hex(pal->text_dim), 0);
    lv_label_set_text(g_paint_status, "Draw on canvas, then Save to C:\\PAINT.RAW");

    lv_coord_t px = PAINT_W + 14;
    for (uint32_t i = 0; i < 16; i++) {
        int col = (int)(i % 4);
        int row = (int)(i / 4);
        lv_obj_t* sw = lv_btn_create(c);
        lv_obj_set_pos(sw, px + col * 42, row * 40);
        lv_obj_set_size(sw, 34, 34);
        lv_obj_set_style_bg_color(sw, lv_color_hex(g_paint_palette[i]), 0);
        lv_obj_set_style_radius(sw, 17, 0);
        lv_obj_set_style_border_width(sw, 2, 0);
        lv_obj_set_style_border_color(sw, lv_color_hex(pal->border), 0);
        lv_obj_add_event_cb(sw, paint_color_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    }

    static const uint8_t b_sizes[3] = {1, 2, 4};
    static const char*   b_names[3] = {"1px", "2px", "4px"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t* b = lv_btn_create(c);
        lv_obj_set_pos(b, px + i * 56, 174);
        lv_obj_set_size(b, 50, 32);
        lv_obj_set_style_bg_color(b, lv_color_hex(pal->surface_3), 0);
        lv_obj_add_event_cb(b, paint_brush_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)b_sizes[i]);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, b_names[i]);
        lv_obj_center(l);
    }

    lv_obj_t* b_clr = lv_btn_create(c);
    lv_obj_set_pos(b_clr, px, 218);
    lv_obj_set_size(b_clr, 78, 34);
    lv_obj_set_style_bg_color(b_clr, lv_color_hex(pal->surface_2), 0);
    lv_obj_add_event_cb(b_clr, paint_clear_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lc = lv_label_create(b_clr);
    lv_label_set_text(lc, "Clear");
    lv_obj_center(lc);

    lv_obj_t* b_sav = lv_btn_create(c);
    lv_obj_set_pos(b_sav, px + 84, 218);
    lv_obj_set_size(b_sav, 78, 34);
    lv_obj_set_style_bg_color(b_sav, lv_color_hex(pal->accent), 0);
    lv_obj_add_event_cb(b_sav, paint_save_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ls = lv_label_create(b_sav);
    lv_label_set_text(ls, "Save");
    lv_obj_center(ls);
}

/* ================================================================== */
/* 2. SNAKE (42x25 grid, 8px cells, D-pad + Arrow/WASD + Tap)         */
/* ================================================================== */

#define SNAKE_CELL   8
#define SNAKE_COLS   42
#define SNAKE_ROWS   25
#define SNAKE_W      (SNAKE_COLS * SNAKE_CELL)
#define SNAKE_H      (SNAKE_ROWS * SNAKE_CELL)
#define SNAKE_MAX    300U
#define SNAKE_STEP_MS 140ULL

enum { SN_UP = 0, SN_RIGHT = 1, SN_DOWN = 2, SN_LEFT = 3 };

static lv_obj_t*   g_snake_canvas = NULL;
static lv_color_t* g_snake_buf = NULL;
static lv_obj_t*   g_snake_score_lbl = NULL;
static lv_obj_t*   g_snake_status_lbl = NULL;

static uint16_t g_snake_cells[SNAKE_MAX];
static uint16_t g_snake_len = 5;
static uint16_t g_snake_score = 0;
static uint16_t g_snake_food = 0;
static uint8_t  g_snake_dir = SN_RIGHT;
static uint8_t  g_snake_want = SN_RIGHT;
static bool     g_snake_alive = false;
static uint64_t g_snake_last_step = 0;
static uint32_t g_snake_rng = 0xD14E05U;

static uint32_t snake_rand(void) {
    g_snake_rng ^= g_snake_rng << 13;
    g_snake_rng ^= g_snake_rng >> 17;
    g_snake_rng ^= g_snake_rng << 5;
    if (g_snake_rng == 0) g_snake_rng = 0xD14E05U;
    return g_snake_rng;
}

static bool snake_has_cell(uint16_t cell) {
    for (uint16_t i = 0; i < g_snake_len; i++) {
        if (g_snake_cells[i] == cell) return true;
    }
    return false;
}

static void snake_place_food(void) {
    for (int i = 0; i < 400; i++) {
        uint16_t cand = (uint16_t)(snake_rand() % (uint32_t)(SNAKE_COLS * SNAKE_ROWS));
        if (!snake_has_cell(cand)) {
            g_snake_food = cand;
            return;
        }
    }
    g_snake_food = 0;
}

static void snake_render(void) {
    if (!g_snake_buf || !g_snake_canvas) return;
    cbuf_fill_rect(g_snake_buf, SNAKE_W, SNAKE_H, 0, 0, SNAKE_W, SNAKE_H, 0x0B2E1A);
    for (int x = 0; x < SNAKE_W; x += SNAKE_CELL * 3) {
        cbuf_fill_rect(g_snake_buf, SNAKE_W, SNAKE_H, x, 0, 1, SNAKE_H, 0x134427);
    }
    for (int y = 0; y < SNAKE_H; y += SNAKE_CELL * 3) {
        cbuf_fill_rect(g_snake_buf, SNAKE_W, SNAKE_H, 0, y, SNAKE_W, 1, 0x134427);
    }

    int fx = (int)(g_snake_food % SNAKE_COLS) * SNAKE_CELL + 4;
    int fy = (int)(g_snake_food / SNAKE_COLS) * SNAKE_CELL + 4;
    cbuf_fill_circle(g_snake_buf, SNAKE_W, SNAKE_H, fx, fy, 3, 0xFF5252);

    for (uint16_t i = 0; i < g_snake_len; i++) {
        int sx = (int)(g_snake_cells[i] % SNAKE_COLS) * SNAKE_CELL + 1;
        int sy = (int)(g_snake_cells[i] / SNAKE_COLS) * SNAKE_CELL + 1;
        uint32_t col = (i == 0) ? 0xFFFFFF : 0x38BDF8;
        cbuf_fill_rect(g_snake_buf, SNAKE_W, SNAKE_H, sx, sy, SNAKE_CELL - 2, SNAKE_CELL - 2, col);
    }
    lv_obj_invalidate(g_snake_canvas);

    if (g_snake_score_lbl) {
        lv_label_set_text_fmt(g_snake_score_lbl, "Score: %u  |  Len: %u",
                              g_snake_score, g_snake_len);
    }
    if (g_snake_status_lbl) {
        lv_label_set_text(g_snake_status_lbl,
                          g_snake_alive ? "Use Arrows, D-pad, or click field" : "GAME OVER! Click New Game");
    }
}

static void snake_start(void) {
    g_snake_rng = (uint32_t)HalGetTickCount() ^ 0x9E3779B9U;
    g_snake_len = 5;
    g_snake_score = 0;
    g_snake_dir = SN_RIGHT;
    g_snake_want = SN_RIGHT;
    g_snake_alive = true;
    g_snake_last_step = HalGetTickCount();
    uint16_t head = (uint16_t)((SNAKE_ROWS / 2) * SNAKE_COLS + (SNAKE_COLS / 2));
    for (uint16_t i = 0; i < g_snake_len; i++) {
        g_snake_cells[i] = (uint16_t)(head - i);
    }
    snake_place_food();
    snake_render();
}

static void snake_step(void) {
    uint16_t head = g_snake_cells[0];
    uint16_t r = (uint16_t)(head / SNAKE_COLS);
    uint16_t c = (uint16_t)(head % SNAKE_COLS);
    g_snake_dir = g_snake_want;

    if (g_snake_dir == SN_UP && r > 0) r--;
    else if (g_snake_dir == SN_DOWN && r + 1 < SNAKE_ROWS) r++;
    else if (g_snake_dir == SN_LEFT && c > 0) c--;
    else if (g_snake_dir == SN_RIGHT && c + 1 < SNAKE_COLS) c++;
    else {
        g_snake_alive = false;
        DrvSoundAlert();
        snake_render();
        return;
    }

    uint16_t next = (uint16_t)(r * SNAKE_COLS + c);
    bool grew = false;
    if (next == g_snake_food) {
        grew = true;
        g_snake_score += 10;
        DrvSoundBeep();
    } else if (snake_has_cell(next)) {
        g_snake_alive = false;
        DrvSoundAlert();
        snake_render();
        return;
    }

    if (grew && g_snake_len < SNAKE_MAX) {
        for (uint16_t i = g_snake_len; i > 0; i--) {
            g_snake_cells[i] = g_snake_cells[i - 1];
        }
        g_snake_len++;
    } else {
        for (uint16_t i = (uint16_t)(g_snake_len - 1); i > 0; i--) {
            g_snake_cells[i] = g_snake_cells[i - 1];
        }
    }
    g_snake_cells[0] = next;
    if (grew) snake_place_food();
    snake_render();
}

static void snake_set_dir(uint8_t d) {
    if (d == SN_UP && g_snake_dir != SN_DOWN) g_snake_want = SN_UP;
    else if (d == SN_DOWN && g_snake_dir != SN_UP) g_snake_want = SN_DOWN;
    else if (d == SN_LEFT && g_snake_dir != SN_RIGHT) g_snake_want = SN_LEFT;
    else if (d == SN_RIGHT && g_snake_dir != SN_LEFT) g_snake_want = SN_RIGHT;
}

static void snake_dpad_cb(lv_event_t* e) {
    uint8_t d = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    snake_set_dir(d);
}

static void snake_new_cb(lv_event_t* e) {
    UNUSED(e);
    snake_start();
}

static void snake_field_click_cb(lv_event_t* e) {
    UNUSED(e);
    if (!g_snake_canvas || !g_snake_alive) return;
    lv_indev_t* indev = lv_indev_get_act();
    if (!indev) return;
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);
    lv_area_t coords;
    lv_obj_get_coords(g_snake_canvas, &coords);
    int cx = (int)(pt.x - coords.x1);
    int cy = (int)(pt.y - coords.y1);
    int hx = (int)(g_snake_cells[0] % SNAKE_COLS) * SNAKE_CELL + 4;
    int hy = (int)(g_snake_cells[0] / SNAKE_COLS) * SNAKE_CELL + 4;
    int dx = cx - hx;
    int dy = cy - hy;
    if (abs(dx) > abs(dy)) {
        snake_set_dir(dx > 0 ? SN_RIGHT : SN_LEFT);
    } else {
        snake_set_dir(dy > 0 ? SN_DOWN : SN_UP);
    }
}

void AppOpenSnake(void) {
    lv_obj_t* c = GuiCreateWindowFrame(APP_SNAKE, "Snake", 170, 70, 560, 310);
    if (!c) return;
    const DIMA_PALETTE* pal = GuiGetPalette();

    g_snake_buf = (lv_color_t*)ExAllocatePool((size_t)SNAKE_W * SNAKE_H * sizeof(lv_color_t));
    if (!g_snake_buf) return;

    g_snake_canvas = lv_canvas_create(c);
    lv_canvas_set_buffer(g_snake_canvas, g_snake_buf, SNAKE_W, SNAKE_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(g_snake_canvas, 0, 0);
    lv_obj_add_flag(g_snake_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_snake_canvas, snake_field_click_cb, LV_EVENT_CLICKED, NULL);

    g_snake_status_lbl = lv_label_create(c);
    lv_obj_set_pos(g_snake_status_lbl, 0, SNAKE_H + 14);
    lv_obj_set_style_text_color(g_snake_status_lbl, lv_color_hex(pal->text_dim), 0);

    lv_coord_t sx = SNAKE_W + 18;
    g_snake_score_lbl = lv_label_create(c);
    lv_obj_set_pos(g_snake_score_lbl, sx, 4);
    lv_obj_set_style_text_font(g_snake_score_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_snake_score_lbl, lv_color_hex(pal->text), 0);

    static const char* d_lbls[4] = {"^", ">", "v", "<"};
    static const lv_coord_t d_x[4] = {56, 112, 56, 0};
    static const lv_coord_t d_y[4] = {44, 94, 94, 94};
    for (int i = 0; i < 4; i++) {
        lv_obj_t* b = lv_btn_create(c);
        lv_obj_set_pos(b, sx + d_x[i], d_y[i]);
        lv_obj_set_size(b, 48, 42);
        lv_obj_set_style_bg_color(b, lv_color_hex(pal->surface_3), 0);
        lv_obj_add_event_cb(b, snake_dpad_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, d_lbls[i]);
        lv_obj_center(l);
    }

    lv_obj_t* b_new = lv_btn_create(c);
    lv_obj_set_pos(b_new, sx + 12, 162);
    lv_obj_set_size(b_new, 140, 38);
    lv_obj_set_style_bg_color(b_new, lv_color_hex(pal->accent), 0);
    lv_obj_add_event_cb(b_new, snake_new_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ln = lv_label_create(b_new);
    lv_label_set_text(ln, "New Game");
    lv_obj_center(ln);

    snake_start();
}

/* ================================================================== */
/* 3. MINESWEEPER (9x9, 10 mines, Flag mode + Right click)            */
/* ================================================================== */

#define MINES_GRID  9
#define MINES_COUNT 10

enum { M_HIDDEN = 0, M_OPENED = 1, M_MARKED = 2 };

static lv_obj_t* g_mines_btns[MINES_GRID * MINES_GRID];
static lv_obj_t* g_mines_lbls[MINES_GRID * MINES_GRID];
static lv_obj_t* g_mines_count_lbl = NULL;
static lv_obj_t* g_mines_status_lbl = NULL;
static lv_obj_t* g_mines_flag_btn = NULL;
static lv_obj_t* g_mines_flag_lbl = NULL;

static uint8_t  g_m_state[MINES_GRID * MINES_GRID];
static bool     g_m_mine[MINES_GRID * MINES_GRID];
static uint8_t  g_m_neigh[MINES_GRID * MINES_GRID];
static uint16_t g_m_opened = 0;
static uint16_t g_m_flags = 0;
static bool     g_m_flag_mode = false;
static bool     g_m_over = false;
static bool     g_m_won = false;
static uint32_t g_m_rng = 0x5EED1234U;
static bool     g_m_prev_right = false;

static uint32_t mines_rand(void) {
    g_m_rng ^= g_m_rng << 13;
    g_m_rng ^= g_m_rng >> 17;
    g_m_rng ^= g_m_rng << 5;
    if (g_m_rng == 0) g_m_rng = 0x5EED1234U;
    return g_m_rng;
}

static uint32_t mines_num_color(uint8_t n) {
    switch (n) {
        case 1: return 0x60A5FA;
        case 2: return 0x3DDC84;
        case 3: return 0xFF6464;
        case 4: return 0x818CF8;
        case 5: return 0xF59E0B;
        case 6: return 0x2DD4BF;
        default: return 0xE2E8F0;
    }
}

static void mines_refresh_ui(void) {
    const DIMA_PALETTE* pal = GuiGetPalette();
    for (int i = 0; i < MINES_GRID * MINES_GRID; i++) {
        if (!g_mines_btns[i] || !g_mines_lbls[i]) continue;
        if (g_m_state[i] == M_HIDDEN) {
            lv_obj_set_style_bg_color(g_mines_btns[i], lv_color_hex(pal->surface_3), 0);
            lv_label_set_text(g_mines_lbls[i], "");
        } else if (g_m_state[i] == M_MARKED) {
            lv_obj_set_style_bg_color(g_mines_btns[i], lv_color_hex(pal->warn), 0);
            lv_label_set_text(g_mines_lbls[i], "F");
            lv_obj_set_style_text_color(g_mines_lbls[i], lv_color_hex(0x111111), 0);
        } else {
            lv_obj_set_style_bg_color(g_mines_btns[i], lv_color_hex(pal->surface_2), 0);
            if (g_m_mine[i]) {
                lv_obj_set_style_bg_color(g_mines_btns[i], lv_color_hex(pal->danger), 0);
                lv_label_set_text(g_mines_lbls[i], "*");
                lv_obj_set_style_text_color(g_mines_lbls[i], lv_color_hex(0xFFFFFF), 0);
            } else if (g_m_neigh[i] > 0) {
                char d[2] = {(char)('0' + g_m_neigh[i]), '\0'};
                lv_label_set_text(g_mines_lbls[i], d);
                lv_obj_set_style_text_color(g_mines_lbls[i], lv_color_hex(mines_num_color(g_m_neigh[i])), 0);
            } else {
                lv_label_set_text(g_mines_lbls[i], "");
            }
        }
    }
    if (g_mines_count_lbl) {
        int left = (int)MINES_COUNT - (int)g_m_flags;
        lv_label_set_text_fmt(g_mines_count_lbl, "Mines left: %d", left);
    }
    if (g_mines_flag_btn && g_mines_flag_lbl) {
        lv_obj_set_style_bg_color(g_mines_flag_btn,
            lv_color_hex(g_m_flag_mode ? pal->warn : pal->surface_3), 0);
        lv_label_set_text_fmt(g_mines_flag_lbl, "Flag Mode: %s", g_m_flag_mode ? "ON" : "OFF");
    }
    if (g_mines_status_lbl) {
        if (g_m_won) lv_label_set_text(g_mines_status_lbl, "CLEARED! You win!");
        else if (g_m_over) lv_label_set_text(g_mines_status_lbl, "BOOM! Tap New Game.");
        else lv_label_set_text(g_mines_status_lbl, "Left click: open | Right click or Flag Mode: mark");
    }
}

static void mines_start(void) {
    g_m_rng = (uint32_t)HalGetTickCount() ^ 0x1234ABCDU;
    g_m_opened = 0;
    g_m_flags = 0;
    g_m_over = false;
    g_m_won = false;
    memset(g_m_state, M_HIDDEN, sizeof(g_m_state));
    memset(g_m_mine, 0, sizeof(g_m_mine));
    memset(g_m_neigh, 0, sizeof(g_m_neigh));

    uint16_t placed = 0;
    while (placed < MINES_COUNT) {
        uint16_t c = (uint16_t)(mines_rand() % (MINES_GRID * MINES_GRID));
        if (!g_m_mine[c]) {
            g_m_mine[c] = true;
            placed++;
        }
    }

    for (int r = 0; r < MINES_GRID; r++) {
        for (int c = 0; c < MINES_GRID; c++) {
            uint8_t cnt = 0;
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    int nr = r + dr, nc = c + dc;
                    if (nr >= 0 && nr < MINES_GRID && nc >= 0 && nc < MINES_GRID) {
                        if (g_m_mine[nr * MINES_GRID + nc]) cnt++;
                    }
                }
            }
            g_m_neigh[r * MINES_GRID + c] = cnt;
        }
    }
    mines_refresh_ui();
}

static void mines_flood_open(uint16_t start) {
    uint16_t stack[MINES_GRID * MINES_GRID];
    uint16_t sp = 0;
    if (g_m_state[start] != M_HIDDEN) return;
    stack[sp++] = start;

    while (sp > 0) {
        uint16_t cell = stack[--sp];
        if (g_m_state[cell] != M_HIDDEN) continue;
        g_m_state[cell] = M_OPENED;
        g_m_opened++;
        if (g_m_neigh[cell] != 0) continue;
        int r = cell / MINES_GRID, c = cell % MINES_GRID;
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                int nr = r + dr, nc = c + dc;
                if (nr >= 0 && nr < MINES_GRID && nc >= 0 && nc < MINES_GRID) {
                    uint16_t nb = (uint16_t)(nr * MINES_GRID + nc);
                    if (g_m_state[nb] == M_HIDDEN) stack[sp++] = nb;
                }
            }
        }
    }
}

static void mines_toggle_flag(uint16_t cell) {
    if (g_m_state[cell] == M_HIDDEN) {
        g_m_state[cell] = M_MARKED;
        g_m_flags++;
        DrvSoundBeep();
    } else if (g_m_state[cell] == M_MARKED) {
        g_m_state[cell] = M_HIDDEN;
        g_m_flags--;
    }
    mines_refresh_ui();
}

static void mines_cell_cb(lv_event_t* e) {
    if (g_m_over || g_m_won) return;
    uint16_t cell = (uint16_t)(uintptr_t)lv_event_get_user_data(e);

    if (g_m_flag_mode) {
        mines_toggle_flag(cell);
        return;
    }
    if (g_m_state[cell] == M_MARKED) {
        mines_toggle_flag(cell);
        return;
    }
    if (g_m_mine[cell]) {
        g_m_over = true;
        DrvSoundAlert();
        for (int i = 0; i < MINES_GRID * MINES_GRID; i++) {
            if (g_m_mine[i]) g_m_state[i] = M_OPENED;
        }
        mines_refresh_ui();
        return;
    }
    mines_flood_open(cell);
    if (g_m_opened == (MINES_GRID * MINES_GRID - MINES_COUNT)) {
        g_m_won = true;
        DrvSoundPlayStartup();
    }
    mines_refresh_ui();
}

static void mines_flag_mode_cb(lv_event_t* e) {
    UNUSED(e);
    g_m_flag_mode = !g_m_flag_mode;
    mines_refresh_ui();
}

static void mines_new_cb(lv_event_t* e) {
    UNUSED(e);
    mines_start();
}

void AppOpenMines(void) {
    lv_obj_t* c = GuiCreateWindowFrame(APP_MINES, "Minesweeper", 200, 62, 480, 344);
    if (!c) return;
    const DIMA_PALETTE* pal = GuiGetPalette();

    g_m_flag_mode = false;
    g_m_prev_right = false;

    for (int r = 0; r < MINES_GRID; r++) {
        for (int col = 0; col < MINES_GRID; col++) {
            int idx = r * MINES_GRID + col;
            lv_obj_t* b = lv_btn_create(c);
            lv_obj_set_pos(b, col * 30, r * 30);
            lv_obj_set_size(b, 27, 27);
            lv_obj_set_style_radius(b, 5, 0);
            lv_obj_set_style_pad_all(b, 0, 0);
            lv_obj_add_event_cb(b, mines_cell_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)idx);
            lv_obj_t* l = lv_label_create(b);
            lv_obj_center(l);
            g_mines_btns[idx] = b;
            g_mines_lbls[idx] = l;
        }
    }

    lv_coord_t sx = 286;
    g_mines_count_lbl = lv_label_create(c);
    lv_obj_set_pos(g_mines_count_lbl, sx, 6);
    lv_obj_set_style_text_font(g_mines_count_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_mines_count_lbl, lv_color_hex(pal->text), 0);

    g_mines_flag_btn = lv_btn_create(c);
    lv_obj_set_pos(g_mines_flag_btn, sx, 54);
    lv_obj_set_size(g_mines_flag_btn, 166, 38);
    lv_obj_add_event_cb(g_mines_flag_btn, mines_flag_mode_cb, LV_EVENT_CLICKED, NULL);
    g_mines_flag_lbl = lv_label_create(g_mines_flag_btn);
    lv_obj_center(g_mines_flag_lbl);

    lv_obj_t* b_new = lv_btn_create(c);
    lv_obj_set_pos(b_new, sx, 104);
    lv_obj_set_size(b_new, 166, 38);
    lv_obj_set_style_bg_color(b_new, lv_color_hex(pal->accent), 0);
    lv_obj_add_event_cb(b_new, mines_new_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ln = lv_label_create(b_new);
    lv_label_set_text(ln, "New Game");
    lv_obj_center(ln);

    g_mines_status_lbl = lv_label_create(c);
    lv_obj_set_pos(g_mines_status_lbl, 0, 274);
    lv_obj_set_style_text_color(g_mines_status_lbl, lv_color_hex(pal->text_dim), 0);

    mines_start();
}

/* ================================================================== */
/* 4. MUSIC (PC Speaker Jukebox: Chime, March, Waltz)                 */
/* ================================================================== */

static lv_obj_t* g_music_title_lbl = NULL;
static lv_obj_t* g_music_note_lbl = NULL;
static lv_obj_t* g_music_bar = NULL;
static lv_obj_t* g_music_play_lbl = NULL;
static uint8_t   g_music_track = 0;
static uint64_t  g_music_started = 0;

static const char* g_track_names[3] = {"Startup Chime", "Tiny March", "Spring Waltz"};
static const char* g_track_desc[3]  = {"Major arpeggio fanfare", "Four-four parade", "Three-four time"};

static void music_play_track(uint8_t idx) {
    g_music_track = (uint8_t)(idx % 3);
    g_music_started = HalGetTickCount();
    if (g_music_track == 0) DrvSoundPlayStartup();
    else if (g_music_track == 1) DrvSoundPlayMarch();
    else DrvSoundPlayWaltz();
    if (g_music_title_lbl) lv_label_set_text(g_music_title_lbl, g_track_names[g_music_track]);
    if (g_music_note_lbl) lv_label_set_text(g_music_note_lbl, g_track_desc[g_music_track]);
}

static void music_play_cb(lv_event_t* e) {
    UNUSED(e);
    if (DrvSoundIsPlaying()) DrvSoundStop();
    else music_play_track(g_music_track);
}

static void music_prev_cb(lv_event_t* e) {
    UNUSED(e);
    music_play_track((uint8_t)((g_music_track + 2) % 3));
}

static void music_next_cb(lv_event_t* e) {
    UNUSED(e);
    music_play_track((uint8_t)((g_music_track + 1) % 3));
}

static void music_stop_cb(lv_event_t* e) {
    UNUSED(e);
    DrvSoundStop();
}

static void music_sel_cb(lv_event_t* e) {
    music_play_track((uint8_t)(uintptr_t)lv_event_get_user_data(e));
}

void AppOpenMusic(void) {
    lv_obj_t* c = GuiCreateWindowFrame(APP_MUSIC, "Music", 230, 88, 490, 290);
    if (!c) return;
    const DIMA_PALETTE* pal = GuiGetPalette();
    lv_obj_set_style_bg_color(c, lv_color_hex(0x161224), 0);

    g_music_track = 0;
    g_music_started = HalGetTickCount();

    /* Album Art Card */
    lv_obj_t* art = lv_obj_create(c);
    lv_obj_set_pos(art, 4, 4);
    lv_obj_set_size(art, 150, 150);
    lv_obj_set_style_bg_color(art, lv_color_hex(0xEC4899), 0);
    lv_obj_set_style_bg_grad_color(art, lv_color_hex(0x4F46E5), 0);
    lv_obj_set_style_bg_grad_dir(art, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(art, 0, 0);
    lv_obj_set_style_radius(art, 18, 0);
    lv_obj_t* art_lbl = lv_label_create(art);
    lv_obj_set_style_text_font(art_lbl, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(art_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(art_lbl, LV_SYMBOL_AUDIO);
    lv_obj_center(art_lbl);

    lv_obj_t* np = lv_label_create(c);
    lv_obj_set_pos(np, 174, 8);
    lv_obj_set_style_text_color(np, lv_color_hex(pal->text_dim), 0);
    lv_label_set_text(np, "NOW PLAYING (PC SPEAKER)");

    g_music_title_lbl = lv_label_create(c);
    lv_obj_set_pos(g_music_title_lbl, 174, 30);
    lv_obj_set_style_text_font(g_music_title_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_music_title_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(g_music_title_lbl, g_track_names[0]);

    g_music_note_lbl = lv_label_create(c);
    lv_obj_set_pos(g_music_note_lbl, 174, 58);
    lv_obj_set_style_text_color(g_music_note_lbl, lv_color_hex(0xCBD5E1), 0);
    lv_label_set_text(g_music_note_lbl, g_track_desc[0]);

    g_music_bar = lv_bar_create(c);
    lv_obj_set_pos(g_music_bar, 174, 88);
    lv_obj_set_size(g_music_bar, 286, 10);
    lv_bar_set_range(g_music_bar, 0, 100);
    lv_obj_set_style_bg_color(g_music_bar, lv_color_hex(0xEC4899), LV_PART_INDICATOR);

    lv_obj_t* b_prev = lv_btn_create(c);
    lv_obj_set_pos(b_prev, 174, 114);
    lv_obj_set_size(b_prev, 54, 38);
    lv_obj_set_style_bg_color(b_prev, lv_color_hex(0x2B2740), 0);
    lv_obj_add_event_cb(b_prev, music_prev_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lp = lv_label_create(b_prev);
    lv_label_set_text(lp, "<<");
    lv_obj_center(lp);

    lv_obj_t* b_play = lv_btn_create(c);
    lv_obj_set_pos(b_play, 236, 112);
    lv_obj_set_size(b_play, 96, 42);
    lv_obj_set_style_bg_color(b_play, lv_color_hex(0xEC4899), 0);
    lv_obj_add_event_cb(b_play, music_play_cb, LV_EVENT_CLICKED, NULL);
    g_music_play_lbl = lv_label_create(b_play);
    lv_label_set_text(g_music_play_lbl, "Play");
    lv_obj_center(g_music_play_lbl);

    lv_obj_t* b_next = lv_btn_create(c);
    lv_obj_set_pos(b_next, 340, 114);
    lv_obj_set_size(b_next, 54, 38);
    lv_obj_set_style_bg_color(b_next, lv_color_hex(0x2B2740), 0);
    lv_obj_add_event_cb(b_next, music_next_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ln = lv_label_create(b_next);
    lv_label_set_text(ln, ">>");
    lv_obj_center(ln);

    lv_obj_t* b_stop = lv_btn_create(c);
    lv_obj_set_pos(b_stop, 402, 114);
    lv_obj_set_size(b_stop, 58, 38);
    lv_obj_set_style_bg_color(b_stop, lv_color_hex(pal->surface_3), 0);
    lv_obj_add_event_cb(b_stop, music_stop_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ls = lv_label_create(b_stop);
    lv_label_set_text(ls, "Stop");
    lv_obj_center(ls);

    for (int i = 0; i < 3; i++) {
        lv_obj_t* tb = lv_btn_create(c);
        lv_obj_set_pos(tb, 4 + i * 156, 178);
        lv_obj_set_size(tb, 148, 36);
        lv_obj_set_style_bg_color(tb, lv_color_hex(0x26213A), 0);
        lv_obj_add_event_cb(tb, music_sel_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
        lv_obj_t* tl = lv_label_create(tb);
        lv_label_set_text(tl, g_track_names[i]);
        lv_obj_center(tl);
    }
}

/* ================================================================== */
/* 5. CHEESY BALLS (from DimOS-moya app_cheesy.c)                     */
/* ================================================================== */

#define CHEESY_W       320
#define CHEESY_H       230
#define BALLS_PLATE    9U
#define BURP_AT        12U
#define DROP_COUNT     12U

static const int g_ball_x[BALLS_PLATE] = {100, 140, 180, 220, 120, 160, 200, 140, 180};
static const int g_ball_y[BALLS_PLATE] = {138, 138, 138, 138, 102, 102, 102, 68, 68};
static const char* g_yum_phrases[8] = {
    "Am-nyam-nyam!", "Crunch!", "So cheesy!", "Ketchup love!",
    "One more!", "Hot and golden!", "Chef is proud!", "Nyam. NYAM!"
};

static lv_obj_t*   g_cheesy_canvas = NULL;
static lv_color_t* g_cheesy_buf = NULL;
static lv_obj_t*   g_cheesy_stats = NULL;
static lv_obj_t*   g_cheesy_msg = NULL;

static uint16_t g_ch_plate = BALLS_PLATE;
static uint32_t g_ch_eaten = 0;
static uint16_t g_ch_streak = 0;
static uint32_t g_ch_burps = 0;
static uint64_t g_ch_chew_until = 0;
static uint64_t g_ch_shake_until = 0;
static uint16_t g_ch_phrase = 0;
static uint32_t g_ch_rng = 0x12345U;
static int16_t  g_drops_x[DROP_COUNT];
static int16_t  g_drops_y[DROP_COUNT];
static uint8_t  g_drops_spd[DROP_COUNT];

static uint32_t ch_rand(void) {
    g_ch_rng = g_ch_rng * 1103515245U + 12345U;
    return (g_ch_rng >> 16) & 0x7FFFU;
}

static void cheesy_render(uint64_t now_ms) {
    if (!g_cheesy_buf || !g_cheesy_canvas) return;
    cbuf_fill_rect(g_cheesy_buf, CHEESY_W, CHEESY_H, 0, 0, CHEESY_W, CHEESY_H, 0x262936);

    /* 12-lamp burp-o-meter */
    for (uint16_t i = 0; i < BURP_AT; i++) {
        uint32_t col = (i < g_ch_streak) ? 0xF59E0B : 0x475569;
        cbuf_fill_circle(g_cheesy_buf, CHEESY_W, CHEESY_H, 24 + i * 24, 18, 6, col);
    }

    int shake = 0;
    if (now_ms < g_ch_shake_until) {
        shake = ((now_ms / 50ULL) % 2ULL == 0ULL) ? 3 : -3;
    }

    /* Cast iron bowl */
    cbuf_fill_ellipse(g_cheesy_buf, CHEESY_W, CHEESY_H, 160 + shake, 178, 130, 42, 0x111827);
    cbuf_fill_ellipse(g_cheesy_buf, CHEESY_W, CHEESY_H, 160 + shake, 170, 124, 34, 0x374151);

    /* Cheese balls */
    for (uint16_t i = 0; i < g_ch_plate; i++) {
        int phase = (int)(((now_ms / 160ULL) + i * 2ULL) % 6ULL);
        int bob = (phase < 3) ? phase : (5 - phase);
        int bx = g_ball_x[i] + shake;
        int by = g_ball_y[i] - bob;
        cbuf_fill_circle(g_cheesy_buf, CHEESY_W, CHEESY_H, bx, by, 19, 0xF59E0B);
        cbuf_fill_circle(g_cheesy_buf, CHEESY_W, CHEESY_H, bx - 6, by - 7, 6, 0xFDE68A);
        if (i + 1 == g_ch_plate) {
            cbuf_fill_circle(g_cheesy_buf, CHEESY_W, CHEESY_H, bx, by + 2, 14, 0xFB923C);
        }
        cbuf_fill_circle(g_cheesy_buf, CHEESY_W, CHEESY_H, bx + 5, by - 8, 5, 0xEF4444);
    }

    if (g_ch_plate > 0) {
        int tx = g_ball_x[g_ch_plate - 1] + shake;
        int ty = g_ball_y[g_ch_plate - 1];
        cbuf_fill_rect(g_cheesy_buf, CHEESY_W, CHEESY_H, tx + 2, ty - 8, 5, 10, 0xEF4444);
        cbuf_fill_circle(g_cheesy_buf, CHEESY_W, CHEESY_H, tx + 4, ty + 4, 4, 0xEF4444);
    }

    /* Ketchup splash particles */
    for (uint8_t i = 0; i < DROP_COUNT; i++) {
        if (g_drops_spd[i] != 0) {
            uint32_t dcol = (i % 2 == 0) ? 0xEF4444 : 0xEC4899;
            cbuf_fill_circle(g_cheesy_buf, CHEESY_W, CHEESY_H, g_drops_x[i], g_drops_y[i], 3, dcol);
        }
    }

    lv_obj_invalidate(g_cheesy_canvas);

    if (g_cheesy_stats) {
        lv_label_set_text_fmt(g_cheesy_stats,
                              "In bowl: %u\nEaten: %u\nBurps: %u",
                              g_ch_plate, (unsigned)g_ch_eaten, (unsigned)g_ch_burps);
    }
    if (g_cheesy_msg) {
        if (now_ms < g_ch_shake_until) {
            lv_label_set_text(g_cheesy_msg, "BURRRP!!");
        } else if (now_ms < g_ch_chew_until) {
            lv_label_set_text(g_cheesy_msg, "OM NOM!");
        } else if (g_ch_plate == 0) {
            lv_label_set_text(g_cheesy_msg, "Bowl empty! Press Cook more");
        } else {
            lv_label_set_text(g_cheesy_msg, g_yum_phrases[g_ch_phrase % 8]);
        }
    }
}

static void cheesy_nom_cb(lv_event_t* e) {
    UNUSED(e);
    if (g_ch_plate == 0) {
        DrvSoundAlert();
        return;
    }
    g_ch_plate--;
    g_ch_eaten++;
    g_ch_streak++;
    g_ch_phrase++;
    uint64_t now = HalGetTickCount();
    g_ch_chew_until = now + 450ULL;
    for (uint8_t i = 0; i < DROP_COUNT; i++) {
        g_drops_x[i] = (int16_t)(95 + (ch_rand() % 130));
        g_drops_y[i] = (int16_t)(60 + (ch_rand() % 30));
        g_drops_spd[i] = (uint8_t)(2 + (ch_rand() % 4));
    }
    DrvSoundPlayNom();
    if (g_ch_streak >= BURP_AT) {
        g_ch_streak = 0;
        g_ch_burps++;
        g_ch_shake_until = now + 900ULL;
        DrvSoundPlayBurp();
    }
    cheesy_render(now);
}

static void cheesy_cook_cb(lv_event_t* e) {
    UNUSED(e);
    if (g_ch_plate > 0) return;
    g_ch_plate = BALLS_PLATE;
    g_ch_chew_until = 0;
    DrvSoundBeep();
    cheesy_render(HalGetTickCount());
}

void AppOpenCheesy(void) {
    lv_obj_t* c = GuiCreateWindowFrame(APP_CHEESY, "Cheesy Balls", 190, 72, 540, 316);
    if (!c) return;
    const DIMA_PALETTE* pal = GuiGetPalette();

    g_cheesy_buf = (lv_color_t*)ExAllocatePool((size_t)CHEESY_W * CHEESY_H * sizeof(lv_color_t));
    if (!g_cheesy_buf) return;

    g_ch_plate = BALLS_PLATE;
    g_ch_streak = 0;
    g_ch_chew_until = 0;
    g_ch_shake_until = 0;
    g_ch_rng = (uint32_t)HalGetTickCount() | 1U;
    memset(g_drops_spd, 0, sizeof(g_drops_spd));

    g_cheesy_canvas = lv_canvas_create(c);
    lv_canvas_set_buffer(g_cheesy_canvas, g_cheesy_buf, CHEESY_W, CHEESY_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_pos(g_cheesy_canvas, 0, 0);

    g_cheesy_msg = lv_label_create(c);
    lv_obj_set_pos(g_cheesy_msg, 0, CHEESY_H + 8);
    lv_obj_set_style_text_font(g_cheesy_msg, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_cheesy_msg, lv_color_hex(pal->warn), 0);

    lv_coord_t rx = CHEESY_W + 16;
    lv_obj_t* hdr = lv_label_create(c);
    lv_obj_set_pos(hdr, rx, 0);
    lv_obj_set_style_text_font(hdr, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(hdr, lv_color_hex(pal->text), 0);
    lv_label_set_text(hdr, "Kitchen");

    g_cheesy_stats = lv_label_create(c);
    lv_obj_set_pos(g_cheesy_stats, rx, 34);
    lv_obj_set_style_text_color(g_cheesy_stats, lv_color_hex(pal->text_dim), 0);

    lv_obj_t* b_nom = lv_btn_create(c);
    lv_obj_set_pos(b_nom, rx, 114);
    lv_obj_set_size(b_nom, 180, 52);
    lv_obj_set_style_bg_color(b_nom, lv_color_hex(pal->warn), 0);
    lv_obj_add_event_cb(b_nom, cheesy_nom_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lnom = lv_label_create(b_nom);
    lv_obj_set_style_text_font(lnom, &lv_font_montserrat_20, 0);
    lv_label_set_text(lnom, "NYAM!");
    lv_obj_center(lnom);

    lv_obj_t* b_cook = lv_btn_create(c);
    lv_obj_set_pos(b_cook, rx, 178);
    lv_obj_set_size(b_cook, 180, 42);
    lv_obj_set_style_bg_color(b_cook, lv_color_hex(pal->surface_3), 0);
    lv_obj_add_event_cb(b_cook, cheesy_cook_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lck = lv_label_create(b_cook);
    lv_label_set_text(lck, "Cook more");
    lv_obj_center(lck);

    cheesy_render(HalGetTickCount());
}

/* ================================================================== */
/* Shared Lifecycle Hooks                                             */
/* ================================================================== */

void AppOnWindowClosed(DIMA_APP_ID id) {
    AppSystemOnClosed(id);
    if (id == APP_PAINT) {
        g_paint_canvas = NULL;
        g_paint_status = NULL;
        if (g_paint_buf) { ExFreePool(g_paint_buf); g_paint_buf = NULL; }
    } else if (id == APP_SNAKE) {
        g_snake_canvas = NULL;
        g_snake_score_lbl = NULL;
        g_snake_status_lbl = NULL;
        g_snake_alive = false;
        if (g_snake_buf) { ExFreePool(g_snake_buf); g_snake_buf = NULL; }
    } else if (id == APP_MINES) {
        memset(g_mines_btns, 0, sizeof(g_mines_btns));
        memset(g_mines_lbls, 0, sizeof(g_mines_lbls));
        g_mines_count_lbl = NULL;
        g_mines_status_lbl = NULL;
        g_mines_flag_btn = NULL;
        g_mines_flag_lbl = NULL;
    } else if (id == APP_MUSIC) {
        g_music_title_lbl = NULL;
        g_music_note_lbl = NULL;
        g_music_bar = NULL;
        g_music_play_lbl = NULL;
    } else if (id == APP_CHEESY) {
        g_cheesy_canvas = NULL;
        g_cheesy_stats = NULL;
        g_cheesy_msg = NULL;
        if (g_cheesy_buf) { ExFreePool(g_cheesy_buf); g_cheesy_buf = NULL; }
    }
}

void AppTick(uint64_t now_ms) {
    /* 1. Snake step */
    if (GuiIsWindowOpen(APP_SNAKE) && g_snake_alive) {
        if (now_ms - g_snake_last_step >= SNAKE_STEP_MS) {
            g_snake_last_step = now_ms;
            snake_step();
        }
    }

    /* 2. Mines right-click detection directly over cell widgets */
    if (GuiIsWindowActive(APP_MINES) && !g_m_over && !g_m_won) {
        MOUSE_STATE ms;
        DrvMouseGetState(&ms);
        if (ms.right_button && !g_m_prev_right) {
            for (int i = 0; i < MINES_GRID * MINES_GRID; i++) {
                if (!g_mines_btns[i]) continue;
                lv_area_t area;
                lv_obj_get_coords(g_mines_btns[i], &area);
                if (ms.x >= area.x1 && ms.x <= area.x2 &&
                    ms.y >= area.y1 && ms.y <= area.y2) {
                    mines_toggle_flag((uint16_t)i);
                    break;
                }
            }
        }
        g_m_prev_right = ms.right_button;
    } else {
        g_m_prev_right = false;
    }

    /* 3. Music progress bar */
    if (GuiIsWindowOpen(APP_MUSIC)) {
        if (g_music_play_lbl) {
            lv_label_set_text(g_music_play_lbl, DrvSoundIsPlaying() ? "Pause" : "Play");
        }
        if (g_music_bar) {
            if (DrvSoundIsPlaying()) {
                uint32_t pct = (uint32_t)(((now_ms - g_music_started) / 100ULL) % 100ULL);
                lv_bar_set_value(g_music_bar, (int32_t)pct, LV_ANIM_OFF);
            } else {
                lv_bar_set_value(g_music_bar, 0, LV_ANIM_OFF);
            }
        }
    }

    /* 4. Cheesy Balls animation (bobbing, shake, ketchup drops) */
    static uint64_t last_cheesy_ms = 0;
    if (GuiIsWindowOpen(APP_CHEESY) && now_ms - last_cheesy_ms >= 45ULL) {
        last_cheesy_ms = now_ms;
        for (uint8_t i = 0; i < DROP_COUNT; i++) {
            if (g_drops_spd[i] != 0) {
                g_drops_y[i] += g_drops_spd[i];
                if (g_drops_y[i] > 160) g_drops_spd[i] = 0;
            }
        }
        cheesy_render(now_ms);
    }
}

void AppHandleKey(const KEY_EVENT* ev) {
    if (!ev || !ev->pressed) return;
    if (GuiIsWindowActive(APP_SNAKE) && g_snake_alive) {
        if (ev->keycode == DIMA_KEY_UP || ev->ascii == 'w' || ev->ascii == 'W') {
            snake_set_dir(SN_UP);
        } else if (ev->keycode == DIMA_KEY_DOWN || ev->ascii == 's' || ev->ascii == 'S') {
            snake_set_dir(SN_DOWN);
        } else if (ev->keycode == DIMA_KEY_LEFT || ev->ascii == 'a' || ev->ascii == 'A') {
            snake_set_dir(SN_LEFT);
        } else if (ev->keycode == DIMA_KEY_RIGHT || ev->ascii == 'd' || ev->ascii == 'D') {
            snake_set_dir(SN_RIGHT);
        }
    } else if (GuiIsWindowActive(APP_SNAKE) && !g_snake_alive) {
        if (ev->ascii == '\n' || ev->ascii == 'n' || ev->ascii == 'N') {
            snake_start();
        }
    }
}
