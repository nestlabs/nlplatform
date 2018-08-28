/*
 *
 *    Copyright (c) 2017-2018 Nest Labs, Inc.
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
 *      This file implements a low-level trace utility using time stamped events
 *
 */

#include <nlerassert.h>
#include <nlplatform.h>
#include <nlplatform/nltrace.h>
#include <nlplatform/nltimer.h>
#include <nlplatform/nlwatchdog.h>
#include <FreeRTOS.h>
#include <task.h>

typedef struct nltrace_event_s {
    const char *task_name;
    const char *name;
    uint32_t start_timestamp_us;
    uint32_t end_timestamp_us;
    uint8_t next_ind;
    uint8_t prev_ind;
} nltrace_event_t;

#ifndef NL_TRACE_MAX_EVENTS
#define NL_TRACE_MAX_EVENTS (64)
#endif

NLER_STATIC_ASSERT(NL_TRACE_MAX_EVENTS < UINT8_MAX, "nltrace event buffer size must be less than UINT8_MAX elements");

#define INVALID_INDEX (NL_TRACE_MAX_EVENTS + 1)

typedef struct
{
    nltrace_event_t events[NL_TRACE_MAX_EVENTS];
    uint8_t head_ind;
    uint8_t tail_ind;
} nltrace_t;

static nltrace_t s_trace;
static bool s_trace_enable = false;
static nltimer_id_t s_trace_timer_id;

static uint8_t nltrace_alloc_index_locked(void)
{
    uint8_t ret = INVALID_INDEX;

    for (uint8_t i = 0; i < NL_TRACE_MAX_EVENTS; i++)
    {
        if (s_trace.events[i].name == NULL)
        {
            ret = i;
            break;
        }
    }

    return ret;
}

static void nltrace_insert_locked(uint8_t ind)
{
    if (s_trace.head_ind == INVALID_INDEX && s_trace.tail_ind == INVALID_INDEX)
    {
        s_trace.head_ind = ind;
    }
    else
    {
        s_trace.events[ind].prev_ind = s_trace.tail_ind;
        s_trace.events[s_trace.tail_ind].next_ind = ind;
        s_trace.events[ind].next_ind = INVALID_INDEX;
    }

    s_trace.tail_ind = ind;
}

static uint8_t nltrace_remove_locked(uint8_t ind)
{
    uint8_t next = s_trace.events[ind].next_ind;
    uint8_t prev = s_trace.events[ind].prev_ind;

    // Remove sole event
    if (next == INVALID_INDEX && prev == INVALID_INDEX)
    {
        s_trace.head_ind = INVALID_INDEX;
        s_trace.tail_ind = INVALID_INDEX;
    }
    // Remove tail
    else if (next == INVALID_INDEX)
    {
        s_trace.tail_ind = prev;
        s_trace.events[prev].next_ind = INVALID_INDEX;
    }
    // Remove head
    else if (prev == INVALID_INDEX)
    {
        s_trace.head_ind = next;
        s_trace.events[next].prev_ind = INVALID_INDEX;
    }
    // Remove from middle
    else
    {
        s_trace.events[prev].next_ind = next;
        s_trace.events[next].prev_ind = prev;
    }

    s_trace.events[ind].name = NULL;
    s_trace.events[ind].start_timestamp_us = 0;
    s_trace.events[ind].end_timestamp_us = 0;
    s_trace.events[ind].next_ind = INVALID_INDEX;
    s_trace.events[ind].prev_ind = INVALID_INDEX;

    return next;
}

static void nltrace_print_locked(void)
{
    uint8_t ind = s_trace.head_ind;

    while (ind != INVALID_INDEX)
    {
        if (s_trace.events[ind].end_timestamp_us != 0)
        {
            nlwatchdog_refresh();
            NL_LOG_CLEARTEXT(lrAPP, "%s_%s: %u usec (start) %u usec (elapsed)\n",
                             s_trace.events[ind].task_name,
                             s_trace.events[ind].name,
                             s_trace.events[ind].start_timestamp_us,
                             s_trace.events[ind].end_timestamp_us - s_trace.events[ind].start_timestamp_us
                );

            // Remove the event after printing it to make space for new events
            ind = nltrace_remove_locked(ind);
        }
        else
        {
            break;
        }
    }
}

void nltrace_init(nltimer_id_t timer_id)
{
    nlplatform_interrupt_disable();

    NLER_ASSERT(s_trace_enable == false);

    s_trace.head_ind = INVALID_INDEX;
    s_trace.tail_ind = INVALID_INDEX;
    s_trace_timer_id = timer_id;

    for (uint8_t ind = 0; ind < NL_TRACE_MAX_EVENTS; ind++)
    {
        s_trace.events[ind].name = NULL;
        s_trace.events[ind].start_timestamp_us = 0;
        s_trace.events[ind].end_timestamp_us = 0;
        s_trace.events[ind].next_ind = INVALID_INDEX;
        s_trace.events[ind].prev_ind = INVALID_INDEX;
    }

    nlplatform_interrupt_enable();
}

void nltrace_enable(bool enable)
{
    s_trace_enable = enable;
}

bool nltrace_enabled(void)
{
    return s_trace_enable;
}

uint8_t nltrace_event_start_with_taskname(const char *name, const char *task_name)
{
    uint8_t ind = INVALID_INDEX;

    nlplatform_interrupt_disable();

    if (s_trace_enable)
    {
        ind = nltrace_alloc_index_locked();

        if (ind != INVALID_INDEX)
        {
            s_trace.events[ind].task_name = task_name;
            s_trace.events[ind].name = name;
            s_trace.events[ind].start_timestamp_us = nltimer_elapsed(s_trace_timer_id);
            nltrace_insert_locked(ind);
        }
        else
        {
            NL_LOG_CLEARTEXT(lrAPP, "Not enough space to trace event: %s\n", name);
        }
    }

    nlplatform_interrupt_enable();

    return ind;
}

uint8_t nltrace_event_start(const char *name)
{
    const char *task_name;

    if (nlplatform_in_interrupt())
    {
        task_name = "ISR";
    }
    else
    {
        task_name = pcTaskGetTaskName(xTaskGetCurrentTaskHandle());
    }

    return nltrace_event_start_with_taskname(name, task_name);
}

void nltrace_event_end(uint8_t id, const char *name)
{
    nlplatform_interrupt_disable();

    if (s_trace_enable)
    {
        if (id < INVALID_INDEX)
        {
            // Set the end timestamp if it has not already been set and if the event name matches
            if (s_trace.events[id].name == name && s_trace.events[id].end_timestamp_us == 0)
            {
                s_trace.events[id].end_timestamp_us = nltimer_elapsed(s_trace_timer_id);
            }
        }
    }

    nlplatform_interrupt_enable();
}

void nltrace_print(void)
{
    nlplatform_interrupt_disable();
    nltrace_print_locked();
    nlplatform_interrupt_enable();
}
