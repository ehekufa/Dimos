#ifndef DIMAOS_PS_H
#define DIMAOS_PS_H

#include "types.h"
#include "ob.h"
#include "hal.h"

typedef enum {
    THREAD_STATE_READY = 0,
    THREAD_STATE_RUNNING,
    THREAD_STATE_BLOCKED,
    THREAD_STATE_TERMINATED
} THREAD_STATE;

typedef struct _KPROCESS {
    OBJECT_HEADER header;
    uint32_t      pid;
    uint64_t*     pml4;
    HANDLE_TABLE  handle_table;
    char          image_name[64];
    struct _KPROCESS* next;
} KPROCESS;

typedef struct _KTHREAD {
    OBJECT_HEADER header;
    uint32_t      tid;
    THREAD_STATE  state;
    KPROCESS*     process;
    uint64_t      rsp;
    uint64_t      kernel_stack_base;
    uint64_t      kernel_stack_size;
    struct _KTHREAD* next;
} KTHREAD;

typedef void (*KTHREAD_START_ROUTINE)(void* context);

void      PsInit(void);
KPROCESS* PsCreateProcess(const char* name, uint64_t* pml4);
KTHREAD*  PsCreateThread(KPROCESS* process, KTHREAD_START_ROUTINE entry, void* context);
void      PsSchedule(KTRAP_FRAME* frame);
void      PsYield(void);
KTHREAD*  PsGetCurrentThread(void);
KPROCESS* PsGetCurrentProcess(void);
uint32_t  PsGetProcessCount(void);
uint32_t  PsGetThreadCount(void);

#endif /* DIMAOS_PS_H */
