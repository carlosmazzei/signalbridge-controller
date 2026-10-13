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
size_t cobs_encode(const uint8_t *data, size_t length, uint8_t *buffer)
{
	const uint8_t *input = data;
	size_t write_index = 1U; // First slot is reserved for the first code byte
	size_t code_index = 0U;
	uint8_t code = 1U;

	for (size_t read_index = 0U; read_index < length; read_index++)
	{
		const uint8_t byte = input[read_index];
		const bool more_input = ((read_index + 1U) < length);

		if (byte != 0U) // Byte not zero, write it
		{
			buffer[write_index] = byte;
			write_index++;
			code++;
		}

		if ((byte == 0U) || (code == 0xFFU)) // Input is zero or block completed, restart
		{
			buffer[code_index] = code;
			code = 1U;
			code_index = write_index;
			if ((byte == 0U) || more_input)
			{
				write_index++;
			}
		}
	}

	if (code_index < write_index)
	{
		buffer[code_index] = code; // A full final block needs no extra code byte.
	}

	return write_index;
}

/*
 * @brief Decode a COBS-encoded payload.
 *
 * The function reconstructs the original payload produced by
 * cobs_encode(). A zero code, a zero byte inside a block, a truncated block
 * or insufficient output capacity rejects the whole frame.
 *
 * @param[in]  buffer         Pointer to the COBS-encoded input bytes.
 * @param[in]  length         Number of encoded bytes available in @p buffer.
 * @param[out] data           Destination buffer that receives the decoded bytes.
 * @param[in]  capacity       Size of @p data in bytes.
 * @param[out] decoded_length Decoded byte count; zero on failure.
 *
 * @return true when the frame is valid and fully decoded.
 */
bool cobs_decode_checked(const uint8_t *buffer, size_t length, uint8_t *data,
                         size_t capacity, size_t *decoded_length)
{
	size_t input = 0U;
	size_t output = 0U;
	uint8_t *decoded = data;
	bool valid = false;

	if (decoded_length != NULL)
	{
		*decoded_length = 0U;
		valid = (buffer != NULL) && (data != NULL) && (length != 0U);
	}

	while (valid && (input < length))
	{
		const uint8_t code = buffer[input];
		input++;
		const size_t count = (size_t)code - 1U;

		if ((code == 0U) || (count > (length - input)) || (count > (capacity - output)))
		{
			valid = false;
		}

		for (size_t i = 0U; valid && (i < count); i++)
		{
			if (buffer[input] == 0U)
			{
				valid = false;
			}
			else
			{
				decoded[output] = buffer[input];
				output++;
				input++;
			}
		}

		if (valid && (code != 0xFFU) && (input < length))
		{
			if (output >= capacity)
			{
				valid = false;
			}
			else
			{
				decoded[output] = 0U;
				output++;
			}
		}
	}

	if (valid)
	{
		*decoded_length = output;
	}
	return valid;
}
