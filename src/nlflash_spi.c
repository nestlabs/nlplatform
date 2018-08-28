/*
 *
 *    Copyright (c) 2012-2018 Nest Labs, Inc.
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
 *      This file implements an API for accessing flash through a
 *      SPI interface.  It supports FLASH chips like N25Q and
 *      MX25U1635 that have no intermediate SRAM buffers.  With
 *      some changes, it could be made to optionally support
 *      FLASH chips with intermediate SRAM buffers.
 *
 *      The APIs in this file are called by the nlflash.c layer,
 *      which provides mutex (recursive) locking, so no locking
 *      is required here.
 */

#include <errno.h>
#include <string.h>

#include <nlassert.h>
#include <nlplatform.h>

#if FLASH_SPI_SIZE > 0
#include <nlplatform/nlflash_spi.h>
#include <nlplatform/nlgpio.h>
#include <nlutilities.h>

#ifndef FLASH_SPI_SPLIT_TRANSACTIONS
#error "Must define FLASH_SPI_SPLIT_TRANSACTIONS to 0 or 1"
#endif

#ifndef FLASH_SPI_MAX_CHIP_ID_CHECK_COUNT
#define FLASH_SPI_MAX_CHIP_ID_CHECK_COUNT 3
#endif

#ifndef FLASH_SPI_FAULT_ON_REQUEST_FAILURE
#define FLASH_SPI_FAULT_ON_REQUEST_FAILURE 0
#endif

#ifndef FLASH_SPI_NUMBER_REQUEST_ATTEMPTS
#define FLASH_SPI_NUMBER_REQUEST_ATTEMPTS 3
#endif

typedef struct nlflash_spi_device_s {
#ifdef FLASH_SPI_USE_PARTIAL_PAGE_BUFFER
    uint8_t partial_page[FLASH_SPI_MAX_PAGE_SIZE];
    uint32_t partial_page_index;
    uint32_t write_loc;
#endif
    uint8_t enable_ref;
} nlflash_spi_device_t;

const nlflash_info_t g_flash_spi_info =
{
    .name = "SPIFlash",
    .base_addr = 0,
    .size = FLASH_SPI_SIZE,
    .erase_size = FLASH_SPI_ERASE_SIZE,
    .fast_erase_size = FLASH_SPI_FAST_ERASE_SIZE,
    .write_size = FLASH_SPI_WRITE_SIZE
};

// only support one flash SPI device currently
static nlflash_spi_device_t flash_spi_device =
{
#ifdef FLASH_SPI_USE_PARTIAL_PAGE_BUFFER
    .partial_page = {0},
    .partial_page_index = 0,
    .write_loc = 0,
#endif
    .enable_ref = 0
};

static int read_register(uint8_t cmd, uint8_t *buf, uint8_t num_bytes);

static int check_chip_id(void)
{
    int retval;
    uint8_t id_buf[FLASH_SPI_READ_ID_SIZE] = {0};

#if (FLASH_SPI_SPLIT_TRANSACTIONS == 1)
    retval = nlspi_request(&g_flash_spi_slave);
    if (retval < 0)
        return retval;
#endif

    retval = read_register(CMD_RDID, id_buf, sizeof(id_buf));
    nlREQUIRE(retval >= 0, done);

#ifdef FLASH_SPI_NUM_SOURCES

    int i;

    for (i = 0 ; i < FLASH_SPI_NUM_SOURCES ; i++)
    {
        if (id_buf[0] == FLASH_SPI_MANUFACTORY_ID(i) &&
            id_buf[1] == FLASH_SPI_MEMORY_TYPE_ID(i) &&
            id_buf[2] == FLASH_SPI_MEMORY_DENSITY_ID(i))
        {
            break;
        }
    }

    if (i < FLASH_SPI_NUM_SOURCES)
    {
        FLASH_SPI_SET_SOURCE(i);
    }
    else
    {
        retval = -1; // check ID failed
    }

#else

    if (id_buf[0] != FLASH_SPI_MANUFACTORY_ID ||
        id_buf[1] != FLASH_SPI_MEMORY_TYPE_ID ||
        id_buf[2] != FLASH_SPI_MEMORY_DENSITY_ID)
    {
        retval = -1; // check ID failed
    }

#endif

done:
#if (FLASH_SPI_SPLIT_TRANSACTIONS == 1)
    nlspi_release(&g_flash_spi_slave);
#endif
    return retval;
}

int nlflash_spi_request(void)
{
    int retval = 0;
    int attempts = FLASH_SPI_NUMBER_REQUEST_ATTEMPTS;

    flash_spi_device.enable_ref++;
    if (flash_spi_device.enable_ref > 1)
    {
        goto done;
    }

    while (attempts > 0)
    {
#if (FLASH_SPI_SPLIT_TRANSACTIONS == 0)
        // request the SPI bus and keep it for duration of request.
        retval = nlspi_request(&g_flash_spi_slave);
        if (retval < 0)
        {
            // undo ref count increment
            flash_spi_device.enable_ref = 0;
            goto done;
        }
#else
        // power up the spi slave now because there's usually high
        // latency associated with doing so.  we only request the
        // spi controller during read/write transactions, to keep
        // the spi controller available to other devices that might
        // want to use it during long flash operations.
        nlspi_slave_enable(&g_flash_spi_slave);
#endif

#ifdef FLASH_SPI_USE_POWERDOWN
        // send return from deep powerdown cmd.  this must be a separate
        // transaction from the rest becaues the chip requires
        // CS to go high at end of the cmd.
        {
            const uint8_t cmd_rdp[1] = {CMD_RDP};
            retval = nlspi_write(&g_flash_spi_slave, cmd_rdp, sizeof(cmd_rdp));
            if (retval < 0)
            {
                // undo ref count increment
                flash_spi_device.enable_ref = 0;
                goto done;
            }
        }
#endif

        // Use check_chip_id() to determine whether the flash is in a valid
        // state for interaction.
        for (unsigned i = 0; i < FLASH_SPI_MAX_CHIP_ID_CHECK_COUNT; i++)
        {
            if (check_chip_id() >= 0) {
                retval = 0;
                goto done;
            }
            nlplatform_delay_ms(1);
        }

        // ID check error, turn off flash and possibly try again
#if (FLASH_SPI_SPLIT_TRANSACTIONS == 0)
        nlspi_release(&g_flash_spi_slave);
#else
        nlspi_slave_disable(&g_flash_spi_slave);
#endif
        attempts--;
    }

    // We power cycled external flash several times and were unable to get a chip ID
    // Undo ref count and return error
    flash_spi_device.enable_ref = 0;
    retval = -1;
done:
#if (FLASH_SPI_FAULT_ON_REQUEST_FAILURE)
    if (retval < 0)
    {
        __builtin_trap();
    }
#endif
    return retval;
}

int nlflash_spi_release(void)
{
    nlASSERT(flash_spi_device.enable_ref > 0);
    flash_spi_device.enable_ref--;
    if (flash_spi_device.enable_ref == 0)
    {
#ifdef FLASH_SPI_USE_POWERDOWN
        // send deep powerdown cmd
        const uint8_t cmd_dp[1] = {CMD_DP};
        nlspi_write(&g_flash_spi_slave, cmd_dp, sizeof(cmd_dp));
#endif

#if (FLASH_SPI_SPLIT_TRANSACTIONS == 0)
        // Allow access to SPI bus now that flash regulator is disabled.
        nlspi_release(&g_flash_spi_slave);
#else
        // Disable the flash spi power and control lines
        nlspi_slave_disable(&g_flash_spi_slave);
#endif
    }
    return 0;
}

// read a register with the given size
static int read_register(uint8_t cmd, uint8_t *buf, uint8_t num_bytes)
{
    nlspi_transfer_t xfers[2];

    xfers[0].tx = &cmd;
    xfers[0].rx = NULL;
    xfers[0].num = sizeof(cmd);
    xfers[0].callback = NULL;
    xfers[1].tx = NULL;
    xfers[1].rx = buf,
    xfers[1].num = num_bytes;
    xfers[1].callback = NULL;

    return nlspi_transfer(&g_flash_spi_slave, xfers, ARRAY_SIZE(xfers));
}

static int flash_is_busy(void)
{
    int retval;
    uint8_t status;
    retval = read_register(CMD_RDSR, &status, sizeof(status));
    if (retval) {
        return retval;
    }
    if ((status & M_STAT_BUSY_BIT) == M_STAT_BUSY_VALUE) {
        return -EBUSY;
    }
    return 0;
}

// Sends a multi-part transaction via spi.
// If this is a write operation, it first sends a WREN cmd as a separate
// spi transfer (CS required to go high at end of cmd).
// Then it sends the cmd+address.
// Optionally, if there is data transfer involved, it will send dummy_bytes
// and then the transfer.
static int spi_cmd_address_data(uint8_t cmd, uint32_t address, bool write, uint8_t *buf, size_t len, uint8_t dummy_bytes)
{
    int retval;
    nlspi_transfer_t xfers[2]; // max 2 transactions, one for cmd+addr+dummy, and one for data
    uint8_t cmd_buf[4];
#ifdef CMD_WREN
    const uint8_t wren[1] = {CMD_WREN};
#endif

    // for devices like at45db041e with non-binary multiple page sizes,
    // the address format is page + page offset
#if (FLASH_SPI_USE_PAGE_OFFSET_ADDRESSING == 1)
    unsigned page = address / FLASH_SPI_WRITE_SIZE;
    unsigned offset = address % FLASH_SPI_WRITE_SIZE;
    // convert address 18 bits of "address" into
    // FLASH_SPI_USE_OFFSET_BITS of offset and the
    // rest as page address and dummy bits
    address = (page << FLASH_SPI_NUM_OFFSET_BITS) | offset;
#endif
    cmd_buf[3] = ((address >> 0) & 0xFF);
    cmd_buf[2] = ((address >> 8) & 0xFF);
    cmd_buf[1] = ((address >> 16) & 0xFF);
    cmd_buf[0] = cmd;

#if (FLASH_SPI_SPLIT_TRANSACTIONS == 1)
    retval = nlspi_request(&g_flash_spi_slave);
    if (retval < 0)
        return retval;
#endif

    // abort if FLASH chip is busy
    retval = flash_is_busy();
    nlREQUIRE(retval >= 0, err_path);

#ifdef CMD_WREN
    // send WREN cmd if this is a write operation.  this must be a separate transaction
    // from the rest because the chip requires CS to go high at end of WREN cmd.
    if (write) {
        retval = nlspi_write(&g_flash_spi_slave, wren, sizeof(wren));
        nlREQUIRE(retval >= 0, err_path);
    }
#endif

    // send cmd and address + dummy bytes.  For dummy bytes, it will wind up
    // sending garbage from stack past cmd address, but that should be okay.
    xfers[0].tx = cmd_buf;
    xfers[0].rx = NULL;
    xfers[0].num = sizeof(cmd_buf) + dummy_bytes;
    xfers[0].callback = NULL;

    if (len) {
        if (write) {
            xfers[1].tx = buf;
            xfers[1].rx = NULL;
        } else {
            xfers[1].tx = NULL;
            xfers[1].rx = buf;
        }
        xfers[1].num = len;
        xfers[1].callback = NULL;
    }
    retval = nlspi_transfer(&g_flash_spi_slave, xfers, len ? 2 : 1);
    nlREQUIRE(retval >= 0, err_path);

err_path:
#if (FLASH_SPI_SPLIT_TRANSACTIONS == 1)
    nlspi_release(&g_flash_spi_slave);
#endif
    return retval;
}

// poll status register of chip until it's not busy or we timeout
static int wait_until_not_busy(uint32_t retry_cnt, uint32_t retry_delay_ms)
{
    int retval;
    uint8_t status = 0;

    do {
        nlplatform_delay_ms(retry_delay_ms);
#if (FLASH_SPI_SPLIT_TRANSACTIONS == 1)
        retval = nlspi_request(&g_flash_spi_slave);
        nlREQUIRE(retval >= 0, err_path);
#endif
        retval = read_register(CMD_RDSR, &status, sizeof(status));
#if (FLASH_SPI_SPLIT_TRANSACTIONS == 1)
        nlspi_release(&g_flash_spi_slave);
#endif
        if ((status & M_STAT_BUSY_BIT) == M_STAT_READY_VALUE) {
            break;
        }
        retry_cnt--;
    } while ((retry_cnt > 0) && (retval >= 0));
#if (FLASH_SPI_SPLIT_TRANSACTIONS == 1)
err_path:
#endif
    if (retry_cnt == 0) {
        return -EIO;
    }
    return retval;
}

static int erase_sector(uint32_t addr, bool isSubSector)
{
    int retval;
    uint8_t cmd;
    uint32_t retry_cnt;
    uint32_t retry_delay_ms;

    if (isSubSector)
    {
        cmd = CMD_SSE;
        retry_cnt = SSE_DELAY_LOOP_COUNT;
        retry_delay_ms = SSE_DELAY_MSEC;
    }
    else
    {
        cmd = CMD_SE;
        retry_cnt = SE_DELAY_LOOP_COUNT;
        retry_delay_ms = SE_DELAY_MSEC;
    }

    retval = spi_cmd_address_data(cmd, addr, true, NULL, 0, 0);
    nlREQUIRE(retval >= 0, err_path);

    retval = wait_until_not_busy(retry_cnt, retry_delay_ms);

err_path:
    return retval;
}

int nlflash_spi_init(void)
{
#ifdef FLASH_SPI_USE_PARTIAL_PAGE_BUFFER
    memset(flash_spi_device.partial_page, 0xff, sizeof(flash_spi_device.partial_page));
#endif
    return 0;
}

int nlflash_spi_erase(uint32_t addr, size_t len, size_t *retlen, nlloop_callback_fp callback)
{
    uint32_t num_sub_sectors_beg = 0;
    uint32_t num_sectors = 0;
    uint32_t num_sub_sectors_end = 0;
    uint32_t i;
    int retval;
#ifdef CMD_WREN
    const uint8_t wren[1] = {CMD_WREN};
#endif

    retval = nlflash_spi_request();
    nlREQUIRE(retval >= 0, done);

    // Check for bulk erase first
    if ((addr == 0) && (len == FLASH_SPI_SIZE))
    {
        // erase the entire chip
#if CMD_BE_ADDR
        retval = spi_cmd_address_data(CMD_BE, CMD_BE_ADDR, true, NULL, 0, 0);
#else
        nlspi_transfer_t xfer;
        uint8_t cmd = CMD_BE;

        xfer.tx = &cmd;
        xfer.rx = NULL;
        xfer.num = sizeof(cmd);
        xfer.callback = NULL;

        retval = nlspi_write(&g_flash_spi_slave, wren, sizeof(wren));
        nlREQUIRE(retval >= 0, done);

        retval  = nlspi_transfer(&g_flash_spi_slave, &xfer, 1);
#endif
        nlREQUIRE(retval >= 0, done);
        retval = wait_until_not_busy(BE_DELAY_LOOP_COUNT, BE_DELAY_MSEC);
        if (retval >= 0) {
            *retlen = FLASH_SPI_SIZE;
        }
        goto done;
    }

    // Assume we're at least sub-sector aligned (addr and length)
    if (len >= FLASH_SPI_FAST_ERASE_SIZE)
    {
        // Check sector alignment (assume power of two for sector size)
        uint32_t left_over = addr % FLASH_SPI_FAST_ERASE_SIZE;
        if (left_over)
        {
            num_sub_sectors_beg = ((FLASH_SPI_FAST_ERASE_SIZE - left_over) / FLASH_SPI_ERASE_SIZE);
        }

        // Calculate number of sectors (could do with mask and shift...)
        len -= (num_sub_sectors_beg * FLASH_SPI_ERASE_SIZE);
        num_sectors = (len / FLASH_SPI_FAST_ERASE_SIZE);

        // Finally, get the number of sub-sectors left at the end
        len -= (num_sectors * FLASH_SPI_FAST_ERASE_SIZE);
        num_sub_sectors_end = (len / FLASH_SPI_ERASE_SIZE);
    }
    else
    {
        num_sub_sectors_beg = (len / FLASH_SPI_ERASE_SIZE);
    }

    *retlen = 0;

    for (i = 0; i < num_sub_sectors_beg; i++)
    {
        retval = erase_sector(addr, true);
        nlREQUIRE(retval >= 0, done);

        addr += FLASH_SPI_ERASE_SIZE;
        *retlen += FLASH_SPI_ERASE_SIZE;

        if (callback != NULL)
        {
            retval = callback();
            nlREQUIRE(retval >= 0, done);
        }
    }

    for (i = 0; i < num_sectors; i++)
    {
        retval = erase_sector(addr, false);
        nlREQUIRE(retval >= 0, done);

        addr += FLASH_SPI_FAST_ERASE_SIZE;
        *retlen += FLASH_SPI_FAST_ERASE_SIZE;

        if (callback != NULL)
        {
            retval = callback();
            nlREQUIRE(retval >= 0, done);
        }
    }

    for (i = 0; i < num_sub_sectors_end; i++)
    {
        retval = erase_sector(addr, true);
        nlREQUIRE(retval >= 0, done);

        addr += FLASH_SPI_ERASE_SIZE;
        *retlen += FLASH_SPI_ERASE_SIZE;

        if (callback != NULL)
        {
            retval = callback();
            nlREQUIRE(retval >= 0, done);
        }
    }

done:
    nlflash_spi_release();
    return retval;
}

int nlflash_spi_read(uint32_t addr, size_t len, size_t *retlen, uint8_t *buf, nlloop_callback_fp callback)
{
    int retval;

    *retlen = 0;

    retval = nlflash_spi_request();
    nlREQUIRE(retval >= 0, done);

#if FLASH_SPI_HZ > FLASH_SPI_READ_FREQ_HZ
    // use fast read if the operating frequecy of spi is faster than the regular speed of the FLASH chip
    retval = spi_cmd_address_data(CMD_FAST_READ, addr, false, buf, len, FLASH_SPI_FAST_READ_DUMMY_CYCLES);
#else
    retval = spi_cmd_address_data(CMD_READ, addr, false, buf, len, FLASH_SPI_READ_DUMMY_CYCLES);
#endif
    nlREQUIRE(retval >= 0, done);
    *retlen = len;

done:
    nlflash_spi_release();
    return retval;
}

static int write_internal(uint32_t addr, size_t len, const uint8_t *buf)
{
    int retval = spi_cmd_address_data(CMD_PP, addr, true, (uint8_t *)buf, len, 0);
    nlREQUIRE(retval >= 0, done);

    retval = wait_until_not_busy(PP_DELAY_LOOP_COUNT, PP_DELAY_MSEC);
done:
    return retval;
}

int nlflash_spi_write(uint32_t addr, size_t len, size_t *retlen, const uint8_t *buf, nlloop_callback_fp callback)
{
#ifdef FLASH_SPI_USE_PARTIAL_PAGE_BUFFER
    uint32_t address = addr;
    const uint8_t *buffer = buf;
    size_t length = len;
    size_t written = 0;
    int retval = 0;

    // If not appending to the previously cached page, flush then acquire the cache.
    if ((length > 0) &&
        (address != ROUNDDOWN(flash_spi_device.write_loc, FLASH_EXTERNAL_WRITE_SIZE) + flash_spi_device.partial_page_index)) {
        retval = nlflash_spi_flush();
        flash_spi_device.write_loc = address;
    }

    // If starting mid-page, cache the data.
    // If the page boundary is hit, flush the cache.
    if ((retval >= 0) && (length > 0))
    {
        size_t offset = address % FLASH_EXTERNAL_WRITE_SIZE;
        size_t stride = MIN(FLASH_EXTERNAL_WRITE_SIZE - offset, length);
        uint8_t *cache = flash_spi_device.partial_page + offset;

        if (offset > 0)
        {
            memcpy(cache, buffer, stride);
            flash_spi_device.partial_page_index = offset + stride;

            address += stride;
            buffer += stride;
            length -= stride;
            written += stride;
        }

        if (flash_spi_device.partial_page_index == FLASH_EXTERNAL_WRITE_SIZE)
        {
            retval = nlflash_spi_flush();
        }
    }

    // While there's a full page of data, write the data to flash.
    if ((retval >= 0) && (length > 0))
    {
        size_t stride = FLASH_EXTERNAL_WRITE_SIZE;

        while (length >= stride)
        {
            retval = write_internal(address, stride, buffer);
            if (retval < 0)
            {
                break;
            }

            address += stride;
            buffer += stride;
            length -= stride;
            written += stride;
        }

        flash_spi_device.write_loc = address;
    }

    // Cache any data that's left.
    if ((retval >= 0) && (length > 0))
    {
        size_t offset = address % FLASH_EXTERNAL_WRITE_SIZE;
        size_t stride = length;
        uint8_t *cache = flash_spi_device.partial_page + offset;

        memcpy(cache, buffer, stride);
        flash_spi_device.partial_page_index = offset + stride;

        address += stride;
        buffer += stride;
        length -= stride;
        written += stride;
    }

    if (retlen != NULL)
    {
        *retlen = written;
    }

    return retval;
#else /* !defined(FLASH_SPI_USE_PARTIAL_PAGE_BUFFER) */
    int retval;
    *retlen = 0;

    retval = nlflash_spi_request();
    nlREQUIRE(retval >= 0, done);

    uint32_t offsetInPage = addr % FLASH_SPI_WRITE_SIZE;

    // write any leading partial pages that don't start on page boundary
    if (offsetInPage)
    {
        int partial_page_size;
        if (offsetInPage + len > FLASH_SPI_WRITE_SIZE) {
            partial_page_size = FLASH_SPI_WRITE_SIZE - offsetInPage;
        } else {
            partial_page_size = len;
        }
        retval = write_internal(addr, partial_page_size, buf);
        if (retval == 0) {
            *retlen += partial_page_size;
            if (partial_page_size == len) {
                // we wrote it all requested, we're done
                goto done;
            }
        } else {
            goto done;
        }
        len -= partial_page_size;
        buf += partial_page_size;
        addr += partial_page_size;
    }
    // write any whole pages
    while (len >= FLASH_SPI_WRITE_SIZE) {
        retval = write_internal(addr, FLASH_SPI_WRITE_SIZE, buf);
        if (retval < 0)
            break;
        len -= FLASH_SPI_WRITE_SIZE;
        addr += FLASH_SPI_WRITE_SIZE;
        buf += FLASH_SPI_WRITE_SIZE;
        *retlen += FLASH_SPI_WRITE_SIZE;
    }
    // write any trailing partial pages
    if ((retval == 0) && (len > 0)) {
        retval = write_internal(addr, len, buf);
        if (retval == 0) {
            *retlen += len;
        }
    }

done:
    nlflash_spi_release();
    return retval;
#endif /* defined(FLASH_SPI_USE_PARTIAL_PAGE_BUFFER) */
}

int nlflash_spi_flush(void)
{
#ifdef FLASH_SPI_USE_PARTIAL_PAGE_BUFFER

    int retval = 0;

    if (flash_spi_device.partial_page_index != 0)
    {
        // The write location is the destination address,
        // thus the offset into the page represents the offset
        // into the page buffer where the valid data starts.
        uint32_t address = flash_spi_device.write_loc;
        size_t offset = address % FLASH_EXTERNAL_WRITE_SIZE;
        size_t stride = flash_spi_device.partial_page_index > offset ?
                        flash_spi_device.partial_page_index - offset: 0;
        uint8_t *cache = flash_spi_device.partial_page + offset;

        retval = write_internal(address, stride, cache);

        // Reset the partial page
        if (retval >= 0)
        {
            flash_spi_device.write_loc = 0;
            flash_spi_device.partial_page_index = 0;
            memset(flash_spi_device.partial_page, 0xff, sizeof(flash_spi_device.partial_page));
        }
    }
    return retval;

#else /* !defined(FLASH_SPI_USE_PARTIAL_PAGE_BUFFER) */

    return 0;

#endif /* defined(FLASH_SPI_USE_PARTIAL_PAGE_BUFFER) */
}

int nlflash_spi_read_id(uint8_t *id_buf, size_t id_buf_size)
{
    int retval;

    if (id_buf_size < FLASH_SPI_READ_ID_SIZE) {
        return -EINVAL;
    }

#if (FLASH_SPI_SPLIT_TRANSACTIONS == 1)
    retval = nlspi_request(&g_flash_spi_slave);
    if (retval < 0)
        return retval;
#endif

    retval = nlflash_spi_request();
    nlREQUIRE(retval >= 0, done);

    retval = flash_is_busy();
    nlREQUIRE(retval >= 0, done);

    retval = read_register(CMD_RDID, id_buf, FLASH_SPI_READ_ID_SIZE);
    nlREQUIRE(retval >= 0, done);

done:
    nlflash_spi_release();

#if (FLASH_SPI_SPLIT_TRANSACTIONS == 1)
    retval = nlspi_release(&g_flash_spi_slave);
#endif

    return retval;
}

const nlflash_info_t *nlflash_spi_get_info(void)
{
    return &g_flash_spi_info;
}
#endif /* FLASH_SPI_SIZE > 0 */
