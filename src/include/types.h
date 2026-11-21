#ifndef DIMAOS_TYPES_H
#define DIMAOS_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

typedef int32_t  NTSTATUS;
typedef void*    PVOID;
typedef uint64_t HANDLE;
typedef uint32_t ULONG;
typedef uint16_t USHORT;
typedef uint8_t  UCHAR;

#define INVALID_HANDLE_VALUE ((HANDLE)-1)

#define STATUS_SUCCESS           ((NTSTATUS)0x00000000)
#define STATUS_UNSUCCESSFUL      ((NTSTATUS)0xC0000001)
#define STATUS_NO_MEMORY         ((NTSTATUS)0xC0000017)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xC000000D)
#define STATUS_NOT_FOUND         ((NTSTATUS)0xC0000225)

#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

#define UNUSED(x) ((void)(x))
#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

#endif /* DIMAOS_TYPES_H */
