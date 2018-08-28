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
 *      This file defines an API for hw accelerated CRC
 *
 */

#ifndef __NLCRC_H_INCLUDED__
#define __NLCRC_H_INCLUDED__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    kTransposeTypeWriteNone = 0,
    kTransposeTypeWriteBitsOnly,
    kTransposeTypeWriteBoth,
    kTransposeTypeWriteBytesOnly,
} nlcrc_transpose_write_t;

typedef enum
{
    kTransposeTypeReadNone = 0,
    kTransposeTypeReadBitsOnly,
    kTransposeTypeReadBoth,
    kTransposeTypeReadBytesOnly,
} nlcrc_transpose_read_t;

typedef enum
{
    kCrcLen8Bits = 0,
    kCrcLen16Bits,
    kCrcLen32Bits
} nlcrc_len_t;

#ifdef NLCRC_TRANSPOSE_WRITE_PRODUCT_DEFAULT
#define NLCRC_TRANSPOSE_WRITE_DEFAULT NLCRC_TRANSPOSE_WRITE_PRODUCT_DEFAULT
#else
#define NLCRC_TRANSPOSE_WRITE_DEFAULT kTransposeTypeWriteNone
#endif

#ifdef NLCRC_TRANSPOSE_READ_PRODUCT_DEFAULT
#define NLCRC_TRANSPOSE_READ_DEFAULT NLCRC_TRANSPOSE_READ_PRODUCT_DEFAULT
#else
#define NLCRC_TRANSPOSE_READ_DEFAULT kTransposeTypeReadNone
#endif

#ifdef NLCRC_XOR_ON_READ_PRODUCT_DEFAULT
#define NLCRC_XOR_ON_READ_DEFAULT NLCRC_XOR_ON_READ_PRODUCT_DEFAULT
#else
#define NLCRC_XOR_ON_READ_DEFAULT true
#endif

#ifdef NLCRC_LEN_PRODUCT_DEFAULT
#define NLCRC_LEN_DEFAULT NLCRC_LEN_PRODUCT_DEFAULT
#else
#define NLCRC_LEN_DEFAULT kCrcLen32Bits
#endif

#ifdef NLCRC_POLY_PRODUCT_DEFAULT
#define NLCRC_POLY_DEFAULT NLCRC_POLY_PRODUCT_DEFAULT
#else
#define NLCRC_POLY_DEFAULT 0x04C11DB7
#endif

#ifdef NLCRC_SEED_PRODUCT_DEFAULT
#define NLCRC_SEED_DEFAULT NLCRC_SEED_PRODUCT_DEFAULT
#else
#define NLCRC_SEED_DEFAULT 0xffffffff
#endif

typedef void (*nlcrc_handler_t)(unsigned crc_result, int error_code);

int nlcrc_request(nlcrc_transpose_write_t writeType,
                  nlcrc_transpose_read_t readType,
                  bool xorOnRead,
                  nlcrc_len_t crcLen,
                  unsigned poly);

int nlcrc_release(void);

unsigned nlcrc_compute(unsigned crc, const void *data, size_t len);
unsigned nlcrc_compute_async(unsigned crc, const void *data, size_t len, nlcrc_handler_t callback);

void nlcrc_set_locking(int (*lock)(void *), int (*unlock)(void *), void* context);
    
#ifdef __cplusplus
}
#endif

#endif /* __NLCRC_H_INCLUDED__ */
