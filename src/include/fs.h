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

/* Persistent FAT12 IDE Volume (from DimOS-moya) */
#define VOLUME_NAME_LENGTH 13

bool     VolInit(void);
bool     VolFormat(void);
bool     VolPresent(void);
uint32_t VolSectors(void);
uint32_t VolTotalBytes(void);
uint32_t VolFreeBytes(void);
uint16_t VolFileCount(void);
bool     VolGetFile(uint16_t slot, char* name, uint32_t* size);
uint32_t VolReadFile(const char* name, void* buffer, uint32_t capacity);
bool     VolWriteFile(const char* name, const void* data, uint32_t length);
bool     VolDeleteFile(const char* name);

/* Persistent Configuration (CONFIG.CFG) */
typedef enum {
    THEME_DARK = 0,
    THEME_GREEN,
    THEME_AMBER,
    THEME_MONO,
    THEME_COUNT
} DIMA_THEME;

void    CfgInit(void);
bool    CfgSave(uint8_t theme, uint8_t brightness, uint8_t volume);
bool    CfgIsSaved(void);
uint8_t CfgGetTheme(void);
uint8_t CfgGetBrightness(void);
uint8_t CfgGetVolume(void);
void    CfgSetCurrent(uint8_t theme, uint8_t brightness, uint8_t volume);
bool    CfgForget(void);

#endif /* DIMAOS_FS_H */
