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
 *      This file...
 *
 */

#include <nlertime.h>
#include <nlmacros.h>

#include <nlplatform.h>
#include <nlplatform/nltime.h>

#ifdef BUILD_FEATURE_SW_TIMER
#include <nlplatform/nlswtimer.h>

nltime_system_64_t nltime_get_system_ms(void)
{
    // Need to divide by 1,000,000 to convert ns to ms.  Rather than divide,
    // multiply by a millionth, in Q49 fixed point.       

    const uint32_t millionth = 0x218DEF41; // Q49: 19 leading zeros bits and 30 significant bits 
    uint64_t sys_time = nl_swtimer_get_time_ns();
    uint64_t upper = sys_time >> 32;
    uint64_t lower = sys_time & 0xffffffff;
    upper *= millionth;
    lower *= millionth;
    upper += lower >> 32;
    upper += 1<<16; // Round
    upper >>= 17; // Q49 - 32 bits in lower
    return (nltime_system_64_t)upper; // Convert from unsigned to signed.
}

#else

nltime_system_64_t nltime_get_system_ms(void)
{
    // Notes:
    // - Returning 64 bit type, but only have 32 bits of data
    // - Millisecond value will rollover, so only use result for computing differences
    // - When native units rollover, millisecond value will have a sudden
    //   jump in value (not a smooth rollover).
    return nl_time_native_to_time_ms(nl_get_time_native());
}

#endif

nltime_system_us_64_t nltime_get_system_us(void)
{
    // TODO: Add microsecond-level system time. Use nltime_get_system_ms() in
    //       the interim.
    return nltime_get_system_ms() * US_PER_MS;
}
