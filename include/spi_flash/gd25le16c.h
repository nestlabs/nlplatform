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
 *      This file contains macro defines of the GD25LE16C SPI flash chip.
 *
 */

#ifndef __GD25LE16C_H_INCLUDED__
#define __GD25LE16C_H_INCLUDED__

/* *************************************************************************
 * Configuration specific to nlflash-spi.c driver
 * *************************************************************************/

/* Uncomment the following to allow partial page buffer being used when
   Write() is called. Flush() is used to write everything in the page buffer
   out to the external flash.
*/
//#define FLASH_SPI_USE_PARTIAL_PAGE_BUFFER

/* Try for 20 msec to read chip ID. Reading a valid chip ID indicates the flash
 * is ready for interaction. Micron says the flash should complete all internal
 * processes and return a valid chip ID within 17 msec.
 */
#define FLASH_SPI_MAX_CHIP_ID_CHECK_COUNT 20

/* Special configurations */
#define FLASH_SPI_REQUEST_SPI_CONTROLLER_ON_REQUEST   1
#define FLASH_SPI_TRISTATE_CS_RST_DURING_POWER_UP     1

/* spi bus, spi freq, and chip select pin should be defined in nlplatform_product_config.h */
#define GD25LE16C_MAX_SPI_FREQ_HZ   (104000000)
#define FLASH_SPI_MODE              SPI_MODE_0

/* Size parameters */
#define FLASH_SPI_SIZE              (0x200000)              /* 2,097,152 Bytes or 2MB */
#define FLASH_SPI_ERASE_SIZE        (4096)                  /* 4KB (smallest erasable unit) */
#define FLASH_SPI_FAST_ERASE_SIZE   (0x10000)               /* 65,536 Bytes or 64KB */
#define FLASH_SPI_WRITE_SIZE        (256)                   /* writable unit */
#define FLASH_SPI_READ_ID_SIZE      (3)

#define FLASH_SPI_FAST_READ_DUMMY_CYCLES    (1) /* default number of dummy cycles is 1 byte when using FAST_READ */
#define FLASH_SPI_READ_DUMMY_CYCLES         (0) /* default number of dummy cycles is 0 byte when using regular READ */
#define FLASH_SPI_FAST_READ_FREQ_HZ         (104000000)
#define FLASH_SPI_READ_FREQ_HZ              (80000000)

/* GD25LE16C Status Register and masks */
#define CMD_RDSR            (0x05) /* to read out the status reg */
#define CMD_WRSR            (0x01) /* to write new val to the status reg */

#define M_STAT_SUS1         (1<<15) /* erase suspend status */
#define M_STAT_CMP          (1<<14) /* chip memory protection */
#define M_STAT_LB3          (1<<13) /* security reg lock */
#define M_STAT_LB2          (1<<12)
#define M_STAT_LB1          (1<<11)
#define M_STAT_SUS2         (1<<10) /* program suspend status */
#define M_STAT_QE           (1<<9) /* Quad mode enable */
#define M_STAT_SRP1         (1<<8) /* status reg protect */
#define M_STAT_SRP0         (1<<7)
#define M_STAT_BP4          (1<<6) /* level of protected block */
#define M_STAT_BP3          (1<<5) /* level of protected block */
#define M_STAT_BP2          (1<<4)
#define M_STAT_BP1          (1<<3)
#define M_STAT_BP0          (1<<2)
#define M_STAT_WEL          (1<<1) /* write enable latch */
#define M_STAT_BUSY_BIT     (1<<0) /* busy bit mask */
#define M_STAT_READY_VALUE  (0<<0) /* value of busy bit to indicate device is ready */
#define M_STAT_BUSY_VALUE   (1<<0) /* value of busy bit to indicate device is busy */

/* Register/Setting Commands */
#define CMD_WREN                    (0x06) /* write enable */
#define CMD_WRDI                    (0x04) /* write disable */
#define CMD_DP                      (0xB9) /* enter deep power down mode */
#define CMD_RDP                     (0xAB) /* release from deep power down mode */

/* Read/Write Array Commands */
#define CMD_READ                    (0x03) /* max speed 33 MHz */
#define CMD_FAST_READ               (0x0B) /* max speed 70 MHz */

#define CMD_PP                      (0x02) /* to program the selected page */
#define CMD_SSE                     (0x20) /* subsector erase: "4K subsector" is called "sector" on MX25 */
#define CMD_SE                      (0xD8) /* sector erase: "64K sector" is called "block" on MX25 */
#define CMD_BE                      (0xC7) /* bulk erase: "bulk erase" is called "chip erase" on MX25 */

/* Read device ID */
#define CMD_RDID                    (0x9F) /* read device ID */

/* GD25LE16C Chip IDs */
#define FLASH_SPI_MANUFACTORY_ID    (0xC8)
#define FLASH_SPI_MEMORY_TYPE_ID    (0x60)
#define FLASH_SPI_MEMORY_DENSITY_ID (0x15)

/* delay time for different operations. DELAY_MSEC * DELAY_LOOP_COUNT == total delay time */
#define PP_DELAY_MSEC               (1)
#define PP_DELAY_LOOP_COUNT         (4)  /* PageProgram 256B, Typ./Max.: 0.7ms/2.4ms */
#define BE_DELAY_MSEC               (500)
#define BE_DELAY_LOOP_COUNT         (21) /* ChipErase Typ./Max.: 5s/10s */
#define SE_DELAY_MSEC               (75)
#define SE_DELAY_LOOP_COUNT         (15)  /* BlockErase64K Typ./Max.: 0.18s/1s */
#define SSE_DELAY_MSEC              (10)
#define SSE_DELAY_LOOP_COUNT        (35)  /* SectorErase4K Typ./Max.: 40ms/300ms */

#endif /* __GD25LE16C_H_INCLUDED__ */
