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
 *      This file defines API for accessing a I2C bus.
 *
 */

#ifndef __NLI2C_H_INCLUDED__
#define __NLI2C_H_INCLUDED__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* slave device drivers should have a const instace of this structure
 * to pass to transaction calls.
 */
#define I2C_FLAG_REG_ADDRESS_SIZE_MASK        0x03
#define I2C_FLAG_REG_ADDRESS_SIZE_0_BYTE      0x00 /* no registers addresses used */
#define I2C_FLAG_REG_ADDRESS_SIZE_1_BYTE      0x01
#define I2C_FLAG_REG_ADDRESS_SIZE_2_BYTE      0x02
#define I2C_FLAG_SLAVE_ADDRESS_SIZE_MASK      0x10
#define I2C_FLAG_SLAVE_ADDRESS_SIZE_7_BITS    0x00
#define I2C_FLAG_SLAVE_ADDRESS_SIZE_10_BITS   0x10

/* slave device drivers should have a const instance of this structure
 * to pass to transaction calls
 */
typedef struct nli2c_slave_s 
{
    uint8_t controller_id;
    uint8_t flags;
    uint16_t slave_addr; /* 7 or 10 bits depending on flags */
} nli2c_slave_t;

typedef void (*nli2c_handler_t)(nli2c_slave_t *i2c_slave, int result);

void nli2c_init(void);
int nli2c_request(const nli2c_slave_t *i2c_slave);
int nli2c_release(const nli2c_slave_t *i2c_slave);
        
/* Supply callback for asynchronous read or write.
 * read returns number of bytes read in synchronous mode, < 0 on error.
 * write returns number of bytes written in synchronous mode, < 0 on error.
 * read returns 0 on successful start of transfer in asynchronous mode, < 0 on error.
 * write returns 0 on successful start of transfer in asynchronous mode, < 0 on error. */
int nli2c_read(const nli2c_slave_t *i2c_slave, uint16_t regAddr, uint8_t *buf,
               size_t len, nli2c_handler_t callback);
int nli2c_write(const nli2c_slave_t *i2c_slave, uint16_t regAddr,
                const uint8_t *buf, size_t len, nli2c_handler_t callback);

#ifdef __cplusplus
}
#endif

#endif /* __NLI2C_H_INCLUDED__ */
