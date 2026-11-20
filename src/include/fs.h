#ifndef DIMAOS_FS_H
#define DIMAOS_FS_H

#include "types.h"

#define FS_MAX_PATH     128
#define FS_MAX_FILES    64
#define FS_MAX_FILENAME 64

typedef struct {
    char     name[FS_MAX_FILENAME];
    uint8_t* data;
    size_t   size;
    size_t   capacity;
    bool     is_directory;
    bool     in_use;
} FS_NODE;

typedef struct {
    FS_NODE* node;
    size_t   offset;
} FS_FILE_HANDLE;

void     FsInit(void);
HANDLE   FsOpen(const char* path, bool create_if_missing);
NTSTATUS FsRead(HANDLE file, void* buffer, size_t size, size_t* bytes_read);
NTSTATUS FsWrite(HANDLE file, const void* buffer, size_t size, size_t* bytes_written);
void     FsClose(HANDLE file);
NTSTATUS FsDelete(const char* path);
size_t   FsListDir(const char* path, char* out_buf, size_t buf_size);
FS_NODE* FsGetFileList(size_t* count);

#endif /* DIMAOS_FS_H */
