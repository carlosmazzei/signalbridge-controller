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
 * Keypad definitions
 */
#define KEYPAD_ROWS 8U
#define KEYPAD_COLUMNS 8U

#define KEYPAD_MAX_COLS 8U
#define KEYPAD_MAX_ROWS 8U

#define KEYPAD_STABILITY_BITS 3
#define KEYPAD_STABILITY_MASK ((1U << KEYPAD_STABILITY_BITS) - 1U)

#define KEYPAD_COL_MUX_A 8U
#define KEYPAD_COL_MUX_B 9UL
#define KEYPAD_COL_MUX_C 10UL
#define KEYPAD_COL_MUX_CS 11UL

#define KEYPAD_ROW_INPUT 0U
#define KEYPAD_ROW_MUX_A 1U
#define KEYPAD_ROW_MUX_B 2U
#define KEYPAD_ROW_MUX_C 3U
#define KEYPAD_ROW_MUX_CS 6U

#define KEY_PRESSED_MASK 0x03U  // 011: two consecutive actives
#define KEY_RELEASED_MASK 0x04U // 100: two consecutive inactives
#define KEY_PRESSED 1U
#define KEY_RELEASED 0U

/**
 * ADC definitions
 */

#define ADC0_MUX_CS 7U
#define ADC1_MUX_CS 21U
#define ADC_MUX_A 14U
#define ADC_MUX_B 15U
#define ADC_MUX_C 22U

#define ADC_CHANNELS 16
#define ADC_NUM_TAPS 4

/**
 * Encoder definitions
 */

#define MAX_NUM_ENCODERS 8U

/**
 * Structure to hold the inputs configuration.
 */
typedef struct input_config_t
{
	uint8_t rows;                    // Number of rows
	uint8_t columns;                 // Number of columns
	uint16_t key_settling_time_ms;   // Time to wait for key to settle
	uint8_t adc_banks;               // Number of banks of mux connected to the ADCs
	uint8_t adc_channels;            // ADC Channels per bank
	uint16_t adc_settling_time_ms;   // Time to wait for ADC to settle
	QueueHandle_t input_event_queue; // Queue to send input events to
	bool encoder_mask[MAX_NUM_ENCODERS]; // Encoder mask to enable/disable
	uint16_t encoder_settling_time_ms; // Time to wait for encoder to settle
} input_config_t;

extern input_config_t input_config;

typedef enum input_result_t {
	INPUT_OK = 0,      // Input operation successful
	INPUT_ERROR = 1,       // Input operation failed
	INPUT_INVALID_CONFIG = 2, // Invalid configuration provided
	INPUT_QUEUE_FULL = 3,  // Input event queue is full
} input_result_t;

/**
 * Structure to hold the ADC states.
 */
typedef struct adc_states_t
{
	uint16_t adc_previous_value[ADC_CHANNELS];         // 16 x 16 = 256
	uint32_t adc_sum_values[ADC_CHANNELS];             // 16 x 16 = 256
	uint16_t adc_sample_value[ADC_CHANNELS][ADC_NUM_TAPS]; // 16 x 16 x 4 = 1024
	uint16_t samples_index[ADC_CHANNELS];              // 16 x 16 = 256
} adc_states_t;

/**
 * Structure to hold the encoder states.
 */
typedef struct encoder_states_t
{
	uint8_t old_encoder;
	int8_t count_encoder;
} encoder_states_t;

extern uint8_t keypad_state[KEYPAD_MAX_COLS * KEYPAD_MAX_ROWS]; // State of the keypad

/**
 *  Function prototypes
 */

/** @brief Initialize the keypad.
 *
 * @param config Pointer to the keypad configuration.
 * @return true if the keypad was successfully initialized.
 */
input_result_t input_init(const input_config_t *config);

void keypad_task(void *pvParameters);
void keypad_set_columns(uint8_t columns);
void keypad_set_rows(uint8_t rows);
static inline void keypad_cs_rows(bool select);
static inline void keypad_cs_columns(bool select);
void keypad_generate_event(uint8_t row, uint8_t column, uint8_t state);
void adc_read_task(void *pvParameters);
void adc_mux_select(bool bank, uint8_t channel, bool select);


void encoder_read_task(void *pvParameters);
void encoder_generate_event(uint8_t rotary, uint16_t dir);

#endif