#include "../include/drivers.h"
#include "../include/hal.h"

#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64
#define KBD_BUF_SIZE    128

static KEY_EVENT g_kbd_buf[KBD_BUF_SIZE];
static volatile size_t g_kbd_head = 0;
static volatile size_t g_kbd_tail = 0;

static bool g_shift = false;
static bool g_ctrl  = false;
static bool g_alt   = false;
static bool g_extended = false;
static uint8_t g_ignore_bytes = 0;

static const char g_scancode_normal[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0
};

static const char g_scancode_shift[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
};

static uint16_t translate_extended(uint8_t code) {
    switch (code) {
        case 0x48: return DIMA_KEY_UP;
        case 0x50: return DIMA_KEY_DOWN;
        case 0x4B: return DIMA_KEY_LEFT;
        case 0x4D: return DIMA_KEY_RIGHT;
        case 0x47: return DIMA_KEY_HOME;
        case 0x4F: return DIMA_KEY_END;
        case 0x49: return DIMA_KEY_PGUP;
        case 0x51: return DIMA_KEY_PGDN;
        case 0x53: return DIMA_KEY_DEL;
        case 0x1C: return '\n';
        case 0x35: return '/';
        default:   return DIMA_KEY_NONE;
    }
}

void DrvKeyboardFeedByte(uint8_t sc) {
    if (g_ignore_bytes > 0) {
        g_ignore_bytes--;
        return;
    }
    if (sc == 0xE1) {
        g_ignore_bytes = 5; /* Pause key sends 6 bytes */
        return;
    }
    if (sc == 0xE0) {
        g_extended = true;
        return;
    }

    bool pressed = (sc & 0x80) == 0;
    uint8_t code = sc & 0x7F;
    bool was_extended = g_extended;
    g_extended = false;

    if (!was_extended && (code == 0x2A || code == 0x36)) {
        g_shift = pressed;
        return;
    }
    if (code == 0x1D) {
        g_ctrl = pressed;
        return;
    }
    if (code == 0x38) {
        g_alt = pressed;
        return;
    }

    char ascii = 0;
    uint16_t keycode = DIMA_KEY_NONE;

    if (was_extended) {
        uint16_t ext = translate_extended(code);
        if (ext == '\n' || ext == '/') {
            ascii = (char)ext;
        } else {
            keycode = ext;
        }
    } else {
        if (code < 128) {
            ascii = g_shift ? g_scancode_shift[code] : g_scancode_normal[code];
        }
        /* Also map non-extended numpad nav keys when no ASCII char */
        if (ascii == 0) {
            keycode = translate_extended(code);
        }
    }

    if (ascii == 0 && keycode == DIMA_KEY_NONE) {
        return;
    }

    size_t next = (g_kbd_head + 1) % KBD_BUF_SIZE;
    if (next != g_kbd_tail) {
        g_kbd_buf[g_kbd_head].scancode = code;
        g_kbd_buf[g_kbd_head].keycode  = keycode;
        g_kbd_buf[g_kbd_head].ascii    = ascii;
        g_kbd_buf[g_kbd_head].pressed  = pressed;
        g_kbd_buf[g_kbd_head].shift    = g_shift;
        g_kbd_buf[g_kbd_head].ctrl     = g_ctrl;
        g_kbd_buf[g_kbd_head].alt      = g_alt;
        g_kbd_head = next;
    }
}

static void keyboard_irq_handler(KTRAP_FRAME* frame) {
    UNUSED(frame);
    uint8_t status = HalInPort8(KBD_STATUS_PORT);
    if (!(status & 0x01)) return;
    uint8_t data = HalInPort8(KBD_DATA_PORT);
    if (status & 0x20) {
        DrvMouseFeedByte(data);
    } else {
        DrvKeyboardFeedByte(data);
    }
}

void DrvKeyboardInit(void) {
    g_kbd_head = 0;
    g_kbd_tail = 0;
    g_shift = false;
    g_ctrl = false;
    g_alt = false;
    g_extended = false;
    g_ignore_bytes = 0;
    HalRegisterIrqHandler(1, keyboard_irq_handler);
    HalPrintf("[DRV] PS/2 Keyboard initialized (IRQ1)\n");
}

bool DrvKeyboardHasChar(void) {
    return g_kbd_head != g_kbd_tail;
}

bool DrvKeyboardGetEvent(KEY_EVENT* ev) {
    if (g_kbd_head == g_kbd_tail) return false;
    *ev = g_kbd_buf[g_kbd_tail];
    g_kbd_tail = (g_kbd_tail + 1) % KBD_BUF_SIZE;
    return true;
}

char DrvKeyboardGetChar(void) {
    KEY_EVENT ev;
    while (DrvKeyboardGetEvent(&ev)) {
        if (ev.pressed && ev.ascii != 0) {
            return ev.ascii;
        }
    }
    return 0;
}
