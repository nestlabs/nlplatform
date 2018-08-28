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
 *      This file implements EABI symbols that aren't
 *      defined by nllibclite.
 */

#include <stddef.h>
#include <string.h>
#include <stdint.h>

#ifdef __ARM_EABI__

void __aeabi_memcpy(void *dest, const void *src, size_t n);
void __aeabi_memset (void *dest, size_t n, int c);

void __aeabi_memcpy(void *dest, const void *src, size_t n)
{
    size_t i;
    uint8_t *d = (uint8_t*) dest;
    uint8_t *s = (uint8_t*) src;

    for( i=0; i<n; i++)
    {
        *d++ = *s++;
    }
}


// note: __aeabi_memset has different parameter order than
// the usual memset
void __aeabi_memset (void *dest, size_t n, int c)
{
    size_t i;
    uint8_t *d = (uint8_t*) dest;
    for( i=0; i<n; i++)
    {
        *d++ = c;
    }
}

unsigned long long
__aeabi_uldivmod(unsigned long long n, unsigned long long d);
unsigned long long
__aeabi_uldivmod(unsigned long long n, unsigned long long d)
{
    // Clang output is not executed on ARM, so this is not
    // required.
    __builtin_trap();
    return 0;
}

double
__aeabi_f2d(float n);
double
__aeabi_f2d(float n)
{
    // Clang output is not executed on ARM, so this is not
    // required.
    __builtin_trap();
    return 0;
}

#endif
