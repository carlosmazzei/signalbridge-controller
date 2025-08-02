#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

/**
 * @defgroup KEYPAD Keypad definitions
 * @{
 */
/** @def KEYPAD_ROWS
 *  @brief Number of keypad rows. */
#define KEYPAD_ROWS 8U
/** @def KEYPAD_COLUMNS
 * @brief Number of keypad columns. */
#define KEYPAD_COLUMNS 8U
/** @def KEYPAD_MAX_COLS
 * @brief Maximum number of keypad columns. */
#define KEYPAD_MAX_COLS 8U
/** @def KEYPAD_MAX_ROWS
 * @brief Maximum number of keypad rows. */
#define KEYPAD_MAX_ROWS 8U
/** * @def KEYPAD_STABILITY_BITS
 * @brief Number of stability bits for debounce. */
#define KEYPAD_STABILITY_BITS 3
/** @def KEYPAD_STABILITY_MASK
 * @brief Bitmask for keypad stability. */
#define KEYPAD_STABILITY_MASK ((1U << KEYPAD_STABILITY_BITS) - 1U)
/** @def KEYPAD_COL_MUX_A
 * @brief GPIO pin for keypad column multiplexer A. */
#define KEYPAD_COL_MUX_A 0U
/** @def KEYPAD_COL_MUX_B
 * @brief GPIO pin for keypad column multiplexer B. */
#define KEYPAD_COL_MUX_B 1UL
/** @def KEYPAD_COL_MUX_C
 * @brief GPIO pin for keypad column multiplexer C. */
#define KEYPAD_COL_MUX_C 2UL
/** @def KEYPAD_COL_MUX_CS
 * @brief GPIO pin for keypad column multiplexer chip select. */
#define KEYPAD_COL_MUX_CS 17UL
/** @def KEYPAD_ROW_INPUT
 * @brief GPIO pin for keypad row input. */
#define KEYPAD_ROW_INPUT 9U
/** @def KEYPAD_ROW_MUX_A
 * @brief GPIO pin for keypad row multiplexer A. */
#define KEYPAD_ROW_MUX_A 6U
/** @def KEYPAD_ROW_MUX_B
 * @brief GPIO pin for keypad row multiplexer B. */
#define KEYPAD_ROW_MUX_B 7U
/** @def KEYPAD_ROW_MUX_C
 * @brief GPIO pin for keypad row multiplexer C. */
#define KEYPAD_ROW_MUX_C 3U
/** @def KEYPAD_ROW_MUX_CS
 * @brief GPIO pin for keypad row multiplexer chip select. */
#define KEYPAD_ROW_MUX_CS 8U
/** @def KEY_PRESSED_MASK
 * @brief Bitmask for key pressed state (two consecutives actives). */
#define KEY_PRESSED_MASK 0x03U
/** @def KEY_RELEASED_MASK
 * @brief Bitmask for key released state (two consecutives inactives). */
#define KEY_RELEASED_MASK 0x04U
/** @def  KEY_PRESSED
 * @brief Key pressed state. */
#define KEY_PRESSED 1U
/** @def KEY_RELEASED
 * @brief Key released state. */
#define KEY_RELEASED 0U
/** @} */

/**
 * @defgroup ADC ADC definitions
 * @{
 */
/** @def ADC_MUX_A
 * @brief GPIO pin for ADC multiplexer A. */
#define ADC_MUX_A 20U
/** @def ADC_MUX_B
 * @brief GPIO pin for ADC multiplexer B. */
#define ADC_MUX_B 21U
/** @def ADC_MUX_C
 * @brief GPIO pin for ADC multiplexer C. */
#define ADC_MUX_C 22U
/** @def ADC_MUX_D
 * @brief GPIO pin for ADC multiplexer D. */
#define ADC_MUX_D 11U
/** @def ADC_CHANNELS
 * @brief Maximum number of ADC channels. */
#define ADC_CHANNELS 16
/** @def ADC_NUM_TAPS
 * @brief Number of taps for moving average filter.
 * This defines how many samples are used for filtering ADC values.
 */
#define ADC_NUM_TAPS 4
/** @} */

/**
 * @defgroup ENCODER Encoder definitions
 * @{
 */
/** @def MAX_NUM_ENCODERS
 * @brief Maximum number of rotary encoders supported.
 */
#define MAX_NUM_ENCODERS 8U
/** @} */

/**
 * @brief Input configuration structure (keypad, ADC, encoder).
 */
typedef struct input_config_t
{
	uint8_t rows;                /**< Number of keypad rows */
	uint8_t columns;             /**< Number of keypad columns */
	uint16_t key_settling_time_ms; /**< Key settling time in milliseconds */
	uint8_t adc_channels;        /**< ADC channels per bank */
	uint16_t adc_settling_time_ms; /**< ADC settling time in milliseconds */
	QueueHandle_t input_event_queue; /**< Queue for sending input events */
	bool encoder_mask[MAX_NUM_ENCODERS]; /**< Mask to enable/disable encoders */
	uint16_t encoder_settling_time_ms; /**< Encoder settling time in milliseconds */
} input_config_t;

/**
 * @brief Global input configuration instance.
 */
extern input_config_t input_config;

/**
 * @brief Possible results for input functions.
 */
typedef enum input_result_t {
	INPUT_OK = 0,        /**< Operation successful */
	INPUT_ERROR = 1,     /**< Operation failed */
	INPUT_INVALID_CONFIG = 2,/**< Invalid configuration */
	INPUT_QUEUE_FULL = 3 /**< Input event queue is full */
} input_result_t;

/**
 * @brief Structure to hold ADC states (filters, samples, indices).
 */
typedef struct adc_states_t
{
	uint16_t adc_previous_value[ADC_CHANNELS];     /**< Last filtered value for each channel */
	uint32_t adc_sum_values[ADC_CHANNELS];         /**< Sum of values for moving average */
	uint16_t adc_sample_value[ADC_CHANNELS][ADC_NUM_TAPS]; /**< Recent samples for each channel */
	uint16_t samples_index[ADC_CHANNELS];          /**< Circular index for samples */
} adc_states_t;

/**
 * @brief Structure to hold rotary encoder states.
 */
typedef struct encoder_states_t
{
	uint8_t old_encoder; /**< Previous encoder state (for transition detection) */
	int8_t count_encoder; /**< Encoder step counter */
} encoder_states_t;

/**
 * @brief Current state of each key in the keypad matrix.
 *
 * The size is KEYPAD_MAX_COLS * KEYPAD_MAX_ROWS.
 */
extern uint8_t keypad_state[KEYPAD_MAX_COLS * KEYPAD_MAX_ROWS];

/**
 * @brief Initialize the input system (keypad, ADC, encoder).
 *
 * @param[in] config Pointer to the input configuration structure.
 * @return INPUT_OK if initialized successfully, INPUT_INVALID_CONFIG if parameters are invalid.
 */
input_result_t input_init(const input_config_t *config);

/**
 * @brief Keypad task to update and populate key events
 * @param[in,out] pvParameters Pointer to the keypad task parameters.
 */
void keypad_task(void *pvParameters);

/**
 * @brief Set the rows of the keypad.
 * @param[in] rows Rows to set.
 */
void keypad_set_rows(uint8_t rows);

/**
 * @brief Set the columns of the keypad.
 * @param[in] columns Columns to set.
 */
void keypad_set_columns(uint8_t columns);

/**
 * @brief Generate and send a key event to the input event queue.
 * @param[in] row Key row.
 * @param[in] column Key column.
 * @param[in] state Key state (KEY_PRESSED or KEY_RELEASED).
 * @details The event is only sent if the input event queue is not NULL.
 */
void keypad_generate_event(uint8_t row, uint8_t column, uint8_t state);

/**
 * @brief FreeRTOS task for reading ADC channels, filtering, and generating events.
 *
 * @param[in] pvParameters Pointer to task parameters (task_props_t*).
 */
void adc_read_task(void *pvParameters);

/**
 * @brief Select the ADC input.
 * @param[in] channel Channel to select
 */
void adc_mux_select(uint8_t channel);

/**
 * @brief FreeRTOS task for reading rotary encoders and generating events.
 *
 * @param[in] pvParameters Pointer to task parameters (task_props_t*).
 */
void encoder_read_task(void *pvParameters);

/**
 * @brief Generate and send a rotary encoder event to the event queue.
 *
 * @param[in] rotary Encoder number.
 * @param[in] dir Rotation direction (1 for clockwise, 0 for counterclockwise).
 */
void encoder_generate_event(uint8_t rotary, uint16_t dir);

#endif