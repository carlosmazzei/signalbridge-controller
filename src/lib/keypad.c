#include "keypad.h"
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

    // Initiate keypad task

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
                    keypad_generate_event(PC_KEY_CMD, r, c, KEY_PRESSED);
                }
                else if ((keypad_state[keycode] & KEYPAD_STABILITY_MASK) == KEY_RELEASED_MASK)
                {
                    keypad_generate_event(PC_KEY_CMD, r, c, KEY_RELEASED);
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
 * @param command Command to send to the event queue
 * @param Row number
 * @param Column number
 * @param State of the key
 *
 */
void keypad_generate_event(uint8_t command, uint8_t row, uint8_t column, uint8_t state)
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
    uint adc_states[ADC_CHANNELS];

    while (true)
    {
        for (uint8_t bank = 0; bank < input_config.adc_banks; bank++)
        {
            uint16_t offset = bank * ADC_CHANNELS;

            for (uint8_t chan = 0; chan < input_config.adc_channels; chan++)
            {
                // Select the ADC to read from
                adc_mux_select(bank, chan, true);
                adc_select_input(bank);

                // Settle the column
                vTaskDelay(pdMS_TO_TICKS(input_config.adc_settling_time_ms));

                // TO-DO: Filter and generate event
                uint16_t adc_raw = adc_read();
                adc_states[offset + chan] = adc_raw;

                adc_generate_event(PC_AD_CMD, offset + chan, adc_raw);
            }

            // Deselect the CS pin of the ADC mux
            adc_mux_select(bank, 0, false);
        }
    }

    vTaskDelete(NULL); // Delete task if for some reason it gets out of the loop
}

/** @brief Select the ADC input.
 *
 * @param select Level to set the adc mux CS pin
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
 * @param command Command to send to the event queue
 * @param channel Channel read
 * @param value Value read
 */
void adc_generate_event(uint8_t command, uint8_t channel, uint16_t value)
{
    if (input_config.input_event_queue == NULL)
        return;

    uint16_t payload = (channel << 12) & 0xF000;
    payload |= (value & 0x0FFF);

    data_events_t adc_event;
    adc_event.command = PC_KEY_CMD;
    adc_event.data[0] = (payload >> 8) & 0xFF;
    adc_event.data[1] = payload & 0xFF;
    adc_event.data_length = 2;
    xQueueSend(input_config.input_event_queue, &adc_event, portMAX_DELAY);
}
