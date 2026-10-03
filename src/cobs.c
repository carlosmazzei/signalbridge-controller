/**
 * @file cobs.c
 * @brief COBS encoding and decoding implementation.
 */

#include <stddef.h>
#include <stdint.h>

#include "cobs.h"

/*
 * @brief Encode data using the Consistent Overhead Byte Stuffing algorithm.
 *
 * The encoded stream omits zero bytes to make it suitable for protocols that
 * rely on zero as a frame delimiter. The caller must append the delimiter
 * manually when required by the transport.
 *
 * @param[in]  data   Pointer to the raw payload to encode.
 * @param[in]  length Number of bytes available in @p data.
 * @param[out] buffer Destination buffer that receives the encoded payload.
 *
 * @return Number of encoded bytes written into @p buffer.
 */
size_t cobs_encode(const void *data, size_t length, uint8_t *buffer)
{
	uint8_t *encode = buffer; // Encoded byte pointer
	uint8_t *codep = encode;
	encode++; // Output code pointer
	uint8_t code = 1; // Code value

	for (const uint8_t *byte = (const uint8_t *)data; length--; ++byte)
	{
		if (*byte != 0) // Byte not zero, write it
		{
			*encode++ = *byte, ++code;
		}

		if (!*byte || code == 0xff) // Input is zero or block completed, restart
		{
			*codep = code, code = 1, codep = encode;
			if (!*byte || length)
				++encode;
		}
	}
	if (codep < encode)
		*codep = code; // A full final block needs no extra code byte.

	return (size_t)(encode - buffer);
}

/*
 * @brief Decode a COBS-encoded payload.
 *
 * The function reconstructs the original payload produced by
 * cobs_encode(). If a delimiter code (zero) is encountered the operation
 * stops, mirroring the behaviour expected by typical framing layers.
 *
 * @param[in]  buffer Pointer to the COBS-encoded input bytes.
 * @param[in]  length Number of encoded bytes available in @p buffer.
 * @param[out] data   Destination buffer that receives the decoded bytes.
 *
 * @return Number of decoded payload bytes stored in @p data.
 */
bool cobs_decode_checked(const uint8_t *buffer, size_t length, void *data,
                         size_t capacity, size_t *decoded_length)
{
	size_t input = 0U;
	size_t output = 0U;
	uint8_t *decoded = data;
	if (decoded_length == NULL)
		return false;
	*decoded_length = 0U;
	if ((buffer == NULL) || (data == NULL) || (length == 0U))
		return false;
	while (input < length)
	{
		uint8_t code = buffer[input++];
		size_t count = (size_t)code - 1U;
		if ((code == 0U) || (count > length - input) || (count > capacity - output))
			return false;
		for (size_t i = 0U; i < count; i++)
		{
			if (buffer[input] == 0U)
				return false;
			decoded[output++] = buffer[input++];
		}
		if ((code != 0xFFU) && (input < length))
		{
			if (output >= capacity)
				return false;
			decoded[output++] = 0U;
		}
	}
	*decoded_length = output;
	return true;
}

size_t cobs_decode(const uint8_t *buffer, size_t length, void *data)
{
	size_t decoded_length = 0U;
	(void)cobs_decode_checked(buffer, length, data, length, &decoded_length);
	return decoded_length;
}
