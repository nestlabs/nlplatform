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
 *      This file defines API for ADC
 */
#ifndef __NLADC_H_INCLUDED__
#define __NLADC_H_INCLUDED__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* configuration information is implementation specific */
typedef struct nladc_config_s nladc_config_t;

/* callback to be called when async sample is finished
 * context - Pointer to structure that was specified when nladc_read_async was called
 * */
typedef void (*adc_cb_t)(void *context);

void nladc_init(void);

/* buffer should be a pointer to a buffer of size (samples * ADC_SAMPLE_SIZE),
 * where ADC_SAMPLE_SIZE is implementation specific
 */
int nladc_read(const nladc_config_t *config, void *buffer, size_t samples);

/* Asynchronously take ADC reading
 *
 * config - implementation-specific ADC configuration
 * buffer - pointer to a buffer of size (samples * ADC_SAMPLE_SIZE) where ADC_SAMPLE_SIZE is
 * implementation-specific
 * samples - number of ADC samples to take
 * cb - callback to call when sampling has finished
 * context - pointer to context passed to callback
 */
int nladc_read_async(const nladc_config_t *config, void *buffer, size_t samples, adc_cb_t cb, void *context);

int nladc_calibrate(void);

typedef struct nladc_calibration_s
{
    uint16_t gain;
    int16_t offset;
} nladc_calibration_t;
int nladc_get_calibration(nladc_calibration_t *cal);

#if defined(BUILD_CONFIG_DIAGNOSTICS)
void nladc_reset_calibration(void);
void nladc_apply_calibration(uint16_t gain, int16_t offset);
void nladc_print_calibration(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __NLADC_H_INCLUDED__ */
