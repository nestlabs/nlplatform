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
 *      This file implements an API for the watchdog registers
 *      in an ARM Cortex-M3 and Cortex-M0 (using the CMSIS headers).
 *      Note CMSIS for cm0 and cm0plus don't define the DWT
 *      or CoreDebug registers because they are considered
 *      an optional HW extension.  The register locations are
 *      the same as for the cm3.
 */

#include <assert.h>
#include <stdint.h>

#include <nlplatform.h>
#include <nlplatform_soc.h>
#include <nlplatform/nlwatchpoint.h>

/* nlplatform_soc.h should include the correct CMSIS header file (or
 * equivalent), which defines needed values like DWT and CoreDebug.
 * If these aren't defined, assume for now that this soc isn't supporting
 * watchpoints and just do nothing.  If those soc's want to support
 * watchpoint APIs, they'll have to define DWT and CoreDbug.
 */
#if defined(DWT) && defined(CoreDebug)

typedef struct
{
    volatile uint32_t comp_reg;
    volatile uint32_t mask_reg;
    volatile uint32_t function_reg;
    volatile uint32_t reserved;
} cortex_watchpoint_comparator_regs;

void nl_watchpoint_enable(unsigned watchpoint_index, unsigned addr, nl_watchpoint_type_t watchpoint_type, unsigned mask)
{
    unsigned i;
    cortex_watchpoint_comparator_regs *dwt_wp_regs = (cortex_watchpoint_comparator_regs*)&DWT->COMP0;

    assert(watchpoint_index < (DWT->CTRL >> DWT_CTRL_NUMCOMP_Pos));
    dwt_wp_regs += watchpoint_index;

    // if this is the first enable of the DWT (which may happen when coming
    // out of deep sleep), immediately writing a DWT register after enabling
    // the DWT has been shown to not take.  we don't have any documentation
    // on how long DWT takes to enable when we set the TRCENA bit, but a little
    // empiracle testing seems to indicate it takes just one or two extra
    // clock cycles.  to be safe, we retry a number of times, and assert in
    // case the limit we choose was insufficient.
    #define MAX_DWT_WRITE_CHECK_COUNT 100
    for (i = 0; i < MAX_DWT_WRITE_CHECK_COUNT; i++)
    {
        dwt_wp_regs->comp_reg = addr;
        if (dwt_wp_regs->comp_reg == addr)
        {
            break;
        }
    }
    assert(dwt_wp_regs->comp_reg == addr);
    dwt_wp_regs->mask_reg = mask;
    dwt_wp_regs->function_reg = watchpoint_type;
}

void nl_watchpoint_set_type(unsigned watchpoint_index, nl_watchpoint_type_t watchpoint_type)
{
    cortex_watchpoint_comparator_regs *dwt_wp_regs = (cortex_watchpoint_comparator_regs*)&DWT->COMP0;

    assert(watchpoint_index < (DWT->CTRL >> DWT_CTRL_NUMCOMP_Pos));
    dwt_wp_regs += watchpoint_index;
    dwt_wp_regs->function_reg = watchpoint_type;
}

#endif /* defined(DWT) && defined(CoreDebug) */
