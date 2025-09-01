#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef unsigned int uint;

// GPIO function declarations  
void gpio_init(uint pin);
void gpio_init_mask(uint32_t mask);
void gpio_set_dir(uint pin, bool out);
void gpio_set_dir_masked(uint32_t mask, uint32_t value);
void gpio_put(uint pin, bool value);
void gpio_put_masked(uint32_t mask, uint32_t value);
bool gpio_get(uint pin);
void gpio_set_function(uint pin, uint fn);
uint gpio_get_function(uint pin);
void gpio_pull_up(uint pin);
void gpio_pull_down(uint pin);
void gpio_deinit(uint pin);
void adc_gpio_init(uint pin);

// Additional ADC functions
void adc_init(void);
void adc_select_input(uint input);
uint16_t adc_read(void);
