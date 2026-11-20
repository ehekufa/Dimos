#ifndef DIMAOS_OB_H
#define DIMAOS_OB_H

#include "types.h"

typedef enum {
    OBJECT_TYPE_NONE = 0,
    OBJECT_TYPE_PROCESS,
    OBJECT_TYPE_THREAD,
    OBJECT_TYPE_FILE,
    OBJECT_TYPE_EVENT
} OBJECT_TYPE;

typedef struct _OBJECT_HEADER {
    OBJECT_TYPE type;
    uint32_t    ref_count;
    char        name[64];
    void        (*destructor)(void* obj);
} OBJECT_HEADER;

#define MAX_HANDLES 256

typedef struct _HANDLE_TABLE {
    OBJECT_HEADER* entries[MAX_HANDLES];
} HANDLE_TABLE;

void           ObInit(void);
OBJECT_HEADER* ObCreateObject(OBJECT_TYPE type, size_t body_size, const char* name, void (*destructor)(void*));
void           ObReferenceObject(void* obj);
void           ObDereferenceObject(void* obj);

HANDLE         ObCreateHandle(HANDLE_TABLE* table, void* obj);
void*          ObLookupHandle(HANDLE_TABLE* table, HANDLE handle, OBJECT_TYPE expected_type);
NTSTATUS       ObCloseHandle(HANDLE_TABLE* table, HANDLE handle);

#endif /* DIMAOS_OB_H */
