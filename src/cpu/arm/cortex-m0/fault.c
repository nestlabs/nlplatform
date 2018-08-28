/*
 *
 *    Copyright (c) 2016-2018 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/*
 *    Description:
 *      This file implements an API for handling crashes and provides fault
 *      handlers. This file also defines data structures, that are saved to RAM
 *      on a hard fault, that are useful for debugging.
 *
 */

#include <nlplatform.h>
#include <nlplatform/nlfault.h>

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>

#include <nlertask.h>

#include <nlplatform.h>
#include <nlplatform/nlfault.h>
#if defined(BUILD_FEATURE_RAM_CONSOLE)
#include <nlplatform/nlram_console.h>
#endif
#include <nlplatform/nlwatchdog.h>
#include <nlbacktrace.h>

#ifdef BUILD_FEATURE_BREADCRUMBS
#include <nlbreadcrumbs.h>
#include <nlbreadcrumbs-local.h>
#include <nlbreadcrumbs-all.h>
#endif /* BUILD_FEATURE_BREADCRUMBS */

#include <nlutilities.h>


#define  SCB_CFSR_IACCVIOL                   ((uint32_t)0x00000001)
#define  SCB_CFSR_DACCVIOL                   ((uint32_t)0x00000002)
#define  SCB_CFSR_MUNSTKERR                  ((uint32_t)0x00000008)
#define  SCB_CFSR_MSTKERR                    ((uint32_t)0x00000010)
#define  SCB_CFSR_MMARVALID                  ((uint32_t)0x00000080)
#define  SCB_CFSR_IBUSERR                    ((uint32_t)0x00000100)
#define  SCB_CFSR_PRECISERR                  ((uint32_t)0x00000200)
#define  SCB_CFSR_IMPRECISERR                ((uint32_t)0x00000400)
#define  SCB_CFSR_UNSTKERR                   ((uint32_t)0x00000800)
#define  SCB_CFSR_STKERR                     ((uint32_t)0x00001000)
#define  SCB_CFSR_BFARVALID                  ((uint32_t)0x00008000)
#define  SCB_CFSR_UNDEFINSTR                 ((uint32_t)0x00010000)
#define  SCB_CFSR_INVSTATE                   ((uint32_t)0x00020000)
#define  SCB_CFSR_INVPC                      ((uint32_t)0x00040000)
#define  SCB_CFSR_NOCP                       ((uint32_t)0x00080000)
#define  SCB_CFSR_UNALIGNED                  ((uint32_t)0x01000000)
#define  SCB_CFSR_DIVBYZERO                  ((uint32_t)0x02000000)

/* For stack overflows, we want nlbacktrace()/nlbacktrace_with_lr() to return a minimum
 * number of backtrace levels, since a stack overflow should have many call levels.
 * If the minimum is not met, we call nlbacktrace_no_context()
 * instead to get a longer list of possible LR values from the stack because we
 * presume that the former failed for some reason (bad pc or lr or both).
 */
#define  MIN_BACKTRACE_LEVELS_FOR_STACK_OVERFLOW 3

typedef enum {
    FAULT = 0,
    WDT
} crashType;

// table to convert from crashType to nl_reset_reason_t
const nl_reset_reason_t kCrashTypeToResetReason[] =
{
    NL_RESET_REASON_HARD_FAULT,       // FAULT, generic
    NL_RESET_REASON_WATCHDOG          // WDT
};

/* r0-r3: low registers saved on process stack by CPU automatically on exception */
#define NUM_LOW_REGISTERS_ON_PROCESS_STACK 4
/* r12, lr (r14), pc (r15), psr: high reigsters saved on process stack */
#define NUM_HIGH_REGISTERS_ON_PROCESS_STACK 4
/* r4-r11 are registers we saved in common_fault_handler_c onto the main stack */
#define NUM_REGISTERS_ON_MAIN_STACK 8

/* number of entries to dump from the stack */
#define NUM_STACK_DUMP_ENTRIES 16

/* Struct representing what FreeRTOS pushed onto the stack of a non-current task */
typedef struct {
    /* r4-r11 is pushed by the PendSV SW interrupt handler of FreeRTOS */
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    /* r0-r3, r12, lr, pc, psr are pushed by the PendSV HW interrupt of Cortex M0 */
    ExceptionStackFrame_t pendSVFrame;
} switched_out_stack_frame_t;

/* saved/restored stack has an extra word if bit 9 of psr is set */
#define PSR_EXTRA_STACK_ALIGN_BIT (1 << 9)

#ifdef DEBUG
    #define faultDebugPrint(...)    printf(__VA_ARGS__)
#else
    #define faultDebugPrint(...)
#endif

/* Special macro to put string literals into named sections so dead
 * stripping works in older versions of gcc.  This does prevent
 * merging of the same string, so it's best if these are unique.
 */
#define UNIQUE_STRING_LITERAL(str) ({ static const char __str[] = str; __str; })

/* When tokenizing, the first argument to printf must be a
 * string literal, but when not tokenizing, we want the
 * string to named so it can be dead stripped by the
 * current version of gcc we're using (future versions don't
 * need this).
 */
#ifdef BUILD_FEATURE_PRINTF_TOKENIZATION
#define UNIQUE_PRINTF_FORMAT_STRING(str) str
#else
#define UNIQUE_PRINTF_FORMAT_STRING(str) UNIQUE_STRING_LITERAL(str)
#endif

#if !defined(BUILD_FEATURE_RESET_INFO) || (defined(BUILD_FEATURE_RESET_INFO) && !defined(BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM))
static uint32_t s_backtrace[NL_FAULT_DIAGS_NUM_BT_ENTRIES];
#endif

/* Functions that are designed to be replaced by linker scripts for
 * different binaries.  Binaries that have no freertos should replace
 * crash_dump with crash_dump_nortos, or use crash_dump_default but
 * replace some of the subroutines called with stubs to get just
 * a subset of the information depending on FLASH limitations
 * (e.g. one could get just the registers without backtraces, or
 * registers and backtraces but no breadcrumbs, etc).
 */
void crash_dump(uint32_t *faultingStackAddress , uint32_t *machineStackAddress, crashType type) LINKER_REPLACEABLE_FUNCTION(crash_dump_default);

/* Parts of crash_dump_default() that can be stubbed to reduce FLASH footprint.
 * Bootloader/AUPD can use crash_dump_nortos() for of crash_dump() to get
 * the smallest size, but will have no fault information.  Alternatively,
 * they can use crash_dump_default(), but stub out parts of it to reduce
 * flash size but still get some crash information.
 * nortos builds using crash_dump_default() should always stub the following
 * since they don't have tasks:
 *
 *    get_and_dump_current_task_info()
 *    dump_watchdog_flags()
 *    dump_all_tasks()
 *
 * They can also stub out these to reduce info collected, and reduce flash size:
 *
 *    crash_dump_breadcrumbs() - probably don't want to write to flash in bootloader
 *    dump_context_to_reset_info()
 *    dump_backtrace() - backtrace library is pretty large
 *    dump_context() - if no printf, this is not useful
 */
void get_and_dump_current_task_info(const char **name, nl_reset_reason_t *reset_reason, uint32_t *stackTop) LINKER_REPLACEABLE_FUNCTION(get_and_dump_current_task_info_default);
void dump_watchdog_flags(void) LINKER_REPLACEABLE_FUNCTION(dump_watchdog_flags_default);
void dump_context(uint32_t *faultingStackAddress , uint32_t *machineStackAddress, crashType type, uint32_t prefault_sp) LINKER_REPLACEABLE_FUNCTION(dump_context_default);
void dump_backtrace(ExceptionStackFrame_t *faultFrame, uint32_t *backtrace_buf, unsigned *backtrace_count,
                    uint32_t prefault_sp, uint32_t stackTop, nl_reset_reason_t reset_reason) LINKER_REPLACEABLE_FUNCTION(dump_backtrace_default);

#ifdef BUILD_FEATURE_RESET_INFO
#define STACK_TRACE_DEPTH NL_FAULT_DIAGS_NUM_BT_ENTRIES
#define TASK_NAME_LEN     NL_FAULT_DIAGS_TASK_NAME_LEN
#else
#define STACK_TRACE_DEPTH 7
#define TASK_NAME_LEN 4
#endif

// The TCB might have more characters in the name than 4, but to keep things
// small for breadcrumbs, we just use 4 (3 for name, and 1 for task state).
typedef struct ThreadDumpInfo {
    uint32_t stackDepth;
    uint32_t stackTrace[STACK_TRACE_DEPTH];
    char name[TASK_NAME_LEN]; //make it aligned
} ThreadDumpInfo_t;

int threadNum = 0;

// Optional API to dump SOC-specific context information on fault
void nlplatform_soc_dump_context(void) NL_WEAK_ATTRIBUTE;

// Optional API to dump product-specific context information on fault
void nlproduct_dump_context(void) NL_WEAK_ATTRIBUTE;

static uint32_t get_task_stack_top(const xTaskHandle taskHandle)
{
    uint32_t stackTop;
    nltask_t *nltask_p = (nltask_t*)taskHandle;

    if (nltask_p == NULL)
    {
        // Not an NLER task, just use 0 and use the looser check for valid SP.
        // This is probably the FreeRTOS idle task.
        stackTop = 0;
    }
    else
    {
        // Get the stack top from the nltask_t
        stackTop = (uint32_t)nltask_p->mStackTop;
    }
    return stackTop;
}

void nlfault_dump_callstack(void)
{
    uint32_t buffer[8];
    int j;
    int num_backtraces;
    uint32_t stackTop = 0;
    uint32_t sp = nlplatform_get_sp();
    uint32_t pc = nlplatform_get_pc();
    xTaskHandle taskHandle = xTaskGetCurrentTaskHandle();
    if (taskHandle)
    {
        stackTop = get_task_stack_top(taskHandle);
    }

    num_backtraces = nlbacktrace(pc, sp, stackTop, buffer, ARRAY_SIZE(buffer));
    for (j = 0; j < num_backtraces; j++)
    {
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("0x%08lx\n"), buffer[j]);
    }
}

static void  __attribute__( ( naked ) ) __attribute__( ( used ) ) common_fault_handler_c(void)
{
    __asm volatile
    (
        // push r4-r12 and lr on the stack. in ARMV7m, this is equivalent to
        // "stmdb sp!, {r4-r12,lr}" but ARMV6 has no stmdb and push can
        // only push r0-r7. must push registers in multiples of 2 to
        // keep stack aligned on 8-byte boundaries.
        " push {r4-r7}                                              \n"
        " movs r0, r8                                               \n"
        " movs r1, r9                                               \n"
        " movs r2, r10                                              \n"
        " movs r3, r11                                              \n"
        " movs r4, r12                                              \n"
        " push {r0-r4, lr}                                          \n"

        // test lr bit #4 to see if this was a fault on the main stack
        // or on the process/thread stack
        " movs r0, #4                                               \n"
        " movs r1, lr                                               \n"
        " tst r1, r0                                                \n"
        // If fault on main stack, load main stack pointer plus 36 (for the
        // registers we just pushed above) to r0, so that r0 points to
        // the exception frame (i.e. does not have the extra registers
        // we just pushed)
        " bne hard_fault_on_process_stack                           \n"
        " hard_fault_on_main_stack:                                 \n"
        " mrs r0, msp                                               \n"
        " add r0, r0, #40                                           \n"
        " b call_crash_dump                                         \n"

        " hard_fault_on_process_stack:                              \n"
        // If fault on process/thread stack, load r0 with psp
        " mrs r0, psp                                               \n"

        " call_crash_dump:                                          \n"

        // Load r1 with msp
        " mrs r1, msp                                               \n"

        // Call our generic crash_dump function.
        " bl crash_dump                                             \n"

        // In case we're told to resume, restore registers from stack.
        // Equivalent on ARMV7 to "ldmia sp!, {r4-r12,pc}
        " pop {r0-r5}                                               \n"
        " movs r8, r0                                               \n"
        " movs r9, r1                                               \n"
        " movs r10, r2                                              \n"
        " movs r11, r3                                              \n"
        " movs r12, r4                                              \n"
        " movs lr, r5                                               \n"
        " pop {r4-r7}                                               \n"
        " bx lr                                                     \n"
    );
}

void  __attribute__( ( naked ) ) nlfault_hard_fault_handler_c(void)
{
    __asm volatile
    (
        " mov r2, #0                                                \n"
        " b common_fault_handler_c                                  \n"
    );
}

void  __attribute__( ( naked ) ) nlfault_pre_watchdog_handler_c(void)
{
     __asm volatile
    (
        " mov r2, #1                                                \n"
        " b common_fault_handler_c                                  \n"
    );
}

#if defined(BUILD_FEATURE_FAULT_DUMP_TASK_STACKS)
void dump_all_tasks(bool task_fault) LINKER_REPLACEABLE_FUNCTION(dump_all_tasks_default);

#if !defined(configMAX_NUM_TASKS)
#define THREAD_DUMP_MAX_NUM_THREADS 11
#else
#define THREAD_DUMP_MAX_NUM_THREADS configMAX_NUM_TASKS
#endif

typedef struct fakeTCB {
    volatile switched_out_stack_frame_t *switched_out_stack_frame_p;
} fakeTCB_t;

//(gdb) p/d &((tskTCB *)0)->pcTaskName
//$3 = 52
#define TCB_OFFSET_NAME 52

#define tskACTIVE_CHAR      ( 'A' )
#define tskBLOCKED_CHAR     ( 'B' )
#define tskREADY_CHAR       ( 'R' )
#define tskDELETED_CHAR     ( 'D' )
#define tskSUSPENDED_CHAR   ( 'S' )

// Don't allow this to be inlined to minimize stack usage of calling function.
// The calling task does printf calls which, with tokenization and ram_console,
// needs a lot of stack space.
static uint32_t threadDump(TaskStatus_t *task_status, char *task_name, char *task_state, uint32_t *backtrace_buf, ExceptionStackFrame_t *custom_exception_frame) __attribute__((noinline));
static uint32_t threadDump(TaskStatus_t *task_status, char *task_name, char *task_state, uint32_t *backtrace_buf, ExceptionStackFrame_t *custom_exception_frame)
{
    volatile fakeTCB_t *tcb = (volatile fakeTCB_t *)task_status->xHandle;
    uint32_t lr;
    uint32_t pc;
    uint32_t psr;
    uint32_t sp;
    char cStatus;

    switch (task_status->eCurrentState)
    {
    case eRunning:      cStatus = tskACTIVE_CHAR;
        break;

    case eReady:        cStatus = tskREADY_CHAR;
        break;

    case eBlocked:      cStatus = tskBLOCKED_CHAR;
        break;

    case eSuspended:    cStatus = tskSUSPENDED_CHAR;
        break;

    case eDeleted:      cStatus = tskDELETED_CHAR;
        break;

    default:            /* Should not get here, but it is included
                           to prevent static checking errors. */
        cStatus = 0x00;
        break;
    }

    if (custom_exception_frame == NULL)
    {
        lr = tcb->switched_out_stack_frame_p->pendSVFrame.lr;
        pc = tcb->switched_out_stack_frame_p->pendSVFrame.pc;
        psr = tcb->switched_out_stack_frame_p->pendSVFrame.psr;
        sp = (uint32_t)&tcb->switched_out_stack_frame_p->pendSVFrame.stack[0];
    }
    else
    {
        lr = custom_exception_frame->lr;
        pc = custom_exception_frame->pc;
        psr = custom_exception_frame->psr;
        sp = (uint32_t)&custom_exception_frame->stack[0];
    }

    if (psr & PSR_EXTRA_STACK_ALIGN_BIT)
    {
        sp += 4;
    }
    strncpy(task_name, pcTaskGetTaskName(task_status->xHandle), TASK_NAME_LEN-1);
    task_state[0] = cStatus;
    return nlbacktrace_with_lr(pc, lr, sp, get_task_stack_top(task_status->xHandle),
                                          backtrace_buf, STACK_TRACE_DEPTH);
}

// Don't allow this to be inlined to minimize stack usage of calling function
static void threadDumpLocalBuf(TaskStatus_t *task_status, ExceptionStackFrame_t *custom_exception_frame) __attribute__((noinline));
static void threadDumpLocalBuf(TaskStatus_t *task_status, ExceptionStackFrame_t *custom_exception_frame)
{
    ThreadDumpInfo_t localThreadState = {0};
    uint32_t backtrace_count = threadDump(task_status,
                                          localThreadState.name,
                                          &localThreadState.name[sizeof(localThreadState.name)-1],
                                          localThreadState.stackTrace,
                                          custom_exception_frame);
    faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("Task: %.*s%c\n"),
                    sizeof(localThreadState.name), localThreadState.name, localThreadState.name[sizeof(localThreadState.name)-1]);
    for (unsigned i = 0; i < backtrace_count; i++)
    {
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("  0x%08lx\n"), localThreadState.stackTrace[i]);
    }
}

void dump_all_tasks_default(bool task_fault);
void dump_all_tasks_default(bool task_fault)
{

    // Get task handles into statusArray, then set their statuses,
    // get task and backtrace information using threadDump(), and
    // then print that info
    static TaskStatus_t statusArray[THREAD_DUMP_MAX_NUM_THREADS];
    unsigned backtrace_idx;
    xTaskHandle active_task_handle = xTaskGetCurrentTaskHandle();

    faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("All tasks\n"));

#if defined(BUILD_FEATURE_RESET_INFO)
    bool save_fault_to_reset_info;
#ifdef BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM
    // g_reset_info is just a cache so we always save to it and
    // let the subsequent boot decide whether to move the RAM
    // copy to the external FLASH backing store
    save_fault_to_reset_info = true;
#else // BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM
    // g_reset_info is our backing store since it is dedicated
    // retained RAM. only save this fault info if the any previous
    // fault has been cleared.
    if ((g_reset_info.fault_info.reason == NL_RESET_REASON_UNKNOWN) ||
        (g_reset_info.fault_info.reason == NL_RESET_REASON_ASSERT))
    {
        save_fault_to_reset_info = true;
    }
    else
    {
        save_fault_to_reset_info = false;
    }
#endif // BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM

    if (save_fault_to_reset_info)
    {
        if (!task_fault)
        {
            strcpy(g_reset_info.fault_info.active_task_name, "N/A");
            backtrace_idx = 0;
        }
        else
        {
            // Current task always occupies index 0 of fault_info struct. It has already been populated by backtrace dump
            // routine.  Populate the name field and skip copying the backtrace.
            memset(&g_reset_info.fault_info.active_task_name, 0, sizeof(g_reset_info.fault_info.active_task_name));
            memset(&g_reset_info.fault_info.task_info[0].task_name, 0, sizeof(g_reset_info.fault_info.task_info[0].task_name));
            strncpy(g_reset_info.fault_info.active_task_name, pcTaskGetTaskName(active_task_handle), NL_FAULT_DIAGS_TASK_NAME_LEN - 1);
            strncpy(g_reset_info.fault_info.task_info[0].task_name, pcTaskGetTaskName(active_task_handle), NL_FAULT_DIAGS_TASK_NAME_LEN - 1);
            g_reset_info.fault_info.task_info[0].task_state[0] = tskREADY_CHAR;
            backtrace_idx = 1;
        }
    }
#endif // BUILD_FEATURE_RESET_INFO

    // Note that uxTaskGetNumberOfTasks() might not return the same
    // count as uxTaskGetSystemStateFromFault() if a fault occurred in
    // FreeRTOS when the current task was being removed from the
    // ready list but before it was placed on a blocked list.  In
    // this special case, uxTaskGetNumberOfTasks() would return one
    // more than the value uxTaskGetSystemStateFromFault() returns.
    unsigned numTasks = (unsigned) uxTaskGetSystemStateFromFault(statusArray, THREAD_DUMP_MAX_NUM_THREADS, NULL);

    for (unsigned idx = 0; idx < numTasks; idx++) {
        ExceptionStackFrame_t *active_task_psp = NULL;

        /* The information about the current task was dumped from the
         * exception context.  The FreeRTOS TCB context will not be valid
         * and trying to dump from it might lead to double faults because
         * the values from the TCB stack top won't be valid.
         */
        if (!task_fault && active_task_handle == statusArray[idx].xHandle)
        {
            /* This is a fault on the machine stack, and we are trying to dump the active task when the
             * interrupt or exception occurred.  We want to get the stack pointer from the PSP, and get the
             * PC and LR from the exception frame dumped on the machine stack
             */
            active_task_psp = (ExceptionStackFrame_t*)nlplatform_get_psp();
        }
        else if (statusArray[idx].xHandle == active_task_handle)
        {
            /* This is a fault on a task stack, and this is the TCB of that task. We have already dumped out
             * the backtrace so just skip this one.
             */
            continue;
        }

#if defined(BUILD_FEATURE_RESET_INFO)
        if (save_fault_to_reset_info)
        {
            memset(&g_reset_info.fault_info.task_info[backtrace_idx], 0, sizeof(g_reset_info.fault_info.task_info[backtrace_idx]));
            uint32_t backtrace_count = threadDump(&statusArray[idx],
                                                  g_reset_info.fault_info.task_info[backtrace_idx].task_name,
                                                  g_reset_info.fault_info.task_info[backtrace_idx].task_state,
                                                  g_reset_info.fault_info.task_info[backtrace_idx].backtrace,
                                                  active_task_psp);
            // print outside of thread_dump() to give faultDebugPrintf() as much free stack as possible.
            // tokenized logs w/ ram_console takes over 200 bytes of stack!
            faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("Task: %.*s%c\n"),
                            sizeof(g_reset_info.fault_info.task_info[backtrace_idx].task_name),
                            g_reset_info.fault_info.task_info[backtrace_idx].task_name,
                            g_reset_info.fault_info.task_info[backtrace_idx].task_state[0]);
            for (unsigned i = 0; i < backtrace_count; i++)
            {
                faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("  0x%08lx\n"),
                                g_reset_info.fault_info.task_info[backtrace_idx].backtrace[i]);
            }
        }
        else
        {
            threadDumpLocalBuf(&statusArray[idx], active_task_psp);
        }
#else
        threadDumpLocalBuf(&statusArray[idx], active_task_psp);
#endif
        backtrace_idx++;
    }
}

#endif /* defined(BUILD_FEATURE_FAULT_DUMP_TASK_STACKS) */

#ifdef BUILD_FEATURE_BREADCRUMBS
/* Use linker script to replace crash_dump_breadcrumbs with
 * a stub function if no breadcrumbs from faults is desired.
 */
void crash_dump_breadcrumbs(const uint32_t *backtrace, size_t num_backtrace_entries, const char * current_task_name, bool dump_all_tasks) LINKER_REPLACEABLE_FUNCTION(nltransfer_fault_to_breadcrumbs);

void nltransfer_fault_to_breadcrumbs(const uint32_t *backtrace, size_t num_backtrace_entries, const char * current_task_name, bool dump_all_tasks)
{
    ThreadDumpInfo_t faultState = {0};

    faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("breadcrumbs'ing\n"));

    faultState.stackDepth = num_backtrace_entries;
    memcpy(faultState.stackTrace, backtrace, MIN(num_backtrace_entries * sizeof(uint32_t), sizeof(faultState.stackTrace)));

    if (current_task_name != NULL)
    {
        memcpy(faultState.name, current_task_name, sizeof(faultState.name));
    }

    /* This is a macro so we can't alias it directly in linker script */
    nlBREADCRUMBS_crash_dump(&faultState, sizeof(faultState));
#if defined(BUILD_FEATURE_FAULT_DUMP_TASK_STACKS) && (tskKERNEL_VERSION_MAJOR < 8)
    if (dump_all_tasks)
    {
        for (int i = 0; i < threadNum && i < THREAD_DUMP_MAX_NUM_THREADS; i++)
        {
            nlBREADCRUMBS_crash_dump(&threadState[i], sizeof(ThreadDumpInfo_t));
        }
    }
#endif
}
#endif /* BUILD_FEATURE_BREADCRUMBS */

#ifdef BUILD_FEATURE_RESET_INFO
uint32_t *dump_context_to_reset_info(uint32_t *faultingStackAddress , uint32_t *machineStackAddress, nl_reset_reason_t reset_reason, uint32_t prefault_sp, const char *current_task_name, bool task_fault) LINKER_REPLACEABLE_FUNCTION(dump_context_to_reset_info_default);
void dump_stack_overflow_info_to_reset_info(const char *current_task_name) LINKER_REPLACEABLE_FUNCTION(dump_stack_overflow_info_to_reset_info_default);

static size_t append_string_to_fault_info_description(size_t index, const char *src)
{
    while (index < sizeof(g_reset_info.fault_info.description) - 1)
    {
        g_reset_info.fault_info.description[index++] = *src++;
        if (*src == '\0')
        {
            break;
        }
    }
    return index;
}

void dump_stack_overflow_info_to_reset_info_default(const char *current_task_name);
void dump_stack_overflow_info_to_reset_info_default(const char *current_task_name)
{
    size_t i = append_string_to_fault_info_description(0, "stack_overflow");
    if (current_task_name)
    {
        // append the task name if there is one.  don't
        // use snprintf because it bloats bootloaders too much.
        i = append_string_to_fault_info_description(i, ": ");
        i = append_string_to_fault_info_description(i, current_task_name);
    }
    // guarantee null termination
    g_reset_info.fault_info.description[i] = '\0';
}

uint32_t *dump_context_to_reset_info_default(uint32_t *faultingStackAddress , uint32_t *machineStackAddress, nl_reset_reason_t reset_reason, uint32_t prefault_sp, const char *current_task_name, bool task_fault);
uint32_t *dump_context_to_reset_info_default(uint32_t *faultingStackAddress , uint32_t *machineStackAddress, nl_reset_reason_t reset_reason, uint32_t prefault_sp, const char *current_task_name, bool task_fault)
{
    ExceptionStackFrame_t *faultFrame = (ExceptionStackFrame_t*) faultingStackAddress;
    bool save_fault_to_reset_info;
    uint32_t *backtrace_buf;

#ifndef BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM
    // g_reset_info is our backing store since it is dedicated
    // retained RAM. only save this fault info if the any previous
    // fault has been cleared.
    if ((g_reset_info.fault_info.reason == NL_RESET_REASON_UNKNOWN) ||
        (g_reset_info.fault_info.reason == NL_RESET_REASON_ASSERT))
    {
#endif // BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM
        save_fault_to_reset_info = true;
        // we directly use the g_reset_info.fault_info.task_info[0] buffer
        // when we generate the backtrace so we don't need to save it
        // again
        if (task_fault)
        {
            backtrace_buf = g_reset_info.fault_info.task_info[0].backtrace;
            memset(&g_reset_info.fault_info.task_info[0], 0, sizeof(g_reset_info.fault_info.task_info[0]));
        }
        else
        {
            backtrace_buf = g_reset_info.fault_info.machine_backtrace;
        }
        memset(&g_reset_info.fault_info.machine_backtrace, 0, sizeof(g_reset_info.fault_info.machine_backtrace));
        // If this is an assert, this API will return NL_RESET_REASON_ASSERT.  If it is a hard fault, it will read unknown
        if (nl_reset_info_get_reset_reason() == NL_RESET_REASON_UNKNOWN)
        {
            memset(g_reset_info.fault_info.description, 0, sizeof(g_reset_info.fault_info.description));
        }
#ifndef BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM
    }
    else
    {
        save_fault_to_reset_info = false;
        // fault_info isn't clean, so generate backtrace to
        // static buffer and don't save fault_info.
        backtrace_buf = s_backtrace;
    }
#endif // BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM

    if (save_fault_to_reset_info)
    {
        // save r0-r3
        memcpy((void*)&g_reset_info.fault_info.registers, (void*)faultingStackAddress,
               NUM_LOW_REGISTERS_ON_PROCESS_STACK * sizeof(uint32_t));
        // save r4-r11
        memcpy((void*)&g_reset_info.fault_info.registers[4], machineStackAddress,
               NUM_REGISTERS_ON_MAIN_STACK * sizeof(uint32_t));
        // save r12
        g_reset_info.fault_info.registers[12] = faultFrame->r12;
        // save psp (r13)
        g_reset_info.fault_info.registers[13] = prefault_sp;
        // save lr (r14), pc (r15), psr
        g_reset_info.fault_info.registers[14] = faultFrame->lr;
        g_reset_info.fault_info.registers[15] = faultFrame->pc;
        g_reset_info.fault_info.registers[16] = faultFrame->psr;

        if (reset_reason == NL_RESET_REASON_WATCHDOG)
        {
            dump_watchdog_flags();
        }
        else if (reset_reason == NL_RESET_REASON_STACK_OVERFLOW)
        {
            dump_stack_overflow_info_to_reset_info(current_task_name);
        }
    }
    return backtrace_buf;
}
#endif /* BUILD_FEATURE_RESET_INFO */

void dump_context_default(uint32_t *faultingStackAddress , uint32_t *machineStackAddress, crashType type, uint32_t prefault_sp);
void dump_context_default(uint32_t *faultingStackAddress , uint32_t *machineStackAddress, crashType type, uint32_t prefault_sp)
{
    unsigned i;

    if (type == FAULT)
    {
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("Oops!\n"));
    }
    else if (type == WDT)
    {
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("!!wdog!!\n"));
    }

#if defined(BUILD_FEATURE_RAM_CONSOLE)
    // Remaining fault dump does not go into RAM console because it goes to
    // service via an event.  Allow above print to go to RAM console, so that
    // RAM console indicates that a fault has occured.
    nl_ram_console_disable();
#endif

    // register dump: r0-r3
    for (i = 0; i < NUM_LOW_REGISTERS_ON_PROCESS_STACK; i++)
    {
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("r%d\t0x%08lx\n"), i, faultingStackAddress[i]);
    }

    // register dump: r4-r11
    for (i = 0; i < NUM_REGISTERS_ON_MAIN_STACK; i++)
    {
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("r%d\t0x%08lx\n"), i+4, machineStackAddress[i]);
    }

    // register dump: r12, lr, pc, psr
#ifdef DEBUG
    const char* names[] = { "r12", " lr", " pc", "psr" };
#endif
    for (i = 0; i < NUM_HIGH_REGISTERS_ON_PROCESS_STACK; i++)
    {
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("%s\t0x%08lx\n"), names[i], faultingStackAddress[i+NUM_LOW_REGISTERS_ON_PROCESS_STACK]);
    }
    // register dump: psp.
    faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("psp\t0x%08lx\n"), prefault_sp);

    nlplatform_soc_dump_context();
    nlproduct_dump_context();

    // stack dump
    faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("- stack -\n"));
    for ( i = 0; i < NUM_STACK_DUMP_ENTRIES; i++ )
    {
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("%08lx\n"), *(((uint32_t*)(prefault_sp)) + i));
    }
}

void dump_backtrace_default(ExceptionStackFrame_t *faultFrame, uint32_t *backtrace_buf, unsigned *backtrace_count,
                            uint32_t prefault_sp, uint32_t stackTop, nl_reset_reason_t reset_reason);
void dump_backtrace_default(ExceptionStackFrame_t *faultFrame, uint32_t *backtrace_buf, unsigned *backtrace_count,
                            uint32_t prefault_sp, uint32_t stackTop, nl_reset_reason_t reset_reason)
{
    int size;
    size_t i;

    nlwatchdog_refresh();
    size = nlbacktrace_with_lr(faultFrame->pc, faultFrame->lr, prefault_sp, stackTop, backtrace_buf, NL_FAULT_DIAGS_NUM_BT_ENTRIES);
    // If the nlbacktrace_with_lr() returned no backtrace
    // then use nlbacktrace_no_context() to get a longer backtrace.
    if (size == 0)
    {
        backtrace_buf[0] = faultFrame->pc;
        backtrace_buf[1] = faultFrame->lr;
        nlwatchdog_refresh();
        size = nlbacktrace_no_context(prefault_sp, stackTop, backtrace_buf+2, NL_FAULT_DIAGS_NUM_BT_ENTRIES-2, 3, 16) + 2;
    }
    *backtrace_count = size;

    nlwatchdog_refresh();
    for (i = 0; i < size; i++)
    {
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("  0x%08lx\n"), backtrace_buf[i]);
    }
}

void get_and_dump_current_task_info_default(const char **name, nl_reset_reason_t *reset_reason, uint32_t *stackTop);
void get_and_dump_current_task_info_default(const char **name, nl_reset_reason_t *reset_reason, uint32_t *stackTop)
{
    const char *current_task_name;
    xTaskHandle handle = xTaskGetCurrentTaskHandle();
    current_task_name = (const char*)pcTaskGetTaskName(handle);
    faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("Task: %sR\n"), current_task_name);
    *name = current_task_name;

    if (handle)
    {
        *stackTop = get_task_stack_top(handle);
    }
}

void dump_watchdog_flags_default(void);
void dump_watchdog_flags_default(void)
{
#if defined(BUILD_FEATURE_RESET_INFO)
    // save watchdog flags to the description
    nlwatchdog_log_flags((char*)&g_reset_info.fault_info.description,
                         sizeof(g_reset_info.fault_info.description));
#endif

    // print the watchdog flags to the console
    nlwatchdog_print_flags();
}

void crash_dump_default( uint32_t *faultingStackAddress , uint32_t *machineStackAddress, crashType type);
void crash_dump_default( uint32_t *faultingStackAddress , uint32_t *machineStackAddress, crashType type)
{
    ExceptionStackFrame_t *faultFrame = (ExceptionStackFrame_t*) faultingStackAddress;
    uint32_t *backtrace_buf;
    unsigned backtrace_count = 0;
    nl_reset_reason_t reset_reason;
    const char *current_task_name = NULL;
    uint32_t stackTop = 0;
#if !defined(BUILD_CONFIG_RELEASE) || defined(BUILD_FEATURE_RESET_INFO) || defined(BUILD_FEATURE_FAULT_DUMP_TASK_STACKS)
    // check if it's a fault on the main stack or a process (task) stack fault
    bool task_fault = (faultingStackAddress == machineStackAddress + 10) ? false : true;
#endif

    uint32_t prefault_sp  = (uint32_t)&faultFrame->stack[0];
    if (faultFrame->psr & PSR_EXTRA_STACK_ALIGN_BIT)
    {
        prefault_sp += 4;
    }

    if (type < ARRAY_SIZE(kCrashTypeToResetReason))
    {
        reset_reason = kCrashTypeToResetReason[type];
    }
    else
    {
        reset_reason = NL_RESET_REASON_HARD_FAULT; // if all else fails
    }

#if BUILD_FEATURE_PRE_WATCHDOG_ISR_EXTENSION
    if ((type == WDT) && (nlwatchdog_ignore_pre_watchdog_isr() == true))
    {
        goto done;
    }
#else
    // pet the watchdog so we have enough time to hopefully do the dump
    nlwatchdog_refresh();
#endif

    // quiesce system.  this also could be used to disable watchdog if desired.
    nlplatform_quiesce_on_fault();

    dump_context(faultingStackAddress, machineStackAddress, type, prefault_sp);

#if !defined(BUILD_CONFIG_RELEASE)
    if (task_fault)
    {
        get_and_dump_current_task_info(&current_task_name, &reset_reason, &stackTop);
    }
    else
    {
        /* To get the end of the Main stack, we expect a linker script to
         * provide the symbol "_eusrstack".  The linker script might have
         * other symbols with the same value, but this file will expect
         * all products to export a _eusrstack symbol.
         */
        extern const uint32_t _eusrstack; /* linker symbol for end of MSP stack */
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("- Fault on main stack (ISR?)) -\n"));
        stackTop = (uint32_t)&_eusrstack;
    }
#endif

#ifdef BUILD_FEATURE_RESET_INFO
    // Copy R0-R12, sp, LR, PC, xPSR
    backtrace_buf = dump_context_to_reset_info(faultingStackAddress, machineStackAddress, reset_reason, prefault_sp, current_task_name, task_fault);
#else // BUILD_FEATURE_RESET_INFO
    backtrace_buf = s_backtrace;
#endif // BUILD_FEATURE_RESET_INFO

    // dump backtrace of current context
    dump_backtrace(faultFrame, backtrace_buf, &backtrace_count, prefault_sp, stackTop, reset_reason);

    //all thread backtrace
#if defined(BUILD_FEATURE_FAULT_DUMP_TASK_STACKS)
    dump_all_tasks(task_fault);
#endif

#ifdef BUILD_FEATURE_BREADCRUMBS
    //writing breadcrumb
    //fault breadcrumbs goes here:
    crash_dump_breadcrumbs(backtrace_buf, backtrace_count, current_task_name, type == WDT);
#endif

#if defined(BUILD_CONFIG_RELEASE)
    // always reset in release builds
    faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("resetting\n"));
    nlplatform_reset(reset_reason);
#else /* BUILD_CONFIG_RELEASE */

#if defined(BUILD_FEATURE_NO_BKPT_ON_FAULT)
    if (0) { }
#else
    // cortex-m0 does not (may not?) have cpu accessible CoreDebug registers
    // so use a SOC specific API to determine if debugger is connected
    if (nlplatform_debugger_is_attached()) {
        // if a debugger is connected, this should trigger a breakpoint.
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("breaking\n"));
        __asm__ __volatile__("bkpt #0");
        // just single step in the debugger to restore the frame that
        // caused the fault to see what happened.
    }
#endif

    else {
#if defined(BUILD_FEATURE_RESET_ON_FAULT)
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("resetting\n"));
        nlplatform_reset(reset_reason);
#else
#if defined(BUILD_FEATURE_RESET_INFO)
        // when no debugger is connected, spin.  if watchdog isn't
        // disabled, it will cause a reset.  otherwise the spinning
        // will allow connecting a debugger later.
        nl_reset_info_prepare_reset(reset_reason, NULL);
#endif

        while (1);
#endif // defined(BUILD_FEATURE_RESET_ON_FAULT)
    }
#endif // defined(BUILD_CONFIG_RELEASE)
#if BUILD_FEATURE_PRE_WATCHDOG_ISR_EXTENSION
done:
    return;
#endif
}

/* no freertos linking, alias crash_dump to this function
 * to dead strip the rtos using version and it's symbols.
 * not static to avoid unused function warnings.
 */
void crash_dump_nortos( uint32_t *faultingStackAddress , uint32_t *machineStackAddress, crashType type);
void crash_dump_nortos( uint32_t *faultingStackAddress , uint32_t *machineStackAddress, crashType type)
{
    nl_reset_reason_t reset_reason = (type == WDT) ? NL_RESET_REASON_WATCHDOG : NL_RESET_REASON_HARD_FAULT;
    ExceptionStackFrame_t *faultFrame = (ExceptionStackFrame_t*) faultingStackAddress;
    uint32_t prefault_sp  = (uint32_t)&faultFrame->stack[0];
    if (faultFrame->psr & PSR_EXTRA_STACK_ALIGN_BIT)
    {
        prefault_sp += 4;
    }

#if BUILD_FEATURE_PRE_WATCHDOG_ISR_EXTENSION
    if ((type == WDT) && (nlwatchdog_ignore_pre_watchdog_isr() == true))
    {
        goto done;
    }
#else
    // pet the watchdog so we have enough time to hopefully do the dump
    nlwatchdog_refresh();
#endif

    dump_context(faultingStackAddress, machineStackAddress, type, prefault_sp);

#if defined(BUILD_FEATURE_NO_BKPT_ON_FAULT)
    if (0) { }
#else
    // cortex-m0 does not (may not?) have cpu accessible CoreDebug registers
    // so use a SOC specific API to determine if debugger is connected
    if (nlplatform_debugger_is_attached()) {
        // if a debugger is connected, this should trigger a breakpoint.
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("breaking\n"));
        __asm__ __volatile__("bkpt #0");
        // just single step in the debugger to restore the frame that
        // caused the fault to see what happened.
    }
#endif

    else {
        // when no debugger is connected, we reset to not spin consuming
        // our battery needlessly
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("resetting\n"));
        nlplatform_reset(reset_reason);
    }

#if BUILD_FEATURE_PRE_WATCHDOG_ISR_EXTENSION
done:
    return;
#endif
}

/* Use this as the FreeRTOS vApplicationStackOverflowHook to dump backtrace
 * about the task that overflowed it's stack.
 */
void nlfault_freertos_stack_overflow_handler_c(xTaskHandle pxTask, signed char *pcTaskName);
void nlfault_freertos_stack_overflow_handler_c(xTaskHandle pxTask, signed char *pcTaskName)
{
    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */
    switched_out_stack_frame_t *stack_frame_p;
    uint32_t *backtrace_buf;
    uint32_t top_of_stack;
    size_t num_backtraces;
    uint32_t prefault_sp;
    uint32_t stackTop = get_task_stack_top(pxTask);

    taskDISABLE_INTERRUPTS();

    // pet the watchdog so we have enough time to hopefully do the dump
    nlwatchdog_refresh();

    // quiesce system.  this also could be used to disable watchdog if desired.
    nlplatform_quiesce_on_fault();

    faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("\nSTACK_OVERFLOW_DETECTED\n"));
    faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("%s\n"), (const char *)pcTaskName);

    /* FreeRTOS doesn't give an API for getting the top of the stack
     * but we know it's the first entry in the TCB pointed to by pxTask.
     */
    top_of_stack = (((uint32_t*)pxTask)[0]);
    stack_frame_p = (switched_out_stack_frame_t*)top_of_stack;

    prefault_sp  = (uint32_t)&stack_frame_p->pendSVFrame.stack[0];
    if (stack_frame_p->pendSVFrame.psr & PSR_EXTRA_STACK_ALIGN_BIT)
    {
        prefault_sp += 4;
    }

#ifdef BUILD_FEATURE_RESET_INFO
    bool save_fault_to_reset_info;
#ifdef BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM
    // g_reset_info is just a cache so we always save to it and
    // let the subsequent boot decide whether to move the RAM
    // copy to the external FLASH backing store
    save_fault_to_reset_info = true;
    // we directly use the g_reset_info.fault_info.task_info[0] buffer
    // when we generate the backtrace so we don't need to save it
    // again
    backtrace_buf = g_reset_info.fault_info.task_info[0].backtrace;
    memset(&g_reset_info.fault_info.task_info[0], 0, sizeof(g_reset_info.fault_info.task_info[0]));
#else // BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM
    // g_reset_info is our backing store since it is dedicated
    // retained RAM. only save this fault info if the any previous
    // fault has been cleared.
    if ((g_reset_info.fault_info.reason == NL_RESET_REASON_UNKNOWN) ||
        (g_reset_info.fault_info.reason == NL_RESET_REASON_ASSERT))
    {
        save_fault_to_reset_info = true;
        // directly use the g_reset_info.fault_info.task_info[0].backtrace buffer
        // so no need to copy it
        backtrace_buf = g_reset_info.fault_info.task_info[0].backtrace;
        memset(&g_reset_info.fault_info.task_info[0], 0, sizeof(g_reset_info.fault_info.task_info[0]));
    }
    else
    {
        save_fault_to_reset_info = false;
        // fault_info isn't clean, so generate backtrace to
        // static buffer and don't save fault_info.
        backtrace_buf = s_backtrace;
    }
#endif // BUILD_FEATURE_RESET_INFO_IN_TEMP_RAM
    if (save_fault_to_reset_info)
    {
        // save r0-r3
        g_reset_info.fault_info.registers[0] = stack_frame_p->pendSVFrame.r0;
        g_reset_info.fault_info.registers[1] = stack_frame_p->pendSVFrame.r1;
        g_reset_info.fault_info.registers[2] = stack_frame_p->pendSVFrame.r2;
        g_reset_info.fault_info.registers[3] = stack_frame_p->pendSVFrame.r3;

        // save r4-r11
        g_reset_info.fault_info.registers[4] = stack_frame_p->r4;
        g_reset_info.fault_info.registers[5] = stack_frame_p->r5;
        g_reset_info.fault_info.registers[6] = stack_frame_p->r6;
        g_reset_info.fault_info.registers[7] = stack_frame_p->r7;
        g_reset_info.fault_info.registers[8] = stack_frame_p->r8;
        g_reset_info.fault_info.registers[9] = stack_frame_p->r9;
        g_reset_info.fault_info.registers[10] = stack_frame_p->r10;
        g_reset_info.fault_info.registers[11] = stack_frame_p->r11;

        // save r12
        g_reset_info.fault_info.registers[12] = stack_frame_p->pendSVFrame.r12;
        // save psp (r13)
        g_reset_info.fault_info.registers[13] = prefault_sp;
        // save lr (r14), pc (r15), psr
        g_reset_info.fault_info.registers[14] = stack_frame_p->pendSVFrame.lr;
        g_reset_info.fault_info.registers[15] = stack_frame_p->pendSVFrame.pc;
        g_reset_info.fault_info.registers[16] = stack_frame_p->pendSVFrame.psr;

        snprintf(g_reset_info.fault_info.description,
                 sizeof(g_reset_info.fault_info.description),
                 UNIQUE_STRING_LITERAL("%s stack overflow"), pcTaskName);
    }
#else // BUILD_FEATURE_RESET_INFO
    backtrace_buf = s_backtrace;
#endif // BUILD_FEATURE_RESET_INFO
    num_backtraces = nlbacktrace_with_lr(stack_frame_p->pendSVFrame.pc,
                                         stack_frame_p->pendSVFrame.lr,
                                         prefault_sp, stackTop,
                                         backtrace_buf,
                                         NL_FAULT_DIAGS_NUM_BT_ENTRIES);
    if (num_backtraces < MIN_BACKTRACE_LEVELS_FOR_STACK_OVERFLOW)
    {
        // nlbacktrace_with_lr didn't find anything useful.  since we
        // have a stack overflow, try alternative method
        num_backtraces = nlbacktrace_no_context(prefault_sp, stackTop, backtrace_buf + 2,
                                                NL_FAULT_DIAGS_NUM_BT_ENTRIES - 2,
                                                MIN_BACKTRACE_LEVELS_FOR_STACK_OVERFLOW,
                                                16); /* max LRs to test from stack */
        backtrace_buf[0] = stack_frame_p->pendSVFrame.pc;
        backtrace_buf[1] = stack_frame_p->pendSVFrame.lr;
    }
#if !defined(BUILD_FEATURE_RESET_INFO)
    for (unsigned i = 0; i < num_backtraces; i++)
    {
        faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("  0x%08lx\n"), backtrace_buf[i]);
    }
#endif
    faultDebugPrint(UNIQUE_PRINTF_FORMAT_STRING("resetting\n"));
    nlplatform_reset(NL_RESET_REASON_STACK_OVERFLOW);
}
