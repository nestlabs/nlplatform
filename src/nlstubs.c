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
 *      This file...
 *
 */

#include <errno.h>
#include <nlplatform.h>

int einval_stub_function(void)
{
    return -EINVAL;
}

void fault_stub_function(void)
{
    __builtin_trap();
}

void void_stub_function(void)
{
}

int zero_stub_function(void)
{
    return 0;
}

const char *emptystring_stub_function(void)
{
    return "";
}
