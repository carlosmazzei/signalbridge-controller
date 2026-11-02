#ifndef COBS_H
#define COBS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @file cobs.h
 * @brief Consistent Overhead Byte Stuffing (COBS) codec interface.
 */

/**
 * @brief Encode a payload using the COBS algorithm.
 *
 * The input buffer is transformed into a zero-free representation that is
 * suitable for framing protocols relying on a zero byte as packet delimiter.
 * The caller must ensure that @p buffer has enough space to hold the encoded
 * output (worst-case length equals input length plus one byte per 254 input
 * bytes).
 *
 * @param[in]  data    Pointer to the raw payload to encode.
 * @param[in]  length  Number of bytes in @p data.
 * @param[out] buffer  Destination buffer that receives the encoded bytes.
 *
 * @return Number of encoded bytes written to @p buffer.
 */
size_t cobs_encode(const uint8_t *data, size_t length, uint8_t *buffer);

/**
 * @brief Decode a complete COBS-encoded frame into a bounded destination.
 *
 * The input excludes the transport delimiter. A zero code byte, embedded zero,
 * truncated block, empty input, or insufficient output capacity rejects the
 * frame. On failure, @p decoded_length is set to zero and any partially
 * written destination bytes are invalid.
 *
 * @param[in] buffer Encoded frame bytes.
 * @param[in] length Number of bytes in @p buffer.
 * @param[out] data Destination for decoded bytes.
 * @param[in] capacity Capacity of @p data in bytes.
 * @param[out] decoded_length Receives the decoded length, or zero on failure;
 *                            must not be NULL.
 *
 * @return `true` if the entire frame was decoded; otherwise `false`.
 */
bool cobs_decode_checked(const uint8_t *buffer, size_t length, uint8_t *data,
                         size_t capacity, size_t *decoded_length);

#endif // COBS_H
