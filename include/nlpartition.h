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
 *      This file defines a partition.
 */

#ifndef __NLPARTITION_H_INCLUDED__
#define __NLPARTITION_H_INCLUDED__

#include <stddef.h>
#include <stdbool.h>
#include <nlplatform/nlfs.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct partition_s
{
    const char *name;
    size_t offset;
    size_t size;
    bool isReadOnly;
} __attribute__((__packed__)) nlpartition_t;

/* ELF section names and ids for sub-partitions */
typedef struct sub_partition_info_s {
    const char *name;
    nlfs_fileid_t fid;
} __attribute__((__packed__)) nlsub_partition_info_t;

/* to be implemented by each product */
#ifdef BUILD_FEATURE_SOFT_PARTITIONS
extern nlpartition_t g_flash_partitions[NL_NUM_FLASH_PARTITIONS];
#else
extern const nlpartition_t g_flash_partitions[NL_NUM_FLASH_PARTITIONS];
#endif

extern const nlsub_partition_info_t g_sub_partition_info[NL_NUM_SUBPARTITIONS];

#ifdef __cplusplus
}
#endif

#endif /* __NLPARTITION_H_INCLUDED__ */
