#ifndef _LED_H_
#define _LED_H_

#include <stdint.h>
#include <stdbool.h>
#include "hardware/spi.h"

bool led_out(uint8_t index, uint8_t *states, uint8_t len);

#endif