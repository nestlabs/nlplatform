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
 *      This file defines API for RTC
 */
#ifndef __NLRTC_H_INCLUDED__
#define __NLRTC_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct nlrtc_s nlrtc_t;

nlrtc_t *nlrtc_init(int rtc_id);

uint32_t nlrtc_get(nlrtc_t *rtc);
uint32_t nlrtc_get_from_isr(nlrtc_t *rtc);

#ifdef __cplusplus
}
#endif

#endif /* __NLRTC_H_INCLUDED__ */
