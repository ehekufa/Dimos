#include "../include/hal.h"

#define ACPI_HEADER_BYTES       36U
#define ACPI_SMI_ENABLE_LIMIT   100000U
#define ACPI_PM1_CNT_SCI_EN     0x0001U
#define ACPI_PM1_CNT_SLP_TYP    0x1C00U
#define ACPI_PM1_CNT_SLP_EN     0x2000U

static uint64_t g_rsdp_virt = 0;
static uint64_t g_hhdm_offset = 0;

static inline uint64_t phys_to_virt(uint64_t phys) {
    if (phys == 0 || g_hhdm_offset == 0) return 0;
    return (phys < g_hhdm_offset) ? (phys + g_hhdm_offset) : phys;
}

__attribute__((noinline))
static uint8_t phys_byte(uint64_t phys) {
    uint64_t virt = phys_to_virt(phys);
    if (virt == 0) return 0;
    return *(const volatile uint8_t*)(uintptr_t)virt;
}

static uint16_t phys_word(uint64_t phys) {
    return (uint16_t)((uint16_t)phys_byte(phys) |
                      ((uint16_t)phys_byte(phys + 1) << 8));
}

static uint32_t phys_dword(uint64_t phys) {
    return (uint32_t)phys_byte(phys) |
           ((uint32_t)phys_byte(phys + 1) << 8) |
           ((uint32_t)phys_byte(phys + 2) << 16) |
           ((uint32_t)phys_byte(phys + 3) << 24);
}

static uint64_t phys_qword(uint64_t phys) {
    return (uint64_t)phys_dword(phys) |
           ((uint64_t)phys_dword(phys + 4) << 32);
}

static bool sig_is(uint64_t phys, const char sig[4]) {
    for (int i = 0; i < 4; i++) {
        if (phys_byte(phys + (uint64_t)i) != (uint8_t)sig[i]) return false;
    }
    return true;
}

static bool checksum_zero(uint64_t phys, uint32_t len) {
    uint8_t sum = 0;
    for (uint32_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + phys_byte(phys + i));
    }
    return sum == 0;
}

static bool rsdp_valid_at(uint64_t phys) {
    return sig_is(phys, "RSD ") &&
           phys_byte(phys + 4) == 'P' &&
           phys_byte(phys + 5) == 'T' &&
           phys_byte(phys + 6) == 'R' &&
           phys_byte(phys + 7) == ' ' &&
           checksum_zero(phys, 20);
}

static uint64_t scan_rsdp(uint64_t start, uint64_t end) {
    for (uint64_t addr = start; addr + 20 <= end; addr += 16) {
        if (rsdp_valid_at(addr)) return addr;
    }
    return 0;
}

static uint64_t find_rsdp(void) {
    if (g_rsdp_virt != 0) {
        uint64_t phys = (g_rsdp_virt >= g_hhdm_offset) ? (g_rsdp_virt - g_hhdm_offset) : g_rsdp_virt;
        if (rsdp_valid_at(phys)) return phys;
    }
    uint64_t ebda_seg = phys_word(0x40E);
    uint64_t ebda = ebda_seg << 4;
    if (ebda >= 0x80000 && ebda < 0xA0000) {
        uint64_t r = scan_rsdp(ebda, ebda + 1024);
        if (r != 0) return r;
    }
    return scan_rsdp(0xE0000, 0x100000);
}

static bool table_valid(uint64_t phys, const char sig[4]) {
    if (phys == 0 || !sig_is(phys, sig)) return false;
    uint32_t len = phys_dword(phys + 4);
    if (len < ACPI_HEADER_BYTES || len > 0x00100000U) return false;
    return checksum_zero(phys, len);
}

static uint64_t find_fadt(uint64_t rsdp) {
    uint8_t rev = phys_byte(rsdp + 15);
    if (rev >= 2) {
        uint64_t xsdt = phys_qword(rsdp + 24);
        if (table_valid(xsdt, "XSDT")) {
            uint32_t len = phys_dword(xsdt + 4);
            for (uint32_t off = ACPI_HEADER_BYTES; off + 8 <= len; off += 8) {
                uint64_t tbl = phys_qword(xsdt + off);
                if (table_valid(tbl, "FACP")) return tbl;
            }
        }
    }
    uint64_t rsdt = phys_dword(rsdp + 16);
    if (table_valid(rsdt, "RSDT")) {
        uint32_t len = phys_dword(rsdt + 4);
        for (uint32_t off = ACPI_HEADER_BYTES; off + 4 <= len; off += 4) {
            uint64_t tbl = phys_dword(rsdt + off);
            if (table_valid(tbl, "FACP")) return tbl;
        }
    }
    return 0;
}

static bool aml_integer(uint64_t* cursor, uint64_t limit, uint32_t* value) {
    uint8_t op = phys_byte(*cursor);
    (*cursor)++;
    if (op == 0x00) { *value = 0; return true; }
    if (op == 0x01) { *value = 1; return true; }
    if (op == 0xFF) { *value = 0xFFFFFFFFU; return true; }
    if (op == 0x0A && *cursor + 1 <= limit) {
        *value = phys_byte(*cursor);
        *cursor += 1;
        return true;
    }
    if (op == 0x0B && *cursor + 2 <= limit) {
        *value = phys_word(*cursor);
        *cursor += 2;
        return true;
    }
    if (op == 0x0C && *cursor + 4 <= limit) {
        *value = phys_dword(*cursor);
        *cursor += 4;
        return true;
    }
    if (op == 0x0E && *cursor + 8 <= limit) {
        if (phys_dword(*cursor + 4) != 0) return false;
        *value = phys_dword(*cursor);
        *cursor += 8;
        return true;
    }
    return false;
}

static bool aml_pkg_len(uint64_t addr, uint64_t limit, uint32_t* length, uint8_t* enc_bytes) {
    uint8_t first = phys_byte(addr);
    uint8_t follow = (uint8_t)(first >> 6);
    if (addr + 1 + follow > limit || follow > 3) return false;
    if (follow == 0) {
        *length = (uint32_t)(first & 0x3F);
        *enc_bytes = 1;
        return true;
    }
    *length = (uint32_t)(first & 0x0F);
    for (uint8_t i = 0; i < follow; i++) {
        *length |= (uint32_t)phys_byte(addr + 1 + i) << (4 + (uint32_t)i * 8);
    }
    *enc_bytes = (uint8_t)(follow + 1);
    return true;
}

static bool find_s5_types(uint64_t dsdt, uint32_t* sleep_a, uint32_t* sleep_b) {
    uint32_t len = phys_dword(dsdt + 4);
    uint64_t end = dsdt + len;
    for (uint32_t off = ACPI_HEADER_BYTES; off + 8 < len; off++) {
        if (phys_byte(dsdt + off) != 0x08) continue;
        uint64_t name = dsdt + off + 1;
        if (phys_byte(name) == 0x5C) name++;
        if (phys_byte(name) != '_' || phys_byte(name + 1) != 'S' ||
            phys_byte(name + 2) != '5' || phys_byte(name + 3) != '_') {
            continue;
        }
        uint64_t pkg = name + 4;
        uint32_t pkg_len = 0;
        uint8_t enc_bytes = 0;
        if (phys_byte(pkg) != 0x12 || pkg + 2 >= end ||
            !aml_pkg_len(pkg + 1, end, &pkg_len, &enc_bytes)) {
            continue;
        }
        uint64_t pkg_end = pkg + 1 + pkg_len;
        uint64_t cursor = pkg + 1 + enc_bytes;
        if (pkg_len < enc_bytes || pkg_end > end || cursor >= pkg_end) continue;
        uint8_t elements = phys_byte(cursor++);
        uint32_t va = 0, vb = 0;
        if (elements < 2 ||
            !aml_integer(&cursor, pkg_end, &va) ||
            !aml_integer(&cursor, pkg_end, &vb) ||
            va > 7 || vb > 7) {
            continue;
        }
        *sleep_a = va;
        *sleep_b = vb;
        return true;
    }
    return false;
}

static uint16_t acpi_ctrl_port(uint64_t fadt, uint32_t table_len,
                               uint32_t legacy_off, uint32_t ext_off) {
    if (legacy_off + 4 <= table_len) {
        uint32_t addr = phys_dword(fadt + legacy_off);
        if (addr != 0 && addr <= 0xFFFF) return (uint16_t)addr;
    }
    if (ext_off + 12 <= table_len &&
        phys_byte(fadt + ext_off) == 1 &&
        phys_byte(fadt + ext_off + 1) >= 16) {
        uint32_t addr = phys_dword(fadt + ext_off + 4);
        if (phys_dword(fadt + ext_off + 8) == 0 && addr != 0 && addr <= 0xFFFF) {
            return (uint16_t)addr;
        }
    }
    return 0;
}

void HalAcpiInit(uint64_t rsdp_addr, uint64_t hhdm_offset) {
    g_rsdp_virt = rsdp_addr;
    g_hhdm_offset = hhdm_offset;
    HalPrintf("[HAL] ACPI initialized (RSDP=%p)\n", (void*)rsdp_addr);
}

bool HalAcpiPowerOff(void) {
    uint64_t rsdp = find_rsdp();
    uint64_t fadt = (rsdp != 0) ? find_fadt(rsdp) : 0;
    if (fadt == 0) return false;

    uint32_t fadt_len = phys_dword(fadt + 4);
    if (fadt_len < 90) return false;

    uint64_t dsdt = phys_dword(fadt + 40);
    if (dsdt == 0 && fadt_len >= 148) {
        dsdt = phys_qword(fadt + 140);
    }
    uint32_t sleep_a = 0, sleep_b = 0;
    if (!table_valid(dsdt, "DSDT") || !find_s5_types(dsdt, &sleep_a, &sleep_b)) {
        return false;
    }

    uint16_t pm1a = acpi_ctrl_port(fadt, fadt_len, 64, 172);
    uint16_t pm1b = acpi_ctrl_port(fadt, fadt_len, 68, 184);
    if (pm1a == 0 || HalInPort16(pm1a) == 0xFFFF) return false;

    uint32_t smi_cmd = phys_dword(fadt + 48);
    uint8_t acpi_en = phys_byte(fadt + 52);
    if ((HalInPort16(pm1a) & ACPI_PM1_CNT_SCI_EN) == 0 &&
        smi_cmd != 0 && smi_cmd <= 0xFFFF && acpi_en != 0) {
        HalOutPort8((uint16_t)smi_cmd, acpi_en);
        uint32_t guard = 0;
        for (; guard < ACPI_SMI_ENABLE_LIMIT; guard++) {
            if ((HalInPort16(pm1a) & ACPI_PM1_CNT_SCI_EN) != 0) break;
        }
        if (guard == ACPI_SMI_ENABLE_LIMIT) return false;
    }

    uint16_t ctrl = HalInPort16(pm1a);
    ctrl = (uint16_t)((ctrl & (uint16_t)~(ACPI_PM1_CNT_SLP_TYP | ACPI_PM1_CNT_SLP_EN)) |
                      (uint16_t)(sleep_a << 10) | ACPI_PM1_CNT_SLP_EN);
    if (pm1b != 0 && HalInPort16(pm1b) != 0xFFFF) {
        uint16_t ctrl_b = HalInPort16(pm1b);
        ctrl_b = (uint16_t)((ctrl_b & (uint16_t)~(ACPI_PM1_CNT_SLP_TYP | ACPI_PM1_CNT_SLP_EN)) |
                            (uint16_t)(sleep_b << 10) | ACPI_PM1_CNT_SLP_EN);
        HalOutPort16(pm1b, ctrl_b);
    }
    HalOutPort16(pm1a, ctrl);
    return true;
}
