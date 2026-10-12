/**
 * @file input_delivery.h
 * @brief Retained, versioned delivery of absolute keypad and ADC snapshots.
 *
 * Snapshot state is shared by application tasks and protected by FreeRTOS
 * critical sections. These functions must be called from task context.
 */

#ifndef INPUT_DELIVERY_H
#define INPUT_DELIVERY_H

#include <stdbool.h>
#include <stdint.h>
#include "data_event.h"

/** Number of retained snapshots: 64 keypad positions and 16 ADC channels. */
#define INPUT_SNAPSHOT_COUNT 80U
/** Slot value used by packets that do not complete an absolute snapshot. */
#define INPUT_SNAPSHOT_NONE 255U

/**
 * @brief Clear all retained input values and delivery state.
 *
 * @note Task context only.
 */
void input_delivery_reset(void);

/**
 * @brief Begin a CDC session and make known snapshots eligible for delivery.
 *
 * A changed session clears delivered and pending markers without discarding
 * the latest known input values, so those values can be republished.
 *
 * @param[in] session Current CDC session generation.
 *
 * @note Task context only.
 */
void input_delivery_session(uint32_t session);

/**
 * @brief Store the latest state of a keypad position.
 *
 * The snapshot version advances only when the encoded key state changes.
 *
 * @param[in] row Keypad row index (0-7).
 * @param[in] column Keypad column index (0-7).
 * @param[in] pressed `true` when pressed, otherwise released.
 *
 * @note Out-of-range coordinates are ignored. Call from task context.
 */
void input_delivery_key(uint8_t row, uint8_t column, bool pressed);

/**
 * @brief Store the latest value of an ADC channel.
 *
 * The snapshot version advances only when the encoded channel value changes.
 *
 * @param[in] channel ADC channel index (0-15).
 * @param[in] value 12-bit ADC reading stored as a 16-bit value.
 *
 * @note Out-of-range channels are ignored. Call from task context.
 */
void input_delivery_adc(uint8_t channel, uint16_t value);

/**
 * @brief Select the next undelivered snapshot in round-robin order.
 *
 * A selected version is marked pending until @ref input_delivery_complete()
 * reports the result.
 *
 * @param[out] event Receives the snapshot event when one is available.
 * @param[out] slot Receives its snapshot slot.
 * @param[out] version Receives its current version.
 *
 * @return `true` if a snapshot was selected; otherwise `false`.
 *
 * @note Output pointers must be valid. Call from task context.
 */
bool input_delivery_next(data_events_t *event, uint8_t *slot, uint32_t *version);

/**
 * @brief Complete or release a pending snapshot delivery attempt.
 *
 * Only a successful completion for the current session and pending version
 * marks a snapshot delivered. A failed attempt remains eligible for retry.
 *
 * @param[in] slot Snapshot slot returned by @ref input_delivery_next().
 * @param[in] version Snapshot version returned by @ref input_delivery_next().
 * @param[in] session CDC session generation associated with the attempt.
 * @param[in] success Whether the complete packet was accepted by the USB writer.
 *
 * @note Call from task context.
 */
void input_delivery_complete(uint8_t slot, uint32_t version, uint32_t session, bool success);

#endif
