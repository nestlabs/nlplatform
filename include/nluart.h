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
 *      This file defines API for UART
 */
#ifndef __NLUART_H_INCLUDED__
#define __NLUART_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <nlplatform.h>

typedef void (*nluart_handler_t)(const nluart_id_t uart_id, int result);
typedef void (*nluart_wakeup_t)(const nluart_id_t uart_id);
typedef void (*nluart_rx_t)(const nluart_id_t uart_id);
typedef void (*nluart_tx_blocking_cb_t)(void);

void nluart_init(void);

typedef enum
{
    NL_UART_PARITY_NONE,
    NL_UART_PARITY_ODD,
    NL_UART_PARITY_EVEN,
} nluart_parity_t;

typedef struct nluart_config_s
{
    uint32_t baud_rate;
    uint8_t nl_cr_enable:1;
    uint8_t flow_control_enable:1;
    uint8_t echo_recv_chars:1;
    uint8_t power_save:1;
    nluart_parity_t parity:2;
    uint8_t unused:2;
#if NL_FEATURE_SIMULATEABLE_HW
    const char *dev_tty;
#endif
} __attribute__((__packed__)) nluart_config_t;

int nluart_request(const nluart_id_t uart_id, const nluart_config_t *config);
int nluart_release(const nluart_id_t uart_id);

/* Supply callback for asynchronous read or write.
 * read returns number of bytes read in synchronous mode, < 0 on error.
 * write returns number of bytes written in synchronous mode, < 0 on error.
 * read returns 0 on successful start of transfer in asynchronous mode, < 0 on error.
 * write returns 0 on successful start of transfer in asynchronous mode, < 0 on error. */
int nluart_read(const nluart_id_t uart_id, uint8_t *buf, size_t len, nluart_handler_t callback);
int nluart_write(const nluart_id_t uart_id, const uint8_t *buf, size_t len, nluart_handler_t callback);

/* Blocks until character is written, with timeout. */
int nluart_putchar(const nluart_id_t uart_id, uint8_t ch, unsigned timeout_ms);

/* Blocks until character is written, with timeout. Calls callback while waiting for the conditions that will
 * allow the driver to write the byte to the UART. */
int nluart_putchar_callback(const nluart_id_t uart_id, uint8_t ch, unsigned timeout_ms, nluart_tx_blocking_cb_t callback);

/* Stores a character into ch if one is available, with timeout. */
int nluart_getchar(const nluart_id_t uart_id, uint8_t *ch, unsigned timeout_ms);

/* Multi-byte put/get API */
size_t nluart_putchars(const nluart_id_t uart_id, const uint8_t *data, size_t len, unsigned timeout_ms);
size_t nluart_getchars(const nluart_id_t uart_id, uint8_t *data, size_t max_len, unsigned timeout_ms);

/* Return 0 (false) or 1 (true). */
bool nluart_canput(const nluart_id_t uart_id);
bool nluart_canget(const nluart_id_t uart_id);
bool nluart_tx_idle(const nluart_id_t uart_id);

bool nluart_flush(const nluart_id_t uart_id, unsigned timeout_ms);
bool nluart_tx_flush(const nluart_id_t uart_id, unsigned timeout_ms);
void nluart_rx_flush(const nluart_id_t uart_id);

/* Wakeup callback.  Set a callback function which is called when the
 * device wakes up from deep sleep due to rx.  Set to NULL to disable callback.
 */
void nluart_set_wakeup_callback(const nluart_id_t uart_id, nluart_wakeup_t callback);

/* Character received callback.  Set a callback function which is called
 * each time a character is received.  Set to NULL to disable callback.
 */
void nluart_set_rx_callback(const nluart_id_t uart_id, nluart_rx_t callback);

/* Force mode to async, used in fault handling case.
 */
void nluart_force_sync(const nluart_id_t uart_id);

/* Detect whether there is a transmitter connected.
 * Returns 0 - not connected, > 0 - connected, < 0 - unknown/error.
 */
int nluart_is_connected(const nluart_id_t uart_id);

/* Suspend all active uart controllers before going to sleep.
 */
void nluart_suspend(void);

/* Resume all previously active uart controllers after sleep.
 */
void nluart_resume(void);

#ifdef __cplusplus
}
#endif

#endif /* __NLUART_H_INCLUDED__ */
