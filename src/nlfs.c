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
 *      This file implements the a filesystem interface.
 *      FAT is currently not supported.
 */

#include <nlplatform.h>

#if NL_NUM_FLASH_IDS > 0 /* No nlfs if no flash */
#include <errno.h>
#include <string.h>
#include <nlplatform/nlfs.h>
#include <nlplatform/nlflash.h>
#include <nlplatform/nlcrc.h>
#include <nlenv.h>
#include <nlassert.h>
#include "nlelf-loader.h"

/* Product wide defaults are used if nlfs specific values aren't specified
 * in nlproduct_config.h
 */
#ifndef NL_FS_CRC_TRANSPOSE_WRITE
#define NL_FS_CRC_TRANSPOSE_WRITE NLCRC_TRANSPOSE_WRITE_DEFAULT
#endif
#ifndef NL_FS_CRC_TRANSPOSE_READ
#define NL_FS_CRC_TRANSPOSE_READ NLCRC_TRANSPOSE_READ_DEFAULT
#endif
#ifndef NL_FS_CRC_XOR_ON_READ
#define NL_FS_CRC_XOR_ON_READ NLCRC_XOR_ON_READ_DEFAULT
#endif
#ifndef NL_FS_CRC_LEN
#define NL_FS_CRC_LEN NLCRC_LEN_DEFAULT
#endif
#ifndef NL_FS_CRC_POLY
#define NL_FS_CRC_POLY NLCRC_POLY_DEFAULT
#endif
#ifndef NL_FS_CRC_SEED
#define NL_FS_CRC_SEED NLCRC_SEED_DEFAULT
#endif

static int readFile(void* buf, uint32_t from, size_t len, void* context)
{
    size_t retlen;
    return nlflash_read(NLFLASH_EXTERNAL, from, len, &retlen, buf, NULL);
}

static int calcCrc(void *buf, size_t len, void * context)
{
    uint32_t *crc_value = (uint32_t *)context;
    *crc_value = nlcrc_compute(*crc_value, buf, len);
    return *crc_value;
}

#ifdef BUILD_FEATURE_FAT_FILES
/*
 * use linker script to set fat functions to stub routines if
 * fat support isn't wanted in the binary (like in AUPD case).
 */
int fatfileinit(uint8_t partId, nlfs_fat_file_context_t *fatFileContext) LINKER_REPLACEABLE_FUNCTION(fatfileinit_default);
void fatfiledeinit(nl_fat_context *context) LINKER_REPLACEABLE_FUNCTION(fatfiledeinit_default);
int fatfileread( void* outBuffer, uint32_t inAddress, size_t inSize, nl_fat_context *contex) LINKER_REPLACEABLE_FUNCTION(fatfileread_default);

/* not static so the compiler doesn't inline them and prevent linker script replacement from working */
int fatfileread_default(void* outBuffer, uint32_t inAddress, size_t inSize, nl_fat_context *context);
int fatfileinit_default(uint8_t partId, nlfs_fat_file_context_t *fatFileContext);
void fatfiledeinit_default(nl_fat_context *context);

int fatfileinit_default(uint8_t partId, nlfs_fat_file_context_t *fatFileContext)
{
    int partitionNum;
    int retval;

    partitionNum = (partId == GET_PARTITION_ID(kImage0)) ? 0 : 1;

    retval = nl_fat_init_context(&fatFileContext->context, fatFileContext->bufs, NL_NUM_FAT_BUFFS, partitionNum);
    return retval;
}

void fatfiledeinit_default(nl_fat_context *context)
{
    nl_fat_deinit_context(context);
}

int fatfileread_default(void* outBuffer, uint32_t inAddress, size_t inSize, nl_fat_context *context)
{
    return nl_fat_read_file(outBuffer, inAddress, inSize, context);
}

#endif /* BUILD_FEATURE_FAT_FILES */

static void get_image_offset(nlfs_image_location_t loc, uint8_t *partId, uint32_t *offset)
{
    // Default to Image0
    *partId = GET_PARTITION_ID(kImage0);

    if (loc == IMAGE1)
    {
        *partId = GET_PARTITION_ID(kImage1);
    }
    else if (loc != IMAGE0)
    {
        char currentImage[8];

        int error = nl_env_get_string(kCurrentImageKey, currentImage, sizeof(currentImage));

        if (error >= 0)
        {
            // Check if should open Image1 instead
            if ( ((strcmp(currentImage, kImageValue0) == 0) && (loc == ALTERNATE)) ||
                 ((strcmp(currentImage, kImageValue1) == 0) && (loc == INSTALLED)) )
            {
                *partId = GET_PARTITION_ID(kImage1);
            }
        }
        else
        {
            // Set to default Image0 if current_image didn't exist
            nl_env_set_string(kCurrentImageKey, kImageValue0);
        }
    }

    *offset = g_flash_partitions[*partId].offset;
}

static int fileInit(nlfs_fileid_t fid, nlfs_file_mode_t mode, nlfs_image_location_t loc, bool isFat, void *context, nlfs_file_t *file)
{
    int retval = 0;

    file->currentPos = 0;
    file->partId = GET_PARTITION_ID(fid);
    file->partType = GET_PARTITION_TYPE(fid);
    file->chipId = (file->partType == PARTITION_TYPE_INT) ? NLFLASH_INTERNAL : NLFLASH_EXTERNAL;
    file->mode = mode;
    file->isOpen = true;
    file->context = context;
    file->isFat = isFat;

    // If main partition
    if (file->partType != PARTITION_TYPE_EXT_SUB)
    {
        if (file->partId != GET_PARTITION_ID(kImage))
        {
            file->offset = g_flash_partitions[file->partId].offset;
            file->len = g_flash_partitions[file->partId].size;
#if !defined(BUILD_CONFIG_RELEASE) && defined(MAX_ALLOWED_WAV_LENGTH)
            if (file->partId == GET_PARTITION_ID(kCustomAudio))
            {
                file->len = MAX_ALLOWED_WAV_LENGTH;
            }
#endif
        }
        else
        {
            // Overwrite the partition ID if kImage
            get_image_offset(loc, &file->partId, &file->offset);

            file->len = g_flash_partitions[file->partId].size;

#ifdef BUILD_FEATURE_FAT_FILES
            if (isFat)
            {
                nlfs_fat_file_context_t *fatFileContext = (nlfs_fat_file_context_t *)(file->context);
                retval = fatfileinit(file->partId, fatFileContext);
            }
#endif
        }
    }
    // If sub partition
    else
    {
        uint32_t imageOffset;
        uint8_t partId;
        elfSectionDescription_t section;
        elfReaderHandle_t elfReader;
        uint32_t crc_value = NL_FS_CRC_SEED;

        get_image_offset(loc, &partId, &imageOffset);

        if (!isFat)
        {
            // readFile() accesses external flash.  readFile() will get called repeatedly within
            // the call to elf_find_section_crc(...).  It is necessary that the flash lock and CRC
            // lock are acquired in the same order as what is used by getenv(). That order is
            // flash lock followed by crc lock.  this flash_request aims to ensure that order is
            // maintained, and avoids deadlocks that were witnessed in operation.
            nlflash_request(NLFLASH_EXTERNAL);
            elf_loader_init(&elfReader, calcCrc, readFile, imageOffset, NULL, (void *)&crc_value);
        }
        else
        {
#ifdef BUILD_FEATURE_FAT_FILES
            nlfs_fat_file_context_t *fatFileContext = (nlfs_fat_file_context_t *)(file->context);
            retval = fatfileinit(partId, fatFileContext);
            nlREQUIRE(retval >= 0, done);
            elf_loader_init(&elfReader, calcCrc, (elfReadFunctionPtr_t)fatfileread, imageOffset,
                            (void *)&fatFileContext->context, (void *)&crc_value);
#else
            retval = -EINVAL;
            goto done;
#endif
        }

        nlcrc_request(NL_FS_CRC_TRANSPOSE_WRITE,
                      NL_FS_CRC_TRANSPOSE_READ,
                      NL_FS_CRC_XOR_ON_READ,
                      NL_FS_CRC_LEN,
                      NL_FS_CRC_POLY);

        retval = elf_find_section_crc(&elfReader, g_sub_partition_info[file->partId].name, &section);
        nlcrc_release();

        if (!isFat)
        {
            // This release pairs with nlflash_request up above when the isFat == false.
            nlflash_release(NLFLASH_EXTERNAL);
        }

        nlREQUIRE(retval >= 0, done);

        file->len = section.size;
        file->offset = elfReader.headerOffset + section.offset;
    }

done:
    return retval;
}

int nlfs_open_cb(nlfs_fileid_t fid, nlfs_file_mode_t mode, nlfs_image_location_t loc, nlfs_file_t *file, bool isFat, void *context, nlloop_callback_fp callback)
{
    int retval;
    // Cannot write to a sub-partition
    if ((mode != READ_ONLY)
       && (GET_PARTITION_TYPE(fid) == PARTITION_TYPE_EXT_SUB))
    {
        return -EINVAL;
    }

    // Only have alternates for sub partitions and kImage
    if ((loc != INSTALLED) &&
        (GET_PARTITION_TYPE(fid) != PARTITION_TYPE_EXT_SUB) &&
        (GET_PARTITION_ID(fid) != GET_PARTITION_ID(kImage)))
    {
        return -EINVAL;
    }

    retval = fileInit(fid, mode, loc, isFat, context, file);

    if (retval >= 0)
    {
        // Erase partition if opening for writing
        if (mode == WRITE_ONLY)
        {
            if (!g_flash_partitions[file->partId].isReadOnly)
            {
                size_t retlen;

                // Get the device and tell it to do its own locking during the erase
                retval = nlflash_erase(file->chipId, file->offset, file->len, &retlen, callback);
                if ((retval >= 0) && (retlen != file->len))
                {
                    retval = -EIO;
                }
            }
            else
            {
                retval = -EINVAL;
            }
        }
    }

    return retval;
}

size_t nlfs_read_cb(nlfs_file_t *file, void *buf, size_t bytes, nlloop_callback_fp callback)
{
    int retval = 0;
    size_t len = bytes;
    uint32_t from;
    size_t retlen = 0;

    if (!file->isOpen || (file->mode != READ_ONLY))
    {
        return -EINVAL;
    }

    from = file->offset + file->currentPos;
    if ((file->currentPos + bytes) > file->len)
    {
        len = file->len - file->currentPos;
    }

    if (file->isFat == false)
    {
        retval = nlflash_read(file->chipId, from, len, &retlen, (uint8_t *)buf, callback);
    } else {
#ifdef BUILD_FEATURE_FAT_FILES
        nlfs_fat_file_context_t *fatFileContext = (nlfs_fat_file_context_t *)(file->context);
        nl_fat_context *fatContext = &fatFileContext->context;

        retval = fatfileread(buf, from, len, fatContext);
#else
        return -EINVAL;
#endif
    }

    if (retval >= 0)
    {
        file->currentPos += retlen;
    }

    return retlen;
}

size_t nlfs_write_cb(nlfs_file_t *file, const void *buf, size_t bytes, nlloop_callback_fp callback)
{
    int retval = 0;
    size_t len = bytes;
    size_t retlen = 0;

    if (!file->isOpen
       || (file->mode != WRITE_ONLY)
       || (file->isFat == true))
    {
        return retval;
    }

    // Don't write past the end of the file
    if ((file->currentPos + bytes) > file->len)
    {
        len = file->len - file->currentPos;
    }

    retval = nlflash_write(file->chipId, file->offset + file->currentPos, len, &retlen, (uint8_t *)buf, callback);
    file->currentPos += retlen;

    return retlen;
}

int nlfs_close(nlfs_file_t *file)
{
    int retval = 0;

    if (file->isOpen == false)
        return -EINVAL;

    if (file->mode == WRITE_ONLY)
    {
        retval = nlflash_flush(file->chipId);
        if (retval >= 0)
        {
            file->isOpen = false;
        }
    }

#ifdef BUILD_FEATURE_FAT_FILES
    if (file->isFat == true)
    {
        nlfs_fat_file_context_t *fatFileContext = (nlfs_fat_file_context_t *)(file->context);
        fatfiledeinit(&fatFileContext->context);
    }
#endif

    return retval;
}

int nlfs_seek(nlfs_file_t *file, uint32_t offset, nlfs_origin_pos_t origin)
{
    int retval = 0;

    if ((file->isOpen != true)
       || (file->mode != READ_ONLY)
       || (file->isFat == true))
    {
        return -EINVAL;
    }

    if (origin == BEGINNING)
    {
        // Make sure offset isn't past the end
        if (offset >= file->len)
        {
            return -EINVAL;
        }
        file->currentPos = offset;
    }
    else
    {
        if (file->currentPos + offset >= file->len)
            return -EINVAL;
        file->currentPos += offset;
    }
    return retval;
}

int nlfs_getpos(const nlfs_file_t *file, uint32_t *offset)
{
    if (file->isOpen == false)
        return -EINVAL;

    *offset = file->currentPos;

    return 0;
}

int nlfs_getlen(const nlfs_file_t *file, size_t *len)
{
    if ((file->isOpen == false) || (file->mode != READ_ONLY))
    {
        return -EINVAL;
    }

    *len = file->len;

    return 0;
}

bool nlfs_is_open(const nlfs_file_t *file)
{
    return file->isOpen;
}

#endif /* NL_NUM_FLASH_IDS > 0 */
