/**
 * @file cobs.c
 * @brief COBS encoding implementation
 * @author Carlos Mazzei
 *
 * This file contains the COBS encoding implementation.
 *
 * @copyright
 *   (c) 2020-2025 Carlos Mazzei - All rights reserved.
 */

#include <stddef.h>
#include <stdint.h>
#include "cobs.h"

/** @brief COBS encode data to buffer
 *
 *	@param data Pointer to input data to encode
 *	@param length Number of bytes to encode
 *	@param buffer Pointer to encoded output buffer
 *
 *	@return Encoded buffer length in bytes
 *
 *	@note Does not output delimiter byte
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
	*codep = code; // Write final code value

	return (size_t)(encode - buffer);
}

/** @brief COBS decode data from buffer
 *
 *	@param buffer Pointer to encoded input bytes
 *	@param length Number of bytes to decode
 *	@param data Pointer to decoded output data
 *
 *	@return Number of bytes successfully decoded
 *
 *	@note Stops decoding if delimiter byte is found
 */
size_t cobs_decode(const uint8_t *buffer, size_t length, void *data)
{
	const uint8_t *byte = buffer; // Encoded input byte pointer
	uint8_t *decode = (uint8_t *)data; // Decoded output byte pointer

	for (uint8_t code = 0xff, block = 0; byte < buffer + length; --block)
	{
		if (block) // Decode block byte
			*decode++ = *byte++;
		else
		{
			if (code != 0xff) // Encoded zero, write it
				*decode++ = 0;
			block = code = *byte++; // Next block length
			if (!code) // Delimiter code found
				break;
		}
	}

	return (size_t)(decode - (uint8_t *)data);
}