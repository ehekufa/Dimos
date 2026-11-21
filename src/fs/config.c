#include "../include/fs.h"
#include "../include/hal.h"
#include "../include/rtl.h"

#define CFG_FILE    "CONFIG.CFG"
#define CFG_BYTES   16U
#define CFG_VERSION 1U
#define CFG_MAGIC   0x534F4D44U /* "DMOS" */

static bool    g_cfg_saved = false;
static uint8_t g_cfg_theme = THEME_DARK;
static uint8_t g_cfg_brightness = 20;
static uint8_t g_cfg_volume = 10;

static uint8_t cfg_checksum(const uint8_t* bytes, uint16_t len) {
    uint8_t sum = 0xA5;
    for (uint16_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum ^ bytes[i]);
        sum = (uint8_t)((sum << 1) | (sum >> 7));
    }
    return sum;
}

void CfgInit(void) {
    g_cfg_saved = false;
    g_cfg_theme = THEME_DARK;
    g_cfg_brightness = 20;
    g_cfg_volume = 10;

    if (!VolPresent()) return;

    uint8_t buf[CFG_BYTES];
    if (VolReadFile(CFG_FILE, buf, CFG_BYTES) != CFG_BYTES) return;

    uint32_t magic = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                     ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    if (magic != CFG_MAGIC || buf[4] != CFG_VERSION) return;
    if (cfg_checksum(buf, CFG_BYTES - 1) != buf[15]) return;
    if (buf[5] >= THEME_COUNT || buf[6] > 20 || buf[7] > 20) return;

    g_cfg_theme = buf[5];
    g_cfg_brightness = buf[6];
    g_cfg_volume = buf[7];
    g_cfg_saved = true;
    HalPrintf("[CFG] Loaded CONFIG.CFG (theme=%u, brightness=%u, volume=%u)\n",
              g_cfg_theme, g_cfg_brightness, g_cfg_volume);
}

bool CfgSave(uint8_t theme, uint8_t brightness, uint8_t volume) {
    g_cfg_theme = theme;
    g_cfg_brightness = brightness;
    g_cfg_volume = volume;

    if (!VolPresent()) {
        g_cfg_saved = false;
        return false;
    }

    uint8_t buf[CFG_BYTES];
    memset(buf, 0, CFG_BYTES);
    buf[0] = (uint8_t)(CFG_MAGIC & 0xFF);
    buf[1] = (uint8_t)((CFG_MAGIC >> 8) & 0xFF);
    buf[2] = (uint8_t)((CFG_MAGIC >> 16) & 0xFF);
    buf[3] = (uint8_t)((CFG_MAGIC >> 24) & 0xFF);
    buf[4] = CFG_VERSION;
    buf[5] = theme;
    buf[6] = brightness;
    buf[7] = volume;
    buf[15] = cfg_checksum(buf, CFG_BYTES - 1);

    if (!VolWriteFile(CFG_FILE, buf, CFG_BYTES)) {
        g_cfg_saved = false;
        return false;
    }
    g_cfg_saved = true;
    return true;
}

bool    CfgIsSaved(void)      { return g_cfg_saved; }
uint8_t CfgGetTheme(void)     { return g_cfg_theme; }
uint8_t CfgGetBrightness(void){ return g_cfg_brightness; }
uint8_t CfgGetVolume(void)    { return g_cfg_volume; }

void CfgSetCurrent(uint8_t theme, uint8_t brightness, uint8_t volume) {
    g_cfg_theme = theme;
    g_cfg_brightness = brightness;
    g_cfg_volume = volume;
}

bool CfgForget(void) {
    g_cfg_saved = false;
    if (!VolPresent()) return false;
    return VolDeleteFile(CFG_FILE);
}
