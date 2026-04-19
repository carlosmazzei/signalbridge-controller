/**
 * @file encoded_framer.c
 * @brief Implementation of the inbound COBS frame assembler.
 */

#include "encoded_framer.h"

#include <string.h>

#include "app_config.h"

void encoded_framer_reset(encoded_framer_t *framer)
{
	if (NULL != framer)
	{
		framer->length = 0U;
	}
}

framer_result_t encoded_framer_push_byte(encoded_framer_t *framer,
                                         uint8_t byte,
                                         encoded_frame_t *out_frame)
{
	framer_result_t result = FRAMER_NEED_MORE_DATA;

	if (NULL == framer)
	{
		return FRAMER_OVERFLOW;
	}

	if (PACKET_MARKER == byte)
	{
		if (0U == framer->length)
		{
			result = FRAMER_EMPTY_FRAME;
		}
		else
		{
			if (NULL != out_frame)
			{
				out_frame->length = (uint8_t)framer->length;
				(void)memcpy(out_frame->data, framer->buffer, framer->length);
			}
			framer->length = 0U;
			result = FRAMER_FRAME_READY;
		}
	}
	else if (framer->length < (size_t)MAX_ENCODED_BUFFER_SIZE)
	{
		framer->buffer[framer->length] = byte;
		framer->length++;
		if (framer->length >= (size_t)MAX_ENCODED_BUFFER_SIZE)
		{
			/* No room for more bytes and no marker received: the current
			 * accumulation is not a valid frame.  Drop it and report
			 * overflow so the caller can account for the error. */
			framer->length = 0U;
			result = FRAMER_OVERFLOW;
		}
	}
	else
	{
		framer->length = 0U;
		result = FRAMER_OVERFLOW;
	}

	return result;
}
