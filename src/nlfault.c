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
 *      This file defines APIs used in fault preservation.
 *
 */

#include <nlplatform.h>
#include <nlplatform/nlfault.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <nlassert.h>

/* Do the stack intensive stuff in a helper function that
 * can unwind before the actual trap, so that the fault
 * handler has a larger stack.  This is needed for asserts
 * in ISRs to work with a minimal main stack or else
 * printfs in the fault handler can double fault.
 */
static void assert_helper(const char *file, unsigned line) __attribute__((noinline));
static void assert_helper(const char *file, unsigned line)
{
#ifdef BUILD_FEATURE_RESET_INFO
    char tmp[NL_FAULT_DIAGS_DESCRIPTION_LENGTH];
    char line_string[11]; // 32-bit value should fit in 10 characters plus null-terminator
    size_t file_len = strlen(file);
    size_t line_len;
    size_t max_file_len;

    // first convert the line number to a string
    utoa(line, line_string, 10);
    line_len = strlen(line_string);

    // reserve enough room for the line string, truncating the
    // file name if needed, with a space in between and a NULL terminator
    max_file_len = NL_FAULT_DIAGS_DESCRIPTION_LENGTH - line_len - 2;
    if (file_len > max_file_len)
    {
#ifndef NL_BOOTLOADER // Doesn't fit in bootloader
        // truncate file name from the end
        file = file + file_len - max_file_len;
#endif
        file_len = max_file_len;
    }
    // copy (possibly truncated) file name
    strncpy(tmp, file, file_len);
    // add space
    tmp[file_len] = ' ';
    // append line_length.
    strncpy(tmp + file_len + 1, line_string,
            NL_FAULT_DIAGS_DESCRIPTION_LENGTH - file_len - 1);
    nl_reset_info_prepare_reset(NL_RESET_REASON_ASSERT, tmp);
#endif /* BUILD_FEATURE_RESET_INFO */
    printf("assert failed: file %s, line %u\n", file, line);
}

// For use in NLER assert call-out
void nl_platform_assert_delegate(const char* file, unsigned line)
{
    assert_helper(file, line);
    __builtin_trap();
}
