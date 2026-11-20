#include "../include/ob.h"
#include "../include/mm.h"
#include "../include/hal.h"
#include "../include/rtl.h"

void ObInit(void) {
    HalPrintf("[OB] Object Manager initialized\n");
}

OBJECT_HEADER* ObCreateObject(OBJECT_TYPE type, size_t body_size, const char* name, void (*destructor)(void*)) {
    if (body_size < sizeof(OBJECT_HEADER)) {
        body_size = sizeof(OBJECT_HEADER);
    }
    OBJECT_HEADER* obj = (OBJECT_HEADER*)ExAllocatePool(body_size);
    if (!obj) return NULL;

    obj->type = type;
    obj->ref_count = 1;
    obj->destructor = destructor;
    if (name) {
        strncpy(obj->name, name, sizeof(obj->name) - 1);
    }
    return obj;
}

void ObReferenceObject(void* obj) {
    if (!obj) return;
    OBJECT_HEADER* hdr = (OBJECT_HEADER*)obj;
    hdr->ref_count++;
}

void ObDereferenceObject(void* obj) {
    if (!obj) return;
    OBJECT_HEADER* hdr = (OBJECT_HEADER*)obj;
    if (hdr->ref_count > 0) {
        hdr->ref_count--;
    }
    if (hdr->ref_count == 0) {
        if (hdr->destructor) {
            hdr->destructor(obj);
        }
        ExFreePool(obj);
    }
}

HANDLE ObCreateHandle(HANDLE_TABLE* table, void* obj) {
    if (!table || !obj) return INVALID_HANDLE_VALUE;
    for (size_t i = 1; i < MAX_HANDLES; i++) {
        if (table->entries[i] == NULL) {
            table->entries[i] = (OBJECT_HEADER*)obj;
            ObReferenceObject(obj);
            return (HANDLE)i;
        }
    }
    return INVALID_HANDLE_VALUE;
}

void* ObLookupHandle(HANDLE_TABLE* table, HANDLE handle, OBJECT_TYPE expected_type) {
    if (!table || handle == 0 || handle >= MAX_HANDLES) return NULL;
    OBJECT_HEADER* obj = table->entries[handle];
    if (!obj) return NULL;
    if (expected_type != OBJECT_TYPE_NONE && obj->type != expected_type) {
        return NULL;
    }
    return (void*)obj;
}

NTSTATUS ObCloseHandle(HANDLE_TABLE* table, HANDLE handle) {
    if (!table || handle == 0 || handle >= MAX_HANDLES) return STATUS_INVALID_PARAMETER;
    OBJECT_HEADER* obj = table->entries[handle];
    if (!obj) return STATUS_NOT_FOUND;
    table->entries[handle] = NULL;
    ObDereferenceObject(obj);
    return STATUS_SUCCESS;
}
