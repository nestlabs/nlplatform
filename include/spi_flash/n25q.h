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
 *      This file contains macro defines of the N25Q SPI flash chip.
 *
 */

#ifndef __N25Q_H_INCLUDED__
#define __N25Q_H_INCLUDED__

/* spi bus, spi freq, and chip select pin should be defined in nlproduct_config.h */
#define N25Q_MAX_SPI_FREQ_HZ        (108000000)             /* 108MHz */
#define FLASH_SPI_MODE              SPI_MODE_0

/* Size parameters */
#define FLASH_SPI_SIZE              (0x200000)              /* 2,097,152 Bytes or 2MB */
#define FLASH_SPI_ERASE_SIZE        (4096)                  /* 4KB (smallest erasable unit) */
#define FLASH_SPI_FAST_ERASE_SIZE   (0x10000)               /* 65,536 Bytes or 64KB */
#define FLASH_SPI_WRITESIZE         (256)                   /* writable unit */
#define FLASH_SPI_READ_ID_SIZE      (20)

#define FLASH_SPI_FAST_READ_DUMMY_CYCLES (1) /* default number of dummy cycles is 1 byte when using FAST_READ */
#define FLASH_SPI_READ_DUMMY_CYCLES      (0) /* default number of dummy cycles is 0 byte when using regular READ */
#define FLASH_SPI_FAST_READ_FREQ_HZ  (104000000) /* 104MHz */
#define FLASH_SPI_READ_FREQ_HZ       (50000000)  /* 50MHz */

/* Status Register and masks */
#define CMD_RDSR                    (0x05) /* to read out the status reg */
#define CMD_WRSR                    (0x01) /* to write new val to the status reg */

#define M_STAT_SRWD        (1<<7) /* status reg write disable */
#define M_STAT_BOT         (1<<6)
#define M_STAT_BP3         (1<<5) /* level of protected block */
#define M_STAT_BP2         (1<<4)
#define M_STAT_BP1         (1<<3)
#define M_STAT_BP0         (1<<2)
#define M_STAT_WEL         (1<<1) /* write enable latch */
#define M_STAT_BUSY_BIT    (1<<0) /* busy bit mask */
#define M_STAT_READY_VALUE  (0<<0) /* value of busy bit to indicate device is ready */
#define M_STAT_BUSY_VALUE   (1<<0) /* value of busy bit to indicate device is busy */

/* Register/Setting Commands */
#define CMD_WREN                    (0x06) /* write enable */
#define CMD_WRDI                    (0x04) /* write disable */

/* Read/Write Array Commands */
#define CMD_READ                    (0x03) /* max speed 50 MHz */
#define CMD_FAST_READ               (0x0B) /* max speed 108 MHz */

#define CMD_PP                      (0x02) /* to program the selected page */
#define CMD_SSE                     (0x20) /* subsector erase: "4K subsector" is called "sector" on MX25 */
#define CMD_SE                      (0xD8) /* sector erase: "64K sector" is called "block" on MX25 */
#define CMD_BE                      (0xC7) /* bulk erase: "bulk erase" is called "chip erase" on MX25 */

/* Read device ID */
#define CMD_RDID                    (0x9E) /* read device ID */

/* Chip IDs */
#define FLASH_SPI_MANUFACTORY_ID    (0x20)
#define FLASH_SPI_MEMORY_TYPE_ID    (0xBB)
#define FLASH_SPI_MEMORY_DENSITY_ID (0x15)

/* delay time for different operations. DELAY_MSEC * DELAY_LOOP_COUNT == total delay time */
#define PP_DELAY_MSEC               (1)
#define PP_DELAY_LOOP_COUNT         (200)   /* max delay 200ms */
#define BE_DELAY_MSEC               (2)
#define BE_DELAY_LOOP_COUNT         (15000) /* max delay 150s */
#define SE_DELAY_MSEC               (2)
#define SE_DELAY_LOOP_COUNT         (2000)  /* max delay 4s */
#define SSE_DELAY_MSEC              (2)
#define SSE_DELAY_LOOP_COUNT        (2000)  /* max delay 4s */

#define FLASH_SPI_POWER_UP_MAX_DELAY_MSEC     (1)
#define FLASH_SPI_POWER_DOWN_MAX_DELAY_USEC   (100)

#endif /* __N25Q_H_INCLUDED__ */
