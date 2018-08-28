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
 *      This file defines API for SPI
 */
#ifndef __NLSPI_H_INCLUDED__
#define __NLSPI_H_INCLUDED__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	SPI_CPHA	0x01
#define	SPI_CPOL	0x02
#define	SPI_MODE_0	(0|0)
#define	SPI_MODE_1	(0|SPI_CPHA)
#define	SPI_MODE_2	(SPI_CPOL|0)
#define	SPI_MODE_3	(SPI_CPOL|SPI_CPHA)

#define SPI_FLAG_EXTERNAL_ENABLE (0x01)

typedef struct nlspi_controller_s nlspi_controller_t;

/* slave device drivers should have a const instance of this structure
 * to pass to transaction calls.  When SPI_FLAG_EXTERNAL_ENABLE is set,
 * nlspi_request() and nlspi_release() will not enable/disable
 * the spi slave but instead it is expected that the user of the
 * nlspi interface call nlspi_slave_enable() and nlspi_slave_disable().
 * If SPI_FLAG_EXTERNAL_ENABLE is not set, nlspi_slave_enable() and
 * nlspi_slave_disable() should not be used, or else reference counting
 * must be done at some layer.
 */
typedef struct nlspi_slave_s {
    uint8_t controller_id;
    uint8_t cs_pin;
    uint8_t mode;
    uint8_t flags;
    uint32_t max_freq_hz;
    void (*enable_fp)(const struct nlspi_slave_s *spi_slave);
    void (*disable_fp)(const struct nlspi_slave_s *spi_slave);
} nlspi_slave_t;

void nlspi_init(void);

/* special marker to indicate not to control CS during transfers */
#define NO_CS_GPIO_PIN 0xff

/* Request the spi controller associated with the slave and
 * hold a lock on the controller so one or more operations
 * can be done.
 */
int nlspi_request(const nlspi_slave_t *spi_slave);
/* Release lock on spi controller associated with a slave
 */
int nlspi_release(const nlspi_slave_t *spi_slave);

/* Explicit enable/disable, to be used if SPI_FLAG_EXTERNAL_ENABLE
 * is set in the spi_slave_t structure.
 */
void nlspi_slave_enable(const nlspi_slave_t *spi_slave);
void nlspi_slave_disable(const nlspi_slave_t *spi_slave);

/* Synchronous uni-directional read/write APIs
 * read returns number of bytes read, < 0 on error.
 * write returns number of bytes written, < 0 on error.
 */
int nlspi_read(const nlspi_slave_t *slave, uint8_t *buf, size_t len);
int nlspi_write(const nlspi_slave_t *slave, const uint8_t *buf, size_t len);

/* The callback should return 0 if nlspi_transfer() should continue
 * with the next transfer in the argument array,.  If the function
 * returns a value other than 0, the subsequent transfers are not
 * done and nlspi_transfer() returns the given non-zero value.
 */
struct nlspi_transfer_s;
typedef int (*nlspi_handler_t)(struct nlspi_transfer_s *xfer, int result);

/* Synchronous bi-directional transfers.  supports an array of transfers requests.
 * at end of each transfer, the callback is invoked giving a chance to abort or
 * modify the fields of transfer structures not yet processed.
 */
typedef struct nlspi_transfer_s {
    const uint8_t  *tx;        /* pointer to tx buffer, NULL == no TX */
    uint8_t        *rx;        /* pointer to rx buffer, NULL == no RX */
    uint32_t        num;       /* number of bytes to transfer */
    nlspi_handler_t callback;  /* callback when transfer completes */
} nlspi_transfer_t;
int nlspi_transfer(const nlspi_slave_t *spi_slave, nlspi_transfer_t *transfers, unsigned num_transfers);

/* Asynchronous uni-directional read/write APIs */

/* result == 0 on successful completion of transfer, < 0 on error */
typedef void (*nlspi_async_handler_t)(const nlspi_slave_t *slave, int result);

/* read returns 0 on successful start of transfer, < 0 on error
 * write returns 0 on successful start of transfer, < 0 on error
 * */
int nlspi_read_async(const nlspi_slave_t *slave, uint8_t *buf, size_t len,
                     nlspi_async_handler_t callback);
int nlspi_write_async(const nlspi_slave_t *slave, const uint8_t *buf, size_t len,
                      nlspi_async_handler_t callback);

#ifdef __cplusplus
}
#endif

#endif /* __NLSPI_H_INCLUDED__ */
