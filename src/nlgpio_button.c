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
 *      This file implements a gpio_button
 *
 */

#include <errno.h>
#include <nlassert.h>
#include <nlplatform.h>
#include <nlplatform/nlgpio_button.h>
#ifdef BUILD_FEATURE_BUTTON_DEBOUNCE
#include <nlplatform/nlswtimer.h>
#endif

#if NL_NUM_GPIO_BUTTONS > 0

typedef struct {
#ifdef BUILD_FEATURE_BUTTON_DEBOUNCE
    nl_swtimer_t    timer;
    bool            raw_button;
    bool            debounced_button;
    bool            prev_debounced_button;
#endif
#if NL_FEATURE_SIMULATEABLE_HW
    bool            simulated_state;
#endif
    bool            was_down;
} button_state_t;

static volatile button_state_t s_button_states[NL_NUM_GPIO_BUTTONS];

#if !NL_FEATURE_SIMULATEABLE_HW
#include <nlplatform/nlgpio.h>

#ifdef BUILD_FEATURE_BUTTON_DEBOUNCE
static uint32_t nlgpio_button_debounce_handler(nl_swtimer_t *timer, void *arg)
{
    uint32_t button_id = (uint32_t)arg;
    volatile button_state_t * state = &s_button_states[button_id];
    const nlgpio_button_config_t *config;

    state->prev_debounced_button = state->debounced_button;
    state->debounced_button = state->raw_button;

    if (state->debounced_button != state->prev_debounced_button) {
        config = &nlgpio_button_config_table[button_id];
        config->callback(button_id, state->debounced_button, config->callback_context);
    }
    return 0;
}
#endif /* BUILD_FEATURE_BUTTON_DEBOUNCE */

static void nlgpio_button_isr(const nlgpio_id_t gpio, void *data)
{
    unsigned button_id = (unsigned)data;

    s_button_states[button_id].was_down = true;

#ifdef BUILD_FEATURE_BUTTON_DEBOUNCE
    s_button_states[button_id].raw_button = nlgpio_button_is_down(button_id);
    nl_swtimer_cancel((nl_swtimer_t*)&s_button_states[button_id].timer);
    nl_swtimer_start((nl_swtimer_t*)&s_button_states[button_id].timer, NL_BUTTON_DEBOUNCE_TIME_INTERVAL_MS);
#else
    bool button_down;
    const nlgpio_button_config_t *config = &nlgpio_button_config_table[button_id];
    button_down = nlgpio_button_is_down(button_id);
    config->callback(button_id, button_down, config->callback_context);
#endif
}

int nlgpio_button_init(void)
{
    unsigned i;
    int status = 0;

    // configure GPIO IRQ for all buttons that we want
    // a callback for
    for (i = 0; i < NL_NUM_GPIO_BUTTONS; i++) {
        if (nlgpio_button_config_table[i].callback) {
#ifdef BUILD_CONFIG_DIAGNOSTICS
            // this function can be called after a shell command replaced a GPIO button ISR
            // temporarily in order to handle the GPIO interrupt itself.  Then they
            // call this function to restore the button ISR handler.  In case there is more
            // than one button and the shell command only replaced/released one of them,
            // we want to make sure the old gpio irqs handler is released before we request
            // it again to avoid tripping an assert that the gpio irq is already in use.
            nlgpio_irq_release(nlgpio_button_config_table[i].gpio);
#endif
            status = nlgpio_irq_request(nlgpio_button_config_table[i].gpio,
                    nlgpio_button_config_table[i].gpio_irq_flags,
                    nlgpio_button_isr, (void*)i);
#ifdef BUILD_FEATURE_BUTTON_DEBOUNCE
            nl_swtimer_init((nl_swtimer_t*)&s_button_states[i].timer, nlgpio_button_debounce_handler, (void*)i);
            s_button_states[i].debounced_button = s_button_states[i].prev_debounced_button = nlgpio_button_is_down(i);
#endif
            nlREQUIRE_SUCCESS(status, done);
        }
    }
done:
    return status;
}

bool nlgpio_button_is_down(const nlbutton_id_t button_id)
{
    int button_down = nlgpio_get_value(nlgpio_button_config_table[button_id].gpio);
    if (nlgpio_button_config_table[button_id].low_is_button_down) {
        button_down = !button_down;
    }
    return button_down;
}

#else /* NL_FEATURE_SIMULATEABLE_HW */

void nlgpio_button_simulate_state(const nlbutton_id_t button_id, bool button_down)
{
    const nlgpio_button_config_t *config = &nlgpio_button_config_table[button_id];

    if (button_down == true) {
        s_button_states[button_id].was_down = true;
    }
    s_button_states[button_id].simulated_state = button_down;

    if (config->callback) {
        config->callback(button_id, button_down, config->callback_context);
    }
}

bool nlgpio_button_is_down(const nlbutton_id_t button_id)
{
    return s_button_states[button_id].simulated_state;
}

#endif /* NL_FEATURE_SIMULATEABLE_HW */

bool nlgpio_button_was_down(const nlbutton_id_t button_id)
{
    return s_button_states[button_id].was_down;
}

#endif /* NL_NUM_GPIO_BUTTONS > 0 */
