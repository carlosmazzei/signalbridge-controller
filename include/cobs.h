#ifndef COBS_H
#define COBS_H

#include <stdint.h>

size_t cobs_encode(const void *data, size_t length, uint8_t *buffer);
size_t cobs_decode(const uint8_t *buffer, size_t length, void *data);

#endif /* COBS_H */
