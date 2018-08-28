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
 *      This implements a circular buffer for maintain a copy
 *      of all output sent to the console in a persistent RAM
 *      section.  It's a potentially useful debugging tool to
 *      get the context/history prior to a crash beyond just a
 *      register dump.
 *
 *      It's similar to the Android/Linux kernel feature of the
 *      same name (though more recent versions use pstore I believe)
 *      except we don't use two buffers because we've very RAM
 *      contrained in our devices.  We just use one buffer, but
 *      keep track of indices that provide info about the buffer
 *      usage on the previous boot.  The ram console should be
 *      flipped at the bootloader and continue appended to
 *      by the app.  Ideally the bootloader should not output
 *      very much to the console, and the app should early in
 *      it's boot save the previous ram console buffer contents
 *      out to persistent storage before it starts doing any
 *      console output of it's own.
 */

#include <nlplatform.h>
#include <nlplatform/nlram_console.h>
#include <string.h>

#define RAM_CONSOLE_MAGIC 0xabedface

typedef struct {
    uint32_t callback_watermark;
    uint32_t callback_bytes_written;
    nl_ram_console_cb_t callback;
} nl_ram_console_cb_ctx_t;

typedef struct {
    uint32_t magic;
    uint32_t write_index;
    uint32_t bytes_written;
    uint32_t last_write_index;
    uint32_t last_bytes_written;
    uint32_t buf_size; /* should match NL_RAM_CONSOLE_BUF_SIZE but we
                        * keep it in RAM to help us check/detect if
                        * NL_RAM_CONSOLE_BUF_SIZE has changed between
                        * images in a SW update
                        */
    bool     enabled;
    uint8_t buf[NL_RAM_CONSOLE_BUF_SIZE];
} nl_ram_console_t;

static nl_ram_console_cb_ctx_t s_ram_console_cb_ctx;
static nl_ram_console_t s_ram_console NL_SYMBOL_AT_SECTION(".ram_console");

#define TEST_DYNAMIC_RAM_CONSOLE_BUF_SIZE_CHANGES 0
#if TEST_DYNAMIC_RAM_CONSOLE_BUF_SIZE_CHANGES
// make RAM_CONSOLE_BUF_SIZE a variable to allow easier testing.
// with a debugger and a breakpoint on nl_ram_console_init(),
// the developer can reset and change the size to step through
// the conversion routine during init.
uint32_t g_ram_console_buf_size = NL_RAM_CONSOLE_BUF_SIZE;
#define RAM_CONSOLE_BUF_SIZE g_ram_console_buf_size
#else /* TEST_DYNAMIC_RAM_CONSOLE_BUF_SIZE_CHANGES */
#define RAM_CONSOLE_BUF_SIZE NL_RAM_CONSOLE_BUF_SIZE
#endif /* TEST_DYNAMIC_RAM_CONSOLE_BUF_SIZE_CHANGES */

static void init_ram_console(void)
{
    s_ram_console.magic = RAM_CONSOLE_MAGIC;
    s_ram_console.last_bytes_written = 0;
    s_ram_console.last_write_index = 0;
    s_ram_console.bytes_written = 0;
    s_ram_console.write_index = 0;
    s_ram_console_cb_ctx.callback_watermark = 0;
    s_ram_console_cb_ctx.callback_bytes_written = 0;
    s_ram_console_cb_ctx.callback = NULL;
    s_ram_console.buf_size = RAM_CONSOLE_BUF_SIZE;
}

void nl_ram_console_register_callback(nl_ram_console_cb_t aCallback, uint32_t aWatermark)
{
    s_ram_console_cb_ctx.callback_watermark = aWatermark;
    s_ram_console_cb_ctx.callback_bytes_written = 0;
    s_ram_console_cb_ctx.callback = aCallback;
}

void nl_ram_console_init_bootloader(void)
{
    if (s_ram_console.magic != RAM_CONSOLE_MAGIC)
    {
        init_ram_console();
    }
    else
    {
        /* MAGIC is valid, so push the current info to last */
        s_ram_console.last_bytes_written = s_ram_console.bytes_written;
        s_ram_console.last_write_index = s_ram_console.write_index;
        s_ram_console.bytes_written = 0;
        // current write_index is unchanged, we continue writing where we left off
    }
    // enable it if it wasn't enabled
    s_ram_console.enabled = true;
}

void nl_ram_console_init(uint8_t *aScratchBuf, size_t aScratchBufSize)
{
    // If the magic doesn't match (might happen when old bootloader
    // with no RAM console boots to an image with a RAM console, or
    // if the old bootloader had a different location for the RAM
    // console) initialize it.
    if (s_ram_console.magic != RAM_CONSOLE_MAGIC)
    {
        init_ram_console();
    }
    else if (s_ram_console.buf_size != RAM_CONSOLE_BUF_SIZE)
    {
        // if this image's buffer size is not the same as what
        // we see in s_ram_console, then something has changed.
        // we try to preserve the contents by copying it out
        // to a provided scratch buffer and then copying it
        // back.  trying to preserve in place is hard to
        // impossible if the buffer is smaller.  to keep it
        // simple, we use the same code for the case of growing
        // or shrinking.
        uint32_t last_bytes_written = s_ram_console.last_bytes_written;
        uint32_t bytes_written = s_ram_console.bytes_written;
        uint32_t bytes_to_preserve = last_bytes_written + bytes_written;
        if (bytes_to_preserve > RAM_CONSOLE_BUF_SIZE)
        {
            bytes_to_preserve = RAM_CONSOLE_BUF_SIZE;
        }
        if (bytes_to_preserve > aScratchBufSize)
        {
            bytes_to_preserve = aScratchBufSize;
        }
        if (bytes_to_preserve > 0)
        {
            uint32_t bytes_until_end;
            uint32_t old_buf_size = s_ram_console.buf_size;
            uint32_t write_index = s_ram_console.write_index;
            uint32_t start_index = write_index - bytes_to_preserve;
            if (start_index > old_buf_size)
            {
                // handle wrapping
                start_index += old_buf_size;
            }
            // start_index is the start of the log.
            // move in up to two chunks, the amount before end of the old
            // buffer, and if needed the amount from start of old buffer
            bytes_until_end = old_buf_size - start_index;
            if (bytes_until_end > bytes_to_preserve)
            {
                memcpy(aScratchBuf, &s_ram_console.buf[start_index], bytes_to_preserve);
            }
            else
            {
                memcpy(aScratchBuf, &s_ram_console.buf[start_index], bytes_until_end);
                memcpy(aScratchBuf + bytes_until_end, &s_ram_console.buf[0], bytes_to_preserve - bytes_until_end);
            }

            s_ram_console.buf_size = RAM_CONSOLE_BUF_SIZE;

            // copy back from scratch to beginning of the ram_console buffer
            memcpy(s_ram_console.buf, aScratchBuf, bytes_to_preserve);
            // now update indices and bytes_written values
            if (bytes_to_preserve == RAM_CONSOLE_BUF_SIZE)
            {
                s_ram_console.write_index = 0;
            }
            else
            {
                s_ram_console.write_index = bytes_to_preserve;
            }

            if (bytes_written > bytes_to_preserve)
            {
                bytes_written = bytes_to_preserve;
            }
            s_ram_console.bytes_written = bytes_written;
            bytes_to_preserve -= bytes_written;
            if (last_bytes_written > bytes_to_preserve)
            {
                last_bytes_written = bytes_to_preserve;
            }
            if (last_bytes_written > 0)
            {
                s_ram_console.last_bytes_written = last_bytes_written;
                s_ram_console.last_write_index = s_ram_console.write_index - s_ram_console.bytes_written;
                if (s_ram_console.last_write_index > RAM_CONSOLE_BUF_SIZE)
                {
                    s_ram_console.last_write_index += RAM_CONSOLE_BUF_SIZE;
                }
            }
            else
            {
                s_ram_console.last_bytes_written = 0;
                s_ram_console.last_write_index = 0;
            }
        }
        else
        {
            init_ram_console();
        }
    }

    // start disabled until product level code can save
    // to flash as desired before calling enable.
    s_ram_console.enabled = false;
}

void nl_ram_console_get_info(nl_ram_console_info_t *aInfo)
{
    aInfo->write_index = s_ram_console.write_index;
    aInfo->bytes_written = s_ram_console.bytes_written;
    aInfo->last_write_index = s_ram_console.last_write_index;
    aInfo->last_bytes_written = s_ram_console.last_bytes_written;
    aInfo->write_index = s_ram_console.write_index;
    aInfo->buf = s_ram_console.buf;
    aInfo->buf_size = s_ram_console.buf_size;
    aInfo->callback_watermark = s_ram_console_cb_ctx.callback_watermark;
    aInfo->callback_bytes_written = s_ram_console_cb_ctx.callback_bytes_written;
    aInfo->callback = s_ram_console_cb_ctx.callback;
}

void nl_ram_console_enable(void)
{
    s_ram_console.enabled = true;
}

void nl_ram_console_disable(void)
{
    s_ram_console.enabled = false;
}

/* Always use the s_ram_console.buf_size and not NL_RAM_CONSOLE_BUF_SIZE
 * in this function in case the constant has changed.  For example, if
 * a new updated app has a smaller ram_console than the previous image,
 * on the next boot, we want the bootloader to use that new size instead
 * of it's compiled in constant.
 */
void nl_ram_console_write(const uint8_t *data, size_t len)
{
    size_t orig_len = len;

    /* it's hard to read interleaved content so make the write atomic */
    nlplatform_interrupt_disable();

    if (s_ram_console.enabled)
    {
        const uint32_t buf_size = s_ram_console.buf_size;
        uint32_t non_current_bytes;
        while (len > 0)
        {
            size_t buf_size_before_wrap = buf_size - s_ram_console.write_index;
            size_t bytes_to_write;
            if (len < buf_size_before_wrap)
            {
                bytes_to_write = len;
            }
            else
            {
                bytes_to_write = buf_size_before_wrap;
            }
            memcpy(&s_ram_console.buf[s_ram_console.write_index],
                    data, bytes_to_write);
            data += bytes_to_write;
            len -= bytes_to_write;
            s_ram_console.write_index += bytes_to_write;
            if (s_ram_console.write_index >= buf_size)
            {
                s_ram_console.write_index = 0;
            }
        }
        s_ram_console.bytes_written += orig_len;
        if (s_ram_console.bytes_written > buf_size)
        {
            s_ram_console.bytes_written = buf_size;
        }
        // adjust last_bytes_written.  total should never be more than the buf_size
        // so the last log shrinks as the current one grows
        non_current_bytes = buf_size - s_ram_console.bytes_written;
        if (s_ram_console.last_bytes_written > non_current_bytes)
        {
            s_ram_console.last_bytes_written = non_current_bytes;
        }

        // Call a high watermark callback if required
        if (s_ram_console_cb_ctx.callback != NULL)
        {
            s_ram_console_cb_ctx.callback_bytes_written += orig_len;
            if (s_ram_console_cb_ctx.callback_bytes_written >= s_ram_console_cb_ctx.callback_watermark)
            {
                s_ram_console_cb_ctx.callback();
                s_ram_console_cb_ctx.callback_bytes_written = 0;
            }
        }
    }

    nlplatform_interrupt_enable();
}
