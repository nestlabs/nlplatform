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

//    Description:
//      This file defines the interfaces needed for nlprofile framework
//
//
#ifndef __NLPROFILE_H__
#define __NLPROFILE_H__

#include <stdint.h>
#include <nlprofile_product.h>

#ifdef __cplusplus
extern "C" {
#endif

// Return the current timestamp in whatever unit the product
// is using to track time
uint32_t nl_profile_get_current_time(void);

// Start tracking a dynamically togglable resource
void nl_profile_start(nl_profile_t profile_index);

// Stop tracking a dynamically togglable resource
void nl_profile_stop(nl_profile_t profile_index);

// Tasks are handled in a different way than other trackable resources.
// Called at the end of a profiling interval to total up all the task usage during that interval
typedef struct
{
    char name[NL_PROFILE_TASK_NAME_LEN];
    uint32_t usage_during_interval;
    uint16_t max_unused_stack_bytes;
} nl_profile_task_info_t;

// Information we track about each queue in the system
typedef struct
{
    uint32_t location;
    uint8_t size;
    uint8_t high_watermark;
} nl_profile_queue_info_t;

// Populate task_profile_info array with information about task usage
void nl_profile_interval_get_task_info(nl_profile_task_info_t task_profile_info[NL_PROFILE_NUM_TASKS]);

// Clears structures used to track interval usage
void nl_profile_interval_start_new_interval(void);

// Product-specific interval cleanup tasks
void nl_profile_product_start_new_interval(void);

// Total up trackable resource usage for this interval and copy it to profile_buffer
void nl_profile_interval_calculate_totals(uint32_t interval_expiry, uint32_t profile_buffer[NL_PROFILE_NUM_TRACKED_ITEMS]);

// Record the pbuf high watermark for this interval.  If using variable sized pools, specify
// which pool with pool_idx.
void nl_profile_interval_set_pbuf_highwatermark(uint32_t pbuf_count, uint32_t pool_idx);

// Return the pbuf high watermark for this interval.  If using variable sized pools, specify
// which pool with pool_idx.
uint32_t nl_profile_interval_get_pbuf_highwatermark(uint32_t pool_idx);

// Populate queue_profile_info with information on tracked queue levels
// Returns the number of tracked queues that were recorded
uint32_t nl_profile_interval_get_queue_info(nl_profile_queue_info_t queue_profile_info[NL_PROFILE_NUM_QUEUES]);

// Product-specific array of tasks in the system
extern const char * TaskNames[];

#ifdef __cplusplus
}
#endif

#endif

