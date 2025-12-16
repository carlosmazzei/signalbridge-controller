/**
 * @file app_comm.h
 * @brief Communication helpers for USB CDC data exchange.
 */

#ifndef APP_COMM_H
#define APP_COMM_H

#include <stddef.h>
#include <stdint.h>

#include "app_config.h"

/**
 * @struct cdc_packet_t
 * @brief Holds CDC output queue packets.
 */
typedef struct cdc_packet_t {
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
 */
void app_comm_send_packet(uint16_t id, uint8_t command, const uint8_t *send_data, uint8_t length);

/**
 * @brief Process a decoded inbound packet from the host.
 *
 * @param[in] rx_buffer Pointer to the decoded buffer.
 * @param[in] length    Number of bytes in @p rx_buffer.
 */
void app_comm_process_inbound(const uint8_t *rx_buffer, size_t length);

#endif // APP_COMM_H
