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
 *      This file implements an API for the memory protection unit registers.
 */

#include <nlplatform.h>
#include <nlplatform_soc.h>
#include <nlplatform/nlmpu.h>

#if (__MPU_PRESENT == 1) /* Same check used in CMSIS header */
void nl_mpu_init(void)
{
    unsigned i;
    const unsigned num_regions = nl_mpu_get_num_regions();

    // start by making sure MPU is off and all regions disabled.
    // assumes being run at boot so no critical section taken.
    MPU->CTRL = 0;
    for (i = 0; i < num_regions; i++)
    {
        MPU->RBAR = MPU_RBAR_VALID_Msk | i;
        MPU->RASR = 0;
    }
}

void nl_mpu_enable(bool enable, bool enable_default_memory_map, bool enable_mpu_in_fault_handlers)
{
    unsigned control_value;

    if (enable)
    {
        control_value = MPU_CTRL_ENABLE_Msk;
        // generated requested control register value and set it
        if (enable_default_memory_map)
        {
            control_value |= MPU_CTRL_PRIVDEFENA_Msk;
        }
        if (enable_mpu_in_fault_handlers)
        {
            control_value |= MPU_CTRL_HFNMIENA_Msk;
        }
    }
    else
    {
        control_value = 0;
    }
    /* this is atomic so no critical section needed */
    MPU->CTRL = control_value;
}

unsigned nl_mpu_get_num_regions(void)
{
    return (MPU->TYPE & MPU_TYPE_DREGION_Msk) >> MPU_TYPE_DREGION_Pos;
}

/* A mask of all the reserved bits, to validate the attributes passed to nl_mpu_request_region().
 * None of these bits should be set in the attributes value.
 */
#define MPU_RASR_ATTRIBUTES_Msk (MPU_RASR_XN_Msk | MPU_RASR_AP_Msk | MPU_RASR_TEX_Msk | \
                                 MPU_RASR_S_Msk | MPU_RASR_C_Msk | MPU_RASR_B_Msk | MPU_RASR_SRD_Msk)

/* Different ARM architectures support different attributes/encoding of attributes.
 * Rather than defining a generic description and then translation to the attributes
 * supported by the current processor, the attributes should be the ones used
 * in the corresponding CMSIS header file combined into one unsigned value.
 *
 * For example, for cortex-m3, use the core_cm3.h definitions of RASR for the
 * attributes.
 *
 * Returns the region number just allocated and enabled.
 */
int nl_mpu_request_region(unsigned region_base_address, uint64_t region_size, unsigned attributes)
{
    int i;
    const unsigned num_regions = nl_mpu_get_num_regions();
    unsigned size_value;

    // check that the inputs are reasonable.
    // base address must be 32-byte aligned.
    // region size must be a power of 2, with a minimum of 32-bytes.
    // attributes must be RASR.
    assert((region_base_address & ~MPU_RBAR_ADDR_Msk) == 0);
    assert(region_size >= 32);                  // >= 32
    assert(region_size <= 4*1024*1024*1024ULL); // <= 4GB
    // only attribute bits should be set in the attributes argument
    assert((attributes & ~MPU_RASR_ATTRIBUTES_Msk) == 0);

    // handle special case of 4GB region_size
    if (region_size == 4*1024*1024*1024ULL)
    {
        size_value = 31;
    }
    else
    {
        // for cortex-m3 MPU, size must be a power of 2.
        assert((region_size & (region_size - 1)) == 0);

        // convert to what the RBAR wants for SIZE field
        size_value = __builtin_ffs(region_size) - 2;
    }

    nlplatform_interrupt_disable();

    // look for an unused region
    for (i = 0; i < num_regions; i++)
    {
        MPU->RNR = i;
        if ((MPU->RBAR == i) && (MPU->RASR == 0))
        {
            // found one, configure it. first find the real minimum region
            // size by writing all 1's to the RBAR[31:5] and reading it back.
            MPU->RBAR = MPU_RBAR_ADDR_Msk;

#ifndef NDEBUG
            {
                unsigned min_region_size;
                unsigned rbar_invalid_addr_bits;

                rbar_invalid_addr_bits = ~(MPU->RBAR & MPU_RBAR_ADDR_Msk);
                min_region_size = rbar_invalid_addr_bits + 1;
                assert(region_size >= min_region_size);
                assert((region_base_address & rbar_invalid_addr_bits) == 0);
            }
#endif

            // now write the RBAR register with the base address
            MPU->RBAR = region_base_address;

            // now write the attributes to RASR
            MPU->RASR = attributes | (size_value << MPU_RASR_SIZE_Pos) | MPU_RASR_ENABLE_Msk;

            break;
        }
    }
    if (i == num_regions)
    {
        // failed to find a free region
        i = -1;
    }

    nlplatform_interrupt_enable();
    return i;
}

/* Disables and releases a previously requested region.
 */
void nl_mpu_release_region(int region)
{
#ifndef NDEBUG
    const unsigned num_regions = nl_mpu_get_num_regions();

    assert((region >= 0) && (region < num_regions));
#endif

    nlplatform_interrupt_disable();

    MPU->RBAR = MPU_RBAR_VALID_Msk | region;
    MPU->RASR = 0;

    nlplatform_interrupt_enable();
}
#endif /* (__MPU_PRESENT == 1) */
