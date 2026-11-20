#include "../include/drivers.h"
#include "../include/hal.h"

#define MOUSE_DATA_PORT 0x60
#define MOUSE_CMD_PORT  0x64

static MOUSE_STATE g_mouse;
static int32_t g_max_x = 1024;
static int32_t g_max_y = 768;
static uint8_t g_mouse_cycle = 0;
static int8_t  g_mouse_bytes[3];

static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if ((HalInPort8(MOUSE_CMD_PORT) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((HalInPort8(MOUSE_CMD_PORT) & 2) == 0) return;
        }
    }
}

static void mouse_write(uint8_t data) {
    mouse_wait(1);
    HalOutPort8(MOUSE_CMD_PORT, 0xD4);
    mouse_wait(1);
    HalOutPort8(MOUSE_DATA_PORT, data);
}

static uint8_t mouse_read(void) {
    mouse_wait(0);
    return HalInPort8(MOUSE_DATA_PORT);
}

static void mouse_irq_handler(KTRAP_FRAME* frame) {
    UNUSED(frame);
    uint8_t status = HalInPort8(MOUSE_CMD_PORT);
    if (!(status & 0x20)) return;

    uint8_t data = HalInPort8(MOUSE_DATA_PORT);
    switch (g_mouse_cycle) {
        case 0:
            if (data & 0x08) {
                g_mouse_bytes[0] = (int8_t)data;
                g_mouse_cycle = 1;
            }
            break;
        case 1:
            g_mouse_bytes[1] = (int8_t)data;
            g_mouse_cycle = 2;
            break;
        case 2:
            g_mouse_bytes[2] = (int8_t)data;
            g_mouse_cycle = 0;

            g_mouse.left_button   = (g_mouse_bytes[0] & 0x01) != 0;
            g_mouse.right_button  = (g_mouse_bytes[0] & 0x02) != 0;
            g_mouse.middle_button = (g_mouse_bytes[0] & 0x04) != 0;

            int32_t dx = g_mouse_bytes[1];
            int32_t dy = g_mouse_bytes[2];

            g_mouse.x += dx;
            g_mouse.y -= dy;

            if (g_mouse.x < 0) g_mouse.x = 0;
            if (g_mouse.y < 0) g_mouse.y = 0;
            if (g_mouse.x >= g_max_x) g_mouse.x = g_max_x - 1;
            if (g_mouse.y >= g_max_y) g_mouse.y = g_max_y - 1;
            break;
    }
}

void DrvMouseInit(int32_t screen_width, int32_t screen_height) {
    g_max_x = screen_width;
    g_max_y = screen_height;
    g_mouse.x = screen_width / 2;
    g_mouse.y = screen_height / 2;
    g_mouse.left_button = false;
    g_mouse.right_button = false;
    g_mouse.middle_button = false;

    mouse_wait(1);
    HalOutPort8(MOUSE_CMD_PORT, 0xA8); /* Enable auxiliary device */
    mouse_wait(1);
    HalOutPort8(MOUSE_CMD_PORT, 0x20); /* Get compaq status byte */
    mouse_wait(0);
    uint8_t status = (HalInPort8(MOUSE_DATA_PORT) | 2);
    mouse_wait(1);
    HalOutPort8(MOUSE_CMD_PORT, 0x60); /* Set compaq status */
    mouse_wait(1);
    HalOutPort8(MOUSE_DATA_PORT, status);

    mouse_write(0xF6); /* Set defaults */
    mouse_read();
    mouse_write(0xF4); /* Enable data reporting */
    mouse_read();

    HalRegisterIrqHandler(12, mouse_irq_handler);
    HalPrintf("[DRV] PS/2 Mouse initialized (IRQ12)\n");
}

void DrvMouseGetState(MOUSE_STATE* state) {
    if (state) {
        *state = g_mouse;
    }
}
