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

/**
 * Description:
 *  The following implementations satisfy GCC built-in support on CM0
 *  architecture.
 *
 * @note
 *   GCC supports these API's with built-in inline assembly on CM3-4 architectures.
 *   However due to the lack of a sufficiently robust instruction Set on CM0,
 *   GCC leaves implementation of these API's to the platform. As there are
 *   modules that use these API's, implementation is provided here for CM0
 *   based platforms. When GCC fails to provide a solution, it substitutes the
 *   function call for one whose name is foo_n() where "n" is the size in bytes
 *   of the parameter being tested. Hence the function names below have the
 *   "_n" appended which corresponds to the byte width of the value under test.
 */

#include <stdint.h>
#include <nlplatform.h>

typedef enum
{
    kOperandAnd = 0,
    kOperandAdd,
    kOperandSub,
    kOperandOr,
    kOperandXor
} operand_t;

static bool __sync_bool_compare_and_swap_internal(void *ptr, void *oldval, void *newval, uint8_t size)
{
    bool retval = false;

    nlplatform_interrupt_disable();
    {
        __sync_synchronize();

        switch (size)
        {
            case 1:
                if (*(uint8_t *)ptr == *(uint8_t *)oldval)
                {
                    *(uint8_t *)ptr = *(uint8_t *)newval;
                    retval = true;
                }

                break;

            case 2:
                if (*(uint16_t *)ptr == *(uint16_t *)oldval)
                {
                    *(uint16_t *)ptr = *(uint16_t *)newval;
                    retval = true;
                }

                break;

            case 4:
                if (*(uint32_t *)ptr == *(uint32_t *)oldval)
                {
                    *(uint32_t *)ptr = *(uint32_t *)newval;
                    retval = true;
                }

                break;
            default:
                break;
        }

        if (retval)
        {
            __sync_synchronize();
        }
    }
    nlplatform_interrupt_enable();

    return retval;
}

bool __sync_bool_compare_and_swap_1(uint8_t *ptr, uint8_t oldval, uint8_t newval)
{
    return __sync_bool_compare_and_swap_internal(ptr, &oldval, &newval, 1);
}

bool __sync_bool_compare_and_swap_2(uint16_t *ptr, uint16_t oldval, uint16_t newval)
{
    return __sync_bool_compare_and_swap_internal(ptr, &oldval, &newval, 2);
}

bool __sync_bool_compare_and_swap_4(uint32_t *ptr, uint32_t oldval, uint32_t newval)
{
    return __sync_bool_compare_and_swap_internal(ptr, &oldval, &newval, 4);
}

uint8_t __sync_sub_and_fetch_1(uint8_t *ptr, uint8_t val)
{
    uint8_t retval;

    nlplatform_interrupt_disable();
    {
        __sync_synchronize();

        *ptr -= val;
        retval = *ptr;

        __sync_synchronize();
    }
    nlplatform_interrupt_enable();

    return retval;
}

static uint8_t __sync_fetch_and_1(uint8_t *ptr, uint8_t val, operand_t oper)
{
    uint8_t retval;

    nlplatform_interrupt_disable();
    {
        __sync_synchronize();

        retval = *ptr;

        switch (oper)
        {
            case kOperandAnd:
                *ptr &= val;
                break;

            case kOperandAdd:
                *ptr += val;
                break;

            case kOperandSub:
                *ptr -= val;
                break;

            case kOperandOr:
                *ptr |= val;
                break;

            case kOperandXor:
                *ptr ^= val;
                break;

            default:
                break;
        }

        __sync_synchronize();
    }
    nlplatform_interrupt_enable();

    return retval;
}

uint8_t __sync_fetch_and_sub_1(uint8_t *ptr, uint8_t val)
{
    return __sync_fetch_and_1(ptr, val, kOperandSub);
}

uint8_t __sync_fetch_and_add_1(uint8_t *ptr, uint8_t val)
{
    return __sync_fetch_and_1(ptr, val, kOperandAdd);
}

uint8_t __sync_fetch_and_and_1(uint8_t *ptr, uint8_t val)
{
    return __sync_fetch_and_1(ptr, val, kOperandAnd);
}

uint8_t __sync_fetch_and_or_1(uint8_t *ptr, uint8_t val)
{
    return __sync_fetch_and_1(ptr, val, kOperandOr);
}

uint8_t __sync_fetch_and_xor_1(uint8_t *ptr, uint8_t val)
{
    return __sync_fetch_and_1(ptr, val, kOperandXor);
}
