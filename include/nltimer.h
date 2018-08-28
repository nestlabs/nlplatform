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
 *      This file defines API for HW timers
 */
#ifndef __NLTIMER_H_INCLUDED__
#define __NLTIMER_H_INCLUDED__

#include <stdint.h>
#include <nlplatform.h>

#ifdef __cplusplus
extern "C" {
#endif

/* If in FreeRTOS environment, return 1 if a higher priroity task was woken and
 * a context switch should occur at end of ISR that invoked the callback.
 */
typedef int (*nltimer_handler_t)(nltimer_id_t timer_id, void *context);

void nltimer_init(void);
int nltimer_request(nltimer_id_t timer_id);
int nltimer_release(nltimer_id_t timer_id);

/* If callback is NULL, timer expiration can be polled with nltimer_active.
 * nltimer_start must be called to start the timer. */
int nltimer_set(nltimer_id_t timer_id, uint32_t time_us, nltimer_handler_t callback, void *context, bool auto_restart);

int nltimer_start(nltimer_id_t timer_id);
int nltimer_stop(nltimer_id_t timer_id);

/* Restarts the timer from 0. */
int nltimer_reset(nltimer_id_t timer_id);

/* Returns the number of microseconds since start for an active timer. 
 * If the timer was not active, it returns the starting elapsed time
 * if the timer were restarted without a reset. */
uint32_t nltimer_elapsed(nltimer_id_t timer_id);

/* Return nonzero if the timer is running. */
int nltimer_active(nltimer_id_t timer_id);

#ifdef __cplusplus
}
#endif

#endif /* __NLTIMER_H_INCLUDED__ */
