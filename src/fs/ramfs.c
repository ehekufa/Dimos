#include "../include/fs.h"
#include "../include/mm.h"
#include "../include/hal.h"
#include "../include/rtl.h"
#include "../include/ob.h"
#include "../include/ps.h"

static FS_NODE g_files[FS_MAX_FILES];

static const char* strip_drive(const char* path) {
    if (!path) return "";
    if ((path[0] == 'C' || path[0] == 'c') && path[1] == ':') {
        path += 2;
    }
    while (*path == '\\' || *path == '/') {
        path++;
    }
    return path;
}

static void create_default_file(const char* name, const char* content) {
    HANDLE h = FsOpen(name, true);
    if (h != INVALID_HANDLE_VALUE) {
        size_t written = 0;
        FsWrite(h, content, strlen(content), &written);
        FsClose(h);
    }
}

void FsInit(void) {
    memset(g_files, 0, sizeof(g_files));

    create_default_file("C:\\README.TXT",
        "Welcome to DimaOS v1.0.0!\r\n"
        "64-bit x86_64 Operating System with NT-style Hybrid Kernel.\r\n"
        "Drive C:\\ is mounted in RAM-disk.\r\n"
        "IDE FAT12 persistence & CONFIG.CFG supported when a disk is attached.\r\n");

    create_default_file("C:\\SYSTEM.INI",
        "[DimaOS]\r\n"
        "Version=1.0.0\r\n"
        "Arch=x86_64\r\n"
        "Shell=DimaShell\r\n");

    create_default_file("C:\\NOTES.TXT",
        "My Notes in DimaOS Notepad.\r\n"
        "You can edit and save this file!\r\n");

    HalPrintf("[FS] RAM-Disk File System mounted at C:\\\n");
}

HANDLE FsOpen(const char* path, bool create_if_missing) {
    const char* clean = strip_drive(path);
    if (!clean || clean[0] == '\0') return INVALID_HANDLE_VALUE;

    FS_NODE* target = NULL;
    for (size_t i = 0; i < FS_MAX_FILES; i++) {
        if (g_files[i].in_use && strcmp(g_files[i].name, clean) == 0) {
            target = &g_files[i];
            break;
        }
    }

    if (!target && create_if_missing) {
        for (size_t i = 0; i < FS_MAX_FILES; i++) {
            if (!g_files[i].in_use) {
                target = &g_files[i];
                memset(target, 0, sizeof(FS_NODE));
                strncpy(target->name, clean, FS_MAX_FILENAME - 1);
                target->in_use = true;
                target->capacity = 1024;
                target->data = (uint8_t*)ExAllocatePool(target->capacity);
                break;
            }
        }
    }

    if (!target) return INVALID_HANDLE_VALUE;

    FS_FILE_HANDLE* fh = (FS_FILE_HANDLE*)ExAllocatePool(sizeof(FS_FILE_HANDLE));
    if (!fh) return INVALID_HANDLE_VALUE;
    fh->node = target;
    fh->offset = 0;
    return (HANDLE)fh;
}

NTSTATUS FsRead(HANDLE file, void* buffer, size_t size, size_t* bytes_read) {
    if (file == INVALID_HANDLE_VALUE || !buffer) return STATUS_INVALID_PARAMETER;
    FS_FILE_HANDLE* fh = (FS_FILE_HANDLE*)file;
    if (!fh->node || !fh->node->in_use) return STATUS_NOT_FOUND;

    size_t avail = (fh->offset < fh->node->size) ? (fh->node->size - fh->offset) : 0;
    size_t to_read = (size < avail) ? size : avail;
    if (to_read > 0) {
        memcpy(buffer, fh->node->data + fh->offset, to_read);
        fh->offset += to_read;
    }
    if (bytes_read) *bytes_read = to_read;
    return STATUS_SUCCESS;
}

NTSTATUS FsWrite(HANDLE file, const void* buffer, size_t size, size_t* bytes_written) {
    if (file == INVALID_HANDLE_VALUE || !buffer) return STATUS_INVALID_PARAMETER;
    FS_FILE_HANDLE* fh = (FS_FILE_HANDLE*)file;
    if (!fh->node || !fh->node->in_use) return STATUS_NOT_FOUND;

    size_t needed = fh->offset + size;
    if (needed > fh->node->capacity) {
        size_t new_cap = ALIGN_UP(needed, 1024);
        uint8_t* new_data = (uint8_t*)ExReallocatePool(fh->node->data, new_cap);
        if (!new_data) return STATUS_NO_MEMORY;
        fh->node->data = new_data;
        fh->node->capacity = new_cap;
    }

    memcpy(fh->node->data + fh->offset, buffer, size);
    fh->offset += size;
    if (fh->offset > fh->node->size) {
        fh->node->size = fh->offset;
    }
    if (bytes_written) *bytes_written = size;
    return STATUS_SUCCESS;
}

void FsClose(HANDLE file) {
    if (file != INVALID_HANDLE_VALUE) {
        ExFreePool((void*)file);
    }
}

NTSTATUS FsDelete(const char* path) {
    const char* clean = strip_drive(path);
    for (size_t i = 0; i < FS_MAX_FILES; i++) {
        if (g_files[i].in_use && strcmp(g_files[i].name, clean) == 0) {
            if (g_files[i].data) ExFreePool(g_files[i].data);
            memset(&g_files[i], 0, sizeof(FS_NODE));
            return STATUS_SUCCESS;
        }
    }
    return STATUS_NOT_FOUND;
}

size_t FsListDir(const char* path, char* out_buf, size_t buf_size) {
    UNUSED(path);
    size_t pos = 0;
    for (size_t i = 0; i < FS_MAX_FILES; i++) {
        if (g_files[i].in_use) {
            int n = snprintf(out_buf + pos, (buf_size > pos) ? (buf_size - pos) : 0,
                             "C:\\%s  (%llu bytes)\n", g_files[i].name, (uint64_t)g_files[i].size);
            if (n > 0) pos += (size_t)n;
        }
    }
    return pos;
}

FS_NODE* FsGetFileList(size_t* count) {
    if (count) *count = FS_MAX_FILES;
    return g_files;
}
