#include "../include/gui.h"
#include "../include/hal.h"
#include "../include/mm.h"
#include "../include/ps.h"
#include "../include/fs.h"
#include "../include/drivers.h"
#include "../include/rtl.h"

/* Files / Explorer */
static lv_obj_t* g_files_list = NULL;
static lv_obj_t* g_files_viewer = NULL;

/* Notepad */
static lv_obj_t* g_notepad_ta = NULL;
static lv_obj_t* g_notepad_status = NULL;

/* Terminal */
static lv_obj_t* g_cmd_output = NULL;
static lv_obj_t* g_cmd_input  = NULL;

/* Calculator */
static lv_obj_t* g_calc_display = NULL;
static int64_t   g_calc_acc = 0;
static int64_t   g_calc_cur = 0;
static char      g_calc_op  = 0;
static bool      g_calc_new = true;

/* Settings */
static lv_obj_t* g_set_info = NULL;
static lv_obj_t* g_set_theme_lbl = NULL;
static lv_obj_t* g_set_sound_lbl = NULL;

void AppSystemOnClosed(DIMA_APP_ID id) {
    if (id == APP_MY_COMPUTER) {
        g_files_list = NULL;
        g_files_viewer = NULL;
    } else if (id == APP_NOTEPAD) {
        g_notepad_ta = NULL;
        g_notepad_status = NULL;
    } else if (id == APP_CMD) {
        g_cmd_output = NULL;
        g_cmd_input = NULL;
    } else if (id == APP_CALCULATOR) {
        g_calc_display = NULL;
    } else if (id == APP_ABOUT) {
        g_set_info = NULL;
        g_set_theme_lbl = NULL;
        g_set_sound_lbl = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Files (RAM-disk C:\ + FAT12 IDE Volume)                            */
/* ------------------------------------------------------------------ */

static void files_open_ram_cb(lv_event_t* e) {
    const char* name = (const char*)lv_event_get_user_data(e);
    if (!g_files_viewer || !name) return;
    char path[96];
    snprintf(path, sizeof(path), "C:\\%s", name);
    HANDLE h = FsOpen(path, false);
    if (h == INVALID_HANDLE_VALUE) {
        lv_label_set_text(g_files_viewer, "Could not open file.");
        return;
    }
    char buf[512];
    memset(buf, 0, sizeof(buf));
    size_t r = 0;
    FsRead(h, buf, sizeof(buf) - 1, &r);
    FsClose(h);
    if (r == 0) strcpy(buf, "(empty file)");
    lv_label_set_text_fmt(g_files_viewer, "[C:\\%s]\n%s", name, buf);
}

static void files_open_vol_cb(lv_event_t* e) {
    uint16_t slot = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
    if (!g_files_viewer) return;
    char name[VOLUME_NAME_LENGTH];
    if (!VolGetFile(slot, name, NULL)) return;
    char buf[512];
    memset(buf, 0, sizeof(buf));
    uint32_t r = VolReadFile(name, buf, sizeof(buf) - 1);
    if (r == 0) strcpy(buf, "(empty file)");
    lv_label_set_text_fmt(g_files_viewer, "[IDE:%s]\n%s", name, buf);
}

static void files_refresh_btn_cb(lv_event_t* e) {
    UNUSED(e);
    AppRefreshFiles();
}

void AppRefreshFiles(void) {
    if (!g_files_list) return;
    lv_obj_clean(g_files_list);
    const DIMA_PALETTE* pal = GuiGetPalette();

    lv_obj_t* hdr1 = lv_label_create(g_files_list);
    lv_label_set_text(hdr1, "RAM-Disk (C:\\):");
    lv_obj_set_style_text_color(hdr1, lv_color_hex(pal->accent), 0);

    size_t count = 0;
    FS_NODE* nodes = FsGetFileList(&count);
    for (size_t i = 0; i < count; i++) {
        if (!nodes[i].in_use) continue;
        lv_obj_t* btn = lv_btn_create(g_files_list);
        lv_obj_set_size(btn, lv_pct(100), 28);
        lv_obj_set_style_bg_color(btn, lv_color_hex(pal->surface_2), 0);
        lv_obj_add_event_cb(btn, files_open_ram_cb, LV_EVENT_CLICKED, (void*)nodes[i].name);
        lv_obj_t* l = lv_label_create(btn);
        lv_label_set_text_fmt(l, "C:\\%s (%u B)", nodes[i].name, (unsigned)nodes[i].size);
        lv_obj_set_style_text_color(l, lv_color_hex(pal->text), 0);
        lv_obj_align(l, LV_ALIGN_LEFT_MID, 4, 0);
    }

    lv_obj_t* hdr2 = lv_label_create(g_files_list);
    lv_obj_set_style_text_color(hdr2, lv_color_hex(pal->good), 0);
    if (VolPresent()) {
        lv_label_set_text_fmt(hdr2, "IDE FAT12 (%u KB free):",
                              (unsigned)(VolFreeBytes() / 1024U));
        uint16_t vcount = VolFileCount();
        for (uint16_t i = 0; i < vcount; i++) {
            char name[VOLUME_NAME_LENGTH];
            uint32_t sz = 0;
            if (!VolGetFile(i, name, &sz)) break;
            lv_obj_t* btn = lv_btn_create(g_files_list);
            lv_obj_set_size(btn, lv_pct(100), 28);
            lv_obj_set_style_bg_color(btn, lv_color_hex(pal->surface_2), 0);
            lv_obj_add_event_cb(btn, files_open_vol_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
            lv_obj_t* l = lv_label_create(btn);
            lv_label_set_text_fmt(l, "%s (%u B)", name, (unsigned)sz);
            lv_obj_set_style_text_color(l, lv_color_hex(pal->text), 0);
            lv_obj_align(l, LV_ALIGN_LEFT_MID, 4, 0);
        }
    } else if (DrvAtaPresent()) {
        lv_label_set_text(hdr2, "IDE Disk unformatted (Terminal: FORMAT YES)");
    } else {
        lv_label_set_text(hdr2, "No IDE disk attached");
    }
}

void AppOpenFiles(void) {
    lv_obj_t* c = GuiCreateWindowFrame(APP_MY_COMPUTER, "Files", 60, 48, 540, 350);
    if (!c) return;
    const DIMA_PALETTE* pal = GuiGetPalette();

    lv_obj_t* ref = lv_btn_create(c);
    lv_obj_set_size(ref, 90, 26);
    lv_obj_set_pos(ref, 0, 0);
    lv_obj_set_style_bg_color(ref, lv_color_hex(pal->accent), 0);
    lv_obj_add_event_cb(ref, files_refresh_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* rl = lv_label_create(ref);
    lv_label_set_text(rl, "Refresh");
    lv_obj_center(rl);

    g_files_list = lv_obj_create(c);
    lv_obj_set_pos(g_files_list, 0, 32);
    lv_obj_set_size(g_files_list, 250, 262);
    lv_obj_set_style_bg_color(g_files_list, lv_color_hex(pal->surface_3), 0);
    lv_obj_set_style_border_width(g_files_list, 0, 0);
    lv_obj_set_style_pad_all(g_files_list, 6, 0);
    lv_obj_set_flex_flow(g_files_list, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* view_box = lv_obj_create(c);
    lv_obj_set_pos(view_box, 258, 0);
    lv_obj_set_size(view_box, 260, 294);
    lv_obj_set_style_bg_color(view_box, lv_color_hex(pal->surface_2), 0);
    lv_obj_set_style_border_width(view_box, 1, 0);
    lv_obj_set_style_border_color(view_box, lv_color_hex(pal->border), 0);
    lv_obj_set_style_pad_all(view_box, 8, 0);

    g_files_viewer = lv_label_create(view_box);
    lv_obj_set_width(g_files_viewer, lv_pct(100));
    lv_obj_set_style_text_color(g_files_viewer, lv_color_hex(pal->text), 0);
    lv_label_set_text(g_files_viewer, "Select a file on the left to inspect its contents.");

    AppRefreshFiles();
}

/* ------------------------------------------------------------------ */
/* Notepad                                                            */
/* ------------------------------------------------------------------ */

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
    bool disk_ok = false;
    if (VolPresent()) {
        disk_ok = VolWriteFile("NOTES.TXT", txt, (uint32_t)strlen(txt));
    }
    if (g_notepad_status) {
        lv_label_set_text(g_notepad_status, disk_ok ? "Saved to C:\\ & FAT12" : "Saved to C:\\");
    }
    DrvSoundBeep();
    AppRefreshFiles();
}

void AppOpenNotepad(void) {
    lv_obj_t* c = GuiCreateWindowFrame(APP_NOTEPAD, "Notepad - C:\\NOTES.TXT", 120, 74, 470, 340);
    if (!c) return;
    const DIMA_PALETTE* pal = GuiGetPalette();

    lv_obj_t* btn_save = lv_btn_create(c);
    lv_obj_set_size(btn_save, 96, 28);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(pal->accent), 0);
    lv_obj_set_style_radius(btn_save, 6, 0);
    lv_obj_add_event_cb(btn_save, notepad_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* slbl = lv_label_create(btn_save);
    lv_label_set_text(slbl, "Save");
    lv_obj_center(slbl);

    g_notepad_status = lv_label_create(c);
    lv_obj_set_pos(g_notepad_status, 110, 6);
    lv_obj_set_style_text_color(g_notepad_status, lv_color_hex(pal->text_dim), 0);
    lv_label_set_text(g_notepad_status, "Ready");

    g_notepad_ta = lv_textarea_create(c);
    lv_obj_set_pos(g_notepad_ta, 0, 36);
    lv_obj_set_size(g_notepad_ta, lv_pct(100), 246);
    lv_obj_set_style_bg_color(g_notepad_ta, lv_color_hex(pal->surface_2), 0);
    lv_obj_set_style_text_color(g_notepad_ta, lv_color_hex(pal->text), 0);

    char text[512];
    memset(text, 0, sizeof(text));
    if (VolPresent() && VolReadFile("NOTES.TXT", text, sizeof(text) - 1) > 0) {
        /* Loaded from IDE disk */
    } else {
        HANDLE h = FsOpen("C:\\NOTES.TXT", false);
        if (h != INVALID_HANDLE_VALUE) {
            size_t r = 0;
            FsRead(h, text, sizeof(text) - 1, &r);
            FsClose(h);
        }
    }
    lv_textarea_set_text(g_notepad_ta, text);
    GuiFocusObject(g_notepad_ta);
}

/* ------------------------------------------------------------------ */
/* Terminal (DimOS-moya + DimaOS commands)                            */
/* ------------------------------------------------------------------ */

static void term_append(const char* text) {
    if (!g_cmd_output || !text) return;
    const char* old = lv_label_get_text(g_cmd_output);
    size_t old_len = strlen(old);
    size_t add_len = strlen(text);
    /* Trim oldest lines if output gets very long */
    if (old_len > 2048) {
        const char* cut = strchr(old + (old_len - 1500), '\n');
        old = cut ? (cut + 1) : (old + old_len - 1500);
        old_len = strlen(old);
    }
    char* combined = (char*)ExAllocatePool(old_len + add_len + 2);
    if (combined) {
        strcpy(combined, old);
        strcat(combined, text);
        lv_label_set_text(g_cmd_output, combined);
        ExFreePool(combined);
    }
}

static void cmd_exec_cb(lv_event_t* e) {
    UNUSED(e);
    if (!g_cmd_input || !g_cmd_output) return;
    const char* raw = lv_textarea_get_text(g_cmd_input);
    if (!raw || raw[0] == '\0') return;

    char line[256];
    strncpy(line, raw, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    /* Trim leading spaces */
    char* cmd = line;
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') {
        lv_textarea_set_text(g_cmd_input, "");
        return;
    }

    char* arg = cmd;
    while (*arg != '\0' && *arg != ' ') arg++;
    if (*arg == ' ') {
        *arg++ = '\0';
        while (*arg == ' ') arg++;
    }

    char buf[768];
    if (strcasecmp(cmd, "help") == 0) {
        snprintf(buf, sizeof(buf),
            "\nC:\\>%s\n"
            "  dir | type <f> | del <f> | disk | save <n> <txt> | load <n>\n"
            "  format yes | time | date | mem | ver | theme | beep | lock | cls\n"
            "  apps: snake, mines, paint, music, cheesy, calc, settings, reboot\n",
            raw);
    } else if (strcasecmp(cmd, "ver") == 0) {
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nDimaOS v1.0.0 [x86_64 Long Mode + DimOS-moya suite]\n", raw);
    } else if (strcasecmp(cmd, "mem") == 0) {
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nTotal RAM: %lu MB | Free RAM: %lu MB\n",
                 raw, (unsigned long)(MmGetTotalMemory() / (1024 * 1024)),
                 (unsigned long)(MmGetFreeMemory() / (1024 * 1024)));
    } else if (strcasecmp(cmd, "time") == 0) {
        DIMA_TIME tm;
        HalClockGetTime(&tm);
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nTime: %02u:%02u:%02u\n",
                 raw, tm.hours, tm.minutes, tm.seconds);
    } else if (strcasecmp(cmd, "date") == 0) {
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nDate: %04u-%02u-%02u\n",
                 raw, HalClockYear(), HalClockMonth(), HalClockDay());
    } else if (strcasecmp(cmd, "dir") == 0) {
        char list[384];
        memset(list, 0, sizeof(list));
        FsListDir("C:\\", list, sizeof(list));
        if (VolPresent()) {
            char vline[96];
            snprintf(vline, sizeof(vline), "-- IDE FAT12 (%u files) --\n", VolFileCount());
            strncat_safe: ;
            size_t cur = strlen(list);
            if (cur + strlen(vline) < sizeof(list)) strcat(list, vline);
            for (uint16_t i = 0; i < VolFileCount(); i++) {
                char fn[VOLUME_NAME_LENGTH];
                uint32_t sz = 0;
                if (VolGetFile(i, fn, &sz)) {
                    snprintf(vline, sizeof(vline), "%s (%u B)\n", fn, (unsigned)sz);
                    if (strlen(list) + strlen(vline) < sizeof(list)) strcat(list, vline);
                }
            }
            (void)&&strncat_safe;
        }
        snprintf(buf, sizeof(buf), "\nC:\\>%s\n%s", raw, list);
    } else if (strcasecmp(cmd, "type") == 0) {
        if (*arg == '\0') {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\nUsage: type <filename>\n", raw);
        } else {
            char fpath[96];
            snprintf(fpath, sizeof(fpath), "C:\\%s", arg);
            HANDLE h = FsOpen(fpath, false);
            char data[256];
            memset(data, 0, sizeof(data));
            if (h != INVALID_HANDLE_VALUE) {
                size_t r = 0;
                FsRead(h, data, sizeof(data) - 1, &r);
                FsClose(h);
                snprintf(buf, sizeof(buf), "\nC:\\>%s\n%s\n", raw, data);
            } else if (VolPresent() && VolReadFile(arg, data, sizeof(data) - 1) > 0) {
                snprintf(buf, sizeof(buf), "\nC:\\>%s\n%s\n", raw, data);
            } else {
                snprintf(buf, sizeof(buf), "\nC:\\>%s\nNo such file: %s\n", raw, arg);
            }
        }
    } else if (strcasecmp(cmd, "del") == 0) {
        if (*arg == '\0') {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\nUsage: del <filename>\n", raw);
        } else if (NT_SUCCESS(FsDelete(arg))) {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\nDeleted %s from C:\\\n", raw, arg);
            AppRefreshFiles();
        } else if (VolPresent() && VolDeleteFile(arg)) {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\nDeleted %s from IDE disk\n", raw, arg);
            AppRefreshFiles();
        } else {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\nNo such file: %s\n", raw, arg);
        }
    } else if (strcasecmp(cmd, "disk") == 0) {
        if (!DrvAtaPresent()) {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\nNo IDE disk on primary channel\n", raw);
        } else if (!VolPresent()) {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\n%s (%u sectors) - unformatted (FORMAT YES)\n",
                     raw, DrvAtaModel(), DrvAtaSectors());
        } else {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\n%s: %u KB total, %u KB free, %u files\n",
                     raw, DrvAtaModel(),
                     (unsigned)(VolTotalBytes() / 1024U),
                     (unsigned)(VolFreeBytes() / 1024U),
                     VolFileCount());
        }
    } else if (strcasecmp(cmd, "save") == 0) {
        char fname[VOLUME_NAME_LENGTH];
        uint16_t i = 0;
        while (arg[i] && arg[i] != ' ' && i < VOLUME_NAME_LENGTH - 1) {
            fname[i] = arg[i];
            i++;
        }
        fname[i] = '\0';
        const char* txt = &arg[i];
        while (*txt == ' ') txt++;
        if (fname[0] == '\0') {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\nUsage: save NOTES.TXT hello\n", raw);
        } else if (VolPresent()) {
            bool ok = VolWriteFile(fname, txt, (uint32_t)strlen(txt));
            snprintf(buf, sizeof(buf), "\nC:\\>%s\n%s\n", raw,
                     ok ? "Saved to FAT12 disk" : "Failed (use 8.3 name)");
            AppRefreshFiles();
        } else {
            char fpath[64];
            snprintf(fpath, sizeof(fpath), "C:\\%s", fname);
            HANDLE h = FsOpen(fpath, true);
            if (h != INVALID_HANDLE_VALUE) {
                size_t w = 0;
                FsWrite(h, txt, strlen(txt), &w);
                FsClose(h);
                snprintf(buf, sizeof(buf), "\nC:\\>%s\nSaved to RAM-disk C:\\\n", raw);
                AppRefreshFiles();
            } else {
                snprintf(buf, sizeof(buf), "\nC:\\>%s\nSave failed\n", raw);
            }
        }
    } else if (strcasecmp(cmd, "load") == 0) {
        char data[256];
        memset(data, 0, sizeof(data));
        if (*arg == '\0') {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\nUsage: load NOTES.TXT\n", raw);
        } else if (VolPresent() && VolReadFile(arg, data, sizeof(data) - 1) > 0) {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\n%s\n", raw, data);
        } else {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\nNothing saved under %s\n", raw, arg);
        }
    } else if (strcasecmp(cmd, "format") == 0) {
        if (strcasecmp(arg, "yes") != 0) {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\nType 'format yes' to format IDE disk\n", raw);
        } else if (VolFormat()) {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\nFormatted FAT12 volume.\n", raw);
            AppRefreshFiles();
        } else {
            snprintf(buf, sizeof(buf), "\nC:\\>%s\nNo IDE disk to format.\n", raw);
        }
    } else if (strcasecmp(cmd, "theme") == 0) {
        DIMA_THEME next = (DIMA_THEME)((GuiGetTheme() + 1) % THEME_COUNT);
        GuiSelectTheme(next);
        CfgSave((uint8_t)next, GuiGetBrightness(), GuiGetVolume());
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nTheme: %s\n", raw, GuiGetThemeName(next));
    } else if (strcasecmp(cmd, "beep") == 0) {
        DrvSoundPlayStartup();
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nPlayed startup chime.\n", raw);
    } else if (strcasecmp(cmd, "lock") == 0) {
        lv_textarea_set_text(g_cmd_input, "");
        GuiLock();
        return;
    } else if (strcasecmp(cmd, "cls") == 0 || strcasecmp(cmd, "clear") == 0) {
        lv_label_set_text(g_cmd_output, "DimaOS Terminal [v1.0]\nType 'help' for commands.");
        lv_textarea_set_text(g_cmd_input, "");
        return;
    } else if (strcasecmp(cmd, "snake") == 0) {
        GuiOpenApp(APP_SNAKE);
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nOpened Snake.\n", raw);
    } else if (strcasecmp(cmd, "mines") == 0) {
        GuiOpenApp(APP_MINES);
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nOpened Mines.\n", raw);
    } else if (strcasecmp(cmd, "paint") == 0) {
        GuiOpenApp(APP_PAINT);
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nOpened Paint.\n", raw);
    } else if (strcasecmp(cmd, "music") == 0) {
        GuiOpenApp(APP_MUSIC);
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nOpened Music.\n", raw);
    } else if (strcasecmp(cmd, "cheesy") == 0 || strcasecmp(cmd, "nyam") == 0) {
        GuiOpenApp(APP_CHEESY);
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nOpened Cheesy Balls!\n", raw);
    } else if (strcasecmp(cmd, "calc") == 0) {
        GuiOpenApp(APP_CALCULATOR);
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nOpened Calculator.\n", raw);
    } else if (strcasecmp(cmd, "settings") == 0 || strcasecmp(cmd, "about") == 0) {
        GuiOpenApp(APP_ABOUT);
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nOpened Settings.\n", raw);
    } else if (strcasecmp(cmd, "reboot") == 0) {
        HalReboot();
        return;
    } else if (strcasecmp(cmd, "shutdown") == 0) {
        HalShutdown();
        return;
    } else {
        snprintf(buf, sizeof(buf), "\nC:\\>%s\nUnknown command: %s\n", raw, cmd);
    }

    term_append(buf);
    lv_textarea_set_text(g_cmd_input, "");
}

void AppOpenTerminal(void) {
    lv_obj_t* c = GuiCreateWindowFrame(APP_CMD, "Terminal", 180, 96, 540, 350);
    if (!c) return;
    const DIMA_PALETTE* pal = GuiGetPalette();
    lv_obj_set_style_bg_color(c, lv_color_hex(0x0D1117), 0);

    lv_obj_t* out_scroll = lv_obj_create(c);
    lv_obj_set_pos(out_scroll, 0, 0);
    lv_obj_set_size(out_scroll, lv_pct(100), 250);
    lv_obj_set_style_bg_opa(out_scroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(out_scroll, 0, 0);
    lv_obj_set_style_pad_all(out_scroll, 2, 0);

    g_cmd_output = lv_label_create(out_scroll);
    lv_obj_set_width(g_cmd_output, lv_pct(100));
    lv_obj_set_style_text_color(g_cmd_output, lv_color_hex(pal->good), 0);
    lv_label_set_text(g_cmd_output, "DimaOS Terminal [v1.0]\nType 'help' for commands.");

    g_cmd_input = lv_textarea_create(c);
    lv_textarea_set_one_line(g_cmd_input, true);
    lv_obj_set_size(g_cmd_input, 420, 34);
    lv_obj_align(g_cmd_input, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(g_cmd_input, lv_color_hex(pal->surface_2), 0);
    lv_obj_set_style_text_color(g_cmd_input, lv_color_hex(pal->text), 0);
    lv_textarea_set_placeholder_text(g_cmd_input, "Enter command...");
    lv_obj_add_event_cb(g_cmd_input, cmd_exec_cb, LV_EVENT_READY, NULL);
    GuiFocusObject(g_cmd_input);

    lv_obj_t* run_btn = lv_btn_create(c);
    lv_obj_set_size(run_btn, 88, 34);
    lv_obj_align(run_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(run_btn, lv_color_hex(pal->accent), 0);
    lv_obj_add_event_cb(run_btn, cmd_exec_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* rlbl = lv_label_create(run_btn);
    lv_label_set_text(rlbl, "Run");
    lv_obj_center(rlbl);
}

/* ------------------------------------------------------------------ */
/* Calculator                                                         */
/* ------------------------------------------------------------------ */

static void calc_apply_pending(void) {
    if (g_calc_op == '+') g_calc_cur = g_calc_acc + g_calc_cur;
    else if (g_calc_op == '-') g_calc_cur = g_calc_acc - g_calc_cur;
    else if (g_calc_op == '*') g_calc_cur = g_calc_acc * g_calc_cur;
    else if (g_calc_op == '/' && g_calc_cur != 0) g_calc_cur = g_calc_acc / g_calc_cur;
}

static void calc_btn_cb(lv_event_t* e) {
    const char* txt = (const char*)lv_event_get_user_data(e);
    if (!g_calc_display || !txt) return;
    char ch = txt[0];

    if (ch >= '0' && ch <= '9') {
        if (g_calc_new) { g_calc_cur = 0; g_calc_new = false; }
        g_calc_cur = g_calc_cur * 10 + (ch - '0');
        lv_label_set_text_fmt(g_calc_display, "%ld", (long)g_calc_cur);
    } else if (ch == 'C') {
        g_calc_acc = 0; g_calc_cur = 0; g_calc_op = 0; g_calc_new = true;
        lv_label_set_text(g_calc_display, "0");
    } else if (ch == '~') {
        g_calc_cur = -g_calc_cur;
        lv_label_set_text_fmt(g_calc_display, "%ld", (long)g_calc_cur);
    } else if (ch == '=') {
        calc_apply_pending();
        g_calc_op = 0;
        g_calc_new = true;
        lv_label_set_text_fmt(g_calc_display, "%ld", (long)g_calc_cur);
    } else {
        if (!g_calc_new && g_calc_op != 0) {
            calc_apply_pending();
        }
        g_calc_acc = g_calc_cur;
        g_calc_op = ch;
        g_calc_new = true;
        lv_label_set_text_fmt(g_calc_display, "%ld", (long)g_calc_cur);
    }
    DrvSoundBeep();
}

void AppOpenCalculator(void) {
    lv_obj_t* c = GuiCreateWindowFrame(APP_CALCULATOR, "Calculator", 320, 86, 260, 344);
    if (!c) return;
    const DIMA_PALETTE* pal = GuiGetPalette();

    g_calc_acc = 0; g_calc_cur = 0; g_calc_op = 0; g_calc_new = true;

    lv_obj_t* disp_bg = lv_obj_create(c);
    lv_obj_set_pos(disp_bg, 0, 0);
    lv_obj_set_size(disp_bg, 236, 46);
    lv_obj_set_style_bg_color(disp_bg, lv_color_hex(pal->surface_2), 0);
    lv_obj_set_style_border_width(disp_bg, 0, 0);
    lv_obj_set_style_radius(disp_bg, 8, 0);
    lv_obj_clear_flag(disp_bg, LV_OBJ_FLAG_SCROLLABLE);

    g_calc_display = lv_label_create(disp_bg);
    lv_obj_set_width(g_calc_display, lv_pct(100));
    lv_obj_set_style_text_font(g_calc_display, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(g_calc_display, lv_color_hex(pal->text), 0);
    lv_obj_set_style_text_align(g_calc_display, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(g_calc_display, LV_ALIGN_RIGHT_MID, 0, 0);
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
        bool is_op = (col == 3 || keys[i][0] == '=');
        lv_obj_t* b = lv_btn_create(c);
        lv_obj_set_pos(b, col * 60, 56 + row * 58);
        lv_obj_set_size(b, 54, 50);
        lv_obj_set_style_bg_color(b, lv_color_hex(is_op ? pal->warn : pal->surface_3), 0);
        lv_obj_set_style_radius(b, 10, 0);
        lv_obj_add_event_cb(b, calc_btn_cb, LV_EVENT_CLICKED, (void*)keys[i]);
        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, keys[i]);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(pal->text), 0);
        lv_obj_center(l);
    }
}

/* ------------------------------------------------------------------ */
/* Settings & System Info (from DimOS-moya app_about.c)               */
/* ------------------------------------------------------------------ */

static void settings_refresh(void) {
    if (g_set_info) {
        FB_INFO* fb = DrvFbGetInfo();
        DIMA_TIME tm;
        HalClockGetTime(&tm);

        char disk_line[64];
        if (DrvAtaPresent()) {
            snprintf(disk_line, sizeof(disk_line), "%s", DrvAtaModel());
        } else {
            strcpy(disk_line, "None");
        }

        char vol_line[64];
        if (VolPresent()) {
            snprintf(vol_line, sizeof(vol_line), "FAT12 %u/%u KB free",
                     (unsigned)(VolFreeBytes() / 1024U),
                     (unsigned)(VolTotalBytes() / 1024U));
        } else {
            strcpy(vol_line, "Not formatted");
        }

        lv_label_set_text_fmt(g_set_info,
            "DimaOS v1.0.0 (x86_64 Long Mode)\n"
            "Display: %ux%u (%u bpp)\n"
            "Memory: %lu / %lu MB free\n"
            "Tasks: %u procs | %u threads\n"
            "PS/2 Mouse: %s\n"
            "IDE Disk: %s\n"
            "Volume: %s\n"
            "Config: %s\n"
            "RTC: %04u-%02u-%02u %02u:%02u:%02u",
            fb->width, fb->height, fb->bpp,
            (unsigned long)(MmGetFreeMemory() / (1024 * 1024)),
            (unsigned long)(MmGetTotalMemory() / (1024 * 1024)),
            PsGetProcessCount(), PsGetThreadCount(),
            DrvMouseIsAvailable() ? "Active (IRQ12)" : "No",
            disk_line, vol_line,
            CfgIsSaved() ? "Saved in CONFIG.CFG" : "Session only",
            tm.year, tm.month, tm.day, tm.hours, tm.minutes, tm.seconds);
    }
    if (g_set_theme_lbl) {
        lv_label_set_text_fmt(g_set_theme_lbl, "Theme: %s", GuiGetThemeName(GuiGetTheme()));
    }
    if (g_set_sound_lbl) {
        lv_label_set_text_fmt(g_set_sound_lbl, "Sound: %s",
                              DrvSoundIsEnabled() ? "ON" : "MUTE");
    }
}

static void set_theme_cb(lv_event_t* e) {
    UNUSED(e);
    DIMA_THEME next = (DIMA_THEME)((GuiGetTheme() + 1) % THEME_COUNT);
    GuiSelectTheme(next);
    CfgSave((uint8_t)next, GuiGetBrightness(), GuiGetVolume());
    settings_refresh();
}

static void set_sound_cb(lv_event_t* e) {
    UNUSED(e);
    GuiSetVolume(GuiGetVolume() > 0 ? 0 : 10);
    if (GuiGetVolume() > 0) DrvSoundPlayStartup();
    CfgSave((uint8_t)GuiGetTheme(), GuiGetBrightness(), GuiGetVolume());
    settings_refresh();
}

static void set_chime_cb(lv_event_t* e) {
    UNUSED(e);
    DrvSoundPlayStartup();
}

static void set_save_cfg_cb(lv_event_t* e) {
    UNUSED(e);
    CfgSave((uint8_t)GuiGetTheme(), GuiGetBrightness(), GuiGetVolume());
    DrvSoundBeep();
    settings_refresh();
}

static void set_lock_cb(lv_event_t* e) { UNUSED(e); GuiLock(); }
static void set_reboot_cb(lv_event_t* e) { UNUSED(e); HalReboot(); }
static void set_shut_cb(lv_event_t* e) { UNUSED(e); HalShutdown(); }

void AppOpenSettings(void) {
    lv_obj_t* c = GuiCreateWindowFrame(APP_ABOUT, "Settings", 220, 90, 560, 320);
    if (!c) return;
    const DIMA_PALETTE* pal = GuiGetPalette();

    lv_obj_t* left = lv_obj_create(c);
    lv_obj_set_pos(left, 0, 0);
    lv_obj_set_size(left, 300, 262);
    lv_obj_set_style_bg_color(left, lv_color_hex(pal->surface_2), 0);
    lv_obj_set_style_border_width(left, 1, 0);
    lv_obj_set_style_border_color(left, lv_color_hex(pal->border), 0);
    lv_obj_set_style_pad_all(left, 10, 0);

    g_set_info = lv_label_create(left);
    lv_obj_set_width(g_set_info, lv_pct(100));
    lv_obj_set_style_text_color(g_set_info, lv_color_hex(pal->text), 0);

    /* Right panel: Appearance, Sound, Power */
    lv_obj_t* b_theme = lv_btn_create(c);
    lv_obj_set_pos(b_theme, 312, 0);
    lv_obj_set_size(b_theme, 224, 36);
    lv_obj_set_style_bg_color(b_theme, lv_color_hex(pal->accent), 0);
    lv_obj_add_event_cb(b_theme, set_theme_cb, LV_EVENT_CLICKED, NULL);
    g_set_theme_lbl = lv_label_create(b_theme);
    lv_obj_center(g_set_theme_lbl);

    lv_obj_t* b_snd = lv_btn_create(c);
    lv_obj_set_pos(b_snd, 312, 44);
    lv_obj_set_size(b_snd, 110, 36);
    lv_obj_set_style_bg_color(b_snd, lv_color_hex(pal->surface_3), 0);
    lv_obj_add_event_cb(b_snd, set_sound_cb, LV_EVENT_CLICKED, NULL);
    g_set_sound_lbl = lv_label_create(b_snd);
    lv_obj_center(g_set_sound_lbl);

    lv_obj_t* b_chime = lv_btn_create(c);
    lv_obj_set_pos(b_chime, 428, 44);
    lv_obj_set_size(b_chime, 108, 36);
    lv_obj_set_style_bg_color(b_chime, lv_color_hex(pal->surface_3), 0);
    lv_obj_add_event_cb(b_chime, set_chime_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lc = lv_label_create(b_chime);
    lv_label_set_text(lc, "Chime");
    lv_obj_center(lc);

    lv_obj_t* b_save = lv_btn_create(c);
    lv_obj_set_pos(b_save, 312, 90);
    lv_obj_set_size(b_save, 224, 36);
    lv_obj_set_style_bg_color(b_save, lv_color_hex(pal->surface_3), 0);
    lv_obj_add_event_cb(b_save, set_save_cfg_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ls = lv_label_create(b_save);
    lv_label_set_text(ls, "Save CONFIG.CFG");
    lv_obj_center(ls);

    lv_obj_t* b_lock = lv_btn_create(c);
    lv_obj_set_pos(b_lock, 312, 138);
    lv_obj_set_size(b_lock, 224, 36);
    lv_obj_set_style_bg_color(b_lock, lv_color_hex(pal->surface_2), 0);
    lv_obj_add_event_cb(b_lock, set_lock_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* ll = lv_label_create(b_lock);
    lv_label_set_text(ll, "Lock Screen");
    lv_obj_center(ll);

    lv_obj_t* b_reb = lv_btn_create(c);
    lv_obj_set_pos(b_reb, 312, 186);
    lv_obj_set_size(b_reb, 108, 38);
    lv_obj_set_style_bg_color(b_reb, lv_color_hex(pal->warn), 0);
    lv_obj_add_event_cb(b_reb, set_reboot_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lr = lv_label_create(b_reb);
    lv_label_set_text(lr, "Restart");
    lv_obj_center(lr);

    lv_obj_t* b_sh = lv_btn_create(c);
    lv_obj_set_pos(b_sh, 428, 186);
    lv_obj_set_size(b_sh, 108, 38);
    lv_obj_set_style_bg_color(b_sh, lv_color_hex(pal->danger), 0);
    lv_obj_add_event_cb(b_sh, set_shut_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* lsh = lv_label_create(b_sh);
    lv_label_set_text(lsh, "Shutdown");
    lv_obj_center(lsh);

    settings_refresh();
}
