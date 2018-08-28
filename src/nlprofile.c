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
 *      This file implements an interval based system usage profiling service
 *
 */

#include <nlplatform.h>
#include <nlplatform/nlprofile.h>
#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>
#include <nlassert.h>
#include <string.h>

// Structures to track enable times and total usage per interval
static uint32_t nl_profile_enable_times[NL_PROFILE_NUM_TRACKED_ITEMS] = {0};
static uint32_t nl_profile_total_times[NL_PROFILE_NUM_TRACKED_ITEMS] = {0};
static uint8_t nl_profile_enabled[NL_PROFILE_NUM_TRACKED_ITEMS] = {0};

void nl_profile_start(nl_profile_t profile_index)
{
    if (profile_index != NL_PROFILE_T_INVALID) {
        nl_profile_enable_times[profile_index] = nl_profile_get_current_time();
        nl_profile_enabled[profile_index] = 1;
    }
}

void nl_profile_stop(nl_profile_t profile_index)
{
    nlplatform_interrupt_disable();
    if (profile_index != NL_PROFILE_T_INVALID && nl_profile_enabled[profile_index]) {
        nl_profile_total_times[profile_index] += nl_profile_get_current_time() - nl_profile_enable_times[profile_index];
        nl_profile_enabled[profile_index] = 0;
    }
    nlplatform_interrupt_enable();
}

void nl_profile_interval_start_new_interval(void)
{
    nlplatform_interrupt_disable();
    memset(nl_profile_total_times, 0, sizeof(nl_profile_total_times));
    nlplatform_interrupt_enable();
    nl_profile_product_start_new_interval();
}

void nl_profile_interval_calculate_totals(uint32_t interval_expiry, uint32_t profile_buffer[NL_PROFILE_NUM_TRACKED_ITEMS])
{
    int i;
    // If a tracked item is enabled when we calculate totals, add the time it has been enabled
    // to the total time and bring the enable time up to the current interval
    nlplatform_interrupt_disable();
    for (i = 0; i < NL_PROFILE_NUM_TRACKED_ITEMS; i++) {
        if (nl_profile_enabled[i]) {
            nl_profile_total_times[i] += interval_expiry - nl_profile_enable_times[i];
            nl_profile_enable_times[i] = interval_expiry;
        }
        profile_buffer[i] = nl_profile_total_times[i];
    }
    nlplatform_interrupt_enable();
}

void nl_profile_interval_get_task_info(nl_profile_task_info_t task_profile_info[NL_PROFILE_NUM_TASKS])
{
    static uint32_t last_tick_snapshot[NL_PROFILE_NUM_TASKS] = {0};
    TaskStatus_t task_status_array[NL_PROFILE_NUM_TASKS];
    uint32_t total_run_time;
    int i, j;

    // We only intend to support profiling stats on a steady state system.
    // No dynamic starting or stopping of tasks.
    // If this assumption changes, need to revisit this assert, as well as the
    // mechanism we use to report
    nlASSERT(uxTaskGetNumberOfTasks() == NL_PROFILE_NUM_TASKS);

#if configUSE_TRACE_FACILITY == 1
    // Populate system info to get runtime stats and thread names
    uxTaskGetSystemState(task_status_array, NL_PROFILE_NUM_TASKS, &total_run_time);

    // Extract runtime from each task, populate profile structure
    // Since runtime stats are returned in the order of tasks as they appear in
    // FreeRTOS queues, need to sort the stats by task name.
    for (i = 0; i < NL_PROFILE_NUM_TASKS; i++) {
        for (j = 0; j < NL_PROFILE_NUM_TASKS; j++) {
            if (!strncmp(TaskNames[j], task_status_array[i].pcTaskName, configMAX_TASK_NAME_LEN-1)) {
                break;
            }
        }
        nlASSERT(j != NL_PROFILE_NUM_TASKS);
        memcpy(task_profile_info[j].name, task_status_array[i].pcTaskName, sizeof(task_profile_info[j].name));
        task_profile_info[j].usage_during_interval = task_status_array[i].ulRunTimeCounter - last_tick_snapshot[j];
        task_profile_info[j].max_unused_stack_bytes = task_status_array[i].usStackHighWaterMark * sizeof(StackType_t);
        last_tick_snapshot[j] = task_status_array[i].ulRunTimeCounter;
    }
#else
    memset(task_profile_info, 0, sizeof(task_profile_info));
#endif /* configUSE_TRACE_FACILITY == 1 */
}

#if defined configQUEUE_METRICS
static int s_queue_index = 0;
static nl_profile_queue_info_t *s_queue_info_ptr;

static void nl_queue_info_cb(freertos_queue_metric_t *m, QueueHandle_t queue_handle)
{
    // Ignore queues with size one, these are likely mutexes and semaphores, or
    // uninteresting queues whose high watermark isn't as interesting as deep message queues
    if(m->uxSize != 1)
    {
        nlASSERT(s_queue_index < NL_PROFILE_NUM_QUEUES);
        s_queue_info_ptr[s_queue_index].location = (uint32_t)m->pvBufAddr;
        s_queue_info_ptr[s_queue_index].size = (uint8_t)m->uxSize;
        s_queue_info_ptr[s_queue_index].high_watermark = (uint8_t)m->uxMaxMessagesWaiting;
        s_queue_index++;
    }
}

uint32_t nl_profile_interval_get_queue_info(nl_profile_queue_info_t queue_profile_info[NL_PROFILE_NUM_QUEUES])
{
    s_queue_info_ptr = queue_profile_info;
    s_queue_index = 0;
    vQueueGetMetrics(nl_queue_info_cb);
    return s_queue_index;
}
#endif
