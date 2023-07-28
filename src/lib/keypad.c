#include "keypad.h"
#include "data_event.h"

keypad_config_t keypad_config;

uint8_t keypad_state[KEYPAD_ROWS * KEYPAD_COLUMNS];

/** @brief Initialize the keypad.
 *
 * @param config Pointer to the keypad configuration.
 * @return true if the keypad was successfully initialized.
 */
bool keypad_init(keypad_config_t *config)
{
    if (config->columns > KEYPAD_MAX_COLS)
        return false;
    if (config->rows > KEYPAD_MAX_ROWS)
        return false;

    keypad_config.rows = config->rows;
    keypad_config.columns = config->columns;

    memset(keypad_state, 0, sizeof(keypad_state));
    keypad_config.settling_time_ms = config->settling_time_ms;
    keypad_config.keypad_event_queue = config->keypad_event_queue;

    // Setup IO pins / use gpio_init_mask insted to initialized multiple pins
    gpio_init_mask(
        (1 << KEYPAD_COL_MUX_A) |
        (1 << KEYPAD_COL_MUX_B) |
        (1 << KEYPAD_COL_MUX_C) |
        (1 << KEYPAD_ROW_MUX_A) |
        (1 << KEYPAD_ROW_MUX_B) |
        (1 << KEYPAD_ROW_MUX_C));
    gpio_set_dir_masked(
        (1 << KEYPAD_COL_MUX_A) |
            (1 << KEYPAD_COL_MUX_B) |
            (1 << KEYPAD_COL_MUX_C) |
            (1 << KEYPAD_ROW_MUX_A) |
            (1 << KEYPAD_ROW_MUX_B) |
            (1 << KEYPAD_ROW_MUX_C),
        0xFF);

    gpio_init(KEYPAD_ROW_INPUT);
    gpio_set_dir(KEYPAD_ROW_INPUT, 0);

    return true;
}

/** @brief Keypad task to update and populate key events
 *
 * @param pvParameters Pointer to the keypad task parameters.
 *
 */
void keypad_task(void *pvParameters)
{

    while (true)
    {
        for (uint8_t c = 0; c < keypad_config.columns; c++)
        {
            keypad_set_columns(c);
            uint8_t keycode_base = c * keypad_config.rows;
            vTaskDelay(pdMS_TO_TICKS(keypad_config.settling_time_ms)); // Settle the column

            for (uint8_t r = 0; r < keypad_config.rows; r++)
            {
                keypad_set_rows(r);
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
            }

            // TO-DO: Update with the corresponding column pin.
            // Set pin to high impedance input. Effectively ends column pulse.
            gpio_put(10, 1);
        }
    }

    vTaskDelete(NULL);
}

void keypad_set_columns(uint8_t columns)
{
    gpio_put(KEYPAD_COL_MUX_A, columns & 0x01);
    gpio_put(KEYPAD_COL_MUX_B, columns & 0x02);
    gpio_put(KEYPAD_COL_MUX_C, columns & 0x04);
}

void keypad_set_rows(uint8_t rows)
{
    gpio_put(KEYPAD_ROW_MUX_A, rows & 0x01);
    gpio_put(KEYPAD_ROW_MUX_B, rows & 0x02);
    gpio_put(KEYPAD_ROW_MUX_C, rows & 0x04);
}

void keypad_generate_event(uint8_t command, uint8_t row, uint8_t column, uint8_t state)
{
    data_events_t key_event;
    key_event.command = PC_KEY_CMD;
    key_event.data = ((column << 4) | (row << 1)) & 0xFE; // Add key state to the event.
    key_event.data |= state;
    xQueueSend(keypad_config.keypad_event_queue, &key_event, pdMS_TO_TICKS(keypad_config.settling_time_ms));
}
