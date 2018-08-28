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
 * Description:
 *    This file defines the interfaces for controlling and configuring the
 *    device's Thread radio (IEEE 802.15.4).
 */

#ifndef __NLRADIO_H__
#define __NLRADIO_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The radio PHY layer parameters, could be used to calculate the ota time of the radio packet,
 * it is required when the radio supports IEEE 802.15.4-2015 Header IEs (Information Element)
 * process (PLATFORM-451).
 */
#define PHY_US_PER_SYMBOL     16   ///< Duration of single symbol in microseconds
#define PHY_SYMBOLS_PER_OCTET  2   ///< Number of symbols in single byte (octet)
#define PHR_SIZE               1   ///< Size of PHR field

/**
 * The possible radio states returned by nlradio_get_state().
 */
typedef enum nlradio_state
{
    /**
     * The transceiver is completely disabled and no configuration
     * parameters are retained.
     */
    k_nlradio_state_disabled = 0,
    /**
     * The transceiver is in a sleep state and configuration
     * parameters are retained.
     */
    k_nlradio_state_sleep = 1,
    /**
     * The receive path is enabled and searching for preamble + SFD.
     */
    k_nlradio_state_receive = 2,
    /**
     * The transmit path is enabled.
     */
    k_nlradio_state_transmit = 3,
    /* DO NOT USE idle state - it's going away */
    k_nlradio_state_idle = 4,
    /**
     * The radio is performing an energy scan.
     */
    k_nlradio_state_energy_scan = 5
} nlradio_state_t;

/**
 * The possible radio transmission errors passed as an argument to transmit_complete_cb().
 */
typedef enum nlradio_tx_error
{
    /**
     * The transmission completed successfully, no error.
     */
    k_nlradio_tx_error_none           = 0,
    /**
     * The transmission failed because no ack frame was received.
     */
    k_nlradio_tx_error_no_ack         = -1,
    /**
     * The transmission failed because the channel was busy.
     */
    k_nlradio_tx_error_channel_busy   = -2,
    /**
     * The transmission failed due to a platform specific error.
     */
    k_nlradio_tx_error_platform       = -3,
    /**
     * The transmission failed due to tx done notification timeout.
     */
    k_nlradio_tx_error_done_timeout   = -4,
} nlradio_tx_error_t;

/**
 * The possible radio receive errors passed as an argument to receive_complete_cb().
 */
typedef enum nlradio_rx_error
{
    /**
     * The receive completed successfully, no error.
     */
    k_nlradio_rx_error_none               = 0,
    /**
     * The receive failed because there was no rx buffer
     */
    k_nlradio_rx_error_no_buffer          = -1,
    /**
     * The receive failed because the buffer was too small.
     */
    k_nlradio_rx_error_buffer_too_small   = -2,
    /**
     * The receive operation was cancelled.
     */
    k_nlradio_rx_error_cancelled          = -3,
} nlradio_rx_error_t;

/**
 * The possible nlradio API errors.
 */
typedef enum nlradio_error
{
    k_nlradio_error_none = 0,
    k_nlradio_error_fail = -1
} nlradio_error_t;

/**
 * The possible radio capabilities that can be queried via nlradio_get_capabilities()
 */
typedef enum nlradio_capabilities
{
    k_nlradio_capability_none           = 0x00,
    k_nlradio_capability_ack_timeout    = 0x01,
    k_nlradio_capability_energy_scan    = 0x02,
    k_nlradio_capability_tx_retries     = 0x04,
    k_nlradio_capability_csma_backoff   = 0x08,
} nlradio_capabilities_t;

/**
 * The possible radio receive errors passed as an argument to receive_complete_cb().
 */
typedef enum nlradio_filter_mode
{
    /**
     * Normal MAC filtering is in place.
     */
    k_nlradio_filter_mode_normal                = 0,

    /**
     * All MAC packets matching the network (PANID) are passed up the stack.
     */
    k_nlradio_filter_mode_network_promiscuous   = 1,

    /**
     * All decoded MAC packets are passed up the stack.
     */
    k_nlradio_filter_mode_full_promiscuous      = 2,
} nlradio_filter_mode_t;

/**
 * The possible radio receive errors passed as an argument to receive_complete_cb().
 */
typedef struct nlradio_tx_params
{
    const uint8_t *buffer;
    uint32_t length;
    uint8_t channel;
    int8_t power;
    bool is_cca_enabled;
} nlradio_tx_params_t;

/**
 * A callback provided by the caller to nlradio_transmit.
 * This callback will be called by the ISR when the transmission completes.
 * @param[in] error           The Tx error code.
 * @param[in] framePending    The frame pending value in the received ACK frame,
 *                            or false if no ACK was received.
 * @param[in] ackPower        The rssi (in dBm) of the received ACK frame,
 *                            or kRadioRssiUnknown if no ACK was received
 *                            or no support for providing rssi for ACKs.
 * @param[in] ackLqi          The lqi of the received ACK frame,
 *                            or 0 if no ACK was received or not supported.
 * @param[in] fromIsr         True if the callback is being executed from ISR
 *                            context, false otherwise.
 */
typedef void (*transmit_complete_cb)(nlradio_tx_error_t error, bool framePending, int8_t ackPower, int8_t ackLqi, bool fromIsr);

/**
 * A callback provided by the caller to nlradio_receive.
 * This callback will be called by the ISR when the receive completes.
 * @param[in] error    The Rx error code.
 * @param[in] fromIsr  True if the callback is being executed from ISR
 *                     context, false otherwise.
 */
typedef void (*receive_complete_cb)(nlradio_rx_error_t error, bool fromIsr);

/**
 * A callback provided by the caller to nlradio_transmit.
 * This callback will be called by the ISR when the transmission starts (tx sfd: start-of-frame delimiter).
 *
 * @param[in] psdu     A pointer to the psdu which is going to be sent by radio.
 * @param[in] fromIsr  True if the callback is being executed from ISR
 *                     context, false otherwise.
 */
typedef void (*transmit_start_cb)(const uint8_t *psdu, bool fromeIsr);

/**
 * A callback provided by the caller to nlradio_start_energyscan.
 * This callback will be called by the driver when the energy scan completes.
 * @param[in] rssi     The signal strength result in dbm.
 * @param[in] fromIsr  True if the callback is being executed from ISR
 *                     context, false otherwise.
 */
typedef void (*escan_complete_cb)(int8_t rssi, bool fromIsr);

/**
 * Intialize the radio.
 *
 * @param[in] inContext           A pointer to a nlradio specific context
 *                                provided by nlradio client.
 *
 * @retval k_nlradio_error_none   If radio driver initialization succeeded.
 * @retval k_nlradio_error_fail   If radio driver initialization failed.
 */
int nlradio_init(void *inContext);

/**
 * Initialize and enable the radio, then transition to sleep state.
 *
 * @retval k_nlradio_error_none   If radio driver transitioned to sleep.
 * @retval k_nlradio_error_fail   If radio driver initialization failed.
 */
int nlradio_enable(void);

/**
 * Disable radio and transition to disabled.
 *
 * @retval k_nlradio_error_none   If transitioned to disabled state.
 * @retval k_nlradio_error_fail   If failed to disable radio.
 */
int nlradio_disable(void);

/**
 * Get the current state of the radio driver.
 *
 * @retval The current radio driver state, one of nlradio_state_t.
 */
nlradio_state_t nlradio_get_state(void);

/**
 * Set the PAN ID for address filtering.
 *
 * @param[in] pan_id  The IEEE 802.15.4 PAN ID.
 *
 * @retval k_nlradio_error_none  If the PAN ID set operation succeeded.
 * @retval k_nlradio_error_fail  If the PAN ID set operation failed.
 */
int nlradio_set_pan_id(uint16_t pan_id);

/**
 * Set the default TX power, used for ACK.
 *
 * @param[in] aPower The TX power to use in dBm
 *
 * @retval k_nlradio_error_none  If the default TX power set operation succeeded.
 * @retval k_nlradio_error_fail  If the default TX power set operation failed.
 */
int nlradio_set_tx_power(int8_t aPower);

/**
 * Set the Extended Address for address filtering.
 *
 * @param[in] extended_address  A pointer of 8 bytes containing the IEEE 802.15.4 Extended Address.
 *
 * @retval k_nlradio_error_none  If the Extended Address set operation succeeded.
 * @retval k_nlradio_error_fail  If the Extended Address set operation failed.
 */
int nlradio_set_extended_address(const uint8_t *extended_address);

/**
 * Set the Short Address for address filtering.
 *
 * @param[in] short_address  The IEEE 802.15.4 Short Address.
 *
 * @retval k_nlradio_error_none  If the Short Address set operation succeeded.
 * @retval k_nlradio_error_fail  If the Short Address set operation failed.
 */
int nlradio_set_short_address(uint16_t short_address);

/**
 * Put the radio to sleep and the driver into the sleep state.
 *
 * @retval k_nlradio_error_none  If the radio was successfully put in sleep mode.
 * @retval k_nlradio_error_fail  If the function failed.
 */
int nlradio_sleep(void);

#ifdef BUILD_FEATURE_RADIO_HEADER_IE
/**
 * Transmit the provided buffer on the specified channel.
 *
 * @note
 *    The buffer cannot be reused until the transmit_complete_cb function is
 *    called because it may be referenced by the HW DMA engine.
 *    A successful return indicates that the radio driver has transitioned
 *    into the k_nlradio_state_transmit state. The radio will remain in this state
 *    until the transmit operation completes upon which it will execute the optional
 *    callback (tx_complete_cb) if provided.
 *    Both tx_complete_cb and tx_start_cb are executed by the ISR and must therefore be
 *    ISR safe.
 *
 *
 * @param[in] radio_tx_params   Tx packet and other params. Contents of this struct can be changed by the caller. Please make a copy if using elsewhere.
 * @param[in] tx_complete_cb    An optional callback that will be called when the transmit completes. Can be NULL.
 * @param[in] tx_start_cb       An optional callback that will be called when the transmit starts. Can be NULL.
 *
 * @retval k_nlradio_error_none  If the transmission was started successfully and the radio transitioned to the k_nlradio_state_transmit.
 * @retval k_nlradio_error_fail  If the transmission failed to start.
 */
int nlradio_transmit(nlradio_tx_params_t *radio_tx_params, transmit_complete_cb tx_complete_cb, transmit_start_cb tx_start_cb);
#else
/**
 * Transmit the provided buffer on the specified channel.
 *
 * @note
 *    The buffer cannot be reused until the transmit_complete_cb function is
 *    called because it may be referenced by the HW DMA engine.
 *    A successful return indicates that the radio driver has transitioned
 *    into the k_nlradio_state_transmit state. The radio will remain in this state
 *    until the transmit operation completes upon which it will execute the optional
 *    callback (cb) if provided.  The callback is executed by the ISR and must therefore be
 *    ISR safe.
 *
 *
 * @param[in] radio_tx_params   Tx packet and other params. Contents of this struct can be changed by the caller. Please make a copy if using elsewhere.
 * @param[in] cb                An optional callback that will be called when the transmit completes. Can be NULL.
 *
 * @retval k_nlradio_error_none  If the transmission was started successfully and the radio transitioned to the k_nlradio_state_transmit.
 * @retval k_nlradio_error_fail  If the transmission failed to start.
 */
int nlradio_transmit(nlradio_tx_params_t *radio_tx_params, transmit_complete_cb cb);
#endif // BUILD_FEATURE_RADIO_HEADER_IE

/**
 * Transition the radio to receive and execute the callback when complete.
 *
 * @note
 *    A successful return indicates that the radio driver has transitioned
 *    into the k_nlradio_state_receive state. The radio will remain in this state
 *    until the receive operation completes upon which it will execute the callback
 *    (cb).  The callback is executed by the ISR and must therefore be
 *    ISR safe.
 *
 * @param[in] channel       The radio channel upon which the receive operation shall occur.
 * @param[in] cb            A callback that will be called when the receive completes.
 *
 * @retval k_nlradio_error_none  If the reception was started successfully and the radio transitioned to the k_nlradio_state_receive.
 * @retval k_nlradio_error_fail  If the reception failed to start.
 */
int nlradio_receive(uint8_t channel, receive_complete_cb cb);

/**
 * Read radio RSSI.
 *
 * @note
 *    RSSI should be read when there are no detectable ongoing
 *    15.4 packet transmissions.
 *
 * @param[out] rssi                A pointer to hold the RSSI value.
 *
 * @retval k_nlradio_error_none    Successfully read RSSI.
 * @retval k_nlradio_error_fail    Radio was busy, could not read RSSI.
 */
int nlradio_get_rssi(int8_t *rssi);

#ifdef BUILD_FEATURE_RADIO_HEADER_IE
/**
 * Called to perform post-processing of a previously received frame.
 *
 * @note
 *    The caller is expected to call this API only after having been notified
 *    by execution of the receive_complete_cb that was passed into nlradio_receive().
 *
 * @param[out] buffer        A buffer pointer to hold the received frame on completion.
 * @param[out] num_bytes     The number of bytes used in buffer to store the RX frame.
 * @param[out] channel       The channel the radio was on when it received the frame.
 * @param[out] power         The signal strength (RSSI) of the received frame.
 * @param[out] lqi           The link quality indicator for the received frame.
 * @param[out] timestamp     The time since boot when the radio began to receive the frame (rx sfd), in microseconds.
 *
 * @retval k_nlradio_error_none  If the driver specific operation completed successfully.
 * @retval k_nlradio_error_fail  If the driver specific operation failed.
 */
int nlradio_post_process_receive(uint8_t **buffer, uint32_t *num_bytes, uint8_t *channel, int8_t *power, uint8_t *lqi, uint64_t *timestamp);
#else
/**
 * Called to perform post-processing of a previously received frame.
 *
 * @note
 *    The caller is expected to call this API only after having been notified
 *    by execution of the receive_complete_cb that was passed into nlradio_receive().
 *
 * @param[out] buffer        A buffer pointer to hold the received frame on completion.
 * @param[out] num_bytes     The number of bytes used in buffer to store the RX frame.
 * @param[out] channel       The channel the radio was on when it received the frame.
 * @param[out] power         The signal strength (RSSI) of the received frame.
 * @param[out] lqi           The link quality indicator for the received frame.
 *
 * @retval k_nlradio_error_none  If the driver specific operation completed successfully.
 * @retval k_nlradio_error_fail  If the driver specific operation failed.
 */
int nlradio_post_process_receive(uint8_t **buffer, uint32_t *num_bytes, uint8_t *channel, int8_t *power, uint8_t *lqi);
#endif // BUILD_FEATURE_RADIO_HEADER_IE

/**
 * Called to free a buffer received in nlradio_post_process_receive.
 *
 * @note
 *    The caller is expected to call this API when it is ready to free a buffer it
 *    acquired from nlradio_post_process_receive.
 *
 * @param[in] buffer        A buffer to be freed.
 *
 * @retval k_nlradio_error_none  If the driver specific operation completed successfully.
 * @retval k_nlradio_error_fail  If the driver specific operation failed.
 */
int nlradio_buffer_free(uint8_t *buffer);

/**
 * Returns the capabilities of the radio.
 *
 * @retval A bitfield consisting of zero or more nlradio_capabilities_t values.
 */
nlradio_capabilities_t nlradio_get_capabilities(void);

/**
 * Override source address matching and set frame pending
 * for all short and extended address data polls.
 *
 * @param[in] enable    true, to set frame pending for all acks
 *                      false, to use the frame pending source address match tables
 *                             to determine whether to set frame pending in ack
 *
 * @retval k_nlradio_error_none  If the override source match operation succeeded.
 * @retval k_nlradio_error_fail  If the override source match operation failed.
 */
int nlradio_override_source_match(bool enable);

/**
 * Clear all Extended Address source match entries.
 *
 * @retval k_nlradio_error_none  If the clear all extended address match operation succeeded.
 * @retval k_nlradio_error_fail  If the clear all extended address match operation failed.
 */
int nlradio_clear_extended_source_match_address_entries(void);

/**
 * Set/Clear the Extended Address source match entry.
 *
 * @param[in] extended_address  A pointer of 8 bytes containing the IEEE 802.15.4 Extended Address.
 * @param[in] pending           true, to add this extended address to the source match table,
 *                              false, to remove this extended address from the source match table
 *
 * @retval k_nlradio_error_none  If the set extended address match operation succeeded.
 * @retval k_nlradio_error_fail  If the address could not be added to the table when pending == true
 *                               or if the address was not found when pending == false
 */
int nlradio_set_extended_source_match_address_entry(const uint8_t *extended_address, bool pending);

/**
 * Clear all Short Address source match entries.
 *
 * @retval k_nlradio_error_none  If the clear all short address match operation succeeded.
 * @retval k_nlradio_error_fail  If the clear all short address match operation failed.
 */
int nlradio_clear_short_source_match_address_entries(void);

/**
 * Set/Clear the Short Address source match entry.
 *
 * @param[in] short_address  The IEEE 802.15.4 Short Address.
 * @param[in] pending           true, to add this short address to the source match table,
 *                              false, to remove this short address from the source match table
 *
 * @retval k_nlradio_error_none  If the set short address match operation succeeded.
 * @retval k_nlradio_error_fail  If the address could not be added to the table when pending == true
 *                               or if the address was not found when pending == false
 */
int nlradio_set_short_source_match_address_entry(uint16_t short_address, bool pending);

/**
 * Get the radio filter mode.
 *
 * @retval The current filter mode.
 */
nlradio_filter_mode_t nlradio_get_filter_mode(void);

/**
 * Set the radio filter mode to either normal (destination address filtering),
 * promiscuous (all to a given panid), or monitor mode (all on given channel).
 *
 * @param[in] filter_mode       The radio filter mode.
 *
 * @retval k_nlradio_error_none  If the set filter mode operation succeeded.
 * @retval k_nlradio_error_fail  If the given filter mode could not be set.
 */
int nlradio_set_filter_mode(nlradio_filter_mode_t filter_mode);

/**
 * Initiate an energy scan on the specified channel for the specified duration.
 *
 * @param[in] channel       The radio channel upon which the energy scan operation shall occur.
 * @param[in] duration_msec The duration in milliseconds for the energy scan.
 * @param[in] cb            A callback that will be called when the energy scan completes.
 *
 * @retval k_nlradio_error_none  If the start energy scan operation succeeded.
 * @retval k_nlradio_error_fail  If the start energy scan operation failed.
 */
int nlradio_start_energy_scan(uint8_t channel, uint32_t duration_msec, escan_complete_cb cb);

/**
 * Get the factory-assigned IEEE EUI-64 for this interface.
 *
 * @param[in]  aInstance             The OpenThread instance structure.
 * @param[out] aIeeeEui64            A pointer to where the factory-assigned IEEE EUI-64 will be placed.
 *
 * @retval k_nlradio_error_none  If the Extended Address set operation succeeded.
 * @retval k_nlradio_error_fail  If the Extended Address set operation failed.
 */
int nlradio_get_ieee_eui64(uint8_t *aIeeeEui64);

/**
 * Get radio receive sensitivity.
 *
 * @retval The radio receive sensitivity value in dBm.
 */
int nlradio_get_rx_sensitivity(void);

#ifdef BUILD_FEATURE_ANTENNA_DIVERSITY
void nlradio_set_prevent_antenna_switch(bool prevent_switching);
#endif

/**
 * set the tx power backoff in dBm
 *
 * @param[in] power backoff in units of 0.01 dBm.
 *
 * @retval k_nlradio_error_none  If the TX power backoff set operation succeeded.
 * @retval k_nlradio_error_fail  If the TX power backoff set operation failed.
 */
int nlradio_set_txpower_backoff(int aBackoff);

/**
 * Set the target TX power to be used for packet transmission &
 * acks.
 *
 * @param[in] target TX power in units of 0.01 dBm.
 *
 * @retval k_nlradio_error_none  If the default TX power set operation succeeded.
 * @retval k_nlradio_error_fail  If the default TX power set operation failed.
 */
int nlradio_set_target_txpower(int aPower);

#ifdef __cplusplus
}
#endif

#endif /* __NLRADIO_H__ */
