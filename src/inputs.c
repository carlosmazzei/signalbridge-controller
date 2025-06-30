/**
 * @file inputs.c
 * @brief Implementation of keyboard and rotary inputs
 * @author Carlos Mazzei
 *
 * This file contains the input functions to read the keyboard and rotary
 *
 * @copyright (c) 2020-2024 Carlos Mazzei
 * All rights reserved.
 */

#include "inputs.h"
#include "data_event.h"
#include "task_props.h"
#include "hardware/watchdog.h"
#include "commands.h"

/**
 * Input configuration.
 */
input_config_t input_config;

/**
 * States of each row
 */
uint8_t keypad_state[KEYPAD_ROWS * KEYPAD_COLUMNS];

input_result_t input_init(const input_config_t *config)
{
	input_result_t result = INPUT_OK;

	if ((config->columns > KEYPAD_MAX_COLS) ||
	    (config->rows > KEYPAD_MAX_ROWS) ||
	    (config->adc_banks > 2U) ||
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
		input_config.adc_banks = config->adc_banks;
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
		                      (1UL << ADC0_MUX_CS) |
		                      (1UL << ADC1_MUX_CS) |
		                      (1UL << ADC_MUX_A) |
		                      (1UL << ADC_MUX_B) |
		                      (1UL << ADC_MUX_C));

		gpio_init_mask(gpio_mask);
		gpio_set_dir_masked(gpio_mask, 0xFFU);

		gpio_init(KEYPAD_ROW_INPUT);
		gpio_set_dir(KEYPAD_ROW_INPUT, false);

		// ADC init
		adc_init();
		// Make sure GPIO is high-impedance, no pullups etc
		adc_gpio_init(26);
		adc_gpio_init(27);
	}

	return result;
}

/** @brief Keypad task to update and populate key events
 *
 * @param pvParameters Pointer to the keypad task parameters.
 *
 */
void keypad_task(void *pvParameters)
{
	/* cppcheck-suppress[misra-c2012-11.5,cstyleCast] ; Required by FreeRTOS DEVIATION(D3) */
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

/** @brief Set the columns of the keypad.
 *
 * @param columns Columns to set.
 */
void keypad_set_columns(uint8_t columns)
{
	gpio_put(KEYPAD_COL_MUX_A, (columns & 0x01U) != 0U);
	gpio_put(KEYPAD_COL_MUX_B, (columns & 0x02U) != 0U);
	gpio_put(KEYPAD_COL_MUX_C, (columns & 0x04U) != 0U);
}

/** @brief Set the rows of the keypad.
 *
 * @param rows Rows to set.
 */
void keypad_set_rows(uint8_t rows)
{
	gpio_put(KEYPAD_ROW_MUX_A, (rows & 0x01U) != 0U);
	gpio_put(KEYPAD_ROW_MUX_B, (rows & 0x02U) != 0U);
	gpio_put(KEYPAD_ROW_MUX_C, (rows & 0x04U) != 0U);
}

/** @brief Select the row mux chip
 *
 * @param select Level to set the rows CS pin
 *
 */
static inline void keypad_cs_rows(bool select)
{
	gpio_put(KEYPAD_ROW_MUX_CS, !select); // Active low pin
}

/** @brief Select the row mux chip
 *
 * @param select Level to set the columns CS pin
 */
static inline void keypad_cs_columns(bool select)
{
	gpio_put(KEYPAD_COL_MUX_CS, !select); // Active low pin
}

/** @brief Generate a key event.
 *
 * @param row number
 * @param column number
 * @param state of the key
 *
 */
void keypad_generate_event(uint8_t row, uint8_t column, uint8_t state)
{
	if (NULL != input_config.input_event_queue)
	{
		data_events_t key_event;
		key_event.command = PC_KEY_CMD;
		key_event.data[0] = ((column << 4U) | (row << 1U)) & 0xFEU; // Add key state to the event.
		key_event.data[0] |= state;
		key_event.data_length = 1;
		xQueueSend(input_config.input_event_queue, &key_event, portMAX_DELAY);
	}
}

/** @brief Generate an ADC event.
 *
 * @param[in] channel Channel read
 * @param[in] value Value read
 * @details This function generates an ADC event and sends it to the input event queue.
 *          The event contains the channel number and the value read from the ADC.
 * @note The function checks if the input event queue is not NULL before sending the event.
 *       If the queue is NULL, the function does nothing.
 * @note The event data is structured as follows:
 *       - data[0]: Channel number (0-15)
 *       - data[1]: High byte of the ADC value
 *       - data[2]: Low byte of the ADC value
 *       - data_length: 3 (indicating the number of bytes in the data array
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

/** @brief Calculate moving average of the last samples of ad
 *
 * @param[in] channel Channel read
 * @param[in] new_sample New sample
 * @param[in,out] samples Array of samples
 * @param[in,out] adc_states ADC states
 *
 * @details This function calculates the moving average of the last samples
 *          of the ADC channel. It updates the sample array and the sum of values
 *          for the specified channel. The moving average is calculated by removing
 *          the oldest sample from the sum, adding the new sample, and then dividing
 *          the sum by the number of samples (ADC_NUM_TAPS).
 * @note The function uses a circular buffer approach to store the samples,
 *       where the `samples_index` keeps track of the current index in the sample array.
 *       When the index reaches the maximum number of samples (ADC_NUM_TAPS), it wraps
 *       around to the beginning of the array.
 * @return The moving average of the last samples for the specified channel.
 * @retval uint16_t The moving average value for the specified channel.
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

/** @brief Read the ADC value.
 *
 * @param pvParameters Parameters passed to the task
 */
void adc_read_task(void *pvParameters)
{
	adc_states_t adc_states;

	/* cppcheck-suppress[misra-c2012-11.5,cstyleCast] ; Required by FreeRTOS DEVIATION(D3) */
	task_props_t * task_props = (task_props_t*) pvParameters;

	/* Initialize the ADC states */
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
		for (uint8_t bank = 0; bank < input_config.adc_banks; bank++)
		{
			uint8_t offset = bank * input_config.adc_channels; // Offset for the current ADC bank

			for (uint8_t chan = 0; chan < input_config.adc_channels; chan++)
			{
				// Select the ADC to read from
				adc_mux_select(bank, chan, true);
				adc_select_input(bank);

				// Calculate channel
				uint8_t channel = offset + chan;

				// Settle the column
				vTaskDelay(pdMS_TO_TICKS(input_config.adc_settling_time_ms));

				uint16_t adc_raw = adc_read();
				uint16_t filtered_value =
					adc_moving_average(channel, adc_raw, adc_states.adc_sample_value[channel], &adc_states);

				if (adc_states.adc_previous_value[channel] != filtered_value)
				{
					adc_generate_event(channel, filtered_value);
					adc_states.adc_previous_value[channel] = filtered_value;
				}
			}

			// Deselect the CS pin of the ADC mux
			adc_mux_select(bank, 0, false);
		}

		task_props->high_watermark = (uint8_t)uxTaskGetStackHighWaterMark(NULL);
		watchdog_update();
	}
}

/** @brief Select the ADC input.
 *
 * @param bank Level to set the adc mux CS pin
 * @param channel Channel to select
 * @param select Select which ADC bank to use
 */
void adc_mux_select(bool bank, uint8_t channel, bool select)
{
	if (bank)
	{
		gpio_put(ADC0_MUX_CS, !select);
	}
	else
	{
		gpio_put(ADC1_MUX_CS, !select);
	}

	gpio_put(ADC_MUX_A, (channel & 0x01U) != 0U);
	gpio_put(ADC_MUX_B, (channel & 0x02U) != 0U);
	gpio_put(ADC_MUX_C, (channel & 0x04U) != 0U);
}

/** @brief Read the encoder value.
 *
 * @param pvParameters Parameters passed to the task
 */
void encoder_read_task(void *pvParameters)
{
	const int8_t encoder_states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
	encoder_states_t encoder_state[MAX_NUM_ENCODERS];

	/* cppcheck-suppress[misra-c2012-11.5,cstyleCast] ; Required by FreeRTOS DEVIATION(D3) */
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
				}                                                  // then the index for enc_states
				else if (encoder_state[0].count_encoder == -4)
				{
					encoder_generate_event(encoder_base, 0);
				}

				keypad_cs_columns(false);
			}

			keypad_cs_rows(false);
		}

		/* Get free heap for the task */
		task_prop->high_watermark = (uint8_t)uxTaskGetStackHighWaterMark(NULL);
		/* Update watchdog timer */
		watchdog_update();
	}
}

/** @brief Generate an encoder event.
 *
 * @param rotary Rotary number
 * @param direction Direction
 */
void encoder_generate_event(uint8_t rotary, uint16_t direction)
{
	if (NULL != input_config.input_event_queue)
	{
		data_events_t encoder_event;

		/* Initialize encoder_event data */
		encoder_event.data[0] = 0;
		encoder_event.data[1] = 0;

		encoder_event.command = PC_ROTARY_CMD;
		encoder_event.data[0] |= rotary << 4;
		encoder_event.data[1] |= direction;
		encoder_event.data_length = 2;
		xQueueSend(input_config.input_event_queue, &encoder_event, portMAX_DELAY);
	}
}

/** @brief Set the encoder mask.
 *
 * @param mask Mask to enable/disable encoders
 */
static void encoder_set_mask(uint8_t mask)
{
	for (uint8_t i = 0; i < MAX_NUM_ENCODERS; i++)
	{
		input_config.encoder_mask[i] = mask & (1U << i);
	}
}
