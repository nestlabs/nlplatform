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

#ifdef __cplusplus
extern "C" {
#endif

void nl_mpu_init(void);

/* if enable == true, enable_default_memory_map and enable_mpu_in_fault_handlers are
 * used the configure the MPU behavior accordingly.
 * if enable == false, the other two arguments are ignored.
 */
void nl_mpu_enable(bool enable, bool enable_default_memory_map, bool enable_mpu_in_fault_handlers);

unsigned nl_mpu_get_num_regions(void);

/* Different ARM architectures support different attributes/encoding of attributes.
 * Rather than defining a generic description and then translation to the attributes
 * supported by the current processor, the attributes should be the ones used
 * in the corresponding CMSIS header file combined into one unsigned value.
 *
 * For example, for cortex-m3, use the core_cm3.h definitions of RASR for the
 * attributes. Invalid parameters are asserted. Base address and size need
 * to meet the minimum requirements for the SoC. Type of region_size is uint64_t
 * to allow specifying a full 4GB region on a 32-bit processor, though it's
 * unlikely we'll need to specify such a large region in practice.
 *
 * Returns the region number just allocated and enabled, or -1 if no free
 * region available.
 */
int nl_mpu_request_region(unsigned region_base_address, uint64_t region_size, unsigned attributes);

/* Disables and releases a previously requested region.
 */
void nl_mpu_release_region(int region);

#ifdef __cplusplus
}
#endif
