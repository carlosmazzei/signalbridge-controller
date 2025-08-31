/**
 * @file hardware_mocks.c
 * @brief Hardware abstraction mocks for testing
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include "hardware/pwm.h"

// Pico SDK mock types and functions
typedef struct {
    uint32_t scratch[8];
} watchdog_hw_t;

static watchdog_hw_t mock_watchdog_instance = {0};
watchdog_hw_t *watchdog_hw = &mock_watchdog_instance;

// GPIO functions
void gpio_init(uint32_t gpio) { (void)gpio; }
void gpio_set_dir(uint32_t gpio, bool out) { (void)gpio; (void)out; }
void gpio_put(uint32_t gpio, bool value) { (void)gpio; (void)value; }
bool gpio_get(uint32_t gpio) { (void)gpio; return false; }
void gpio_init_mask(uint32_t gpio_mask) { (void)gpio_mask; }
void gpio_set_dir_masked(uint32_t gpio_mask, uint32_t value) { (void)gpio_mask; (void)value; }
void gpio_put_masked(uint32_t gpio_mask, uint32_t value) { (void)gpio_mask; (void)value; }
void gpio_deinit(uint32_t gpio) { (void)gpio; }

// Time functions
void busy_wait_ms(uint32_t ms) { (void)ms; }
uint32_t time_us_32(void) { return 0; }

// Watchdog functions
void watchdog_enable(uint32_t timeout_ms, int pause) { (void)timeout_ms; (void)pause; }
void watchdog_update(void) {}

// Interrupt functions
uint32_t save_and_disable_interrupts(void) { return 0; }
void restore_interrupts(uint32_t status) { (void)status; }

// ADC functions  
void adc_init(void) {}
void adc_gpio_init(uint32_t gpio) { (void)gpio; }
void adc_select_input(uint32_t input) { (void)input; }
uint16_t adc_read(void) { return 0; }

// PWM functions
uint32_t pwm_gpio_to_slice_num(uint32_t gpio) { (void)gpio; return 0; }
void pwm_set_wrap(uint32_t slice, uint16_t wrap) { (void)slice; (void)wrap; }
void pwm_set_chan_level(uint32_t slice, uint32_t chan, uint16_t level) {
    (void)slice; (void)chan; (void)level;
}
void pwm_set_enabled(uint32_t slice, bool enabled) { (void)slice; (void)enabled; }
uint32_t pwm_gpio_to_channel(uint32_t gpio) { (void)gpio; return 0; }
pwm_config pwm_get_default_config(void) { pwm_config cfg = {0}; return cfg; }
void pwm_config_set_clkdiv(pwm_config *config, float div) { (void)config; (void)div; }
void pwm_init(uint32_t slice_num, pwm_config *config, bool start) { (void)slice_num; (void)config; (void)start; }

// SPI functions
typedef struct spi_inst {
    int dummy; // Placeholder member
} spi_inst_t;
static spi_inst_t mock_spi_inst = {0};
spi_inst_t *spi0 = &mock_spi_inst;

uint32_t spi_init(spi_inst_t *spi, uint32_t baudrate) { (void)spi; return baudrate; }
void spi_set_format(spi_inst_t *spi, uint32_t data_bits, uint32_t cpol, uint32_t cpha, uint32_t order) {
    (void)spi; (void)data_bits; (void)cpol; (void)cpha; (void)order;
}
void gpio_set_function(uint32_t gpio, uint32_t fn) { (void)gpio; (void)fn; }
int spi_write_blocking(spi_inst_t *spi, const uint8_t *src, size_t len) { (void)spi; (void)src; return (int)len; }

// FreeRTOS real implementation now used - no more mocks needed

// Constants
#define PICO_DEFAULT_LED_PIN 25
#define GPIO_OUT true
#define GPIO_IN false

uint32_t gpio_get_function(uint32_t gpio) { (void)gpio; return 0; }
void gpio_pull_up(uint32_t gpio) { (void)gpio; }
void sleep_us(uint64_t us) { (void)us; }

typedef struct uart_inst {
    int dummy;
} uart_inst_t;
static uart_inst_t mock_uart_inst_uart0 = {0};
uart_inst_t *uart0 = &mock_uart_inst_uart0;
void uart_init(uart_inst_t *uart, uint32_t baudrate) { (void)uart; (void)baudrate; }
void uart_set_fifo_enabled(uart_inst_t *uart, bool enabled) { (void)uart; (void)enabled; }
