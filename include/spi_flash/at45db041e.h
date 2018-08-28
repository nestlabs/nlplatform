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
 *      This file contains macro defines of the AT45DB041E SPI flash chip.
 *
 */

#ifndef __AT45DB041E_H_INCLUDED__
#define __AT45DB041E_H_INCLUDED__

/* *************************************************************************
 * Configuration specific to nlflash-spi.cpp driver
 * *************************************************************************/

/* Uncomment the following to allow partial page buffer being used when 
   Write() is called. Flush() is used to write everything in the page buffer
   out to the external flash.
*/
//#define FLASH_SPI_USE_PARTIAL_PAGE_BUFFER

/* spi bus, spi freq, and chip select pin should be defined in nlproduct_config.h */
#define AT45DB041E_MAX_SPI_FREQ_HZ  (85000000)              // 85MHz
#define FLASH_SPI_MODE              (SPI_MODE_0)

/* Size parameters */
#define FLASH_SPI_SIZE              (2048 * FLASH_SPI_WRITE_SIZE)  // 2048 * 264 = 528KB
#define FLASH_SPI_ERASE_SIZE        (FLASH_SPI_WRITE_SIZE)         // has an 8 page block erase option too
#define FLASH_SPI_FAST_ERASE_SIZE   (256 * FLASH_SPI_WRITE_SIZE)   // 256 pages
#define FLASH_SPI_WRITE_SIZE        (264)                   // writable unit
#define FLASH_SPI_READ_ID_SIZE      (3)                     // number of bytes of ID

#define FLASH_SPI_FAST_READ_DUMMY_CYCLES (1) /* default number of dummy cycles is 1 byte when using FAST_READ */
#define FLASH_SPI_READ_DUMMY_CYCLES      (0) /* default number of dummy cycles is 0 byte when using regular READ */
#define FLASH_SPI_FAST_READ_FREQ_HZ  (85000000)  /* 85MHz */
#define FLASH_SPI_READ_FREQ_HZ       (15000000)  /* 15MHz */

/* AT45DB041EFBAI Status Register and masks */
#define CMD_RDSR                    (0xD7) // to read out the status reg

#define M_STAT_BUSY_BIT         (1<<7) // bit that indicates device is busy or ready
#define M_STAT_READY_VALUE  (1<<7) /* value of busy bit to indicate device is ready */
#define M_STAT_BUSY_VALUE   (0<<7) /* value of busy bit to indicate device is busy */


// Register/Setting Commands
#define CMD_WPSEL                   (0x68) // to enter and enable individual block protect mode
#define CMD_DP                      (0x79) // enter ultra-deep power down mode
#define CMD_RDP                     (0xAB) // release from deep power down mode
#define CMD_SBL                     (0xC0) // to set burst length

// Read/Write Array Commands
#define CMD_READ                    (0x01) // low power continous read, max speed 15 MHz
#define CMD_FAST_READ               (0x0B) // max speed 85 MHz

#define CMD_PP                      (0x02) /* to program the selected page, without built-in erase */
#define CMD_SSE                     (0x81) /* subsector erase: "256byte subsector" is called "page" on AT45 */
#define CMD_SE                      (0x7C) /* sector erase: "64K sector" is called "sector" on AT45 */
#define CMD_BE                      (0xC7) /* bulk erase: "bulk erase" is called "chip erase" on AT45 */
#define CMD_BE_ADDR                 (0x94809A) /* chip erase needs these three additional opcode bytes, we send it as "addr" */

// Read device ID
#define CMD_RDID                    (0x9F) /* read device ID */

// Disable lockdown sequence
#define CMDS_LOCKDOWN_DISABLE		0x34, 0x55, 0xAA, 0x40
#define LEN_LOCKDOWN_DISABLE		(4)

// Chip IDs
#define FLASH_SPI_MANUFACTORY_ID    (0x1F)
#define FLASH_SPI_MEMORY_TYPE_ID    (0x24)
#define FLASH_SPI_MEMORY_DENSITY_ID (0x00)

/* delay time for different operations. DELAY_MSEC * DELAY_LOOP_COUNT == total delay time */
#define PP_DELAY_MSEC               (1)
#define PP_DELAY_LOOP_COUNT         (5)  /* PageProgram 256B, Typ. 1.5ms, Max. 3ms */
#define BE_DELAY_MSEC               (500)
#define BE_DELAY_LOOP_COUNT         (40) /* ChipErase Typ. 6s, Max. 17s */
#define SE_DELAY_MSEC               (100)
#define SE_DELAY_LOOP_COUNT         (20) /* SectorErase64K Typ. 700ms, Max. 1100ms */
#define SSE_DELAY_MSEC              (10)
#define SSE_DELAY_LOOP_COUNT        (5)  /* SubSectorErase2K Typ. 30ms, Max. 35ms */

/* These values are based on the data sheet but a product
 * might want to use longer values based on how the chip
 * has been connected (e.g. if no active discharge, longer
 * power down delay might be necessary).
 */
#define FLASH_SPI_POWER_UP_MAX_DELAY_MSEC     (4)
#define FLASH_SPI_POWER_DOWN_MAX_DELAY_USEC   (1000)

// for the non-multiple power of two page size, configure nlflash_spi
// driver to use page+offset addressing
#define FLASH_SPI_USE_PAGE_OFFSET_ADDRESSING 1
#define FLASH_SPI_NUM_OFFSET_BITS 9 // 264 bytes needs 9 bits of offset

#endif /* __AT45DB041E_H_INCLUDED__ */
