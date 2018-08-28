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
 *      This file defines a gpio button interface.
 */

#ifndef __NLGPIO_BUTTON_H_INCLUDED__
#define __NLGPIO_BUTTON_H_INCLUDED__

#include <stdint.h>
#include <nlplatform.h>

#if NL_NUM_GPIO_BUTTONS > 0

#ifndef NL_BUTTON_DEBOUNCE_TIME_INTERVAL_MS
#define NL_BUTTON_DEBOUNCE_TIME_INTERVAL_MS     24
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (* nlgpio_button_callback_t)(unsigned int button_id, bool button_down, void *context);

typedef struct nlgpio_button_config_s
{
    uint8_t gpio;
    uint8_t gpio_irq_flags;
    uint8_t low_is_button_down;
    uint8_t unused;
    nlgpio_button_callback_t callback;
    void *callback_context;
} nlgpio_button_config_t;

extern const nlgpio_button_config_t nlgpio_button_config_table[NL_NUM_GPIO_BUTTONS];

#if !NL_FEATURE_SIMULATEABLE_HW
int nlgpio_button_init(void);
#else
void nlgpio_button_simulate_state(const nlbutton_id_t button_id, bool button_down);
#endif
bool nlgpio_button_is_down(const nlbutton_id_t button_id);
bool nlgpio_button_was_down(const nlbutton_id_t button_id);

#ifdef __cplusplus
}
#endif

#endif /* NL_NUM_GPIO_BUTTONS > 0 */

#endif /* __NLBUTTON_H_INCLUDED__ */
