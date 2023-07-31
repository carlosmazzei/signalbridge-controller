#ifndef _LED_H_
#define _LED_H_

#include <stdint.h>
#include <stdbool.h>
#include "hardware/spi.h"
#include "pico/gpio.h"
#include "pico/stdlib.h"

/**
 * LED latch output select pin
 */
#define LED_CS_PIN 17

/**
 * Function Prototypes
 */
bool led_out(uint8_t index, uint8_t *states, uint8_t len);
void led_select();
void led_init();

#endif