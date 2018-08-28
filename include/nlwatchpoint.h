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
 *      This file implements an API for the watchpoint registers
 *      in an ARM Cortex-M3 and Cortex-M0 (using the CMSIS headers).
 *      Note CMSIS for cm0 and cm0plus don't define the DWT
 *      or CoreDebug registers because they are considered
 *      an optional HW extension.  The register locations are
 *      the same as for the cm3.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    WATCHPOINT_DISABLED = 0x0,
    WATCHPOINT_ON_PC_MATCH = 0x4,
    WATCHPOINT_ON_READ = 0x5,
    WATCHPOINT_ON_WRITE = 0x6,
    WATCHPOINT_ON_READ_OR_WRITE = 0x7
} nl_watchpoint_type_t;

void nl_watchpoint_enable(unsigned watchpoint_index, unsigned addr, nl_watchpoint_type_t watchpoint_type, unsigned mask);

/* change the watchpoint type to a previously configured watchpoint */
void nl_watchpoint_set_type(unsigned watchpoint_index, nl_watchpoint_type_t watchpoint_type);

#define nl_watchpoint_disable(watchpoint_index) nl_watchpoint_set_type(watchpoint_index, WATCHPOINT_DISABLED)


#ifdef __cplusplus
}
#endif
