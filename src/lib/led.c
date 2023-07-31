#include "led.h"

/**
 * Maximum size of the array that holds the LED states.
 */
#define MAX_LED_STATE_SIZE 8

/**
 * LED states.
 */
uint8_t led_states[MAX_LED_STATE_SIZE];

/** @brief Output the state of the LEDs to the SPI bus.
 *
 * @param index The index of the first byte of LED states to output.
 * @param states The LED states to output.
 * @param len The number of LED states to output.
 */
bool led_out(uint8_t index, uint8_t *states, uint8_t len)
{

    if (index + len < MAX_LED_STATE_SIZE + 1)
    {
        for (int i = 0; i < len; i++)
        {
            led_states[index + i] = states[i];
        }

        size_t count = spi_write_blocking(spi_default, led_states, sizeof(led_states));
        led_select();
        return true;
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

/** @brief Initialize the LEDs.
 * 
 * Initialize the LED output pins.
 */
void led_init()
{
    gpio_init(LED_CS_PIN);
    gpio_set_dir(LED_CS_PIN, GPIO_OUT);
    gpio_put(LED_CS_PIN, 0);
}