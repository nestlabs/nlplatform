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
 *      This file defines API for GPIO
 */
#ifndef __NLGPIO_H_INCLUDED__
#define __NLGPIO_H_INCLUDED__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* include soc specific header file that provides the
 * definitions of gpio flags so we don't have to convert
 * from generic values to specific values.
 * nlgio_id_t is also product specific.
 */
#include <nlgpio_defines.h>

typedef void (*nlgpio_irq_handler_t)(const nlgpio_id_t gpio, void *arg);

void nlgpio_init(void);
bool nlgpio_is_valid(unsigned number);
int nlgpio_request(const nlgpio_id_t gpio, nlgpio_flags_t gpio_flags);
int nlgpio_release(const nlgpio_id_t gpio);

int nlgpio_setmode(const nlgpio_id_t gpio, unsigned mode);
unsigned nlgpio_getmode(const nlgpio_id_t gpio);

int nlgpio_set_input(const nlgpio_id_t gpio);
int nlgpio_set_output(const nlgpio_id_t gpio, unsigned value);

int nlgpio_get_value(const nlgpio_id_t gpio);
int nlgpio_set_value(const nlgpio_id_t gpio, unsigned value);

int nlgpio_irq_request(const nlgpio_id_t gpio, unsigned irq_flags, nlgpio_irq_handler_t callback, void *arg);
int nlgpio_irq_release(const nlgpio_id_t gpio);
bool nlgpio_irq_pending(const nlgpio_id_t gpio);

#ifdef __cplusplus
}
#endif

#endif /* __NLGPIO_H_INCLUDED__ */
