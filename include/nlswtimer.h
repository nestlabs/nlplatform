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
 *      This file defines API for SW timers
 */
#ifndef __NLSWTIMER_H_INCLUDED__
#define __NLSWTIMER_H_INCLUDED__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#ifdef BUILD_FEATURE_SW_TIMER_USES_RTOS_TICK
#include <FreeRTOS.h>
#include <task.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* SW timer memory is provided by the caller but we keep
 * it opaque so that the implemenation can change.
 */
typedef struct
{
    uint32_t hidden[4];
} nl_swtimer_t;

/* callback function that is run in interrupt context when
 * the delay has expired. the resolution & accuracy
 * is based on the implementation but the function
 * should never be called earlier than the requested
 * delay.  the return value of the function is used
 * as a restart delay.  0 means no restart.  it's allowed
 * to call nl_swtimer_start() from the timer function instead
 * but this is more efficient, and the periodicity is more
 * accurate if the delay is returned this way (the delay
 * argument to nl_swtimer_start() might be rounded up or
 * padded to take into account implementation issues.
 */
typedef uint32_t nl_swtimer_func_t(nl_swtimer_t *timer, void *arg);

/* initialize the timer object with the function and arg
 * to invoke when the timer delay expires
 */
void nl_swtimer_init(nl_swtimer_t *timer, nl_swtimer_func_t *func, void *arg);

/* start a timer so that it's function is invoked when the specified
 * delay expires.  the delay is time in milliseconds from the current
 * time.  it is an error (asserted in debug) if the timer is already
 * running.
 */
void nl_swtimer_start(nl_swtimer_t *timer, uint32_t delay_ms);

/* returns true if the timer was active and the cancel
 * removed it.  returns false if the timer was not active
 * (it might have already run, or was never started).
 */
bool nl_swtimer_cancel(nl_swtimer_t *timer);

/* returns true if the timer is active (i.e. had
 * been started and hasn't run yet)
 */
bool nl_swtimer_is_active(const nl_swtimer_t *timer);

#ifdef BUILD_FEATURE_SW_TIMER_USES_RTOS_TICK
/* function to be called on each rtos tick
 */
void nl_swtimer_rtos_tick_handler(void);

/* called by platform sleep hook before attempting to enter sleep
 */
bool nl_swtimer_pre_sleep(TickType_t *before_sleep_tick_count, uint32_t *xExpectedIdleTime);

/* called by platform sleep hook after wake from sleep
 */
void nl_swtimer_post_sleep(TickType_t before_sleep_tick_count);

/** get system time (time since boot) in nanoseconds
 *
 * @return Current system time in nanoseconds
 */
uint64_t nl_swtimer_get_time_ns(void);

#endif

#ifdef __cplusplus
}
#endif

#endif /* __NLSWTIMER_H_INCLUDED__ */
