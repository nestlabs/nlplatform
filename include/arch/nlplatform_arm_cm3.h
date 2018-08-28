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
 *      This file defines API for cortex M3 specific
 */
#ifndef __NLPLATFORM_ARCH_CM3_H_INCLUDED__
#define __NLPLATFORM_ARCH_CM3_H_INCLUDED__

#include <stdbool.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This file is normally included by a nlplatform_soc.h file specific
 * to a processor.  That header file should set USE_INLINE_FUNCTIONS
 * to 0 or 1 before including this header.  Inline functions
 * might be faster but take more code space.
 */
#ifndef USE_INLINE_FUNCTIONS
#define USE_INLINE_FUNCTIONS 1
#endif

static inline bool nlplatform_in_interrupt(void)
{
    uint32_t __regIPSR;

    __asm__ volatile ("MRS %0, ipsr" : "=r" (__regIPSR) );
    return ((__regIPSR) ? true : false);
}

#if USE_INLINE_FUNCTIONS == 1
extern uint8_t int_lock_count;

static inline void nlplatform_interrupt_disable(void)
{
#ifdef PRODUCT_INTERRUPT_DISABLE_BASE_PRIORITY_VALUE
    __set_BASEPRI(PRODUCT_INTERRUPT_DISABLE_BASE_PRIORITY_VALUE);
#else
    __asm__ volatile ("cpsid i");
#endif
    int_lock_count++;
}

static inline void nlplatform_interrupt_enable(void)
{
    assert(int_lock_count > 0);
    int_lock_count--;
    if (int_lock_count == 0)
    {
#ifdef PRODUCT_INTERRUPT_DISABLE_BASE_PRIORITY_VALUE
        __set_BASEPRI(0);
#else
        __asm__ volatile ("cpsie i");
#endif
    }
}
#else /* USE_INLINE_FUNCTIONS */

void nlplatform_interrupt_disable(void);
void nlplatform_interrupt_enable(void);

/* When USE_INLINE_FUNCTIONS == 0, the platform.c for the soc
 * can include this file and set NLPLATFORM_INTERRUPT_FUNCTIONS
 * to get the actual code.
 */
#ifdef NLPLATFORM_INTERRUPT_FUNCTIONS
static uint8_t int_lock_count;

#ifdef PRODUCT_INTERRUPT_DISABLE_BASE_PRIORITY_VALUE
/* Functions needed by FreeRTOS. Use linker script to replace:
 *   ulPortSetInterruptMask = nlplatform_set_interrupt_mask;
 *   vPortClearInterruptMask = nlplatform_clear_interrupt_mask;
 *   vPortEnterCritical = nlplatform_interrupt_disable;
 *   vPortExitCritical = nlplatform_interrupt_enable;
 * so that there is only one int_lock_count used and one
 * priority mask for atomic priority.
 */
uint32_t nlplatform_set_interrupt_mask(void);
void nlplatform_clear_interrupt_mask(uint32_t basepri_value);

uint32_t nlplatform_set_interrupt_mask(void)
{
    uint32_t old_base_pri = __get_BASEPRI();
    __set_BASEPRI(PRODUCT_INTERRUPT_DISABLE_BASE_PRIORITY_VALUE);
    return old_base_pri;
}

void nlplatform_clear_interrupt_mask(uint32_t base_pri_value)
{
    __set_BASEPRI(base_pri_value);
}
#endif

void nlplatform_interrupt_disable(void)
{
    // If PRODUCT_INTERRUPT_DISABLE_BASE_PRIORITY_VALUE is defined,
    // set BASEPRI to that value to disable interrupts via masking
    // instead of using PRIMASK. This allows interrupts with higher
    // priority to not be masked off.
#ifdef PRODUCT_INTERRUPT_DISABLE_BASE_PRIORITY_VALUE
    __set_BASEPRI(PRODUCT_INTERRUPT_DISABLE_BASE_PRIORITY_VALUE);
#else
    __asm__ volatile ("cpsid i");
#endif
    int_lock_count++;
}

void nlplatform_interrupt_enable(void)
{
    assert(int_lock_count > 0);
    int_lock_count--;
    if (int_lock_count == 0)
    {
#ifdef PRODUCT_INTERRUPT_DISABLE_BASE_PRIORITY_VALUE
        __set_BASEPRI(0);
#else
        __asm__ volatile ("cpsie i");
#endif
    }
}
#endif /* NLPLATFORM_INTERRUPT_FUNCTIONS */

#endif /* USE_INLINE_FUNCTIONS */

/* This function must be inlined. */
static inline uint32_t nlplatform_get_lr(void)
{
    register uint32_t result;
    __asm volatile ("mov %0, lr\n"  : "=r" (result) );
    return result;
}

/* This function must be inlined. */
static inline uint32_t nlplatform_get_pc(void)
{
    register uint32_t result;
    __asm volatile ("mov %0, pc\n"  : "=r" (result) );
    return result;
}

/* This function must be inlined. */
static inline uint32_t nlplatform_get_sp(void)
{
    register uint32_t result;
    __asm volatile ("mov %0, sp\n"  : "=r" (result) );
    return result;
}

/* This function must be inlined. */
static inline uint32_t nlplatform_get_psp(void)
{
    register uint32_t result;
    __asm volatile ("mrs %0, psp\n" : "=r" (result));
    return result;
}

#ifdef __cplusplus
}
#endif

#endif /* __NLPLATFORM_ARCH_CM3_H_INCLUDED__ */
