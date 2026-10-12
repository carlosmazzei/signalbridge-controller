/**
 * @file app_comm.h
 * @brief Communication helpers for USB CDC data exchange.
 */

#ifndef APP_COMM_H
#define APP_COMM_H

#include <stddef.h>
#include <stdbool.h>
#include "data_event.h"
#include <stdint.h>

#include "app_config.h"

/**
 * @brief Sentinel value indicating invalid task index in status response.
 */
#define INVALID_TASK_INDEX 0xFFU

/**
 * @struct cdc_packet_t
 * @brief Holds CDC output queue packets.
 */
typedef struct cdc_packet_t {
	uint32_t session;          /**< USB session generation associated with the packet. */
	uint32_t snapshot_version; /**< Input snapshot version completed by this packet, if any. */
	uint8_t snapshot_slot;      /**< Input snapshot slot, or @ref INPUT_SNAPSHOT_NONE for ordinary packets. */
	uint8_t length;                        /**< Number of encoded bytes in @ref data */
	uint8_t data[MAX_ENCODED_BUFFER_SIZE]; /**< Encoded payload ready for TinyUSB */
} cdc_packet_t;

/**
 * @brief Encode and enqueue a packet for transmission over USB CDC.
 *
 * @param[in] id         Identifier of the device sending the packet.
 * @param[in] command    Command identifier.
 * @param[in] send_data  Pointer to the payload buffer.
 * @param[in] length     Number of payload bytes.
 *
 * @return `true` if the encoded packet was accepted by the transmit queue;
 *         `false` if validation or queueing failed. This does not confirm
 *         delivery to the host.
 */
bool app_comm_send_packet(uint16_t id, uint8_t command, const uint8_t *send_data, uint8_t length);

/**
 * @brief Process a decoded inbound packet from the host.
 *
 * @param[in] rx_buffer Pointer to the decoded buffer.
 * @param[in] length    Number of bytes in @p rx_buffer.
 */
void app_comm_process_inbound(const uint8_t *rx_buffer, size_t length);

/**
 * @brief Queue a key or ADC event with its snapshot-delivery metadata.
 *
 * @param[in] event   Event whose command and payload are transmitted.
 * @param[in] slot    Snapshot slot, or @ref INPUT_SNAPSHOT_NONE for a
 *                    best-effort relative event.
 * @param[in] version Snapshot version to mark complete after transmission.
 * @param[in] session USB session generation that owns the event.
 *
 * @return `true` if the encoded packet was accepted by the transmit queue;
 *         `false` if it could not be queued.
 *
 * @note Call from task context, not an ISR.
 * @pre @p event points to a valid event structure.
 */
bool app_comm_send_input(const data_events_t *event, uint8_t slot,
                         uint32_t version, uint32_t session);

#endif // APP_COMM_H
