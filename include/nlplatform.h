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
 *      This file defines generic platform API
 */

#ifndef _NLPLATFORM_H_INCLUDED__
#define _NLPLATFORM_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))
#endif
#ifndef ALIGN_TO_WORDSIZE
#define ALIGN_TO_WORDSIZE(sz) ( ((sz) + (sizeof(size_t) - 1 )) / sizeof(size_t) )
#endif

/* Special macro to put string literals into named sections so dead
 * stripping works in older versions of gcc.  This does prevent
 * merging of the same string, so it's best if these are unique.
 */
#define UNIQUE_STRING_LITERAL(str) ({ static const char __str[] = str; __str; })

/* When tokenizing, the first argument to printf must be a
 * string literal, but when not tokenizing, we want the
 * string to be named so it can be dead stripped by the
 * current version of gcc we're using (future versions don't
 * need this).
 */
#ifdef BUILD_FEATURE_PRINTF_TOKENIZATION
#define UNIQUE_PRINTF_FORMAT_STRING(str) str
#else
#define UNIQUE_PRINTF_FORMAT_STRING(str) UNIQUE_STRING_LITERAL(str)
#endif

/* When tokenizing, the format string to NL_LOG_XXX() macros
 * must be a string literal, but when not tokenizing, we want the
 * string to be named so it can be dead stripped by the
 * current version of gcc we're using (future versions don't
 * need this).
 */
#ifdef BUILD_FEATURE_LOG_TOKENIZATION
#define UNIQUE_LOG_FORMAT_STRING(str) str
#else
#define UNIQUE_LOG_FORMAT_STRING(str) UNIQUE_STRING_LITERAL(str)
#endif

typedef int (*nlloop_callback_fp)(void);

#include <nlplatform/nlreset_info.h>
#include <nlplatform_soc.h>

void nlplatform_init(void);
void nlplatform_product_init(void);
void nlplatform_reset(nl_reset_reason_t reset_reason);
void nlplatform_quiesce_on_fault(void);

/* nlplatform_block_sleep:
 * Disable / enable all forms of sleep depending on value of 'block' parameter.
 * If 'block' is true, a global counter is incremented.  If false, counter is decremented.
 * If counter is non-zero, idle hook will be skipped entirely.
 */
void nlplatform_block_sleep(bool block);
/* Block sleep for a specified number of milliseconds. */
void nlplatform_block_sleep_ms(unsigned ms);
/* Return true if sleep block counter is non-zero */
bool nlplatform_is_sleep_blocked(void);
void nlplatform_force_sleep(void);

void nlplatform_delay_ms(unsigned delay_ms);
void nlplatform_delay_us(unsigned delay_us);

void nlplatform_antenna_switch_enable(void);
void nlplatform_antenna_switch_disable(void);

void nlregulator_enable(unsigned regulator_id);
void nlregulator_disable(unsigned regulator_id);

void nlplatform_print_reset_cause(void);
void nlplatform_print_wakeup_cause(void);

int nlplatform_get_entropy(uint8_t *buf, size_t len);

int nlplatform_get_unique_id(const uint8_t **uid, size_t *len);

/* Some functions are designed to be replaced/aliased using linker scripts,
 * to allow the same library to be used in different ways without building
 * alternate versions.  Sometimes the default version is fine.  Sometimes
 * a stub is desired instead.  Use this alias to indicate such special
 * functions.
 */
#include <nlcompiler.h>
#define LINKER_REPLACEABLE_FUNCTION(default_function_name) NL_WEAK_ALIAS(default_function_name)

/* Stub functions which can be used/shared when the default is not desired.  these
 * return the value specified in the name.
 */
void void_stub_function(void);
int zero_stub_function(void);    /* returns 0 */
int einval_stub_function(void);  /* returns -EINVAL */
/* This stub causes an assertion failure because it's a stub
 * that is never expected to be called, so if it is, the
 * fault will help diagnose why.
 */
void fault_stub_function(void);
const char *emptystring_stub_function(void);

#ifdef __cplusplus
}
#endif

#endif /*_NLPLATFORM_H_INCLUDED__ */
