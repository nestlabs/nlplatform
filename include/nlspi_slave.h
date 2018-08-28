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
 *    Description:
 *      This file defines API for SPI slave device.
 */
#ifndef __NLSPI_SLAVE_H_INCLUDED__
#define __NLSPI_SLAVE_H_INCLUDED__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Slave device drivers should have a const instance of this structure
 * to pass to transaction calls. It is expected that the user of the
 * nlspi interface call nlspi_slave_request() and nlspi_slave_release().
 */
typedef struct nlspi_slave_config_s {
    uint8_t mControllerId;
    uint8_t mHostIntPin;
    uint8_t mMode;
} nlspi_slave_config_t;



/**
 * Indicates that a SPI transaction has completed with
 * the given length. The data written to the slave has been
 * written to the pointer indicated by the `anInputBuf` argument
 * to the previous call to nlspi_slave_prepare_transaction().
 *
 * Once this function is called, nlspi_slave_prepare_transaction()
 * is invalid and must be called again for the next transaction to be
 * valid.
 *
 * Note that this function is always called at the end of a transaction,
 * even if nlspi_slave_prepare_transaction() has not yet been called.
 *
 * @param[in] aConfig          Pointer to SPI nlspi_slave_config_t configuration.
 * @param[in] aOutputBuf       Value of aOutputBuf from last call to
 *                             nlspi_slave_prepare_transaction().
 * @param[in] aOutputBufLen    Value of aOutputBufLen from last call to
 *                             nlspi_slave_prepare_transaction().
 * @param[in] aInputBuf        Value of aInputBuf from last call to
 *                             nlspi_slave_prepare_transaction().
 * @param[in] aInputBufLen     Value of aInputBufLen from last call to
 *                             nlspi_slave_prepare_transaction().
 * @param[in] aTransactionLen  Length of the completed transaction, in bytes.
 * @param[in] aFromIsr         `true` if the callback is being executed from
 *                             ISR context, `false` otherwise.
 */
typedef void (*nlspi_slave_transaction_complete_callback)(
    const nlspi_slave_config_t *aConfig,
    uint8_t  *aOutputBuf,
    size_t    aOutputBufLen,
    uint8_t  *aInputBuf,
    size_t    aInputBufLen,
    size_t    aTransactionLen,
    bool      aFromIsr
);



/**
 * Initialize the SPI slave interface.
 * Note that the SPI slave is not fully ready until a transaction is
 * prepared using nlspi_slave_prepare_transaction().
 *
 * If nlspi_slave_prepare_transaction() is not called before
 * the master begins a transaction, the resulting SPI transaction
 * will send all `0xFF` bytes and discard all received bytes.
 *
 * @param[in] aConfig   Pointer to SPI nlspi_slave_config_t configuration.
 * @param[in] aCallback Pointer to transaction complete callback
 *
 * @retval 0          Successfully request (enabled) the SPI Slave interface.
 * @retval -EALREADY  SPI Slave interface is already requested (enabled).
 * @retval -EIO       Failed to request (enable) the SPI Slave interface.
 */
int nlspi_slave_request(const nlspi_slave_config_t *aConfig,
    nlspi_slave_transaction_complete_callback aCallback);



/**
 * Shutdown and release (disable) the SPI slave interface.
 *
 * @param[in] aConfig   Pointer to SPI nlspi_slave_config_t configuration.
 */
void nlspi_slave_release(const nlspi_slave_config_t *aConfig);



/**
 * Prepare data for the next SPI transaction. Data pointers
 * MUST remain valid until the transaction complete callback
 * is called by the SPI slave driver, or until after the
 * next call to nlspi_slave_prepare_transaction().
 *
 * This function may be called more than once before the SPI
 * master initiates the transaction. Each *successful* call to this
 * function will cause the previous values from earlier calls to
 * be discarded.
 *
 * Not calling this function after a completed transaction is the
 * same as if this function was previously called with both buffer
 * lengths set to zero and aRequestTransactionFlag set to `false`.
 *
 * Once aOutputBufLen bytes of anOutputBuf has been clocked out, the
 * MISO pin shall be set high until the master finishes the SPI
 * transaction. This is the functional equivalent of padding the end
 * of anOutputBuf with 0xFF bytes out to the length of the transaction.
 *
 * Once aInputBufLen bytes of aInputBuf have been clocked in from
 * MOSI, all subsequent values from the MOSI pin are ignored until the
 * SPI master finishes the transaction.
 *
 * Note that even if `aInputBufLen` or `aOutputBufLen` (or both) are
 * exhausted before the SPI master finishes a transaction, the ongoing
 * size of the transaction must still be kept track of to be passed
 * to the transaction complete callback. For example, if `aInputBufLen`
 * is equal to 10 and `aOutputBufLen` equal to 20 and the SPI master
 * clocks out 30 bytes, the value 30 is passed to the transaction
 * complete callback.
 *
 * If a `NULL` pointer is passed in as `aOutputBuf` or `aInputBuf` it
 * means that that buffer pointer should not change from its current
 * value. In such a case, the corresponding length argument should be
 * ignored. For example,`nlspi_slave_prepare_transaction(NULL, 0,
 * aInputBuf, aInputLen, false)` changes the input buffer pointer and
 * its length but keeps the output buffer pointer same as it was
 * before.
 *
 * Any call to this function while a transaction is in progress will
 * cause all of the arguments to be ignored and the return value to
 * be -EBUSY.
 *
 * @param[in] aConfig        Pointer to SPI nlspi_slave_config_t configuration.
 * @param[in] aOutputBuf    Data to be written to MISO pin.
 * @param[in] aOutputBufLen Size of the output buffer, in bytes.
 * @param[in] aInputBuf     Data to be read from MOSI pin.
 * @param[in] aInputBufLen  Size of the input buffer, in bytes.
 * @param[in] aRequestTransactionFlag Set to true if host interrupt should be set.
 *
 * @retval 0            Transaction was successfully prepared.
 * @retval -EBUSY       A transaction is currently in progress.
 * @retval -ENOENT      nlspi_slave_request() hasn't been called.
 */
int nlspi_slave_prepare_transaction(
    const nlspi_slave_config_t *aConfig,
    uint8_t  *aOutputBuf,
    size_t    aOutputBufLen,
    uint8_t  *aInputBuf,
    size_t    aInputBufLen,
    bool      aRequestTransactionFlag
);

/**
 * Suspend all active SPI slave controllers before going to sleep.
 */
void nlspi_slave_suspend(void);

/**
 * Resume all previously active SPI slave controllers after sleep.
 */
void nlspi_slave_resume(void);


#ifdef BUILD_FEATURE_PLATFORM_SPI_SLAVE_STATISTICS

/**
 * This struct defines the SPI slave driver statistics info (counters).
 */
typedef struct nlspi_slave_statistics_s
{
    uint32_t mNumTrans;          /* Number of SPI transactions */
    uint32_t mNumRequestedTrans; /* Number of slave requested transactions */
    uint32_t mNumResumes;        /* Number of SPI resumes from sleep */
    uint32_t mNumWakes;          /* Number of wakes triggered by SPI */
    uint32_t mNumWakeTimeouts;   /* Number of wake timeouts */
} nlspi_slave_statistics_t;

/**
 * This function gets the current statistics information.
 *
 * @param[in]  aConfig  Pointer to SPI configuration struct.
 * @param[out] aStat    Pointer to a statistics struct where
 *                      the statistics info is copied into.
 *
 */
void nlspi_slave_get_statistics(const nlspi_slave_config_t *aConfig, nlspi_slave_statistics_t *aStat);

/**
 * Reset the statistics info (all counters are reset to zero).
 *
 * @param[in] aConfig  Pointer to SPI configuration struct.
 *
 */
void nlspi_slave_reset_statistics(const nlspi_slave_config_t *aConfig);

#endif /* defined(BUILD_FEATURE_PLATFORM_SPI_SLAVE_STATISTICS) */

#ifdef NL_NO_RTOS
/* Synchronous blocking API used by bootloaders. */
int nlspi_slave_receive(const nlspi_slave_config_t *aConfig, uint8_t *aRxBuf, size_t aRxBufLen, size_t *aRxBytes);
int nlspi_slave_transmit(const nlspi_slave_config_t *aConfig, uint8_t *aTxBuf, size_t aTxBufLen, bool aWaitForCompletion);
void nlspi_slave_wait_for_transmit_complete(const nlspi_slave_config_t *aConfig);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __NLSPI_SLAVE_H_INCLUDED__ */

