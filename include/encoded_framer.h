/**
 * @file encoded_framer.h
 * @brief COBS frame assembler that batches inbound bytes into complete frames.
 *
 * The framer is a pure state machine: callers push raw bytes read from the
 * USB CDC endpoint, and the framer signals when a complete COBS frame has
 * been assembled.  Frames are delimited by @ref PACKET_MARKER (0x00) and the
 * marker itself is consumed but not copied into the frame payload.
 *
 * The module has no dependency on FreeRTOS, TinyUSB, or the Pico SDK and is
 * therefore fully exercisable from the host unit-test harness.
 */

#ifndef ENCODED_FRAMER_H
#define ENCODED_FRAMER_H

#include <stddef.h>
#include <stdint.h>

#include "app_config.h"

/**
 * @struct encoded_frame_t
 * @brief Represents a complete COBS-encoded frame (without trailing marker).
 */
typedef struct encoded_frame_t {
	uint8_t length;                        /**< Number of valid bytes in @ref data */
	uint8_t data[MAX_ENCODED_BUFFER_SIZE]; /**< Encoded bytes preceding the marker */
} encoded_frame_t;

/**
 * @struct encoded_framer_t
 * @brief Internal accumulator holding bytes of the frame under construction.
 */
typedef struct encoded_framer_t {
	uint8_t buffer[MAX_ENCODED_BUFFER_SIZE]; /**< Bytes received so far */
	size_t length;                           /**< Number of valid bytes in @ref buffer */
} encoded_framer_t;

/**
 * @enum framer_result_t
 * @brief Outcome of pushing a single byte into the framer.
 */
typedef enum framer_result_t {
	FRAMER_NEED_MORE_DATA = 0, /**< Byte consumed, no frame ready yet */
	FRAMER_FRAME_READY,        /**< Marker reached, @p out_frame populated */
	FRAMER_EMPTY_FRAME,        /**< Marker reached with empty buffer */
	FRAMER_OVERFLOW            /**< Buffer filled without marker, framer reset */
} framer_result_t;

/**
 * @brief Reset the framer to the empty state.
 *
 * @param[in,out] framer Framer instance to reset.
 */
void encoded_framer_reset(encoded_framer_t *framer);

/**
 * @brief Feed a single byte into the framer.
 *
 * When the byte is the packet marker (0x00) and the accumulator is non-empty
 * the accumulated bytes are copied into @p out_frame and the framer is
 * reset.  A marker received with an empty accumulator signals
 * @ref FRAMER_EMPTY_FRAME.  When the accumulator is full and a non-marker
 * byte arrives the framer is reset and @ref FRAMER_OVERFLOW is returned.
 *
 * @param[in,out] framer    Framer instance maintained across calls.
 * @param[in]     byte      Byte read from the USB CDC endpoint.
 * @param[out]    out_frame Buffer receiving the frame on @ref FRAMER_FRAME_READY.
 *                          May be @c NULL when the caller only uses the status
 *                          (the byte is still consumed / the state still
 *                          advances).
 *
 * @return Status of the framer after processing the byte.
 */
framer_result_t encoded_framer_push_byte(encoded_framer_t *framer,
                                         uint8_t byte,
                                         encoded_frame_t *out_frame);

#endif // ENCODED_FRAMER_H
