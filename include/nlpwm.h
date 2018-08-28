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
 * Description:
 *    This file defines the interfaces for the PWM subsystem.
 *
 * Usage:
 *
 *    Initialize the PWM subsystem with nlpwm_init(); this should only be done
 *    once.
 *
 *    Claim and configure a PWM output using nlpwm_request(); this is a blocking
 *    call, and will wait until the PWM is available.
 *
 *    Release ownership of a PWM output using nlpwm_release(). It is an error to
 *    release a previously un-requested PWM output.
 */

#ifndef __NLPWM_H_INCLUDED__
#define __NLPWM_H_INCLUDED__


#include <stdint.h>

#include <nlproduct_config.h>


#define NL_PWM_FREQ_MAX NL_PLATFORM_PWM_FREQ_MAX
#define NL_PWM_DUTY_MAX TYPE_MAX(((nlpwm_config_t*)0)->mDuty)


#ifdef __cplusplus
extern "C" {
#endif


typedef uint8_t nlpwm_id_t;

typedef struct
{
    uint32_t        mFreq;
    uint8_t         mDuty;
} nlpwm_config_t;


void nlpwm_init(void);
int nlpwm_request(nlpwm_id_t aId, const nlpwm_config_t *aConfig);
int nlpwm_release(nlpwm_id_t aId);


#ifdef __cplusplus
}
#endif

#endif /* __NLPWM_H_INCLUDED__ */
