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
 *    Description:
 *      This file has source for the SPI IPC subsystem.
 *
 */

#include <stdio.h>

#include <nlplatform.h>
#include <nlplatform/nlgpio.h>
#include <nlplatform/nlspi_ipc.h>
#include <nlcrc.h>

#ifndef NL_NO_RTOS
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <nlertime.h>

#ifdef BUILD_FEATURE_BREADCRUMBS
#include <nlbreadcrumbs.h>
#include <nlbreadcrumbs-local.h>
#include <nlbreadcrumbs-all.h>
#endif

static TaskHandle_t sTaskToNotify;
static StaticSemaphore_t spi_ipc_mutex;
static bool sInitialized;
#else // NL_NO_RTOS
static volatile uint8_t srdy_edge_triggered;
#endif // NL_NO_RTOS

static int irq_registered;

#ifdef PRODUCT_IPC_OPCODE_ENABLE_ACK_MODE
static bool s_use_ack_nak_mode;
#endif

#if 0
#define DEBUG_TRACE(x) printf x
#else
#define DEBUG_TRACE(x) (void)0
#endif

// Store received data plus CRC
static uint8_t rx_buffer[MAX_IPC_DATA_LEN_FROM_SLAVE + 1];

/* Used to detect rising edge interrupt when waiting for slave to be ready
 * for us to start clocking SPI bus.
 */
static void srdy_deasserted_isr(const nlgpio_id_t gpio, void *data)
{
#ifndef NL_NO_RTOS
    signed portBASE_TYPE yield = pdFALSE;
#endif

    /* disable interrupt until next time */
    irq_registered = 0;
    nlgpio_irq_release(g_spi_ipc_device.srdy_gpio);

#ifndef NL_NO_RTOS
    vTaskNotifyGiveFromISR(sTaskToNotify, &yield);
    portEND_SWITCHING_ISR(yield);
#else
    srdy_edge_triggered = 1;
#endif
}

#ifndef NL_NO_RTOS
/* Used to detect when slave has something to send to us.
 */
static void srdy_asserted_isr(const nlgpio_id_t gpio, void *data)
{
    int should_yield;

    /* disable interrupt until next time */
    irq_registered = 0;
    nlgpio_irq_release(g_spi_ipc_device.srdy_gpio);

    /* notify of pending ipc, which should ready a thread to call spi_ipc_work()
     * to do the transfer and invoke receive handlers
     */
    should_yield = g_spi_ipc_device.rx_pending_handler();

    portEND_SWITCHING_ISR(should_yield ? pdTRUE : pdFALSE);
}

static int fetch_rx_len_func(struct nlspi_transfer_s *xfer, int res)
{
    uint8_t len;
    nlspi_transfer_t *data_xfer = xfer + 1;

    DEBUG_TRACE(("%s: xfer = %p, res = %d\n", __func__, xfer, res));

    /* if we already have an error, just return */
    if (res < 0)
        return res;

    /* if we received a 0 length byte, then there is no actual transaction
     * to conduct.  abort with a result which will not trigger a panic.
     */
    len = xfer->rx[0];
    if (!len) {
        printf("%s: len byte is 0, nothing to do\n", __func__);
        return IPC_RESULT_NO_MSG;
    }

    /* if the length of the packet is too large to fit into the
     * rx buffer, trigger an error.  Reserve one byte for CRC.
     * The received length includes the opcode and the length byte itself
     * as well as the CRC trailer.  Subtract 2 bytes for length/opcode.
     * This yields the data payload length plus CRC trailer.
     */
    len -= 2;
    if (len > data_xfer->num) {
        printf("%s: data_xfer->num %d too small to fit transfer of %d\n",
               __func__, data_xfer->num, len);
        return IPC_RESULT_RX_BUF_TOO_SMALL;
    }
    /* Change data_xfer->num to be equal to the number of bytes being received */
    DEBUG_TRACE(("%s: receiving %d data bytes\n", __func__, len));

    data_xfer->num = len;
    return 0;
}
#endif /* !defined(NL_NO_RTOS) */

void nlspi_ipc_init(void)
{
#ifndef NL_NO_RTOS
    if (sInitialized == false)
    {
         xSemaphoreCreateMutexStatic(&spi_ipc_mutex);
         sInitialized = true;
    }

    DEBUG_TRACE(("%s: SPI IPC clock at %d\n", __func__, g_spi_ipc_device.spi_slave->max_freq_hz));

    // register interrupt for the case where slave has something to send to us.
    irq_registered = 1;
    nlgpio_irq_request(g_spi_ipc_device.srdy_gpio, IRQF_TRIGGER_LOW, srdy_asserted_isr, NULL);
#endif
}

static int check_crc_and_dispatch(uint8_t *length_and_opcode, uint8_t *rx_buf, uint8_t rx_data_len)
{
    int result = 0;
    if (length_and_opcode[0])
    {
        uint8_t crc_calc;

#ifdef PRODUCT_IPC_OPCODE_ENABLE_ACK_MODE
        if (s_use_ack_nak_mode)
        {
            // passed in rx_data_len is one greater than it should
            // be because slave is sending a repeated CRC byte.
            // it was easier for calling code to use the off-by-one
            // value so we adjust here.
            rx_data_len--;
        }
#endif

        /* Compute CRC over length, opcode, data (excludes received CRC). */
        crc_calc = crc8_ccitt(length_and_opcode, 2);
        crc_calc = crc8_ccitt_append(crc_calc, rx_buf, rx_data_len);

#ifdef PRODUCT_IPC_OPCODE_ENABLE_ACK_MODE
        if (s_use_ack_nak_mode)
        {
            /* Compare CRCs. Received CRC is the last two bytes in the rx data buffer.
             * In release builds, only one has to be valid. In non-release builds, we
             * expect both to match the computed CRC and if not, we call the corrupt_handler.
             */
#if BUILD_CONFIG_RELEASE
            if ((crc_calc == rx_buf[rx_data_len]) || (crc_calc == rx_buf[rx_data_len + 1]))
#else
            if ((crc_calc == rx_buf[rx_data_len]) && (crc_calc == rx_buf[rx_data_len + 1]))
#endif
            {
                /* Send an ACK to slave */
                result = 1;
                g_spi_ipc_device.rx_data_handler(length_and_opcode[1], rx_data_len, rx_buf);
            }
            else
            {
                /* Send a NAK to slave */
                result = -1;
                g_spi_ipc_device.rx_corrupt_handler(length_and_opcode[1], rx_data_len, rx_buf);
            }
        }
        else
#endif // PRODUCT_IPC_OPCODE_ENABLE_ACK_MODE
        {
        /* Compare CRCs. Received CRC is the last byte in the rx data buffer. */
        if (crc_calc != rx_buf[rx_data_len])
        {
            g_spi_ipc_device.rx_corrupt_handler(length_and_opcode[1], rx_data_len, rx_buf);
        }
        else
        {
            g_spi_ipc_device.rx_data_handler(length_and_opcode[1], rx_data_len, rx_buf);
        }
    }
}
    return result;
}

#ifndef NL_NO_RTOS // Don't need this function in NO_RTOS builds
void nlspi_ipc_work(void)
{
    /* check if there's something to receive, if so, do the
     * transfer and dispatch it
     */
    nlspi_transfer_t xfer[2];
    uint8_t len_opcode_rx[2];
    int result;

    if (sInitialized == false) {
        printf("spi_ipc_init() hasn't been called\n");
        return;
    }

    DEBUG_TRACE(("%s: Grabbing spi_ipc_mutex\n", __func__));
    xSemaphoreTake(&spi_ipc_mutex, portMAX_DELAY);

    if (nlgpio_get_value(g_spi_ipc_device.srdy_gpio)) {
        DEBUG_TRACE(("%s: SRDY not asserted\n", __func__));
        /* SRDY not asserted, nothing to do */

        // irq might have already been registered in the case where
        // a nlspi_ipc_send() was done after the last wake interrupt occurred,
        // and the nlspi_ipc_send() already rescheduled another wake interrupt.
        // to be safe, we release and request the interrupt again because the
        // gpio system would otherwise register a separate interrupt on
        // a different gpio port pin.
        if (irq_registered) {
            nlgpio_irq_release(g_spi_ipc_device.srdy_gpio);
        }
        // register interrupt for the case where slave has something to send to us.
        irq_registered = 2;
        nlgpio_irq_request(g_spi_ipc_device.srdy_gpio, IRQF_TRIGGER_LOW, srdy_asserted_isr, NULL);
        xSemaphoreGive(&spi_ipc_mutex);
        return;
    }

    DEBUG_TRACE(("%s: Requesting spi bus\n", __func__));

    nlspi_request(g_spi_ipc_device.spi_slave);

    DEBUG_TRACE(("%s: SRDY is asserted (val = %d)\n", __func__, nlgpio_get_value(g_spi_ipc_device.srdy_gpio)));

    /* Before asserting MRDY, register rising edge interrupt so we avoid any
     * races with the slave signaling via SRDY for a requested transfer. If the
     * slave was waiting for us to assert MRDY, it could quickly give the
     * rising edge and polling for an edge transition would fail.
     */
    if (irq_registered) {
        // cancel previously scheduled interrupt (most likely
        // the handler for assert)
        DEBUG_TRACE(("%s: Releasing previous irq request\n", __func__));
        nlgpio_irq_release(g_spi_ipc_device.srdy_gpio);
    }
    irq_registered = 3;
    nlgpio_irq_request(g_spi_ipc_device.srdy_gpio, IRQF_TRIGGER_RISING, srdy_deasserted_isr, NULL);

    DEBUG_TRACE(("%s: asserting MRDY\n", __func__));
    /* assert MRDY now that we're ready */
    nlgpio_request(g_spi_ipc_device.mrdy_gpio, GPIOF_OUT_LOW);

    /* The slave wants to send us something so do the transfer.
     * Setup the transaction so that we check in the callback
     * after the first byte (length) is received what the length
     * is and update it dynamically.
     */
    xfer[0].tx = NULL;
    xfer[0].rx = len_opcode_rx;
    xfer[0].num = sizeof(len_opcode_rx);
    xfer[0].callback = fetch_rx_len_func;
    xfer[1].tx = NULL;
    xfer[1].rx = rx_buffer;
    xfer[1].num = sizeof(rx_buffer); /* to be changed by fetch_rx_len_func */
    xfer[1].callback = NULL;

    DEBUG_TRACE(("%s: waiting for SRDY rising edge\n", __func__));
    /* now wait for rising edge, which is the slave signal that
     * they've seen our chip select and are ready to do a transfer
     */
    {
        nl_time_native_t srdy_timeout;
        srdy_timeout = nl_time_ms_to_time_native(g_spi_ipc_device.srdy_timeout_ms);
        sTaskToNotify = xTaskGetCurrentTaskHandle();
        if (ulTaskNotifyTake(pdTRUE, srdy_timeout) != 1)
        {
            DEBUG_TRACE(("%s: SRDY timeout (%u ticks)\n", __func__, srdy_timeout));
            nlgpio_release(g_spi_ipc_device.mrdy_gpio);
            nlgpio_irq_release(g_spi_ipc_device.srdy_gpio);
            irq_registered = 0;
            nlspi_release(g_spi_ipc_device.spi_slave);
            g_spi_ipc_device.srdy_timeout_handler();
            xSemaphoreGive(&spi_ipc_mutex);
            return;
        }
    }

    DEBUG_TRACE(("%s: SRDY rising edge seen, calling spi->transfer() to read\n", __func__));

    result = nlspi_transfer(g_spi_ipc_device.spi_slave, xfer, 2);

    DEBUG_TRACE(("%s: after calling spi->transfer() to read\n", __func__));
    if (result) {
        printf("Error %d on spi xfer read\n", result);
    }

    // register interrupt for the case where slave has something to send to us.
    if (irq_registered) {
        printf("%s: irq_registered (%d) when we didn't expect it to be\n",
            __func__, irq_registered);
    }
    irq_registered = 4;
    nlgpio_irq_request(g_spi_ipc_device.srdy_gpio, IRQF_TRIGGER_LOW, srdy_asserted_isr, NULL);

    nlgpio_release(g_spi_ipc_device.mrdy_gpio);
    nlspi_release(g_spi_ipc_device.spi_slave);

    result = check_crc_and_dispatch(len_opcode_rx, rx_buffer, xfer[1].num - 1);

    xSemaphoreGive(&spi_ipc_mutex);

    DEBUG_TRACE(("%s: ending, result %d\n", __func__, result));

#if !defined(NL_NO_RTOS) && defined(PRODUCT_IPC_OPCODE_ENABLE_ACK_MODE)
    if (result == -1)
    {
        /* Print some additional debug information to help
         * diagnose circumstances when corruption occurred
         */
        printf("%s: sending NAK\n", __func__);
        nlspi_ipc_send(PRODUCT_IPC_OPCODE_NAK, 0, NULL);
}
    else if (result == 1)
    {
        nlspi_ipc_send(PRODUCT_IPC_OPCODE_ACK, 0, NULL);
    }
#endif // !defined(NL_NO_RTOS) && defined(PRODUCT_IPC_OPCODE_ENABLE_ACK_MODE)
}

#ifdef PRODUCT_IPC_OPCODE_ENABLE_ACK_MODE
void nlspi_ipc_enable_ack_mode(void)
{
    // send slave msg to enable ack mode
    nlspi_ipc_send(PRODUCT_IPC_OPCODE_ENABLE_ACK_MODE, 0, NULL);
    s_use_ack_nak_mode = true;
}
#endif // PRODUCT_IPC_OPCODE_ENABLE_ACK_MODE

#endif /* !defined(NL_NO_RTOS) */

int nlspi_ipc_send(uint8_t opcode, uint8_t data_len, const uint8_t *data)
{
    int result;
    nlspi_transfer_t xfer[3];
    uint8_t len_opcode_tx[2];
    uint8_t len_opcode_rx[2];
    uint8_t crc_calc;
    uint8_t rx_len = 0;
    uint8_t rx_data_len = 0;

#ifndef NL_NO_RTOS
    if (sInitialized == false) {
        printf("spi_ipc_init() hasn't been called\n");
        return IPC_RESULT_NOT_INITIALIZED;
    }
#endif

    if (!g_spi_ipc_device.connected())
    {
        return IPC_RESULT_NO_SLAVE;
    }

    DEBUG_TRACE(("%s: start\n", __func__));

#ifndef NL_NO_RTOS
    DEBUG_TRACE(("%s: Grabbing spi_ipc_mutex\n", __func__));
    xSemaphoreTake(&spi_ipc_mutex, portMAX_DELAY);
#endif

    DEBUG_TRACE(("%s: Requesting spi bus\n", __func__));
    nlspi_request(g_spi_ipc_device.spi_slave);

    /* Before asserting MRDY, register rising edge interrupt so we avoid any
     * races with the slave signaling via SRDY for a requested transfer. If the
     * slave was waiting for us to assert MRDY, it could quickly give the
     * rising edge and polling for an edge transition would fail.
     */
    if (irq_registered) {
        // cancel previously scheduled interrupt (most likely
        // the handler for assert)
        DEBUG_TRACE(("%s: Releasing previous irq request %d\n", __func__, irq_registered));
        nlgpio_irq_release(g_spi_ipc_device.srdy_gpio);
    }
    irq_registered = 5;
#ifdef NL_NO_RTOS
    srdy_edge_triggered = 0;
#endif

    nlgpio_irq_request(g_spi_ipc_device.srdy_gpio, IRQF_TRIGGER_RISING, srdy_deasserted_isr, NULL);

    /* Assert MRDY after registering rising edge interrupt request */
    DEBUG_TRACE(("%s: asserting MRDY\n", __func__));
    nlgpio_request(g_spi_ipc_device.mrdy_gpio, GPIOF_OUT_LOW);

    /* a packet/frame consists of the header of the length
     * and opcode, then the data, then the CRC
     */
    len_opcode_tx[0] = data_len + sizeof(len_opcode_tx) + sizeof(crc_calc);
    len_opcode_tx[1] = opcode;
    len_opcode_rx[0] = 0;
    xfer[0].tx = len_opcode_tx;
    xfer[0].rx = len_opcode_rx;
    xfer[0].num = sizeof(len_opcode_tx);
    xfer[0].callback = NULL;

    DEBUG_TRACE(("%s: waiting for SRDY rising edge\n", __func__));
    /* now wait for rising edge, which is the slave signal that
     * they've seen our chip select and are ready to do a transfer
     */
#ifndef NL_NO_RTOS
    {
        nl_time_native_t srdy_timeout;
        srdy_timeout = nl_time_ms_to_time_native(g_spi_ipc_device.srdy_timeout_ms);
        sTaskToNotify = xTaskGetCurrentTaskHandle();
        if (ulTaskNotifyTake(pdTRUE, srdy_timeout) != 1)
        {
            DEBUG_TRACE(("%s: SRDY timeout (%u ticks)\n", __func__, srdy_timeout));
            nlgpio_release(g_spi_ipc_device.mrdy_gpio);
            nlgpio_irq_release(g_spi_ipc_device.srdy_gpio);
            irq_registered = 0;
            nlspi_release(g_spi_ipc_device.spi_slave);
            g_spi_ipc_device.srdy_timeout_handler();
            xSemaphoreGive(&spi_ipc_mutex);
            return IPC_RESULT_SRDY_TIMEOUT;
        }
    }
#else
    while (srdy_edge_triggered == 0);
#endif

    DEBUG_TRACE(("%s: SRDY rising edge seen\n", __func__));

    crc_calc = crc8_ccitt(len_opcode_tx, sizeof(len_opcode_tx));

    if (data_len) {
        xfer[1].tx = data;
        xfer[1].rx = rx_buffer;
        xfer[1].num = data_len;
        xfer[1].callback = NULL;
        xfer[2].tx = &crc_calc;
        xfer[2].rx = rx_buffer + data_len;
        xfer[2].num = sizeof(crc_calc);
        xfer[2].callback = NULL;
        DEBUG_TRACE(("%s: calling spi->transfer of header+data\n", __func__));
        crc_calc = crc8_ccitt_append(crc_calc, data, data_len);
        result = nlspi_transfer(g_spi_ipc_device.spi_slave, xfer, 3);
    } else {
        xfer[1].tx = &crc_calc;
        xfer[1].rx = rx_buffer;
        xfer[1].num = sizeof(crc_calc);
        xfer[1].callback = NULL;
        DEBUG_TRACE(("%s: calling spi->transfer of just header, no data\n", __func__));
        result = nlspi_transfer(g_spi_ipc_device.spi_slave, xfer, 2);
    }
    DEBUG_TRACE(("%s: spi->transfer returned %d\n", __func__, result));

    if (result) {
        printf("Error %d on spi xfer\n", result);
        goto done;
    }

    /* did the slave have data to send to us?  if so, the rx_len
     * (first received byte of the packet) will be non-zero
     */
    rx_len = len_opcode_rx[0];
    if (rx_len) {
        rx_data_len = rx_len - sizeof(len_opcode_rx) - sizeof(crc_calc);
        if (rx_data_len <= sizeof(rx_buffer)) {
            if (rx_data_len > data_len) {
                /* there is more data to receive then we've clocked so far */
                uint8_t bytes_left = rx_data_len - data_len;
                xfer[0].tx = NULL;
                // We have already received data_len bytes, plus CRC transfer from master to slave
                xfer[0].rx = rx_buffer + data_len + sizeof(crc_calc);
                xfer[0].num = bytes_left;

                result = nlspi_transfer(g_spi_ipc_device.spi_slave, xfer, 1);
                if (result) {
                    printf("Error %d on spi xfer\n", result);
                }
            }
        } else {
            printf("%s: rx_buffer size %d too small to fit packet slave wants to send of %d data bytes\n",
                   __func__, sizeof(rx_buffer), rx_data_len);
            result = IPC_RESULT_RX_BUF_TOO_SMALL;
        }
    }
done:
#ifndef NL_NO_RTOS
    // register interrupt for the case where slave has something to send to us.
    if (irq_registered) {
        printf("%s: irq_registered (%d) when we didn't expect it to be\n",
            __func__, irq_registered);
    }
    irq_registered = 6;
    nlgpio_irq_request(g_spi_ipc_device.srdy_gpio, IRQF_TRIGGER_LOW, srdy_asserted_isr, NULL);
#endif

    nlgpio_release(g_spi_ipc_device.mrdy_gpio);
    nlspi_release(g_spi_ipc_device.spi_slave);

    result = check_crc_and_dispatch(len_opcode_rx, rx_buffer, rx_data_len);

#ifndef NL_NO_RTOS
    xSemaphoreGive(&spi_ipc_mutex);
#endif

    DEBUG_TRACE(("%s: ending, result %d\n", __func__, result));

#if !defined(NL_NO_RTOS) && defined(PRODUCT_IPC_OPCODE_ENABLE_ACK_MODE)
    if (result == -1)
    {
        /* Print some additional debug information to help
         * diagnose circumstances when corruption occurred
         */
        printf("%s: sending NAK, tx len = %u, op = %u\n", __func__, len_opcode_tx[0], len_opcode_tx[1]);
        nlspi_ipc_send(PRODUCT_IPC_OPCODE_NAK, 0, NULL);
    }
    else if (result == 1)
    {
        nlspi_ipc_send(PRODUCT_IPC_OPCODE_ACK, 0, NULL);
    }
#endif // !defined(NL_NO_RTOS) && defined(PRODUCT_IPC_OPCODE_ENABLE_ACK_MODE)
    return result;
}
