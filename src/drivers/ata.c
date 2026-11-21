#include "../include/drivers.h"
#include "../include/hal.h"
#include "../include/rtl.h"

#define ATA_PRIMARY_DATA     0x01F0
#define ATA_PRIMARY_ERROR    0x01F1
#define ATA_PRIMARY_COUNT    0x01F2
#define ATA_PRIMARY_LBA_LOW  0x01F3
#define ATA_PRIMARY_LBA_MID  0x01F4
#define ATA_PRIMARY_LBA_HIGH 0x01F5
#define ATA_PRIMARY_DRIVE    0x01F6
#define ATA_PRIMARY_COMMAND  0x01F7
#define ATA_PRIMARY_STATUS   0x01F7
#define ATA_ALTERNATE_STATUS 0x03F6

#define ATA_STATUS_ERROR 0x01
#define ATA_STATUS_DRQ   0x08
#define ATA_STATUS_BUSY  0x80

#define ATA_CMD_READ     0x20
#define ATA_CMD_WRITE    0x30
#define ATA_CMD_FLUSH    0xE7
#define ATA_CMD_IDENTIFY 0xEC

#define ATA_POLL_LIMIT   400000U

static bool     g_disk_present = false;
static bool     g_disk_slave = false;
static uint32_t g_disk_sectors = 0;
static char     g_disk_model[41];
static uint16_t g_ident_block[256];

static void bus_delay(void) {
    for (int i = 0; i < 4; i++) {
        (void)HalInPort8(ATA_ALTERNATE_STATUS);
    }
}

static bool wait_ready(void) {
    for (uint32_t i = 0; i < ATA_POLL_LIMIT; i++) {
        uint8_t status = HalInPort8(ATA_PRIMARY_STATUS);
        if ((status & ATA_STATUS_BUSY) == 0) {
            return (status & ATA_STATUS_ERROR) == 0;
        }
    }
    return false;
}

static bool wait_for_data(void) {
    for (uint32_t i = 0; i < ATA_POLL_LIMIT; i++) {
        uint8_t status = HalInPort8(ATA_PRIMARY_STATUS);
        if (status & ATA_STATUS_BUSY) continue;
        if (status & ATA_STATUS_ERROR) return false;
        if (status & ATA_STATUS_DRQ) return true;
    }
    return false;
}

static void select_drive(bool slave) {
    HalOutPort8(ATA_PRIMARY_DRIVE, (uint8_t)(0xE0 | (slave ? 0x10 : 0x00)));
    bus_delay();
}

static void extract_model(char* out, uint16_t first_word) {
    for (uint16_t i = 0; i < 20; i++) {
        uint16_t w = g_ident_block[first_word + i];
        out[i * 2]     = (char)(w >> 8);
        out[i * 2 + 1] = (char)(w & 0xFF);
    }
    out[40] = '\0';
    size_t len = 40;
    while (len > 0 && out[len - 1] == ' ') {
        out[--len] = '\0';
    }
}

static bool identify_drive(bool slave) {
    select_drive(slave);
    HalOutPort8(ATA_PRIMARY_COUNT, 0);
    HalOutPort8(ATA_PRIMARY_LBA_LOW, 0);
    HalOutPort8(ATA_PRIMARY_LBA_MID, 0);
    HalOutPort8(ATA_PRIMARY_LBA_HIGH, 0);
    HalOutPort8(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);

    uint8_t status = HalInPort8(ATA_PRIMARY_STATUS);
    if (status == 0 || status == 0xFF) return false;

    /* Check ATAPI / SATA signatures before waiting for DRQ */
    uint8_t mid = HalInPort8(ATA_PRIMARY_LBA_MID);
    uint8_t hi  = HalInPort8(ATA_PRIMARY_LBA_HIGH);
    if (mid != 0 || hi != 0) return false;

    if (!wait_ready() || !wait_for_data()) return false;
    for (uint16_t i = 0; i < 256; i++) {
        g_ident_block[i] = HalInPort16(ATA_PRIMARY_DATA);
    }

    g_disk_sectors = (uint32_t)g_ident_block[60] | ((uint32_t)g_ident_block[61] << 16);
    if (g_disk_sectors == 0) return false;

    extract_model(g_disk_model, 27);
    if (g_disk_model[0] == '\0') {
        strcpy(g_disk_model, "IDE Disk");
    }
    return true;
}

bool DrvAtaInit(void) {
    g_disk_present = false;
    g_disk_slave = false;
    g_disk_sectors = 0;
    g_disk_model[0] = '\0';

    if (identify_drive(false)) {
        g_disk_present = true;
    } else if (identify_drive(true)) {
        g_disk_present = true;
        g_disk_slave = true;
    }

    if (g_disk_present) {
        HalPrintf("[DRV] ATA IDE %s: %s (%u sectors, %u KB)\n",
                  g_disk_slave ? "Slave" : "Master",
                  g_disk_model, g_disk_sectors, g_disk_sectors / 2);
    } else {
        HalPrintf("[DRV] ATA IDE: no writable disk found\n");
    }
    return g_disk_present;
}

bool DrvAtaPresent(void) {
    return g_disk_present;
}

uint32_t DrvAtaSectors(void) {
    return g_disk_sectors;
}

const char* DrvAtaModel(void) {
    return g_disk_model;
}

bool DrvAtaRead(uint32_t lba, void* buffer, uint16_t count) {
    if (!g_disk_present || count == 0 || !buffer) return false;
    if (lba >= g_disk_sectors || (uint32_t)count > (g_disk_sectors - lba)) return false;

    uint8_t* dst = (uint8_t*)buffer;
    for (uint16_t done = 0; done < count; done++) {
        uint32_t cur = lba + done;
        if (!wait_ready()) return false;
        select_drive(g_disk_slave);
        HalOutPort8(ATA_PRIMARY_COUNT, 1);
        HalOutPort8(ATA_PRIMARY_LBA_LOW, (uint8_t)(cur & 0xFF));
        HalOutPort8(ATA_PRIMARY_LBA_MID, (uint8_t)((cur >> 8) & 0xFF));
        HalOutPort8(ATA_PRIMARY_LBA_HIGH, (uint8_t)((cur >> 16) & 0xFF));
        HalOutPort8(ATA_PRIMARY_DRIVE,
                    (uint8_t)(0xE0 | (g_disk_slave ? 0x10 : 0x00) | ((cur >> 24) & 0x0F)));
        HalOutPort8(ATA_PRIMARY_COMMAND, ATA_CMD_READ);

        if (!wait_for_data()) return false;
        for (uint16_t w = 0; w < 256; w++) {
            uint16_t val = HalInPort16(ATA_PRIMARY_DATA);
            dst[w * 2]     = (uint8_t)(val & 0xFF);
            dst[w * 2 + 1] = (uint8_t)(val >> 8);
        }
        dst += 512;
    }
    return true;
}

bool DrvAtaWrite(uint32_t lba, const void* buffer, uint16_t count) {
    if (!g_disk_present || count == 0 || !buffer) return false;
    if (lba >= g_disk_sectors || (uint32_t)count > (g_disk_sectors - lba)) return false;

    const uint8_t* src = (const uint8_t*)buffer;
    for (uint16_t done = 0; done < count; done++) {
        uint32_t cur = lba + done;
        if (!wait_ready()) return false;
        select_drive(g_disk_slave);
        HalOutPort8(ATA_PRIMARY_COUNT, 1);
        HalOutPort8(ATA_PRIMARY_LBA_LOW, (uint8_t)(cur & 0xFF));
        HalOutPort8(ATA_PRIMARY_LBA_MID, (uint8_t)((cur >> 8) & 0xFF));
        HalOutPort8(ATA_PRIMARY_LBA_HIGH, (uint8_t)((cur >> 16) & 0xFF));
        HalOutPort8(ATA_PRIMARY_DRIVE,
                    (uint8_t)(0xE0 | (g_disk_slave ? 0x10 : 0x00) | ((cur >> 24) & 0x0F)));
        HalOutPort8(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE);

        if (!wait_for_data()) return false;
        for (uint16_t w = 0; w < 256; w++) {
            uint16_t val = (uint16_t)((uint16_t)src[w * 2] | ((uint16_t)src[w * 2 + 1] << 8));
            HalOutPort16(ATA_PRIMARY_DATA, val);
        }
        src += 512;
        if (!wait_ready()) return false;
    }
    return true;
}

bool DrvAtaFlush(void) {
    if (!g_disk_present) return false;
    HalOutPort8(ATA_PRIMARY_COMMAND, ATA_CMD_FLUSH);
    return wait_ready();
}
