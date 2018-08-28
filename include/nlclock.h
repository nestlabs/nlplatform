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
 *      This file defines API for clock mgmt
 */
#ifndef __NLCLOCK_H_INCLUDED__
#define __NLCLOCK_H_INCLUDED__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int nlclock_init(void);

int nlclock_enable(uint32_t clock_id, int enable);
int nlclock_is_enabled(uint32_t clock_id);

/* Return the rate set. */
uint32_t nlclock_set_rate(uint32_t clock_id, uint32_t rate);

uint32_t nlclock_get_rate(uint32_t clock_id);

#ifdef __cplusplus
}
#endif

#endif /* __NLCLOCK_H_INCLUDED__ */
