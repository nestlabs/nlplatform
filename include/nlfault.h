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
 *      This file defines the structures saved to RAM on a hard fault.
 *      It also declares the API for handling crashes (used as
 *      fault vectors handlers or callable from other ISRs).
 */

#ifndef __NLFAULT_H_INCLUDED__
#define __NLFAULT_H_INCLUDED__

#include <nlplatform.h>
#include <stdint.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

// Exception stack layout per
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0552a/Babefdjc.html
typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t psr;
    uint32_t stack[0]; /* &stack[0] is the original stack address if psr bit[9] is 0
                        * &stack[1] is the original stack address if psr bit[9] is 1
                        */
} ExceptionStackFrame_t;

/* Can be used to hook into fault preservation framework when we assert */
void nl_platform_assert_delegate(const char* file, unsigned line);

/* Fault vector handlers */
void nlfault_hard_fault_handler_c(void);
void nlfault_usage_fault_handler_c(void);
void nlfault_pre_watchdog_handler_c(void);
void nlfault_debug_monitor_handler_c(void);

/* Function to dump callstack to console  */
void nlfault_dump_callstack(void);

#ifdef BUILD_FEATURE_BREADCRUMBS
/* Function to dump preserved backtrace to breadcrumbs on bootup */
void nltransfer_fault_to_breadcrumbs(const uint32_t *backtrace, size_t num_backtrace_entries, const char * current_task_name, bool dump_all_tasks);
#endif

/* Function to dump SOC-specific context information
 * Weak symbol, must be defined in nlplatform_soc repository
 */
void nlplatform_soc_dump_context(void);

/* Function to dump product-specific context information
 * Weak symbol, must be defined in product repository
 */
void nlproduct_dump_context(void);

#ifdef __cplusplus
}
#endif

#endif /* __NLFAULT_H_INCLUDED__ */
