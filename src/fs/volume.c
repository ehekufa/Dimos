#include "../include/fs.h"
#include "../include/drivers.h"
#include "../include/hal.h"
#include "../include/rtl.h"

#define SECTOR_BYTES       512U
#define ENTRY_BYTES        32U
#define ROOT_ENTRIES       224U
#define FAT_COPIES         2U
#define FAT12_LIMIT        4084U
#define CLUSTER_FREE       0x000U
#define CLUSTER_END        0xFFFU
#define NO_CLUSTER         0xFFFFU
#define ENTRY_FREE         0xE5U
#define ENTRY_LAST         0x00U
#define ATTR_ARCHIVE       0x20U
#define ATTR_DIRECTORY     0x10U
#define ATTR_VOLUME        0x08U
#define ATTR_LONG_NAME     0x0FU
#define NO_SECTOR          0xFFFFFFFFU

typedef struct {
    bool     present;
    uint8_t  sectors_per_cluster;
    uint8_t  fat_count;
    uint16_t reserved_sectors;
    uint16_t root_entries;
    uint16_t fat_sectors;
    uint32_t total_sectors;
    uint32_t root_start;
    uint32_t root_sectors;
    uint32_t data_start;
    uint16_t cluster_count;
} FAT12_VOLUME;

static FAT12_VOLUME g_vol;
static uint8_t  g_data_sec[SECTOR_BYTES];
static uint8_t  g_fat_sec[SECTOR_BYTES];
static uint8_t  g_root_sec[SECTOR_BYTES];
static uint32_t g_fat_sec_lba = NO_SECTOR;
static bool     g_fat_dirty = false;
static uint32_t g_root_sec_lba = NO_SECTOR;

static bool read_sec(uint32_t lba, void* buf) {
    return DrvAtaRead(lba, buf, 1);
}

static bool write_sec(uint32_t lba, const void* buf) {
    return DrvAtaWrite(lba, buf, 1);
}

static uint32_t cluster_lba(uint16_t cluster) {
    return g_vol.data_start + (uint32_t)(cluster - 2) * (uint32_t)g_vol.sectors_per_cluster;
}

static uint16_t cluster_bytes(void) {
    return (uint16_t)((uint16_t)g_vol.sectors_per_cluster * SECTOR_BYTES);
}

static bool fat_flush(void) {
    if (!g_fat_dirty) return true;
    for (uint8_t copy = 0; copy < g_vol.fat_count; copy++) {
        uint32_t target = g_fat_sec_lba + (uint32_t)copy * (uint32_t)g_vol.fat_sectors;
        if (!write_sec(target, g_fat_sec)) return false;
    }
    g_fat_dirty = false;
    return true;
}

static bool fat_byte_read(uint32_t idx, uint8_t* out) {
    uint32_t lba = (uint32_t)g_vol.reserved_sectors + idx / SECTOR_BYTES;
    if (g_fat_sec_lba != lba) {
        if (!fat_flush()) return false;
        if (!read_sec(lba, g_fat_sec)) {
            g_fat_sec_lba = NO_SECTOR;
            return false;
        }
        g_fat_sec_lba = lba;
    }
    *out = g_fat_sec[idx % SECTOR_BYTES];
    return true;
}

static bool fat_byte_write(uint32_t idx, uint8_t val) {
    uint32_t lba = (uint32_t)g_vol.reserved_sectors + idx / SECTOR_BYTES;
    if (g_fat_sec_lba != lba) {
        if (!fat_flush()) return false;
        if (!read_sec(lba, g_fat_sec)) {
            g_fat_sec_lba = NO_SECTOR;
            return false;
        }
        g_fat_sec_lba = lba;
    }
    if (g_fat_sec[idx % SECTOR_BYTES] != val) {
        g_fat_sec[idx % SECTOR_BYTES] = val;
        g_fat_dirty = true;
    }
    return true;
}

static uint16_t fat_read(uint16_t cluster) {
    uint32_t first = (uint32_t)cluster * 3U / 2U;
    uint8_t low = 0, high = 0;
    if (!fat_byte_read(first, &low) || !fat_byte_read(first + 1, &high)) {
        return CLUSTER_END;
    }
    if (cluster & 1U) {
        return (uint16_t)(((uint16_t)high << 4) | ((uint16_t)low >> 4));
    }
    return (uint16_t)((uint16_t)low | ((uint16_t)(high & 0x0F) << 8));
}

static bool fat_write(uint16_t cluster, uint16_t val) {
    uint32_t first = (uint32_t)cluster * 3U / 2U;
    uint8_t low = 0, high = 0;
    if (!fat_byte_read(first, &low) || !fat_byte_read(first + 1, &high)) {
        return false;
    }
    if (cluster & 1U) {
        low  = (uint8_t)((low & 0x0F) | (uint8_t)((val << 4) & 0xF0));
        high = (uint8_t)(val >> 4);
    } else {
        low  = (uint8_t)(val & 0xFF);
        high = (uint8_t)((high & 0xF0) | (uint8_t)((val >> 8) & 0x0F));
    }
    return fat_byte_write(first, low) && fat_byte_write(first + 1, high);
}

static void free_chain(uint16_t cluster) {
    uint16_t guard = 0;
    while (cluster >= 2 && cluster < 0xFF0 && guard < 4096) {
        uint16_t next = fat_read(cluster);
        fat_write(cluster, CLUSTER_FREE);
        cluster = next;
        guard++;
    }
}

static uint16_t allocate_chain(uint16_t count) {
    uint16_t first = NO_CLUSTER;
    uint16_t prev = NO_CLUSTER;
    uint16_t found = 0;
    if (count == 0) return NO_CLUSTER;

    for (uint16_t c = 2; c < (uint16_t)(g_vol.cluster_count + 2) && found < count; c++) {
        if (fat_read(c) != CLUSTER_FREE) continue;
        if (!fat_write(c, CLUSTER_END)) break;
        if (prev != NO_CLUSTER) {
            fat_write(prev, c);
        } else {
            first = c;
        }
        prev = c;
        found++;
    }
    if (found < count) {
        if (first != NO_CLUSTER) free_chain(first);
        return NO_CLUSTER;
    }
    return first;
}

static bool entry_is_file(const uint8_t* e) {
    if (e[0] == ENTRY_LAST || e[0] == ENTRY_FREE) return false;
    if (e[11] == ATTR_LONG_NAME) return false;
    if ((e[11] & (ATTR_VOLUME | ATTR_DIRECTORY)) != 0) return false;
    return true;
}

static uint16_t entry_cluster(const uint8_t* e) {
    return (uint16_t)((uint16_t)e[26] | ((uint16_t)e[27] << 8));
}

static uint32_t entry_size(const uint8_t* e) {
    return (uint32_t)e[28] | ((uint32_t)e[29] << 8) |
           ((uint32_t)e[30] << 16) | ((uint32_t)e[31] << 24);
}

static void entry_name(const uint8_t* e, char* out) {
    uint16_t pos = 0;
    for (uint8_t col = 0; col < 8 && e[col] != ' '; col++) {
        out[pos++] = (char)e[col];
    }
    if (e[8] != ' ') {
        out[pos++] = '.';
        for (uint8_t col = 8; col < 11 && e[col] != ' '; col++) {
            out[pos++] = (char)e[col];
        }
    }
    out[pos] = '\0';
}

static const uint8_t* root_entry(uint16_t idx) {
    uint32_t lba = g_vol.root_start + (uint32_t)idx / 16U;
    if (g_root_sec_lba != lba) {
        if (!read_sec(lba, g_root_sec)) {
            g_root_sec_lba = NO_SECTOR;
            return NULL;
        }
        g_root_sec_lba = lba;
    }
    return g_root_sec + ((uint32_t)idx % 16U) * ENTRY_BYTES;
}

static bool root_entry_update(uint16_t idx, const uint8_t* entry) {
    uint32_t lba = g_vol.root_start + (uint32_t)idx / 16U;
    if (g_root_sec_lba != lba) {
        if (!read_sec(lba, g_root_sec)) {
            g_root_sec_lba = NO_SECTOR;
            return false;
        }
        g_root_sec_lba = lba;
    }
    memcpy(g_root_sec + ((uint32_t)idx % 16U) * ENTRY_BYTES, entry, ENTRY_BYTES);
    g_root_sec_lba = NO_SECTOR;
    return write_sec(lba, g_root_sec);
}

static uint16_t find_entry(const char* name, uint16_t* free_slot) {
    uint16_t reusable = NO_CLUSTER;
    for (uint16_t i = 0; i < g_vol.root_entries; i++) {
        const uint8_t* e = root_entry(i);
        if (!e) break;
        if (e[0] == ENTRY_LAST) {
            if (reusable == NO_CLUSTER) reusable = i;
            break;
        }
        if (e[0] == ENTRY_FREE) {
            if (reusable == NO_CLUSTER) reusable = i;
            continue;
        }
        if (entry_is_file(e)) {
            char cand[13];
            entry_name(e, cand);
            if (strcasecmp(cand, name) == 0) {
                if (free_slot) *free_slot = i;
                return i;
            }
        }
    }
    if (free_slot) *free_slot = reusable;
    return NO_CLUSTER;
}

static bool name_char_ok(uint8_t ch, bool first) {
    uint8_t up = (uint8_t)toupper(ch);
    if ((up >= 'A' && up <= 'Z') || (up >= '0' && up <= '9')) return true;
    if (first) return false;
    switch (up) {
        case '!': case '#': case '$': case '%': case '&': case '\'':
        case '-': case '@': case '^': case '_': case '`': case '~':
        case '(': case ')': case '{': case '}':
            return true;
        default:
            return false;
    }
}

static bool encode_name(const char* name, uint8_t* out) {
    for (int i = 0; i < 11; i++) out[i] = ' ';
    uint16_t pos = 0;
    uint8_t base = 0, ext = 0;
    bool seen_dot = false;

    while (name[pos] != '\0') {
        uint8_t ch = (uint8_t)name[pos];
        if (ch == '.') {
            if (seen_dot) return false;
            seen_dot = true;
            pos++;
            continue;
        }
        if (!name_char_ok(ch, (base == 0 && !seen_dot))) return false;
        if (!seen_dot) {
            if (base >= 8) return false;
            out[base++] = (uint8_t)toupper(ch);
        } else {
            if (ext >= 3) return false;
            out[8 + ext++] = (uint8_t)toupper(ch);
        }
        pos++;
    }
    return base != 0 || ext != 0;
}

static uint16_t read_u16(const uint8_t* b, uint16_t off) {
    return (uint16_t)((uint16_t)b[off] | ((uint16_t)b[off + 1] << 8));
}

static uint32_t read_u32(const uint8_t* b, uint16_t off) {
    return (uint32_t)b[off] | ((uint32_t)b[off + 1] << 8) |
           ((uint32_t)b[off + 2] << 16) | ((uint32_t)b[off + 3] << 24);
}

static void write_u16(uint8_t* b, uint16_t off, uint16_t v) {
    b[off] = (uint8_t)(v & 0xFF);
    b[off + 1] = (uint8_t)(v >> 8);
}

static void write_u32(uint8_t* b, uint16_t off, uint32_t v) {
    b[off] = (uint8_t)(v & 0xFF);
    b[off + 1] = (uint8_t)((v >> 8) & 0xFF);
    b[off + 2] = (uint8_t)((v >> 16) & 0xFF);
    b[off + 3] = (uint8_t)((v >> 24) & 0xFF);
}

static void write_padded(uint8_t* b, uint16_t off, const char* txt, uint16_t w) {
    uint16_t i = 0;
    while (i < w && txt[i] != '\0') { b[off + i] = (uint8_t)txt[i]; i++; }
    while (i < w) { b[off + i] = ' '; i++; }
}

static void vol_reset(void) {
    memset(&g_vol, 0, sizeof(g_vol));
    g_fat_sec_lba = NO_SECTOR;
    g_root_sec_lba = NO_SECTOR;
    g_fat_dirty = false;
}

bool VolInit(void) {
    vol_reset();
    if (!DrvAtaPresent() && !DrvAtaInit()) return false;
    if (!read_sec(0, g_data_sec)) return false;
    if (g_data_sec[510] != 0x55 || g_data_sec[511] != 0xAA) return false;
    if (read_u16(g_data_sec, 11) != SECTOR_BYTES) return false;

    g_vol.sectors_per_cluster = g_data_sec[13];
    g_vol.reserved_sectors    = read_u16(g_data_sec, 14);
    g_vol.fat_count           = g_data_sec[16];
    g_vol.root_entries        = read_u16(g_data_sec, 17);
    uint32_t total            = read_u16(g_data_sec, 19);
    if (total == 0) total     = read_u32(g_data_sec, 32);
    g_vol.fat_sectors         = read_u16(g_data_sec, 22);

    if (g_vol.fat_sectors == 0 || g_vol.sectors_per_cluster == 0 ||
        g_vol.reserved_sectors == 0 || g_vol.fat_count == 0 ||
        g_vol.root_entries == 0 || total == 0 || total > DrvAtaSectors()) {
        return false;
    }

    g_vol.total_sectors = total;
    g_vol.root_start = (uint32_t)g_vol.reserved_sectors +
                       (uint32_t)g_vol.fat_count * (uint32_t)g_vol.fat_sectors;
    g_vol.root_sectors = ((uint32_t)g_vol.root_entries * ENTRY_BYTES + SECTOR_BYTES - 1U) / SECTOR_BYTES;
    g_vol.data_start = g_vol.root_start + g_vol.root_sectors;
    if (g_vol.data_start >= total) return false;

    g_vol.cluster_count = (uint16_t)((total - g_vol.data_start) / g_vol.sectors_per_cluster);
    if (g_vol.cluster_count < 1 || g_vol.cluster_count >= FAT12_LIMIT) return false;

    g_vol.present = true;
    HalPrintf("[FS] FAT12 IDE volume mounted (%u KB, %u files)\n",
              VolTotalBytes() / 1024U, VolFileCount());
    return true;
}

bool VolFormat(void) {
    if (!DrvAtaPresent()) return false;
    uint32_t total = DrvAtaSectors();
    if (total > 0xFFFFU) total = 0xFFFFU;
    if (total < 256U) return false;

    uint16_t root_secs = (uint16_t)(((uint32_t)ROOT_ENTRIES * ENTRY_BYTES + SECTOR_BYTES - 1U) / SECTOR_BYTES);
    uint16_t fat_secs = 1;
    uint8_t spc = 1;
    uint32_t clusters = 0;

    for (uint8_t attempt = 0; attempt < 8; attempt++) {
        for (uint8_t round = 0; round < 8; round++) {
            clusters = (total - 1U - root_secs) / spc;
            fat_secs = (uint16_t)(((clusters + 2U) * 3U / 2U + SECTOR_BYTES - 1U) / SECTOR_BYTES);
            if (fat_secs == 0) fat_secs = 1;
            if (total <= 1U + (uint32_t)FAT_COPIES * fat_secs + root_secs) return false;
            uint32_t next = (total - 1U - (uint32_t)FAT_COPIES * fat_secs - root_secs) / spc;
            if (next == clusters) break;
            clusters = next;
        }
        if (clusters > 16U && clusters <= FAT12_LIMIT) break;
        if (spc >= 64) return false;
        spc = (uint8_t)(spc * 2U);
    }
    if (clusters == 0 || clusters > FAT12_LIMIT) return false;

    vol_reset();
    memset(g_data_sec, 0, SECTOR_BYTES);
    g_data_sec[0] = 0xEB; g_data_sec[1] = 0x3C; g_data_sec[2] = 0x90;
    write_padded(g_data_sec, 3, "DIMAOS", 8);
    write_u16(g_data_sec, 11, SECTOR_BYTES);
    g_data_sec[13] = spc;
    write_u16(g_data_sec, 14, 1);
    g_data_sec[16] = FAT_COPIES;
    write_u16(g_data_sec, 17, ROOT_ENTRIES);
    write_u16(g_data_sec, 19, (uint16_t)total);
    g_data_sec[21] = 0xF0;
    write_u16(g_data_sec, 22, fat_secs);
    write_u16(g_data_sec, 24, 18);
    write_u16(g_data_sec, 26, 2);
    g_data_sec[38] = 0x29;
    write_u32(g_data_sec, 39, 0x44494D41U);
    write_padded(g_data_sec, 43, "DIMAOS", 11);
    write_padded(g_data_sec, 54, "FAT12", 8);
    g_data_sec[510] = 0x55; g_data_sec[511] = 0xAA;
    if (!write_sec(0, g_data_sec)) return false;

    for (uint16_t i = 0; i < fat_secs; i++) {
        memset(g_data_sec, 0, SECTOR_BYTES);
        if (i == 0) {
            g_data_sec[0] = 0xF0; g_data_sec[1] = 0xFF; g_data_sec[2] = 0xFF;
        }
        for (uint8_t copy = 0; copy < FAT_COPIES; copy++) {
            if (!write_sec(1U + (uint32_t)copy * fat_secs + i, g_data_sec)) return false;
        }
    }

    memset(g_data_sec, 0, SECTOR_BYTES);
    uint32_t root_start = 1U + (uint32_t)FAT_COPIES * fat_secs;
    for (uint16_t i = 0; i < root_secs; i++) {
        if (!write_sec(root_start + i, g_data_sec)) return false;
    }

    DrvAtaFlush();
    return VolInit();
}

bool     VolPresent(void)    { return g_vol.present; }
uint32_t VolSectors(void)    { return g_vol.present ? g_vol.total_sectors : 0; }
uint32_t VolTotalBytes(void) { return g_vol.present ? g_vol.total_sectors * SECTOR_BYTES : 0; }

uint32_t VolFreeBytes(void) {
    if (!g_vol.present) return 0;
    uint32_t free_clusters = 0;
    for (uint16_t c = 2; c < (uint16_t)(g_vol.cluster_count + 2); c++) {
        if (fat_read(c) == CLUSTER_FREE) free_clusters++;
    }
    fat_flush();
    return free_clusters * (uint32_t)cluster_bytes();
}

uint16_t VolFileCount(void) {
    if (!g_vol.present) return 0;
    uint16_t count = 0;
    for (uint16_t i = 0; i < g_vol.root_entries; i++) {
        const uint8_t* e = root_entry(i);
        if (!e || e[0] == ENTRY_LAST) break;
        if (entry_is_file(e)) count++;
    }
    return count;
}

bool VolGetFile(uint16_t slot, char* name, uint32_t* size) {
    if (!g_vol.present) return false;
    uint16_t seen = 0;
    for (uint16_t i = 0; i < g_vol.root_entries; i++) {
        const uint8_t* e = root_entry(i);
        if (!e || e[0] == ENTRY_LAST) return false;
        if (!entry_is_file(e)) continue;
        if (seen == slot) {
            if (name) entry_name(e, name);
            if (size) *size = entry_size(e);
            return true;
        }
        seen++;
    }
    return false;
}

uint32_t VolReadFile(const char* name, void* buffer, uint32_t capacity) {
    if (!g_vol.present || !name || !buffer) return 0;
    uint16_t free_slot = NO_CLUSTER;
    uint16_t idx = find_entry(name, &free_slot);
    if (idx == NO_CLUSTER) return 0;
    const uint8_t* e = root_entry(idx);
    if (!e) return 0;

    uint16_t cluster = entry_cluster(e);
    uint32_t size = entry_size(e);
    if (size > capacity) size = capacity;

    uint32_t done = 0;
    uint8_t* dst = (uint8_t*)buffer;
    while (done < size && cluster >= 2 && cluster < 0xFF0) {
        for (uint16_t step = 0; step < g_vol.sectors_per_cluster && done < size; step++) {
            uint32_t take = size - done;
            if (!read_sec(cluster_lba(cluster) + step, g_data_sec)) return done;
            if (take > SECTOR_BYTES) take = SECTOR_BYTES;
            memcpy(dst + done, g_data_sec, take);
            done += take;
        }
        cluster = fat_read(cluster);
    }
    return done;
}

bool VolWriteFile(const char* name, const void* data, uint32_t length) {
    uint8_t encoded[11];
    if (!g_vol.present || !name || !encode_name(name, encoded)) return false;

    uint16_t free_slot = NO_CLUSTER;
    uint16_t idx = find_entry(name, &free_slot);
    if (idx == NO_CLUSTER && free_slot == NO_CLUSTER) return false;

    if (idx != NO_CLUSTER) {
        const uint8_t* existing = root_entry(idx);
        if (!existing) return false;
        free_chain(entry_cluster(existing));
    }

    uint16_t first = NO_CLUSTER;
    uint16_t needed = (uint16_t)((length + (uint32_t)cluster_bytes() - 1U) / (uint32_t)cluster_bytes());
    if (needed > 0) {
        first = allocate_chain(needed);
        if (first == NO_CLUSTER) return false;

        uint16_t cluster = first;
        uint32_t remaining = length;
        const uint8_t* src = (const uint8_t*)data;
        while (cluster >= 2 && cluster < 0xFF0 && remaining > 0) {
            for (uint16_t step = 0; step < g_vol.sectors_per_cluster; step++) {
                uint32_t take = remaining;
                memset(g_data_sec, 0, SECTOR_BYTES);
                if (take > SECTOR_BYTES) take = SECTOR_BYTES;
                if (take > 0) {
                    memcpy(g_data_sec, src, take);
                    src += take;
                    remaining -= take;
                }
                if (!write_sec(cluster_lba(cluster) + step, g_data_sec)) return false;
                if (remaining == 0) break;
            }
            cluster = fat_read(cluster);
        }
    }

    if (idx == NO_CLUSTER) idx = free_slot;
    uint8_t entry[ENTRY_BYTES];
    memset(entry, 0, ENTRY_BYTES);
    memcpy(entry, encoded, 11);
    entry[11] = ATTR_ARCHIVE;
    entry[26] = (uint8_t)(first & 0xFF);
    entry[27] = (uint8_t)(first >> 8);
    write_u32(entry, 28, length);
    if (!root_entry_update(idx, entry)) return false;
    if (!fat_flush()) return false;
    DrvAtaFlush();
    return true;
}

bool VolDeleteFile(const char* name) {
    uint8_t encoded[11];
    if (!g_vol.present || !name || !encode_name(name, encoded)) return false;

    uint16_t free_slot = NO_CLUSTER;
    uint16_t idx = find_entry(name, &free_slot);
    if (idx == NO_CLUSTER) return false;

    const uint8_t* existing = root_entry(idx);
    if (!existing) return false;
    uint8_t entry[ENTRY_BYTES];
    memcpy(entry, existing, ENTRY_BYTES);
    free_chain(entry_cluster(entry));
    entry[0] = ENTRY_FREE;
    if (!root_entry_update(idx, entry)) return false;
    if (!fat_flush()) return false;
    DrvAtaFlush();
    return true;
}
