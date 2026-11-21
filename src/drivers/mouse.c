#include "../include/drivers.h"
#include "../include/hal.h"

#define MOUSE_DATA_PORT 0x60
#define MOUSE_CMD_PORT  0x64

#define STATUS_OUT_FULL   0x01
#define STATUS_IN_FULL    0x02
#define STATUS_FROM_MOUSE 0x20

#define MOUSE_ACK    0xFA
#define MOUSE_RESEND 0xFE

static MOUSE_STATE g_mouse;
static int32_t g_max_x = 1024;
static int32_t g_max_y = 768;
static uint8_t g_mouse_cycle = 0;
static uint8_t g_mouse_bytes[4];
static uint8_t g_packet_len = 3;
static uint8_t g_mouse_id = 0;

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 200000;
    if (type == 0) {
        while (timeout--) {
            if ((HalInPort8(MOUSE_CMD_PORT) & STATUS_OUT_FULL) != 0) return;
        }
    } else {
        while (timeout--) {
            if ((HalInPort8(MOUSE_CMD_PORT) & STATUS_IN_FULL) == 0) return;
        }
    }
}

static void controller_drain(void) {
    for (uint32_t i = 0; i < 32; i++) {
        if ((HalInPort8(MOUSE_CMD_PORT) & STATUS_OUT_FULL) == 0) break;
        (void)HalInPort8(MOUSE_DATA_PORT);
    }
}

static void mouse_write(uint8_t data) {
    mouse_wait(1);
    HalOutPort8(MOUSE_CMD_PORT, 0xD4);
    mouse_wait(1);
    HalOutPort8(MOUSE_DATA_PORT, data);
}

static bool mouse_read(uint8_t* out) {
    uint32_t timeout = 200000;
    while (timeout--) {
        if ((HalInPort8(MOUSE_CMD_PORT) & STATUS_OUT_FULL) != 0) {
            *out = HalInPort8(MOUSE_DATA_PORT);
            return true;
        }
    }
    return false;
}

static bool mouse_command(uint8_t cmd) {
    for (uint8_t attempt = 0; attempt < 3; attempt++) {
        uint8_t resp = 0;
        mouse_write(cmd);
        if (!mouse_read(&resp)) return false;
        if (resp == MOUSE_ACK) return true;
        if (resp != MOUSE_RESEND) return false;
    }
    return false;
}

static bool mouse_set_sample_rate(uint8_t rate) {
    return mouse_command(0xF3) && mouse_command(rate);
}

static void mouse_detect_protocol(void) {
    uint8_t id = 0;
    if (mouse_set_sample_rate(200) &&
        mouse_set_sample_rate(100) &&
        mouse_set_sample_rate(80) &&
        mouse_command(0xF2) &&
        mouse_read(&id)) {
        g_mouse_id = id;
    } else {
        g_mouse_id = 0;
    }
    g_packet_len = (g_mouse_id == 3 || g_mouse_id == 4) ? 4 : 3;
}

static void mouse_finish_packet(void) {
    uint8_t flags = g_mouse_bytes[0];
    if ((flags & 0xC0) != 0) {
        return; /* Overflow reported by 8042: drop packet */
    }

    g_mouse.left_button   = (flags & 0x01) != 0;
    g_mouse.right_button  = (flags & 0x02) != 0;
    g_mouse.middle_button = (flags & 0x04) != 0;

    /* 9-bit two's complement signed deltas */
    int32_t dx = (int32_t)g_mouse_bytes[1];
    if (flags & 0x10) dx -= 256;
    int32_t dy = (int32_t)g_mouse_bytes[2];
    if (flags & 0x20) dy -= 256;

    g_mouse.x += dx;
    g_mouse.y -= dy;

    if (g_mouse.x < 0) g_mouse.x = 0;
    if (g_mouse.y < 0) g_mouse.y = 0;
    if (g_mouse.x >= g_max_x) g_mouse.x = g_max_x - 1;
    if (g_mouse.y >= g_max_y) g_mouse.y = g_max_y - 1;

    if (g_packet_len == 4) {
        int8_t wheel = (int8_t)(g_mouse_bytes[3] & 0x0F);
        if (wheel & 0x08) wheel = (int8_t)(wheel - 16);
        g_mouse.wheel += wheel;
    }
}

void DrvMouseFeedByte(uint8_t data) {
    if (g_mouse_cycle == 0) {
        if ((data & 0x08) == 0) return; /* Sync bit must be 1 on byte 0 */
    }
    g_mouse_bytes[g_mouse_cycle++] = data;
    if (g_mouse_cycle >= g_packet_len) {
        g_mouse_cycle = 0;
        mouse_finish_packet();
    }
}

static void mouse_irq_handler(KTRAP_FRAME* frame) {
    UNUSED(frame);
    uint8_t status = HalInPort8(MOUSE_CMD_PORT);
    if (!(status & STATUS_OUT_FULL)) return;
    uint8_t data = HalInPort8(MOUSE_DATA_PORT);
    if (status & STATUS_FROM_MOUSE) {
        DrvMouseFeedByte(data);
    } else {
        DrvKeyboardFeedByte(data);
    }
}

void DrvMouseInit(int32_t screen_width, int32_t screen_height) {
    g_max_x = screen_width;
    g_max_y = screen_height;
    g_mouse.x = screen_width / 2;
    g_mouse.y = screen_height / 2;
    g_mouse.wheel = 0;
    g_mouse.left_button = false;
    g_mouse.right_button = false;
    g_mouse.middle_button = false;
    g_mouse.available = false;
    g_mouse_cycle = 0;
    g_packet_len = 3;
    g_mouse_id = 0;

    controller_drain();

    mouse_wait(1);
    HalOutPort8(MOUSE_CMD_PORT, 0xA8); /* Enable auxiliary device */
    mouse_wait(1);
    HalOutPort8(MOUSE_CMD_PORT, 0x20); /* Get compaq status byte */
    uint8_t status = 0;
    mouse_read(&status);
    status = (uint8_t)((status | 0x02) & ~0x20); /* Enable IRQ12, enable mouse clock */
    mouse_wait(1);
    HalOutPort8(MOUSE_CMD_PORT, 0x60); /* Set compaq status */
    mouse_wait(1);
    HalOutPort8(MOUSE_DATA_PORT, status);

    if (mouse_command(0xF6)) {
        mouse_detect_protocol();
        g_mouse.available = mouse_command(0xF4);
    }

    controller_drain();
    HalRegisterIrqHandler(12, mouse_irq_handler);
    HalPrintf("[DRV] PS/2 Mouse initialized (IRQ12, ID=%u, %u-byte packets)\n",
              g_mouse_id, g_packet_len);
}

void DrvMouseGetState(MOUSE_STATE* state) {
    if (state) {
        *state = g_mouse;
    }
}

int32_t DrvMouseConsumeWheel(void) {
    int32_t w = g_mouse.wheel;
    g_mouse.wheel = 0;
    return w;
}

bool DrvMouseIsAvailable(void) {
    return g_mouse.available;
}

void DrvInputPoll(void) {
    HalCli();
    for (uint32_t i = 0; i < 64; i++) {
        uint8_t status = HalInPort8(MOUSE_CMD_PORT);
        if ((status & STATUS_OUT_FULL) == 0) break;
        uint8_t data = HalInPort8(MOUSE_DATA_PORT);
        if (status & STATUS_FROM_MOUSE) {
            DrvMouseFeedByte(data);
        } else {
            DrvKeyboardFeedByte(data);
        }
    }
    HalSti();
}
