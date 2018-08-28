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
 *      This file defines an APi for accessing flash through a
 *      SPI interface.
 *
 */

#ifndef __NLFLASH_SPI_H_INCLUDED__
#define __NLFLASH_SPI_H_INCLUDED__

#include <nlplatform/nlflash.h>
#include <nlplatform/nlspi.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const nlspi_slave_t g_flash_spi_slave;
extern const nlflash_info_t g_flash_spi_info;

int nlflash_spi_init(void);
int nlflash_spi_request(void);
int nlflash_spi_release(void);
const nlflash_info_t *nlflash_spi_get_info(void);
int nlflash_spi_read_id(uint8_t *id_buf, size_t id_buf_size);
int nlflash_spi_erase(uint32_t addr, size_t len, size_t *retlen, nlloop_callback_fp callback);
int nlflash_spi_read(uint32_t addr, size_t len, size_t *retlen, uint8_t *buf, nlloop_callback_fp callback);
int nlflash_spi_write(uint32_t addr, size_t len, size_t *retlen, const uint8_t *buf, nlloop_callback_fp callback);
int nlflash_spi_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* __NLFLASH_SPI_H_INCLUDED__ */
