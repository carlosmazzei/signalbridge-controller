/**
 * @file tm1637.c
 * @author
 *   Carlos Mazzei <carlos.mazzei@gmail.com>
 * @brief Implementation of the TM1637 LED driver using GPIO bit-banging for Raspberry Pi Pico
 *
 * This driver utilizes GPIO bit-banging to implement the two-wire protocol (CLK and DIO)
 * required to control the TM1637 LED driver chip
 */

#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include "outputs.h"
#include "tm1637.h"

/**
 * @brief Convert tm1637_result_t to output_result_t
 *
 * This function converts TM1637-specific error codes to generic output error codes
 * for consistent error handling throughout the system.
 *
 * @param[in] tm_result TM1637-specific result code
 * @return output_result_t Generic output result code
 */
static output_result_t tm1637_to_output_result(tm1637_result_t tm_result)
{
	output_result_t result;

	switch (tm_result)
	{
	case TM1637_OK:
		result = OUTPUT_OK;
		break;
	case TM1637_ERR_INVALID_PARAM:
		result = OUTPUT_ERR_INVALID_PARAM;
		break;
	default:
		result = OUTPUT_ERR_DISPLAY_OUT;
		break;
	}

	return result;
}


// ---- Bit-bang helpers (open-drain emulation) ----

#define TM1637_DELAY_US (3)

/**
 * @brief Configure a TM1637 signal for open-drain high state.
 *
 * @param[in] pin GPIO number to release.
 */
static inline void tm1637_pin_release(uint8_t pin)
{
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_pull_up(pin);
    gpio_set_dir(pin, GPIO_IN); // high-Z with pull-up -> logic high
}

/**
 * @brief Drive a TM1637 signal low.
 *
 * @param[in] pin GPIO number to pull low.
 */
static inline void tm1637_pin_low(uint8_t pin)
{
    gpio_set_function(pin, GPIO_FUNC_SIO);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, 0);
}

/**
 * @brief Release the CLK line, allowing the pull-up to drive it high.
 *
 * @param[in] config TM1637 driver configuration.
 */
static inline void tm1637_clk_high(const output_driver_t *config)
{
    tm1637_pin_release(config->clk_pin);
}

/**
 * @brief Actively drive the CLK line low.
 *
 * @param[in] config TM1637 driver configuration.
 */
static inline void tm1637_clk_low(const output_driver_t *config)
{
    tm1637_pin_low(config->clk_pin);
}

/**
 * @brief Release the DIO line, allowing the pull-up to drive it high.
 *
 * @param[in] config TM1637 driver configuration.
 */
static inline void tm1637_dio_high(const output_driver_t *config)
{
    tm1637_pin_release(config->dio_pin);
}

/**
 * @brief Drive the DIO line low.
 *
 * @param[in] config TM1637 driver configuration.
 */
static inline void tm1637_dio_low(const output_driver_t *config)
{
    tm1637_pin_low(config->dio_pin);
}

/**
 * @brief Restore GPIO functions after bit-banging transactions.
 *
 * @param[in] config TM1637 driver configuration.
 */
static inline void tm1637_restore_spi_pins(const output_driver_t *config)
{
    // Restore SPI function so TM1639 (or others) can use the bus
    gpio_set_function(config->dio_pin, GPIO_FUNC_SPI);
    gpio_set_function(config->clk_pin, GPIO_FUNC_SPI);
}

/**
 * @brief Generate a TM1637 START condition and select the device.
 *
 * @param[in] config TM1637 driver configuration.
 */
static inline void tm1637_start(const output_driver_t *config)
{
    // Select the chip via multiplexer
    (void)config->select_interface(config->chip_id, true);

    // Ensure idle high on both lines
    tm1637_clk_high(config);
    tm1637_dio_high(config);
    sleep_us(TM1637_DELAY_US);

    // Start: DIO goes low while CLK is high, then pull CLK low
    tm1637_dio_low(config);
    sleep_us(TM1637_DELAY_US);
    tm1637_clk_low(config);
    sleep_us(TM1637_DELAY_US);
}

/**
 * @brief Generate a TM1637 STOP condition and deselect the device.
 *
 * @param[in] config TM1637 driver configuration.
 */
static inline void tm1637_stop(const output_driver_t *config)
{
    // Ensure DIO low, then release CLK, then release DIO
    tm1637_dio_low(config);
    sleep_us(TM1637_DELAY_US);
    tm1637_clk_high(config);
    sleep_us(TM1637_DELAY_US);
    tm1637_dio_high(config);
    sleep_us(TM1637_DELAY_US);

    // Deselect the chip via multiplexer
    (void)config->select_interface(config->chip_id, false);

    // Restore SPI pin function for other devices
    tm1637_restore_spi_pins(config);
}

/**
 * @brief Transmit one byte using the TM1637 two-wire protocol.
 *
 * @param[in] config TM1637 driver configuration.
 * @param[in] data   Byte to transmit (LSB first).
 *
 * @return 1 when an ACK is received from the device, otherwise 0.
 */
static inline int tm1637_write_byte(const output_driver_t *config, uint8_t data)
{
    // Send 8 bits, LSB first
    for (uint8_t i = 0U; i < (uint8_t)8; i++)
    {
        tm1637_clk_low(config);

        if ((data >> i) & 0x01U)
        {
            tm1637_dio_high(config); // release -> logic high
        }
        else
        {
            tm1637_dio_low(config); // drive low
        }

        sleep_us(TM1637_DELAY_US);
        tm1637_clk_high(config);
        sleep_us(TM1637_DELAY_US);
    }

    // ACK cycle: release DIO and sample while CLK high
    tm1637_clk_low(config);
    tm1637_dio_high(config); // release line for ACK from device
    sleep_us(TM1637_DELAY_US);
    tm1637_clk_high(config);
    sleep_us(TM1637_DELAY_US);
    int ack = (gpio_get(config->dio_pin) == 0) ? 1 : 0; // 0 = pulled low by device -> ACK
    tm1637_clk_low(config);
    sleep_us(TM1637_DELAY_US);

    return ack;
}

/**
 * @brief Send a command byte to the TM1637 device.
 *
 * @param[in] config TM1637 driver configuration.
 * @param[in] cmd    Command byte to transmit.
 *
 * @return TM1637_OK on success or an error code otherwise.
 */
static tm1637_result_t tm1637_send_command(const output_driver_t *config, uint8_t cmd)
{
    tm1637_result_t result = TM1637_OK;

	// Parameter validation
	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
        tm1637_start(config);
        int write_result = tm1637_write_byte(config, cmd);
        tm1637_stop(config);

        if (1 != write_result)
        {
            result = TM1637_ERR_WRITE;
        }
    }

    return result;
}

/**
 * @brief Set the display brightness level (0-7).
 *
 * This function sets the brightness level of the TM1637 display. The brightness value
 * is clamped to the range 0 to 7. The function updates the driver configuration and
 * sends the appropriate command to the TM1637 device.
 *
 * @param[in,out] config Pointer to the TM1637 output driver configuration structure. Must not be NULL.
 * @param[in]     level  Desired brightness level (0 = minimum, 7 = maximum).
 *
 * @return TM1637_OK on success,
 *         TM1637_ERR_INVALID_PARAM if config is NULL,
 *         or error code from tm1637_send_command.
 */
static tm1637_result_t tm1637_set_brightness(output_driver_t *config, uint8_t level)
{
	tm1637_result_t result = TM1637_OK;
	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
        uint8_t set_level = (level > (uint8_t)7) ? (uint8_t)7 : level;
        config->brightness = set_level;

        // Display control command with brightness level
        uint8_t cmd = (uint8_t)TM1637_CMD_DISPLAY_ON | set_level;
        result = tm1637_send_command(config, cmd);
    }

	return result;
}

tm1637_result_t tm1637_display_on(output_driver_t *config)
{
	tm1637_result_t result = TM1637_OK;
	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
		config->display_on = true;
		result = tm1637_send_command(config, (uint8_t)TM1637_CMD_DISPLAY_ON | config->brightness);
	}

	return result;
}

output_driver_t* tm1637_init(uint8_t chip_id,
                             output_result_t (*select_interface)(uint8_t chip_id, bool select),
                             spi_inst_t *spi,
                             uint8_t dio_pin,
                             uint8_t clk_pin)
{
	output_driver_t *config = NULL;
	uint8_t valid = 1U;

	config = pvPortMalloc(sizeof(output_driver_t));
	if (NULL == config)
	{
		valid = 0U;
	}

	// Parameter validation
	if ((1U == valid) &&
	    ((chip_id >= (uint8_t)MAX_SPI_INTERFACES) ||
	     (NULL == select_interface) ||
	     (NULL == spi) ||
	     (dio_pin >= (uint8_t)NUM_GPIO) ||
	     (clk_pin >= (uint8_t)NUM_GPIO)))
	{
		valid = 0U;
	}

	// Only initialize if all parameters are valid
	if (1U == valid)
	{
		config->chip_id = chip_id;
		config->select_interface = select_interface;
		config->spi = spi;
		config->dio_pin = dio_pin;
		config->clk_pin = clk_pin;

		// Initialize buffer and state
		(void)memset(config->active_buffer, 0, sizeof(config->active_buffer));
		(void)memset(config->prep_buffer, 0, sizeof(config->prep_buffer));
		config->buffer_modified = false;
		config->brightness = 7U;
		config->display_on = false;
		config->set_digits = &tm1637_set_digits;
		config->set_leds = &tm1637_set_leds;

		// Clear display on startup
		if (TM1637_OK != tm1637_clear(config))
		{
			valid = 0U;
		}

		// Set maximum brightness
		if (TM1637_OK != tm1637_set_brightness(config, 7U))
		{
			valid = 0U;
		}

		// Turn on display
		if (TM1637_OK != tm1637_display_on(config))
		{
			valid = 0U;
		}
	}

	// Free resources and set pointer to NULL on error
	if ((1U != valid) && (NULL != config))
	{
		vPortFree(config);
		config = NULL;
	}

	return config;
}

/**
 * @brief Update specific bytes in the preparation buffer
 */
static tm1637_result_t tm1637_update_buffer(output_driver_t *config, uint8_t addr, uint8_t data)
{
	tm1637_result_t result = TM1637_OK;

	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else if (addr >= TM1637_DISPLAY_BUFFER_SIZE)
	{
		result = TM1637_ERR_ADDRESS_RANGE;
	}
	else
	{
		// Update prep buffer and mark as modified
		config->prep_buffer[addr] = data;
		config->buffer_modified = true;
	}

	return result;
}

/**
 * @brief Flush the prepared buffer to the TM1637 display.
 *
 * This function copies the preparation buffer to the active buffer, resets the modified flag,
 * and writes all buffer data to the TM1637 device.
 *
 * @param[in,out] config Pointer to the TM1637 output driver configuration structure. Must not be NULL.
 *
 * @return TM1637_OK on success, error code otherwise
 */
static tm1637_result_t tm1637_flush(output_driver_t *config)
{
    tm1637_result_t result = TM1637_OK;

	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
		// Copy prep buffer to active buffer
		(void)memcpy(config->active_buffer, config->prep_buffer, TM1637_DISPLAY_BUFFER_SIZE);

		// Reset the modified flag
		config->buffer_modified = false;

        // TM1637 Protocol: Send data command first
        tm1637_start(config);
        if (tm1637_write_byte(config, TM1637_CMD_DATA_WRITE) != 1)
        {
            result = TM1637_ERR_WRITE;
        }
        tm1637_stop(config);

		// TM1637 Protocol: Send address and data
		if (TM1637_OK == result)
		{
            tm1637_start(config);

            // Send starting address (0xC0 for first digit)
            if (tm1637_write_byte(config, TM1637_CMD_ADDR_BASE) != 1)
            {
                result = TM1637_ERR_WRITE;
            }
            else
            {
                // Write all 4 digits data to TM1637
                for (uint8_t i = 0U; (i < TM1637_DIGIT_COUNT) && (TM1637_OK == result); i++)
                {
                    if (tm1637_write_byte(config, config->active_buffer[i]) != 1)
                    {
                        result = TM1637_ERR_WRITE;
                    }
                }
            }
            tm1637_stop(config);
        }
    }

    return result;
}

/**
 * @brief Update the display with the current preparation buffer contents.
 *
 * This function updates the display with the contents of the preparation buffer
 * when it has been marked as modified.
 *
 * @param[in,out] config TM1637 driver configuration.
 *
 * @return TM1637_OK if the display was updated successfully, error code otherwise.
 */
static tm1637_result_t tm1637_update(output_driver_t *config)
{
	tm1637_result_t result = TM1637_OK;
	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
		// Only update if buffer has been modified
		if (config->buffer_modified)
		{
			result = tm1637_flush(config);
		}
	}

	return result;
}

/**
 * @brief Validate a custom character digit array.
 *
 * @param[in] digits Pointer to the digit array to validate.
 * @param[in] length Number of digits provided in @p digits.
 *
 * @return TM1637_OK if every entry is valid, error code otherwise.
 */
static tm1637_result_t tm1637_validate_custom_array(const uint8_t* digits, const size_t length)
{
	tm1637_result_t result = TM1637_OK;

	for (size_t i = 0U; (i < length) && (TM1637_OK == result); i++)
	{
		// Check if high nibble is zero and low nibble is valid (0-F)
		if ((digits[i] & 0xF0U) != 0x00U)
		{
			result = TM1637_ERR_INVALID_CHAR;
		}
	}

	return result;
}

/**
 * @brief Validate TM1637 digit update parameters.
 *
 * @param[in] config        TM1637 driver configuration.
 * @param[in] digits        Pointer to the BCD digit array.
 * @param[in] length        Number of entries stored in @p digits.
 * @param[in] dot_position  Decimal point position or TM1637_NO_DECIMAL_POINT.
 *
 * @return TM1637_OK if all parameters are valid, error code otherwise.
 */
static tm1637_result_t tm1637_validate_parameters(const output_driver_t *config,
                                                  const uint8_t* digits,
                                                  const size_t length,
                                                  const uint8_t dot_position)
{
	tm1637_result_t result = TM1637_OK;

	// Single compound condition reduces cyclomatic complexity
	if ((NULL == config) ||
	    (NULL == digits) ||
	    (length != TM1637_DIGIT_COUNT) ||
	    ((dot_position > 3U) && (dot_position != TM1637_NO_DECIMAL_POINT)))
	{
		result = TM1637_ERR_INVALID_PARAM;
	}

	return result;
}

/**
 * @brief Convert BCD digits into TM1637 segment data.
 *
 * @param[in,out] config      TM1637 driver configuration.
 * @param[in]     digits      Pointer to the BCD digit array.
 * @param[in]     dot_position Decimal point position or TM1637_NO_DECIMAL_POINT.
 *
 * @return TM1637_OK when the preparation buffer was updated successfully.
 */
static tm1637_result_t tm1637_process_digits(output_driver_t *config, const uint8_t* digits, const uint8_t dot_position)
{
	// Segment patterns for custom character set
	// Bits: dp-g-f-e-d-c-b-a (MSB to LSB)
	static const uint8_t tm1637_custom_patterns[16] = {
		0x3FU, 0x06U, 0x5BU, 0x4FU, // 0x00-0x03: 0, 1, 2, 3
		0x66U, 0x6DU, 0x7DU, 0x07U, // 0x04-0x07: 4, 5, 6, 7
		0x7FU, 0x6FU, 0x6DU, 0x1CU, // 0x08-0x0B: 8, 9, S, t
		0x5EU, 0x40U, 0x08U, 0x00U // 0x0C-0x0F: d, -, _, (blank)
	};

	tm1637_result_t result = TM1637_OK;

	for (uint8_t i = 0U; (i < TM1637_DIGIT_COUNT) && (TM1637_OK == result); i++)
	{
		// Extract BCD digit and get pattern
		uint8_t bcd_digit = digits[i] & 0x0FU;
		uint8_t segment_data = tm1637_custom_patterns[bcd_digit];

		// Add decimal point using conditional expression (no if-statement)
		segment_data |= (i == dot_position) ? TM1637_DECIMAL_POINT_MASK : 0U;

		// Store in prep buffer
		config->prep_buffer[i] = segment_data;
	}

	if (TM1637_OK == result)
	{
		config->buffer_modified = true;
	}

	return result;
}

/**
 * @brief Update the TM1637 display with new BCD digits.
 *
 * @param[in,out] config       TM1637 driver configuration.
 * @param[in]     digits       Pointer to the BCD digit array.
 * @param[in]     length       Number of digits stored in @p digits.
 * @param[in]     dot_position Decimal point position or TM1637_NO_DECIMAL_POINT.
 *
 * @return OUTPUT_OK on success or an error code describing the failure.
 */
output_result_t tm1637_set_digits(output_driver_t *config,
                                  const uint8_t *digits,
                                  const size_t length,
                                  const uint8_t dot_position)
{
	tm1637_result_t tm_result;

	// Step 1: Validate parameters
	tm_result = tm1637_validate_parameters(config, digits, length, dot_position);

	// Step 2: Validate BCD encoding
	if (TM1637_OK == tm_result)
	{
		tm_result = tm1637_validate_custom_array(digits, length);
	}

	// Step 3: Process digits
	if (TM1637_OK == tm_result)
	{
		tm_result = tm1637_process_digits(config, digits, dot_position);
	}

	// Step 4: Update display
	if (TM1637_OK == tm_result)
	{
		tm_result = tm1637_update(config);
	}

	return tm1637_to_output_result(tm_result);
}

/**
 * @brief Update an individual LED register on the TM1637 device.
 *
 * @param[in,out] config TM1637 driver configuration.
 * @param[in]     leds   Register index to update.
 * @param[in]     ledstate Segment pattern to apply.
 *
 * @return OUTPUT_OK on success or an error code describing the failure.
 */
output_result_t tm1637_set_leds(output_driver_t *config, const uint8_t leds, const uint8_t ledstate)
{
	tm1637_result_t tm_result = TM1637_OK;

	// Parameter validation
	if (NULL == config)
	{
		tm_result = TM1637_ERR_INVALID_PARAM;
	}
	else if (leds >= TM1637_DISPLAY_BUFFER_SIZE)
	{
		tm_result = TM1637_ERR_ADDRESS_RANGE;
	}
	else
	{
		// Update preparation buffer with LED state
		tm_result = tm1637_update_buffer(config, leds, ledstate);

		// Update display if buffer update was successful
		if (TM1637_OK == tm_result)
		{
			tm_result = tm1637_update(config);
		}
	}

	return tm1637_to_output_result(tm_result);
}

tm1637_result_t tm1637_display_off(output_driver_t *config)
{
	tm1637_result_t result = TM1637_OK;
	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
		config->display_on = false;
		result = tm1637_send_command(config, TM1637_CMD_DISPLAY_OFF);
	}
	return result;
}

tm1637_result_t tm1637_clear(output_driver_t *config)
{
    tm1637_result_t result = TM1637_OK;

	// Parameter validation
	if (NULL == config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
		// Clear internal buffers first
		(void)memset(config->active_buffer, 0, TM1637_DISPLAY_BUFFER_SIZE);
		(void)memset(config->prep_buffer, 0, TM1637_DISPLAY_BUFFER_SIZE);
		config->buffer_modified = false;

        // Send clear data to display
        tm1637_start(config);
        if (1 == tm1637_write_byte(config, TM1637_CMD_DATA_WRITE))
        {
            tm1637_stop(config);

            tm1637_start(config);
            if (1 == tm1637_write_byte(config, TM1637_CMD_ADDR_BASE))
            {
                // Write zeros to clear all 4 digits
                for (uint8_t i = 0U; (i < TM1637_DIGIT_COUNT) && (TM1637_OK == result); i++)
                {
                    if (1 != tm1637_write_byte(config, 0U))
                    {
                        result = TM1637_ERR_WRITE;
                    }
                }
            }
            else
            {
                result = TM1637_ERR_WRITE;
            }
            tm1637_stop(config);
        }
        else
        {
            tm1637_stop(config);
            result = TM1637_ERR_WRITE;
        }
    }

    return result;
}

tm1637_result_t tm1637_deinit(output_driver_t *config)
{
	tm1637_result_t result = TM1637_OK;
	if (!config)
	{
		result = TM1637_ERR_INVALID_PARAM;
	}
	else
	{
		// Turn off display
		result = tm1637_display_off(config);
		
		// Reset GPIO pins to safe state
		gpio_put(config->dio_pin, 0);
		gpio_put(config->clk_pin, 0);
		gpio_deinit(config->dio_pin);
		gpio_deinit(config->clk_pin);
	}
	return result;
}
