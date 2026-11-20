#include "../include/drivers.h"
#include "../include/hal.h"

#define KBD_DATA_PORT 0x60
#define KBD_BUF_SIZE  128

static KEY_EVENT g_kbd_buf[KBD_BUF_SIZE];
static volatile size_t g_kbd_head = 0;
static volatile size_t g_kbd_tail = 0;

static bool g_shift = false;
static bool g_ctrl  = false;
static bool g_alt   = false;

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

static void keyboard_irq_handler(KTRAP_FRAME* frame) {
    UNUSED(frame);
    uint8_t sc = HalInPort8(KBD_DATA_PORT);

    bool pressed = (sc & 0x80) == 0;
    uint8_t code = sc & 0x7F;

    if (code == 0x2A || code == 0x36) {
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
    if (code < 128) {
        ascii = g_shift ? g_scancode_shift[code] : g_scancode_normal[code];
    }

    size_t next = (g_kbd_head + 1) % KBD_BUF_SIZE;
    if (next != g_kbd_tail) {
        g_kbd_buf[g_kbd_head].scancode = code;
        g_kbd_buf[g_kbd_head].ascii    = ascii;
        g_kbd_buf[g_kbd_head].pressed  = pressed;
        g_kbd_buf[g_kbd_head].shift    = g_shift;
        g_kbd_buf[g_kbd_head].ctrl     = g_ctrl;
        g_kbd_buf[g_kbd_head].alt      = g_alt;
        g_kbd_head = next;
    }
}

void DrvKeyboardInit(void) {
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
