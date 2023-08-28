#include "inputs.h"
#include "data_event.h"

/**
 * Keypad configuration.
 */
input_config_t input_config;

/**
 * States of each row
 */
uint8_t keypad_state[KEYPAD_ROWS * KEYPAD_COLUMNS];

/** @brief Initialize the keypad.
 *
 * @param config Pointer to the keypad configuration.
 * @return true if the keypad was successfully initialized.
 */
bool input_init(const input_config_t *config)
{
    if (config->columns > KEYPAD_MAX_COLS)
        return false;
    if (config->rows > KEYPAD_MAX_ROWS)
        return false;

    // Initialize ADC configuration
    input_config.adc_banks = config->adc_banks;
    input_config.adc_channels = config->adc_channels;
    input_config.adc_settling_time_ms = config->adc_settling_time_ms;

    // Initialize keypad configuration
    input_config.rows = config->rows;
    input_config.columns = config->columns;
    memset(keypad_state, 0, sizeof(keypad_state));
    input_config.key_settling_time_ms = config->key_settling_time_ms;
    input_config.input_event_queue = config->input_event_queue;

    // Setup IO pins / use gpio_init_mask insted to initialized multiple pins
    gpio_init_mask(
        (1 << KEYPAD_COL_MUX_A) |
        (1 << KEYPAD_COL_MUX_B) |
        (1 << KEYPAD_COL_MUX_C) |
        (1 << KEYPAD_COL_MUX_CS) |
        (1 << KEYPAD_ROW_MUX_A) |
        (1 << KEYPAD_ROW_MUX_B) |
        (1 << KEYPAD_ROW_MUX_C) |
        (1 << KEYPAD_ROW_MUX_CS) |
        (1 << ADC0_MUX_CS) |
        (1 << ADC1_MUX_CS) |
        (1 << ADC_MUX_A) |
        (1 << ADC_MUX_B) |
        (1 << ADC_MUX_C));
    gpio_set_dir_masked(
        (1 << KEYPAD_COL_MUX_A) |
            (1 << KEYPAD_COL_MUX_B) |
            (1 << KEYPAD_COL_MUX_C) |
            (1 << KEYPAD_COL_MUX_CS) |
            (1 << KEYPAD_ROW_MUX_A) |
            (1 << KEYPAD_ROW_MUX_B) |
            (1 << KEYPAD_ROW_MUX_C) |
            (1 << KEYPAD_ROW_MUX_CS) |
            (1 << ADC0_MUX_CS) |
            (1 << ADC1_MUX_CS) |
            (1 << ADC_MUX_A) |
            (1 << ADC_MUX_B) |
            (1 << ADC_MUX_C),
        0xFF);

    gpio_init(KEYPAD_ROW_INPUT);
    gpio_set_dir(KEYPAD_ROW_INPUT, false);

    // ADC init
    adc_init();
    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(26);
    adc_gpio_init(27);

    return true;
}

/** @brief Keypad task to update and populate key events
 *
 * @param pvParameters Pointer to the keypad task parameters.
 *
 */
void keypad_task(void *pvParameters)
{
    bool toggle_adc_mux = false;

    while (true)
    {
        for (uint8_t c = 0; c < input_config.columns; c++)
        {
            // Select the column
            keypad_set_columns(c);
            keypad_cs_cols(true);
            uint8_t keycode_base = c * input_config.rows;

            // Settle the column
            vTaskDelay(pdMS_TO_TICKS(input_config.key_settling_time_ms));

            for (uint8_t r = 0; r < input_config.rows; r++)
            {
                keypad_set_rows(r); // Also set the ADC channels
                keypad_cs_rows(true);

                uint8_t keycode = keycode_base + r;
                bool pressed = !gpio_get(KEYPAD_ROW_INPUT); // Active low pin
                keypad_state[keycode] = ((keypad_state[keycode] << 1) & 0xFE) | pressed;

                if ((keypad_state[keycode] & KEYPAD_STABILITY_MASK) == KEY_PRESSED_MASK)
                {
                    keypad_generate_event(r, c, KEY_PRESSED);
                }
                else if ((keypad_state[keycode] & KEYPAD_STABILITY_MASK) == KEY_RELEASED_MASK)
                {
                    keypad_generate_event(r, c, KEY_RELEASED);
                }
                keypad_cs_rows(false);
            }

            keypad_cs_cols(false);
        }
    }

    vTaskDelete(NULL); // Delete task if for some reason it gets out of the loop
}

/** @brief Set the columns of the keypad.
 *
 */
void keypad_set_columns(uint8_t columns)
{
    gpio_put(KEYPAD_COL_MUX_A, columns & 0x01);
    gpio_put(KEYPAD_COL_MUX_B, columns & 0x02);
    gpio_put(KEYPAD_COL_MUX_C, columns & 0x04);
}

/** @brief Set the rows of the keypad.
 *
 */
void keypad_set_rows(uint8_t rows)
{
    gpio_put(KEYPAD_ROW_MUX_A, rows & 0x01);
    gpio_put(KEYPAD_ROW_MUX_B, rows & 0x02);
    gpio_put(KEYPAD_ROW_MUX_C, rows & 0x04);
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
static inline void keypad_cs_cols(bool select)
{
    gpio_put(KEYPAD_COL_MUX_CS, !select); // Active low pin
}

/** @brief Generate a key event.
 *
 * @param Row number
 * @param Column number
 * @param State of the key
 *
 */
void keypad_generate_event(uint8_t row, uint8_t column, bool state)
{
    if (input_config.input_event_queue == NULL)
        return;

    data_events_t key_event;
    key_event.command = PC_KEY_CMD;
    key_event.data[0] = ((column << 4) | (row << 1)) & 0xFE; // Add key state to the event.
    key_event.data[0] |= state;
    key_event.data_length = 1;
    xQueueSend(input_config.input_event_queue, &key_event, portMAX_DELAY);
}

/** @brief Read the ADC value.
 *
 * @param pvParameters Parameters passed to the task
 */
void adc_read_task(void *pvParameters)
{
    adc_states_t adc_states;

    while (true)
    {
        for (uint8_t bank = 0; bank < input_config.adc_banks; bank++)
        {
            uint16_t offset = bank * input_config.adc_channels; // Offset for the current ADC bank

            for (uint8_t chan = 0; chan < input_config.adc_channels; chan++)
            {
                // Select the ADC to read from
                adc_mux_select(bank, chan, true);
                adc_select_input(bank);

                // Calculate channel
                uint16_t channel = offset + chan;

                // Settle the column
                vTaskDelay(pdMS_TO_TICKS(input_config.adc_settling_time_ms));

                // TO-DO: Filter and generate event
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
    }

    vTaskDelete(NULL); // Delete task if for some reason it gets out of the loop
}

/** @brief Select the ADC input.
 *
 * @param bank Level to set the adc mux CS pin
 * @param channel Channel to select
 * @param mux Select which ADC bank to use
 */
void adc_mux_select(bool bank, uint8_t channel, bool select)
{
    if (bank)
        gpio_put(ADC0_MUX_CS, !select);
    else
        gpio_put(ADC1_MUX_CS, !select);

    gpio_put(ADC_MUX_A, channel & 0x01);
    gpio_put(ADC_MUX_B, channel & 0x02);
    gpio_put(ADC_MUX_C, channel & 0x04);
}

/** @brief Generate an ADC event.
 *
 * @param channel Channel read
 * @param value Value read
 */
void adc_generate_event(uint8_t channel, uint16_t value)
{
    if (input_config.input_event_queue == NULL)
        return;

    data_events_t adc_event;
    adc_event.command = PC_AD_CMD;
    adc_event.data[0] = channel;
    adc_event.data[1] = (value & 0xFF00) >> 8;
    adc_event.data[2] = value & 0x00FF;
    adc_event.data_length = 3;
    xQueueSend(input_config.input_event_queue, &adc_event, portMAX_DELAY);
}

/** @brief Calculate moving average of the last samples of ad
 *
 * @param channel Channel read
 * @param new_sample New sample
 * @param samples Array of samples
 * @param adc_states ADC states
 *
 * @return Moving average
 */
uint16_t adc_moving_average(uint16_t channel, uint16_t new_sample, uint16_t *samples, adc_states_t *adc_states)
{
    // Remove old sample and add the new one to the sum
    adc_states->adc_sum_values[channel] -= samples[adc_states->samples_index[channel]];
    adc_states->adc_sum_values[channel] += new_sample;
    samples[adc_states->samples_index[channel]] = new_sample;

    // Adjust the new index
    adc_states->samples_index[channel]++;
    if (adc_states->samples_index[channel] >= ADC_NUM_TAPS)
        adc_states->samples_index[channel] = 0;

    // Return the moving average
    return adc_states->adc_sum_values[channel] / ADC_NUM_TAPS;
}
