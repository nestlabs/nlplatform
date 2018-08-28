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
 *      This file defines time API
 */
#ifndef __NLTIME_H_INCLUDED__
#define __NLTIME_H_INCLUDED__

#include <stdint.h>
#include <nlplatform.h>

#ifdef __cplusplus
extern "C" {
#endif

/* system time (time since boot) specified in milliseconds.
 */
typedef int64_t nltime_system_64_t;

/* system time (time since boot) specified in microseconds.
 */
typedef int64_t nltime_system_us_64_t;

/** get current system time (time since boot) in milliseconds
 *
 * @return Current system time in milliseconds
 */
nltime_system_64_t nltime_get_system_ms(void);

/** get current system time (time since boot) in microseconds
 *
 * @return Current system time in microseconds
 */
nltime_system_us_64_t nltime_get_system_us(void);

#ifdef __cplusplus
}
#endif

#endif /* __NLTIME_H_INCLUDED__ */
