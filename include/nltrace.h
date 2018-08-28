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

//    Description:
//      This file defines the interfaces needed for the nltrace framework
//
//
#ifndef __NLTRACE_H__
#define __NLTRACE_H__

#include <stdint.h>
#include <nlplatform.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(BUILD_FEATURE_NL_TRACE)

void nltrace_init(nltimer_id_t timer_id);
uint8_t nltrace_event_start(const char *name);
uint8_t nltrace_event_start_with_taskname(const char *name, const char *task_name);
void nltrace_event_end(uint8_t id, const char *name);
void nltrace_print(void);
void nltrace_enable(bool enable);
bool nltrace_enabled(void);

#else

#define nltrace_init(X)
#define nltrace_event_start(X) (0)
#define nltrace_event_start_with_taskname(X, Y) (0)
#define nltrace_event_end(X, Y)
#define nltrace_print()
#define nltrace_enable(X)
#define nltrace_enabled(X) (false)

#endif // BUILD_FEATURE_NL_TRACE

#ifdef __cplusplus
}
#endif

#endif // __NLTRACE_H__
