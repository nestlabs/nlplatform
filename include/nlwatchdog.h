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
 *      This file defines API for HW watchdog
 *
 */
#ifndef __NLWATCHDOG_H_INCLUDED__
#define __NLWATCHDOG_H_INCLUDED__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void nlwatchdog_init(void);
void nlwatchdog_set_enable(bool enable);
bool nlwatchdog_is_enabled(void);
void nlwatchdog_refresh(void);
void nlwatchdog_test(void);
void nlwatchdog_stop(void);
bool nlwatchdog_is_sleep_safe(void);
int  nlwatchdog_allocate_id(void);
void nlwatchdog_enable_tracking(bool enable, unsigned id);
void nlwatchdog_refresh_flag(unsigned id);
void nlwatchdog_print_flags(void);
void nlwatchdog_log_flags (char *dest, int len);

#if BUILD_FEATURE_PRE_WATCHDOG_ISR_EXTENSION
// optional function to let the pre_watchdog_isr() routine
// in fault.c to ignore the pre_watchdog isr, which
// effectively extends the HW watchdog timeout
bool nlwatchdog_ignore_pre_watchdog_isr(void);
#endif

bool nlwatchdog_get_time_to_expiry(int *time_to_expiry);

#ifdef __cplusplus
}
#endif

#endif /* __NLWATCHDOG_H_INCLUDED__ */
