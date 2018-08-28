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
 *      This file defines the interface for a persistent ram console.
 *      The ram console is a two-copy circular buffer used for logging.
 *      It is located in persistent RAM, so that it is possible in a
 *      subsequent boot to see the logs of a previous run.
 */

#ifndef __NLRAM_CONSOLE_H_INCLUDED__
#define __NLRAM_CONSOLE_H_INCLUDED__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*nl_ram_console_cb_t)(void);

typedef struct {
    uint32_t write_index;
    uint32_t bytes_written;
    uint32_t last_write_index;
    uint32_t last_bytes_written;
    uint32_t callback_watermark;
    uint32_t callback_bytes_written;
    nl_ram_console_cb_t callback;
    const uint8_t *buf;
    uint32_t buf_size;
} nl_ram_console_info_t;

/* Scratch buffer is optional (pass NULL,0 if none) that gives
 * init a work place for converting the ram_console if the size
 * has changed between images.  Conversion is never done in the
 * bootloader, just the app init, since we don't expect bootloaders
 * to change very often or ever in the field.  Conversion only
 * works if the new size is bigger, or smaller but the items in RAM
 * that overlap the old buffer size don't get touched before
 * init is called.  The buffer should be as large as the buffer
 * being converted or else only the last aScratchBufSize bytes will
 * be preserved.
 */
void nl_ram_console_init(uint8_t *aScratchBuf, size_t aScratchBufSize);
/* Bootloader should use this init function instead of the other
 * one.  It only initializes the ram_console if it's not currently
 * valid, like on a cold boot.
 */
void nl_ram_console_init_bootloader(void);

/* Enable new data to be written to the RAM console */
void nl_ram_console_enable(void);

/* Disable new data being written to the RAM console */
void nl_ram_console_disable(void);

/* Fetch information about the RAM console
 * aInfo - pointer to nl_ram_console_info_t that should be
 * populated with RAM console info
 */
void nl_ram_console_get_info(nl_ram_console_info_t *aInfo);

/* Write data to the RAM console.
 * aData - pointer to data to be written to RAM console
 * aLen - number of bytes to write
 */
void nl_ram_console_write(const uint8_t *aData, size_t aLen);

/* Register a callback to call when RAM console has accumulated
 * the specified number of characters.
 * aCallback - callback to be called when fullness threshold reached
 * aWatermark - callback will be called after accumulating aWatermark bytes
 * To disable callbacks, call this function with aCallback=NULL, aWatermark=0
 */
void nl_ram_console_register_callback(nl_ram_console_cb_t aCallback, uint32_t aWatermark);

#ifdef __cplusplus
}
#endif

#endif /* __NLRAM_CONSOLE_H_INCLUDED__ */
