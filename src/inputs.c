/**
 * @file inputs.c
 * @brief Implementation of keypad, ADC, and rotary encoder input handling for the A320 Pico Controller (FreeRTOS).
 * @author
 *   Carlos Mazzei <carlos.mazzei@gmail.com>
 * @date 2020-2025
 *
 * This file implements the input subsystem, including keypad scanning, ADC reading/filtering,
 * and rotary encoder handling. It generates events for each input type and sends them to the event queue.
 *
 *  @copyright
 *   (c) 2020-2025 Carlos Mazzei. All rights reserved.
 */

#include "inputs.h"
#include "data_event.h"
#include "task_props.h"
#include "hardware/watchdog.h"
#include "commands.h"

/**
 * @brief Global input configuration instance.
 */
input_config_t input_config;

/**
 * @brief State array for each key in the keypad matrix.
 */
uint8_t keypad_state[KEYPAD_ROWS * KEYPAD_COLUMNS];

input_result_t input_init(const input_config_t *config)
{
	input_result_t result = INPUT_OK;

	if ((config->columns > KEYPAD_MAX_COLS) ||
	    (config->rows > KEYPAD_MAX_ROWS) ||
	    (config->adc_channels > 16U) ||
	    (0U == config->key_settling_time_ms) ||
	    (0U == config->adc_settling_time_ms) ||
	    (0U == config->encoder_settling_time_ms) ||
	    (NULL == config->input_event_queue))
	{
		result = INPUT_INVALID_CONFIG;
	}
	else
	{
		// Initialize ADC configuration
		input_config.adc_channels = config->adc_channels;
		input_config.adc_settling_time_ms = config->adc_settling_time_ms;

		// Initialize keypad configuration
		input_config.rows = config->rows;
		input_config.columns = config->columns;
		(void)memset(keypad_state, 0, sizeof(keypad_state));
		input_config.key_settling_time_ms = config->key_settling_time_ms;
		input_config.input_event_queue = config->input_event_queue;

		// Initialize encoder configuration
		for (uint8_t i = 0U; i < MAX_NUM_ENCODERS; i++)
		{
			input_config.encoder_mask[i] = config->encoder_mask[i];
		}
		input_config.encoder_settling_time_ms = config->encoder_settling_time_ms;

		// Setup IO pins / use gpio_init_mask insted to initialized multiple pins
		uint32_t gpio_mask = ((1UL << KEYPAD_COL_MUX_A) |
		                      (1UL << KEYPAD_COL_MUX_B) |
		                      (1UL << KEYPAD_COL_MUX_C) |
		                      (1UL << KEYPAD_COL_MUX_CS) |
		                      (1UL << KEYPAD_ROW_MUX_A) |
		                      (1UL << KEYPAD_ROW_MUX_B) |
		                      (1UL << KEYPAD_ROW_MUX_C) |
		                      (1UL << KEYPAD_ROW_MUX_CS) |
		                      (1UL << KEYPAD_ROW_INPUT) |
		                      (1UL << ADC_MUX_A) |
		                      (1UL << ADC_MUX_B) |
		                      (1UL << ADC_MUX_C));

		gpio_init_mask(gpio_mask);
		gpio_set_dir_masked(gpio_mask, 0xFFU);
		gpio_set_dir(KEYPAD_ROW_INPUT, false);
		gpio_put_masked(gpio_mask, 0x00U); // Set all pins to low

		adc_init(); // ADC init
		adc_gpio_init(26); // Make sure GPIO is high-impedance, no pullups etc

	}

	return result;
}

/**
 * @brief Control the chip select (CS) for the keypad row multiplexer.
 * @param[in] select true to enable (active low), false to disable.
 */
static inline void keypad_cs_rows(bool select)
{
	gpio_put(KEYPAD_ROW_MUX_CS, !select); // Active low pin
}

/**
 * @brief Control the chip select (CS) for the keypad column multiplexer.
 * @param[in] select true to enable (active low), false to disable.
 */
static inline void keypad_cs_columns(bool select)
{
	gpio_put(KEYPAD_COL_MUX_CS, !select); // Active low pin
}

void keypad_task(void *pvParameters)
{
	// cppcheck-suppress[misra-c2012-11.5] ; Required by FreeRTOS DEVIATION(D3)
	task_props_t * task_props = (task_props_t*) pvParameters;

	while (true)
	{
		for (uint8_t c = 0; c < input_config.columns; c++)
		{
			// Select the column
			keypad_set_columns(c);
			keypad_cs_columns(true);
			uint8_t keycode_base = c * input_config.rows;

			// Settle the column
			vTaskDelay(pdMS_TO_TICKS(input_config.key_settling_time_ms));

			for (uint8_t r = 0; r < input_config.rows; r++)
			{
				if (true == input_config.encoder_mask[r])
				{
					continue; // Skip encoder
				}

				keypad_set_rows(r); // Also set the ADC channels
				keypad_cs_rows(true);

				uint8_t keycode = keycode_base + r;
				bool pressed = !gpio_get(KEYPAD_ROW_INPUT); // Active low pin
				keypad_state[keycode] = ((keypad_state[keycode] << 1U) & 0xFEU) | (pressed ? 1U : 0U);

				if (KEY_PRESSED_MASK == (keypad_state[keycode] & KEYPAD_STABILITY_MASK))
				{
					keypad_generate_event(r, c, KEY_PRESSED);
				}
				if (KEY_RELEASED_MASK == (keypad_state[keycode] & KEYPAD_STABILITY_MASK))
				{
					keypad_generate_event(r, c, KEY_RELEASED);
				}
				keypad_cs_rows(false);
			}

			keypad_cs_columns(false);
		}

		task_props->high_watermark = (uint8_t)uxTaskGetStackHighWaterMark(NULL);
		watchdog_update();
	}
}

void keypad_set_columns(uint8_t columns)
{
	// Set the columns of the keypad (multiplexer control)
	gpio_put(KEYPAD_COL_MUX_A, (columns & 0x01U) != 0U);
	gpio_put(KEYPAD_COL_MUX_B, (columns & 0x02U) != 0U);
	gpio_put(KEYPAD_COL_MUX_C, (columns & 0x04U) != 0U);
}

void keypad_set_rows(uint8_t rows)
{
	// Set the rows of the keypad (multiplexer control)
	gpio_put(KEYPAD_ROW_MUX_A, (rows & 0x01U) != 0U);
	gpio_put(KEYPAD_ROW_MUX_B, (rows & 0x02U) != 0U);
	gpio_put(KEYPAD_ROW_MUX_C, (rows & 0x04U) != 0U);
}

void keypad_generate_event(uint8_t row, uint8_t column, uint8_t state)
{
	if (NULL != input_config.input_event_queue)
	{
		data_events_t key_event;
		key_event.command = PC_KEY_CMD;
		key_event.data[0] = ((column << 4U) | (row << 1U)) & 0xFEU;
		key_event.data[0] |= state;
		key_event.data_length = 1;
		xQueueSend(input_config.input_event_queue, &key_event, portMAX_DELAY);
	}
}

/**
 * @brief Generate and send an ADC event to the input event queue.
 * @param channel ADC channel number.
 * @param value Filtered ADC value.
 * @details The event contains the channel and the 16-bit value split into two bytes.
 *          The event is only sent if the input event queue is not NULL.
 */
static void adc_generate_event(uint8_t channel, uint16_t value)
{
	if (NULL != input_config.input_event_queue)
	{
		data_events_t adc_event;
		adc_event.command = PC_AD_CMD;
		adc_event.data[0] = channel;
		adc_event.data[1] = (value & 0xFF00U) >> 8;
		adc_event.data[2] = value & 0x00FFU;
		adc_event.data_length = 3;
		xQueueSend(input_config.input_event_queue, &adc_event, portMAX_DELAY);
	}
}

/**
 * @brief Calculate the moving average for an ADC channel.
 * @param[in] channel ADC channel index.
 * @param[in] new_sample New ADC sample value.
 * @param[in,out] samples Pointer to the sample buffer for the channel.
 * @param[in,out] adc_states Pointer to the ADC states structure.
 * @return The moving average value for the specified channel.
 * @details Implements a circular buffer for moving average filtering.
 */
static uint16_t adc_moving_average(uint16_t channel, uint16_t new_sample, uint16_t *samples, adc_states_t *adc_states)
{
	// Remove old sample and add the new one to the sum
	adc_states->adc_sum_values[channel] -= samples[adc_states->samples_index[channel]];
	adc_states->adc_sum_values[channel] += new_sample;
	samples[adc_states->samples_index[channel]] = new_sample;

	// Adjust the new index
	adc_states->samples_index[channel]++;
	if (adc_states->samples_index[channel] >= (uint16_t)ADC_NUM_TAPS)
	{
		adc_states->samples_index[channel] = 0;
	}

	// Return the moving average
	return (uint16_t)(adc_states->adc_sum_values[channel] / (uint16_t)ADC_NUM_TAPS);
}

void adc_read_task(void *pvParameters)
{
	adc_states_t adc_states;

	// cppcheck-suppress[misra-c2012-11.5,cstyleCast] ; Required by FreeRTOS DEVIATION(D3)
	task_props_t * task_props = (task_props_t*) pvParameters;

	// Initialize the ADC states
	for (int i = 0; i < ADC_CHANNELS; i++)
	{
		adc_states.adc_previous_value[i] = 0;
		adc_states.adc_sum_values[i] = 0;
		adc_states.samples_index[i] = 0;

		for (int j = 0; j < ADC_NUM_TAPS; j++)
		{
			adc_states.adc_sample_value[i][j] = 0;
		}
	}

	while (true)
	{
		for (uint8_t chan = 0; chan < input_config.adc_channels; chan++)
		{
			// Select the ADC to read from
			adc_mux_select(chan);
			adc_select_input(0);

			// Settle the column
			vTaskDelay(pdMS_TO_TICKS(input_config.adc_settling_time_ms));

			uint16_t adc_raw = adc_read();
			uint16_t filtered_value =
				adc_moving_average(chan, adc_raw, adc_states.adc_sample_value[chan], &adc_states);

			if (adc_states.adc_previous_value[chan] != filtered_value)
			{
				adc_generate_event(chan, filtered_value);
				adc_states.adc_previous_value[chan] = filtered_value;
			}
		}

		// Deselect the CS pin of the ADC mux
		adc_mux_select(0);

		task_props->high_watermark = (uint8_t)uxTaskGetStackHighWaterMark(NULL);
		watchdog_update();
	}
}

void adc_mux_select(uint8_t channel)
{
	// Select the ADC channel via multiplexer
	gpio_put(ADC_MUX_A, (channel & 0x01U) != 0U);
	gpio_put(ADC_MUX_B, (channel & 0x02U) != 0U);
	gpio_put(ADC_MUX_C, (channel & 0x04U) != 0U);
	gpio_put(ADC_MUX_D, (channel & 0x08U) != 0U);
}

void encoder_read_task(void *pvParameters)
{
	const int8_t encoder_states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
	encoder_states_t encoder_state[MAX_NUM_ENCODERS];

	// cppcheck-suppress[misra-c2012-11.5,cstyleCast] ; Required by FreeRTOS DEVIATION(D3)
	task_props_t * task_prop = (task_props_t*) pvParameters;

	for (uint8_t i = 0; i < MAX_NUM_ENCODERS; i++)
	{
		encoder_state[i].old_encoder = 0;
		encoder_state[i].count_encoder = 0;
	}

	while (true)
	{
		for (uint8_t r = 0; r < input_config.rows; r++)
		{
			if (false == input_config.encoder_mask[r])
			{
				continue;
			}

			uint8_t encoder_base = r * (input_config.columns / 2U);
			keypad_cs_rows(true);
			keypad_set_rows(r);

			for (uint8_t c = 0U; c < (input_config.columns / 2U); c++)
			{
				encoder_base += c;
				keypad_cs_columns(true);
				keypad_set_columns(c);
				vTaskDelay(pdMS_TO_TICKS(input_config.encoder_settling_time_ms));
				bool e11 = !gpio_get(KEYPAD_ROW_INPUT); // Active low pin

				keypad_set_columns(c + 1U);
				vTaskDelay(pdMS_TO_TICKS(input_config.encoder_settling_time_ms));
				bool e12 = !gpio_get(KEYPAD_ROW_INPUT); // Active low pin

				/**
				 * @par Calculate the encoder state
				 * Remember previous state by shifting the lower bits up
				 * AND the lower 2 bits of port a, then OR them with var old_Encoder1 to set new value
				 * the lower 4 bits of old_Encoder1 are
				 */
				encoder_state[encoder_base].old_encoder <<= 2;
				encoder_state[encoder_base].old_encoder |= (uint8_t)(e11 ? 1U : 0U);
				encoder_state[encoder_base].old_encoder |= ((uint8_t)(e12 ? 1U : 0U) << 1U) & 0x03U;
				encoder_state[encoder_base].count_encoder += encoder_states[encoder_state[encoder_base].old_encoder & 0x0FU];

				if (4 == encoder_state[encoder_base].count_encoder)
				{
					encoder_generate_event(encoder_base, 1);
				}                                  // then the index for enc_states
				else if (encoder_state[0].count_encoder == -4)
				{
					encoder_generate_event(encoder_base, 0);
				}

				keypad_cs_columns(false);
			}

			keypad_cs_rows(false);
		}

		// Get free heap for the task
		task_prop->high_watermark = (uint8_t)uxTaskGetStackHighWaterMark(NULL);
		// Update watchdog timer
		watchdog_update();
	}
}

void encoder_generate_event(uint8_t rotary, uint16_t direction)
{
	if (NULL != input_config.input_event_queue)
	{
		data_events_t encoder_event;
		encoder_event.data[0] = 0;
		encoder_event.data[1] = 0;
		encoder_event.command = PC_ROTARY_CMD;
		encoder_event.data[0] |= rotary << 4;
		encoder_event.data[1] |= direction;
		encoder_event.data_length = 2;
		xQueueSend(input_config.input_event_queue, &encoder_event, portMAX_DELAY);
	}
}

/**
 * @brief Set the encoder mask to enable or disable encoders.
 * @param mask Bitmask to enable/disable encoders.
 */
static void encoder_set_mask(uint8_t mask)
{
	for (uint8_t i = 0; i < MAX_NUM_ENCODERS; i++)
	{
		input_config.encoder_mask[i] = mask & (1U << i);
	}
}
