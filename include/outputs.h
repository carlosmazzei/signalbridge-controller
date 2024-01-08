#ifndef _LED_H_
#define _LED_H_

#include <stdint.h>
#include <stdbool.h>
#include "hardware/spi.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"

/**
 * LED latch output select pin
 */
#define LED_CS_PIN 17

/**
 * SPI frequency in Hz
 */
#define SPI_FREQUENCY 10 * 1000 * 1000

/**
 * SPI buffer length
 */
#define SPI_BUF_LEN 10

/**
 * PWM pin
 */
#define PWM_PIN 28

/**
 * Function Prototypes
 */
bool led_out(uint8_t index, uint8_t *states, uint8_t len);
void led_select();
bool output_init();
int display_out(uint8_t *data, uint8_t len);
void set_pwm_duty(uint8_t duty);

#endif