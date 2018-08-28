/*
 *
 *    Copyright (c) 2013-2018 Nest Labs, Inc.
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
 *      This file defines the API for a cryptography accelerator.
 *
 */

#ifndef __NLCRYPTO_H_INCLUDED__
#define __NLCRYPTO_H_INCLUDED__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AES */
// Check whether the AES engine is current available in interrupt context or not.
bool nlplatform_AES_available_in_isr(void);

void nlplatform_AES128ECB_set_encrypt_key(const uint8_t *userKey, uint8_t *key);
void nlplatform_AES128ECB_encrypt(const uint8_t *inBlock, uint8_t *outBlock, const uint8_t *key);
void nlplatform_AES128ECB_set_decrypt_key(const uint8_t *userKey, uint8_t *key);
void nlplatform_AES128ECB_decrypt(const uint8_t *inBlock, uint8_t *outBlock, const uint8_t *key);

void nlplatform_AES256ECB_set_encrypt_key(const uint8_t *userKey, uint8_t *key);
void nlplatform_AES256ECB_encrypt(const uint8_t *inBlock, uint8_t *outBlock, const uint8_t *key);
void nlplatform_AES256ECB_set_decrypt_key(const uint8_t *userKey, uint8_t *key);
void nlplatform_AES256ECB_decrypt(const uint8_t *inBlock, uint8_t *outBlock, const uint8_t *key);

typedef struct nlplatform_aes_cmac_s nlplatform_aes_cmac_t;
void nlplatform_AES_CMAC_init(nlplatform_aes_cmac_t *ctx, const uint8_t *key);
void nlplatform_AES_CMAC_update(nlplatform_aes_cmac_t *ctx, const uint8_t *inData, size_t dataLen);
void nlplatform_AES_CMAC_finish(nlplatform_aes_cmac_t *ctx, uint8_t *macBuf);

/* SHA-1 */
typedef struct nlplatform_sha1_s nlplatform_sha1_t;
void nlplatform_SHA1_init(nlplatform_sha1_t *ctx);
void nlplatform_SHA1_update(nlplatform_sha1_t *ctx, const uint8_t *data, size_t len);
void nlplatform_SHA1_finish(nlplatform_sha1_t *ctx, uint8_t *digest);

/* Single-call variant. Can be used if data is in one contiguous block. */
void nlplatform_SHA1_hash(const uint8_t *data, uint8_t *digest, size_t len);

/* SHA-256. Use separate API so from SHA-1 so unused functions can be deadstripped. */
typedef struct nlplatform_sha256_s nlplatform_sha256_t;
void nlplatform_SHA256_init(nlplatform_sha256_t *ctx);
void nlplatform_SHA256_update(nlplatform_sha256_t *ctx, const uint8_t *data, size_t len);
void nlplatform_SHA256_finish(nlplatform_sha256_t *ctx, uint8_t *digest);

/* Single-call variant. Can be used if data is in one contiguous block. */
void nlplatform_SHA256_hash(const uint8_t *data, uint8_t *digest, size_t len);

typedef enum {
    ECDSA_SIGNATURE_TYPE_NONE             = 0x00,
    ECDSA_SIGNATURE_TYPE_SHA256_SECP224R1 = 0x01
} ecdsa_signature_t;

/* Possible return values for nlplatform_ecdsa_verify().
 * Errors are negative.
 * Success is 0.
 * No check done (because signature type was NONE) is 1.
 */
#define ECDSA_VERIFY_INVALID_SIGNATURE_TYPE    -2
#define ECDSA_VERIFY_INVALID_SIGNATURE         -1
#define ECDSA_VERIFY_SUCCESS                   0
#define ECDSA_VERIFY_NO_SIGNATURE              1

/* ECDSA-verify.  The length of the public_key and the signature are
 * determined by the signature_type (e.g. 56 bytes for SIGNATURE_ECDSA_SHA256_SECP224R1)
 */
int nlplatform_ecdsa_verify(ecdsa_signature_t signature_type, const uint8_t *public_key, const uint8_t *message, size_t length, const uint8_t *signature);

#ifdef __cplusplus
}
#endif

#endif /* __NLCRYPTO_H_INCLUDED__ */
