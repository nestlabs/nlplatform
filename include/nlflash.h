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
 *      This file defines an API for accessing flash.
 *
 */

#ifndef __NLFLASH_H_INCLUDED__
#define __NLFLASH_H_INCLUDED__

#include <stdint.h>
#include <stddef.h>

/* nlflash_id_t is provided by nlproduct_config.h, which is included via nlplatform.h
 * nlplatform.h includes nlplatform_soc.h, which may include soc specific flash driver
 * header, which needs nlflash_info_t.  To break circular dependency, define
 * nlflash_info_t before including nlplatform.h.
 */
typedef struct {
    const char *name;
    uint32_t base_addr;
    uint32_t size;
    uint32_t erase_size;
    uint32_t fast_erase_size;
    uint32_t write_size;
} nlflash_info_t;

#include <nlplatform.h>

#ifdef __cplusplus
extern "C" {
#endif

/* For operations that might take a long time and be done in
 * multiple steps (i.e. in a loop), a callback can be specified
 * that will be invoked in between steps to allow callers to
 * do something like cancel, or pet watchdog, etc.
 */

void nlflash_init(void);
int nlflash_request(nlflash_id_t flash_id);
int nlflash_release(nlflash_id_t flash_id);
int nlflash_flush(nlflash_id_t flash_id);
int nlflash_read_id(nlflash_id_t flash_id, uint8_t *id_buf, size_t id_buf_size);
const nlflash_info_t *nlflash_get_info(nlflash_id_t flash_id);
int nlflash_erase(nlflash_id_t flash_id, uint32_t from, size_t len, size_t *retlen, nlloop_callback_fp callback);
int nlflash_read(nlflash_id_t flash_id, uint32_t from, size_t len, size_t *retlen, uint8_t *buf, nlloop_callback_fp callback);
int nlflash_write(nlflash_id_t flash_id, uint32_t to, size_t len, size_t *retlen, const uint8_t *buf, nlloop_callback_fp callback);
void nlflash_set_lock(nlflash_id_t flash_id, int ( *lock)(void *), int (* unlock)(void *), void *lock_ctx);
int nlflash_lock(nlflash_id_t flash_id);
int nlflash_unlock(nlflash_id_t flash_id);

typedef struct nlflash_func_table_s
{
    int (*init)(void);
    int (*request)(void);
    int (*release)(void);
    int (*flush)(void);
    int (*read_id)(uint8_t *id_buf, size_t id_buf_size);
    const nlflash_info_t * (*get_info)(void);
    int (*erase)(uint32_t from, size_t len, size_t *retlen, nlloop_callback_fp inLoopCallback);
    int (*read)(uint32_t from, size_t len, size_t *retlen, uint8_t *buf, nlloop_callback_fp inLoopCallback);
    int (*write)(uint32_t to, size_t len, size_t *retlen, const uint8_t *buf, nlloop_callback_fp inLoopCallback);
} nlflash_func_table_t;

extern const nlflash_func_table_t g_flash_device_table[NL_NUM_FLASH_IDS];

/* Compatibilty macros until coding conventions are settled */
#define nl_flash_read nlflash_read
#define nl_flash_init nlflash_init
#define nl_flash_request nlflash_request
#define nl_flash_release nlflash_release
#define nl_flash_flush nlflash_flush
#define nl_flash_read_id nlflash_read_id
#define nl_flash_get_info nlflash_get_info
#define nl_flash_erase nlflash_erase
#define nl_flash_read nlflash_read
#define nl_flash_write nlflash_write
#define nl_flash_set_lock nlflash_set_lock
#define nl_flash_lock nlflash_lock
#define nl_flash_unlock nlflash_unlock
#define nl_flash_func_table_t nlflash_func_table_t
#define nl_flash_info_t nlflash_info_t

#ifdef __cplusplus
}
#endif

#endif /* __NLFLASH_H_INCLUDED__ */
