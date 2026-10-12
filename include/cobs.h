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

/** Strict bounded decoder. Returns false on malformed input or insufficient
 * capacity; decoded_length is zero on failure and partial output is invalid. */
bool cobs_decode_checked(const uint8_t *buffer, size_t length, uint8_t *data,
                         size_t capacity, size_t *decoded_length);

#endif // COBS_H
