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
    // To-Do: Select appropriate SPI device
    if (index + len < MAX_LED_STATE_SIZE + 1)
    {
        for (int i = 0; i < len; i++)
        {
            led_states[index + i] = states[i]; 
        }
        
        size_t count = spi_write_blocking(spi_default, led_states, sizeof(led_states));
        return true;
    }
    return false;
}