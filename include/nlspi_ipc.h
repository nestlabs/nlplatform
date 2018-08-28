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
 *    This file the API for the SPI IPC subsystem.
 */

#ifndef __NLSPI_IPC_H_INCLUDED__
#define __NLSPI_IPC_H_INCLUDED__

#include <stdbool.h>

#ifndef NL_NO_RTOS
#include <FreeRTOS.h>
#endif
#include <nlplatform/nlspi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* struct nlspi_ipc_device_t - provides SPI slave device, GPIOs, and handlers
 * to be used in nlspi_ipc.c. Since it's expected that there will only be one
 * such device, nlspi_ipc.c, if built, expects the global g_spi_ipc_device to
 * be defined.
 *
 * @spi_slave: SPI slave device to communicate with.
 * @srdy_gpio: "slave ready" GPIO pin number.
 * @mrdy_gpio: "master ready" or chip-select GPIO pin number.
 * @reset_gpio: GPIO pin number which will reset slave.
 * @srdy_timeout_ms: Milliseconds to wait before giving up on SRDY signal.
 * @rx_pending_handler: Called when slave requests a transfer. Client should
 *     call nlspi_ipc_work in thread context.
 * @rx_data_handler: Called to pass a received message to client.
 * @rx_corrupt_handler: Called to notify client when a received message is
 *     corrupted (CRC).
 * @srdy_timeout_handler: Called to notify client when SRDY signal has been
 *     unresponsive for srdy_timeout_ms. Only on RTOS variant.
 * @connected: Determine whether slave is connected.
 */
typedef struct nlspi_ipc_device_s
{
    const nlspi_slave_t *spi_slave;
    uint8_t srdy_gpio;
    uint8_t mrdy_gpio;
    uint32_t srdy_timeout_ms;
    int (*rx_pending_handler)(void);
    void (*rx_data_handler)(uint8_t opcode, uint8_t data_len, const uint8_t *data);
    void (*rx_corrupt_handler)(uint8_t opcode, uint8_t data_len, const uint8_t *data);
    void (*srdy_timeout_handler)(void);
    bool (*connected)(void);
} nlspi_ipc_device_t;

extern const nlspi_ipc_device_t g_spi_ipc_device;

/* Since some code may need to allocate static max receive buffers and we want
 * to keep those as small as possible, we set different max packet lengths
 * depending on the direction of the IPC.
 */
#ifndef MAX_IPC_DATA_LEN_FROM_SLAVE
#define MAX_IPC_DATA_LEN_FROM_SLAVE 244
#endif

#ifndef MAX_IPC_DATA_LEN_FROM_MASTER
#define MAX_IPC_DATA_LEN_FROM_MASTER 70
#endif

void nlspi_ipc_init(void);

/* Send a packet to the slave. */
#define IPC_RESULT_DONE 0
#define IPC_RESULT_BAD_ARGS -1
#define IPC_RESULT_RX_BUF_TOO_SMALL -2
#define IPC_RESULT_NO_MSG -3
#define IPC_RESULT_NOT_INITIALIZED -4
#define IPC_RESULT_NO_SLAVE -5
#define IPC_RESULT_SRDY_TIMEOUT -6

int nlspi_ipc_send(uint8_t opcode, uint8_t data_len, const uint8_t *data);

#ifndef NL_NO_RTOS
/* To be called in thread context by client after rx_pending_handler is called
 * to process incoming packets from slave. */
void nlspi_ipc_work(void);

#ifdef PRODUCT_IPC_OPCODE_ENABLE_ACK_MODE
void nlspi_ipc_enable_ack_mode(void);
#endif // PRODUCT_IPC_OPCODE_ENABLE_ACK_MODE
#endif // NL_NO_RTOS

#ifdef __cplusplus
}
#endif

#endif //__NLSPI_IPC_H_INCLUDED__
