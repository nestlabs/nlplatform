/*
 *
 *    Copyright (c) 2012-2018 Nest Labs, Inc.
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
 *      This file implements an object for calculating CRCs by wrapping
 *      the software implementation.  If a soc has HW CRC, it should
 *      be implemented in the nlplatform_soc level and BUILD_FEATURE_SW_CRC
 *      should not be defined.
 */

#ifdef BUILD_FEATURE_SW_CRC
#include <stdlib.h>
#include <errno.h>

#include <nlassert.h>
#include <nlplatform/nlcrc.h>
#include <nlcrc.h>

unsigned nlcrc_compute(unsigned crc, const void *data, size_t len)
{
    return crc32_append(crc, data, len);
}

int nlcrc_request(nlcrc_transpose_write_t writeType,
                  nlcrc_transpose_read_t readType,
                  bool xorOnRead,
                  nlcrc_len_t crcLen,
                  unsigned poly)
{
    int retval = 0;

    /* when SW CRC is used, we can only support crc32 ANSI for now.
     * the SW CRC library routine is equivalent to HW crc configured
     * with no transpose on read/write, XorOnRead (needs to be confirmed)
     */
    nlREQUIRE_ACTION(writeType == kTransposeTypeWriteNone, done, retval = -EINVAL);
    nlREQUIRE_ACTION(readType == kTransposeTypeReadNone, done, retval = -EINVAL);
    nlREQUIRE_ACTION(xorOnRead == true, done, retval = -EINVAL);
    nlREQUIRE_ACTION(crcLen == kCrcLen32Bits, done, retval = -EINVAL);
done:
    return retval;
}

int nlcrc_release(void)
{
    /* does nothing when using SW CRC */
    return 0;
}

void nlcrc_set_locking(int (*lock)(void *), int (*unlock)(void *), void* context)
{
    /* unneeded in SW CRC implementation, assert we are never called */
    nlASSERT(0);
}

#endif /* BUILD_FEATURE_SW_CRC */
