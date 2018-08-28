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
 *      This file contains wrappers for flash drivers
 *
 */

#include <nlassert.h>
#include <errno.h>
#include <nlplatform.h>

#if NL_NUM_FLASH_IDS > 0

#include <nlplatform/nlflash.h>

typedef struct
{
    int (* lock)(void *);
    int (* unlock)(void *);
    void *lock_ctx;
} flash_ctx_t;

static flash_ctx_t s_flash_ctxs[NL_NUM_FLASH_IDS];

static bool erase_alignment_is_ok(nlflash_id_t flash_id, uint32_t start, size_t len)
{
    const nlflash_info_t *flash_info = nlflash_get_info(flash_id);

    // Return true if the area to be erased starts and ends on an erase_size boundary.
    return ((start % flash_info->erase_size == 0) &&
            (len   % flash_info->erase_size == 0));
}

void nlflash_set_lock(nlflash_id_t flash_id, int ( *lock)(void *), int (* unlock)(void *), void *lock_ctx)
{
    s_flash_ctxs[flash_id].lock = lock;
    s_flash_ctxs[flash_id].unlock = unlock;
    s_flash_ctxs[flash_id].lock_ctx = lock_ctx;
}

int nlflash_lock(nlflash_id_t flash_id)
{
    if (s_flash_ctxs[flash_id].lock != NULL)
    {
        return s_flash_ctxs[flash_id].lock(s_flash_ctxs[flash_id].lock_ctx);
    }

    return 0;
}

int nlflash_unlock(nlflash_id_t flash_id)
{
    if (s_flash_ctxs[flash_id].unlock != NULL)
    {
        return s_flash_ctxs[flash_id].unlock(s_flash_ctxs[flash_id].lock_ctx);
    }

    return 0;
}

void nlflash_init(void)
{
    unsigned i;
    for (i = 0; i < NL_NUM_FLASH_IDS; i++)
    {
        // init is optional
        if (g_flash_device_table[i].init != NULL)
        {
            int err = g_flash_device_table[i].init();
            nlASSERT(err == 0);
        }
    }
}

int nlflash_request(nlflash_id_t flash_id)
{
    int retval = nlflash_lock(flash_id);

    if (retval >= 0)
    {
        // request is optional
        if (g_flash_device_table[flash_id].request)
        {
            retval = g_flash_device_table[flash_id].request();
        }
    }

    return retval;
}

int nlflash_release(nlflash_id_t flash_id)
{
    int retval = 0;
    int lock_retval;

    // release is optional
    if (g_flash_device_table[flash_id].release)
    {
        retval = g_flash_device_table[flash_id].release();
    }

    lock_retval = nlflash_unlock(flash_id);
    if ((lock_retval < 0) && (retval >= 0))
    {
        retval = lock_retval;
    }

    return retval;
}

int nlflash_flush(nlflash_id_t flash_id)
{
    int retval;
    int lock_retval;

    // flush is optional
    if (g_flash_device_table[flash_id].flush == NULL)
    {
        return 0;
    }

    lock_retval = nlflash_lock(flash_id);
    if (lock_retval < 0)
    {
        return lock_retval;
    }

    retval = g_flash_device_table[flash_id].flush();

    lock_retval = nlflash_unlock(flash_id);
    if ((lock_retval < 0) && (retval >= 0))
    {
        retval = lock_retval;
    }

    return retval;
}

int nlflash_read_id(nlflash_id_t flash_id, uint8_t *id_buf, size_t id_buf_size)
{
    int retval;
    int lock_retval;

    // read_id is optional
    if (g_flash_device_table[flash_id].read_id == NULL)
    {
        return 0;
    }

    lock_retval = nlflash_lock(flash_id);
    if (lock_retval < 0)
    {
        return lock_retval;
    }

    retval = g_flash_device_table[flash_id].read_id(id_buf, id_buf_size);

    lock_retval = nlflash_unlock(flash_id);
    if ((lock_retval < 0) && (retval >= 0))
    {
        retval = lock_retval;
    }

    return retval;
}

const nlflash_info_t *nlflash_get_info(nlflash_id_t flash_id)
{
    const nlflash_info_t *info;
    int lock_retval;

    // get_info is not optional
    nlASSERT(g_flash_device_table[flash_id].get_info != NULL);

    lock_retval = nlflash_lock(flash_id);
    if (lock_retval < 0)
    {
        return NULL;
    }

    info = g_flash_device_table[flash_id].get_info();

    lock_retval = nlflash_unlock(flash_id);
    if (lock_retval < 0)
    {
        return NULL;
    }

    return info;
}

int nlflash_erase(nlflash_id_t flash_id, uint32_t from, size_t len, size_t *retlen, nlloop_callback_fp callback)
{
    int retval;
    int lock_retval;

    // erase is not optional
    nlASSERT(g_flash_device_table[flash_id].erase != NULL);

    // Start and end of the area to be erased must be on erase_size boundaries.
    nlASSERT(erase_alignment_is_ok(flash_id, from, len));

    lock_retval = nlflash_lock(flash_id);
    if (lock_retval < 0)
    {
        return lock_retval;
    }

    retval = g_flash_device_table[flash_id].erase(from, len, retlen, callback);

    lock_retval = nlflash_unlock(flash_id);
    if ((lock_retval < 0) && (retval >= 0))
    {
        retval = lock_retval;
    }

    return retval;
}

int nlflash_read(nlflash_id_t flash_id, uint32_t from, size_t len, size_t *retlen, uint8_t *buf, nlloop_callback_fp callback)
{
    int retval;
    int lock_retval;

    // read is not optional
    nlASSERT(g_flash_device_table[flash_id].read != NULL);

    lock_retval = nlflash_lock(flash_id);
    if (lock_retval < 0)
    {
        return lock_retval;
    }

    retval = g_flash_device_table[flash_id].read(from, len, retlen, buf, callback);

    lock_retval = nlflash_unlock(flash_id);
    if ((lock_retval < 0) && (retval >= 0))
    {
        retval = lock_retval;
    }

    return retval;
}

int nlflash_write(nlflash_id_t flash_id, uint32_t to, size_t len, size_t *retlen,
                  const uint8_t *buf, nlloop_callback_fp callback)
{
    int retval;
    int lock_retval;

    // write is not optional
    nlASSERT(g_flash_device_table[flash_id].write != NULL);

    lock_retval = nlflash_lock(flash_id);
    if (lock_retval < 0)
    {
        return lock_retval;
    }

    // No need to check alignment of area to be written; our write functions
    // either accept unaligned addresses or contain their own assertions.
    retval = g_flash_device_table[flash_id].write(to, len, retlen, buf, callback);

    lock_retval = nlflash_unlock(flash_id);
    if ((lock_retval < 0) && (retval >= 0))
    {
        retval = lock_retval;
    }

    return retval;
}

#endif /* NL_NUM_FLASH_IDS > 0 */
