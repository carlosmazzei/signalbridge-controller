#ifndef __COBS_H__
#define __COBS_H__

#include <stdio.h>
#include <stdint.h>

size_t cobs_encode(const void *data, size_t length, uint8_t *buffer);
size_t cobs_decode(const uint8_t *buffer, size_t length, void *data);

#endif /* __COBS_H__ */
