/*
 *
 *    Copyright (c) 2015-2018 Nest Labs, Inc.
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
 *      This file defines the a mechanism for saving SW reset
 *      reasons and fault information (if reset was due to a fault).
 *      HW reset reasons are not currently included (separate API
 *      may exist in an architectural specific way) though we may want
 *      add this in the future.
 */

#ifndef __NLRESET_INFO_H_INCLUDED__
#define __NLRESET_INFO_H_INCLUDED__

#include <stdint.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The Phoenix schema enum
 * Nest::Trait::Firmware::FirmwareTrait::ResetType
 * must agree with this enum.
 */
typedef enum {
    NL_RESET_REASON_UNSPECIFIED     = 0,
    NL_RESET_REASON_UNKNOWN,
    NL_RESET_REASON_SW_REQUESTED,
    NL_RESET_REASON_SW_UPDATE,
    NL_RESET_REASON_FACTORY_RESET,
    NL_RESET_REASON_COUNT,

    NL_RESET_REASON_FIRST_FAULT     = NL_RESET_REASON_COUNT, /* faults have interesting data in
                                                              * fault_metadata part of .resetinfo
                                                              */
    NL_RESET_REASON_HARD_FAULT      = NL_RESET_REASON_FIRST_FAULT,
    NL_RESET_REASON_ASSERT,
    NL_RESET_REASON_WATCHDOG,
    NL_RESET_REASON_STACK_OVERFLOW,
    NL_RESET_REASON_LAST_FAULT      = NL_RESET_REASON_STACK_OVERFLOW,

    NL_RESET_REASON_FAULT_COUNT     = NL_RESET_REASON_LAST_FAULT - NL_RESET_REASON_FIRST_FAULT + 1
} nl_reset_reason_t;

#ifdef __cplusplus
}
#endif

#include <nlproduct_config.h>

#ifdef __cplusplus
extern "C" {
#endif

/* nlfault.h references nl_reset_reason_t */
#ifndef NL_FAULT_DIAGS_NUM_CONTEXT_REGISTERS
/* Default number of register to save for a fault.
 * For a CortexM3, these consist of the registers (in order):
 *      r0-r12, sp, pc, lr, xpsr
 */
#define NL_FAULT_DIAGS_NUM_CONTEXT_REGISTERS    17
#endif

#ifndef NL_FAULT_DIAGS_NUM_BT_ENTRIES
#define NL_FAULT_DIAGS_NUM_BT_ENTRIES           10
#endif

#ifndef NL_FAULT_DIAGS_DESCRIPTION_LENGTH
#define NL_FAULT_DIAGS_DESCRIPTION_LENGTH       32
#endif

#ifndef NL_FAULT_DIAGS_TASK_NAME_LEN
#define NL_FAULT_DIAGS_TASK_NAME_LEN            4
#endif

#ifndef NL_FAULT_DIAGS_TASK_STATE_LEN
#define NL_FAULT_DIAGS_TASK_STATE_LEN           4
#endif

#ifndef NL_FAULT_DIAGS_MAX_NUM_TASKS
#define NL_FAULT_DIAGS_MAX_NUM_TASKS            8
#endif

#define NL_RESET_INFO_MAGIC   0x1234abcd

#define IS_VALID_RESET_REASON(reason) ((reason) < NL_RESET_REASON_COUNT)
#define IS_VALID_FAULT_REASON(reason) (((reason) >= NL_RESET_REASON_FIRST_FAULT) && \
                                       ((reason) <= NL_RESET_REASON_LAST_FAULT))

typedef struct {
    char        task_name[NL_FAULT_DIAGS_TASK_NAME_LEN];
    uint32_t    backtrace[NL_FAULT_DIAGS_NUM_BT_ENTRIES];
    char        task_state[NL_FAULT_DIAGS_TASK_STATE_LEN];
} nl_fault_task_info_t;

typedef uint32_t nl_fault_registers_t[NL_FAULT_DIAGS_NUM_CONTEXT_REGISTERS];

typedef char nl_fault_description_t[NL_FAULT_DIAGS_DESCRIPTION_LENGTH];

typedef struct {
    uint32_t                reason;
    nl_fault_registers_t    registers;
    char                    active_task_name[NL_FAULT_DIAGS_TASK_NAME_LEN];
    nl_fault_description_t  description;
    uint32_t                machine_backtrace[NL_FAULT_DIAGS_NUM_BT_ENTRIES];
    nl_fault_task_info_t    task_info[NL_FAULT_DIAGS_MAX_NUM_TASKS];
} nl_fault_info_t;

typedef struct {
    uint32_t                magic;
    uint32_t                reset_reason;
    nl_fault_info_t         fault_info;
} nl_reset_info_t;

extern nl_reset_info_t g_reset_info;

void nl_reset_info_init(void);
/* Sets the reset reason if it has not already been set.
 * Also sets the fault_description (if not NULL)
 * and there was no previously uncleared fault.
 * If the implementation is using temp/overlaid RAM, disables task
 * switching to reduce likelihood of conflicting usage of the same RAM
 * addresses.
 */
void nl_reset_info_prepare_reset(nl_reset_reason_t reset_reason, const char *fault_description);
/* Similar to nl_reset_info_prepare_reset() except
 * that if reset_reason is a fault, an already
 * set reset reason and fault_description will be
 * replaced with this new one because a bootloader
 * fault is more important to record than a reset
 * reason from an app.
 * Typically used in bootloaders by linker
 * aliasing to replace nl_reset_info_prepare_reset()
 */
void nl_reset_info_prepare_reset_bootloader(nl_reset_reason_t reset_reason, const char *fault_description);

/* Get last fault info, if any.  Returns 0 if there was a previous fault. */
int  nl_reset_info_get_saved_fault(nl_fault_info_t *saved_fault_info);
/* Clear fault state, once we've processed the fault info */
void nl_reset_info_clear_saved_fault(void);

/* Get last reset reason. Possibly NL_RESET_REASON_UNKNOWN. */
nl_reset_reason_t nl_reset_info_get_reset_reason(void);

#ifdef DEBUG
void nl_reset_info_print(void);
void nl_reset_info_print_saved_fault(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __NLRESET_INFO_H_INCLUDED__ */
