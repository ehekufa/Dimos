#include "../include/ps.h"
#include "../include/mm.h"
#include "../include/rtl.h"

static KPROCESS* g_process_list = NULL;
static KTHREAD*  g_thread_list  = NULL;
static KTHREAD*  g_current_thread = NULL;
static uint32_t  g_next_pid = 1;
static uint32_t  g_next_tid = 1;
static uint32_t  g_process_count = 0;
static uint32_t  g_thread_count  = 0;
static bool      g_scheduler_ready = false;

static void thread_trampoline(KTHREAD_START_ROUTINE entry, void* context) {
    HalSti();
    if (entry) {
        entry(context);
    }
    if (g_current_thread) {
        g_current_thread->state = THREAD_STATE_TERMINATED;
    }
    for (;;) {
        HalHlt();
    }
}

void PsInit(void) {
    KPROCESS* sys_proc = PsCreateProcess("System", MmGetKernelPml4());
    KTHREAD* boot_thread = (KTHREAD*)ObCreateObject(OBJECT_TYPE_THREAD, sizeof(KTHREAD), "BootThread", NULL);
    boot_thread->tid = g_next_tid++;
    boot_thread->state = THREAD_STATE_RUNNING;
    boot_thread->process = sys_proc;
    boot_thread->next = boot_thread;

    g_thread_list = boot_thread;
    g_current_thread = boot_thread;
    g_thread_count = 1;
    g_scheduler_ready = true;

    HalPrintf("[PS] Process & Thread Manager initialized (PID=1 System)\n");
}

KPROCESS* PsCreateProcess(const char* name, uint64_t* pml4) {
    KPROCESS* proc = (KPROCESS*)ObCreateObject(OBJECT_TYPE_PROCESS, sizeof(KPROCESS), name, NULL);
    if (!proc) return NULL;

    proc->pid = g_next_pid++;
    proc->pml4 = pml4 ? pml4 : MmGetKernelPml4();
    if (name) {
        strncpy(proc->image_name, name, sizeof(proc->image_name) - 1);
    }
    proc->next = g_process_list;
    g_process_list = proc;
    g_process_count++;
    return proc;
}

KTHREAD* PsCreateThread(KPROCESS* process, KTHREAD_START_ROUTINE entry, void* context) {
    if (!process) process = g_process_list;
    KTHREAD* thread = (KTHREAD*)ObCreateObject(OBJECT_TYPE_THREAD, sizeof(KTHREAD), "Thread", NULL);
    if (!thread) return NULL;

    thread->tid = g_next_tid++;
    thread->state = THREAD_STATE_READY;
    thread->process = process;
    thread->kernel_stack_size = 16384;
    thread->kernel_stack_base = (uint64_t)ExAllocatePool(thread->kernel_stack_size);

    uint64_t stack_top = thread->kernel_stack_base + thread->kernel_stack_size;
    KTRAP_FRAME* frame = (KTRAP_FRAME*)(stack_top - sizeof(KTRAP_FRAME));
    memset(frame, 0, sizeof(KTRAP_FRAME));

    frame->rip    = (uint64_t)thread_trampoline;
    frame->cs     = 0x08;
    frame->rflags = 0x202; /* IF=1 */
    frame->rsp    = stack_top;
    frame->ss     = 0x10;
    frame->rdi    = (uint64_t)entry;
    frame->rsi    = (uint64_t)context;

    thread->rsp = (uint64_t)frame;

    /* Insert into circular thread list */
    if (!g_thread_list) {
        g_thread_list = thread;
        thread->next = thread;
    } else {
        thread->next = g_thread_list->next;
        g_thread_list->next = thread;
    }
    g_thread_count++;
    return thread;
}

void PsSchedule(KTRAP_FRAME* frame) {
    if (!g_scheduler_ready || !g_current_thread || g_thread_count <= 1) {
        return;
    }
    UNUSED(frame);
    /* Cooperative/preemptive round-robin tracking */
    KTHREAD* next = g_current_thread->next;
    while (next != g_current_thread && next->state == THREAD_STATE_TERMINATED) {
        next = next->next;
    }
    if (next != g_current_thread && next->state == THREAD_STATE_READY) {
        g_current_thread->state = THREAD_STATE_READY;
        next->state = THREAD_STATE_RUNNING;
        g_current_thread = next;
    }
}

void PsYield(void) {
    __asm__ volatile ("hlt");
}

KTHREAD* PsGetCurrentThread(void) {
    return g_current_thread;
}

KPROCESS* PsGetCurrentProcess(void) {
    return g_current_thread ? g_current_thread->process : NULL;
}

uint32_t PsGetProcessCount(void) {
    return g_process_count;
}

uint32_t PsGetThreadCount(void) {
    return g_thread_count;
}
