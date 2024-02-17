/**
 * @file outputs.c
 * @brief Functions to output Leds, PWM and other
 * @author Carlos Mazzei
 *
 * This file contains the functions to output Leds, PWM and other
 *
 * @copyright (c) 2020-2024 Carlos Mazzei
 * All rights reserved.
 */

#include "outputs.h"

/**
 * Maximum size of the array that holds the LED states.
 */
#define MAX_LED_STATE_SIZE 8

/**
 * LED states.
 */
uint8_t led_states[MAX_LED_STATE_SIZE];

/** @brief Initialize the outputs.
 *
 * Initialize all the outputs needed for the application: SPI, LEDs, PWM.
 *
 * @return True if successful.
 * 
 * @todo Implement the I2C initialization.
 * 
 */
bool output_init()
{
	// Initialize LEDs
	gpio_init(LED_CS_PIN);
	gpio_set_dir(LED_CS_PIN, GPIO_OUT);
	gpio_put(LED_CS_PIN, 0);

	// Enable Pico default LED pin (GPIO 25)
	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, 1);
	gpio_put(PICO_DEFAULT_LED_PIN, !PICO_DEFAULT_LED_PIN_INVERTED);

	// Enable SPI 0 at 1 MHz and connect to GPIOs
	spi_init(spi_default, SPI_FREQUENCY);
	gpio_set_function(PICO_DEFAULT_SPI_RX_PIN, GPIO_FUNC_SPI);
	gpio_set_function(PICO_DEFAULT_SPI_SCK_PIN, GPIO_FUNC_SPI);
	gpio_set_function(PICO_DEFAULT_SPI_TX_PIN, GPIO_FUNC_SPI);

	// Make the SPI pins available to picotool
	bi_decl(bi_4pins_with_func(PICO_DEFAULT_SPI_RX_PIN, PICO_DEFAULT_SPI_TX_PIN, PICO_DEFAULT_SPI_SCK_PIN, PICO_DEFAULT_SPI_CSN_PIN, GPIO_FUNC_SPI));

	// Configure PWM pin
	gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
	uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);
	pwm_config config = pwm_get_default_config();
	pwm_config_set_clkdiv(&config, 10.f);
	pwm_init(slice_num, &config, true);

	return true;
}

/** @brief Output the state of the LEDs to the SPI bus.
 *
 * @param index The index of the first byte of LED states to output.
 * @param states The LED states to output.
 * @param len The number of LED states to output.
 * 
 * @return True if successful.
 */
bool led_out(uint8_t index, const uint8_t *states, uint8_t len)
{
	if (index + len < MAX_LED_STATE_SIZE + 1)
	{
		for (int i = 0; i < len; i++)
		{
			led_states[index + i] = states[i];
		}

		int count = spi_write_blocking(spi_default, led_states, sizeof(led_states));
		led_select();
		if (count > 0) return true;
	}
	return false;
}

/** @brief Select the LED output.
 *
 * Toggle the latch output of the shift register.
 */
void led_select()
{
	gpio_put(LED_CS_PIN, 1);
	// TO-DO: Should include delay?
	gpio_put(LED_CS_PIN, 0);
}

/** @brief Send data to display controllers
 *
 * @param data The data to send.
 * @param len The length of the data to send.
 * 
 * @return Bytes written
 */
int display_out(const uint8_t *data, uint8_t len)
{
	size_t count = spi_write_blocking(spi_default, data, len);
	return count;
}

/** @brief Set PWM duty cycle
 *
 * @param duty The duty cycle to set.
 */
void set_pwm_duty(uint8_t duty)
{
	// Square the fade value to make the LED's brightness appear more linear
	// Note this range matches with the wrap value
	pwm_set_gpio_level(PWM_PIN, duty * duty);
}