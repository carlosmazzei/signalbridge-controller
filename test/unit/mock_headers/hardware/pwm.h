#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct pwm_config {
    float clkdiv;
    uint16_t level;
} pwm_config;
typedef unsigned int uint;

uint pwm_gpio_to_slice_num(uint pin);
pwm_config pwm_get_default_config(void);
void pwm_config_set_clkdiv(pwm_config *config, float div);
void pwm_init(uint slice_num, pwm_config *config, bool start);
void pwm_set_gpio_level(uint pin, uint16_t level);
