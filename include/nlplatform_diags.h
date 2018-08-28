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
 * Description:
 *    This file defines the interface for the radio (OpenThread) diagnostics
 *    functions.
 */

#ifndef __NLPLATFORMDIAGS_H__
#define __NLPLATFORMDIAGS_H__

#include <openthread/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Process the platform specific radio diagnostics features.
 *
 * @param[in]  aInstance      The context needed when invoking other otPlatRadio APIs.
 * @param[in]  argc           The argument counter of diagnostics command line.
 * @param[in]  argv           The argument vector of diagnostics command line.
 * @param[out] aOutput        The diagnostics execution result.
 * @param[in]  aOutputMaxLen  The output buffer size.
 */
void nlplatform_diags(otInstance *aInstance, int argc, char *argv[], char *aOutput, size_t aOutputMaxLen);

/**
 * Process the platform specific radio diagnostics alarm
 *
 * @param[in] aInstance  The context needed when invoking other otPlatRadio APIs
 */
void nlplatform_diags_alarm(otInstance *aInstance);

#ifdef __cplusplus
}
#endif

#endif /* __NLPLATFORMDIAGS_H__ */
