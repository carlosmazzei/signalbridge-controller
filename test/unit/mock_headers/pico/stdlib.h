#pragma once
// Mock pico/stdlib.h
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Pull in prototypes used by stdlib consumers
#include "hardware/gpio.h"
#include "pico/time.h"

typedef unsigned int uint;

// GPIO defines
#define GPIO_OUT true
#define GPIO_IN false
#define GPIO_FUNC_SPI 1
#define GPIO_FUNC_PWM 2
#define GPIO_FUNC_UART 3
#define GPIO_FUNC_SIO 4
#define GPIO_FUNC_NULL 0

// SPI defines  
#define SPI_CPOL_0 0
#define SPI_CPHA_0 0
#define SPI_LSB_FIRST true

// External declarations for shared types (avoid redefinition)
extern struct spi_inst_t *spi0;

// Pico board constants
#define PICO_DEFAULT_LED_PIN 25
#define PICO_DEFAULT_SPI_RX_PIN 16
#define PICO_DEFAULT_SPI_TX_PIN 19
#define PICO_DEFAULT_SPI_SCK_PIN 18

// Additional functions
void busy_wait_ms(uint32_t ms);
uint32_t save_and_disable_interrupts(void);
void adc_init(void);


// UART constants
extern void* uart0;
void uart_init(void* uart, uint baudrate);
void uart_set_baudrate(void* uart, uint baudrate);  
void uart_set_hw_flow(void* uart, bool cts, bool rts);
void uart_set_format(void* uart, uint data_bits, uint stop_bits, uint parity);
void uart_set_fifo_enabled(void* uart, bool enabled);


// More Pico SDK constants and functions
#define PICO_DEFAULT_SPI_CSN_PIN 17
// Board info macros (no-op in tests)
#define bi_decl(x)
void bi_4pins_with_func(uint pin1, uint pin2, uint pin3, uint pin4, uint func);

// Note: Do not declare FreeRTOS APIs here; real headers provide them in tests
