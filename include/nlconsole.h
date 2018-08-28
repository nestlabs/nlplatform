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
 *      This file defines a console API.
 */
#ifndef __NLCONSOLE_H_INCLUDED__
#define __NLCONSOLE_H_INCLUDED__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*nlconsole_rx_t)(void);

int nlconsole_request(void);
int nlconsole_release(void);

/* Detect whether there is a transmitter connected.
 * Returns 0 - not connected, > 0 - connected, < 0 - unknown/error.
 */
bool nlconsole_is_connected(void);

bool nlconsole_canget(void);
void nlconsole_putchar(uint8_t ch);
int nlconsole_getchar(uint8_t *ch);
int nlconsole_flush(void);

/* Register a function to be called when a character is received. Set to NULL
 * to disable. */
void nlconsole_set_rx_callback(nlconsole_rx_t callback);

/* Register a function to be called when the system wakes on receive. Set to
 * NULL to disable. */
void nlconsole_set_rx_wakeup_callback(nlconsole_rx_t callback);

#ifdef __cplusplus
}
#endif

#endif /* __NLCONSOLE_H_INCLUDED__ */
