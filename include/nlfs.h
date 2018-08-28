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
 *      This file defines a filesystem interface.
 */

#ifndef __NLFS_H_INCLUDED__
#define __NLFS_H_INCLUDED__

#include <stdint.h>
#include <stddef.h>
#if NL_FEATURE_SIMULATEABLE_HW
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* A nlfs_fileid_t is a uint8_t divided into two parts.
 * The highest 2 bits are the PARTITION_TYPE, either INTERNAL, EXTERNAL,
 * or EXTERNAL_SUBPARTITION (abbreviated INT, EXT, and EXT_SUB respectively).
 * The remaining bits are the PARTITION_ID.
 */
typedef uint8_t nlfs_fileid_t;

#define PARTITION_TYPE_MASK     (0x3<<6)
#define PARTITION_ID_MASK       (0x3f)
#define GET_PARTITION_TYPE(x)   (((x)>>6) & 0x3)
#define GET_PARTITION_ID(x)     ((x) & PARTITION_ID_MASK)
#define DEFINE_FILEID(type, id) (((type) << 6) | (id))

#define PARTITION_TYPE_INT     (0x0)
#define PARTITION_TYPE_EXT     (0x1)
#define PARTITION_TYPE_EXT_SUB (0x2)

/* Set aside max value for invalid */
#define PARITION_ID_INVALID    PARTITION_ID_MASK

#include <nlplatform/nlflash.h>

typedef enum
{
    READ_ONLY,
    WRITE_ONLY,
} nlfs_file_mode_t;

typedef enum
{
    BEGINNING,
    CURRENT,
} nlfs_origin_pos_t;

typedef enum
{
    IMAGE0,
    IMAGE1,
    INSTALLED,
    ALTERNATE,
} nlfs_image_location_t;

typedef struct
{
#if NL_FEATURE_SIMULATEABLE_HW
    FILE *fileHandle;
#endif
    uint32_t offset;
    uint32_t currentPos;
    size_t len;
    void *context;
    uint8_t partId;
    uint8_t partType;
    uint8_t chipId;
    nlfs_file_mode_t mode;
    bool isOpen;
    bool isFat;
} nlfs_file_t;
    
#ifdef BUILD_FEATURE_FAT_FILES
#include <nlfat.h>
#include <nlblocks.h>
#define NL_NUM_FAT_BUFFS    2
typedef struct
{
    nl_fat_context context;
    uint8_t readBuf[BLOCK_SIZE];
    volume_buffer_spec bufs[NL_NUM_FAT_BUFFS];
} nlfs_fat_file_context_t;
#endif

int nlfs_open_cb(nlfs_fileid_t fid,
                 nlfs_file_mode_t mode,
                 nlfs_image_location_t loc,
                 nlfs_file_t *file,
                 bool isFat,
                 void *context,
                 nlloop_callback_fp callback);
size_t nlfs_read_cb(nlfs_file_t *file, void *buf, size_t bytes, nlloop_callback_fp callback);
size_t nlfs_write_cb(nlfs_file_t *file, const void *buf, size_t bytes, nlloop_callback_fp callback);
int nlfs_close(nlfs_file_t *file);
int nlfs_getlen(const nlfs_file_t *file, size_t *len);
int nlfs_getpos(const nlfs_file_t *file, uint32_t *offset);
int nlfs_seek(nlfs_file_t *file, uint32_t offset, nlfs_origin_pos_t origin);
bool nlfs_is_open(const nlfs_file_t *file);

#define nlfs_read(file, buf, bytes) nlfs_read_cb(file, buf, bytes, NULL)
#define nlfs_write(file, buf, bytes) nlfs_write_cb(file, buf, bytes, NULL)
#define nlfs_open(fid, mode, loc, file) nlfs_open_cb(fid, mode, loc, file, false, NULL, NULL)

/* Compatibilty macros until coding conventions are settled */
#define nl_fs_file_mode_t nlfs_file_mode_t
#define nl_fs_origin_pos_t nlfs_origin_pos_t
#define nl_fs_image_location_t nlfs_image_location_t
#define nl_fs_fileid_t nlfs_fileid_t
#define nl_fs_file_t nlfs_file_t
#define nl_fs_fat_file_context_t nlfs_fat_file_context_t
#define nl_fs_open_cb nlfs_open_cb
#define nl_fs_read_cb nlfs_read_cb
#define nl_fs_write_cb nlfs_write_cb
#define nl_fs_close nlfs_close
#define nl_fs_getlen nlfs_getlen
#define nl_fs_getpos nlfs_getpos
#define nl_fs_seek nlfs_seek
#define nl_fs_is_open nlfs_is_open
#define nl_fs_read nlfs_read
#define nl_fs_write nlfs_write
#define nl_fs_open nlfs_open

#ifdef __cplusplus
}
#endif

#endif /* __NLFS_H_INCLUDED__ */
