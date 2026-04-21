/**
 * @file app_inputs.h
 * @brief Input subsystem configuration and task interfaces.
 *
 * The module multiplexes a large keypad matrix, a bank of ADC channels and a
 * configurable number of rotary encoders.  Scan results are converted into
 * @ref data_events_t payloads that are queued for transmission to the host.
 */

#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include <hardware/adc.h>
#include <hardware/gpio.h>
#include <pico/stdlib.h>
#include "queue.h"
#include "task.h"

/**
 * @defgroup input_subsystem Input subsystem
 * @brief Configuration values and task entry points for input scanning.
 *
 * The definitions in this group describe the multiplexed keypad, ADC and
 * encoder front-ends that feed the firmware.  They are shared by
 * @ref app_inputs.c and consumers that need to reason about the generated
 * @ref data_events_t payloads.
 * @{
 */

/**
 * @name Keypad matrix configuration
 * @{
 */
/** Number of rows available in the keypad matrix. */
#define KEYPAD_ROWS 8U
/** Number of columns available in the keypad matrix. */
#define KEYPAD_COLUMNS 8U
/** Upper bound for the keypad column count accepted by @ref input_init(). */
#define KEYPAD_MAX_COLS 8U
/** Upper bound for the keypad row count accepted by @ref input_init(). */
#define KEYPAD_MAX_ROWS 8U
/** Number of consecutive samples used to determine a stable key state. */
#define KEYPAD_STABILITY_BITS 3U
/** Bit-mask composed from @ref KEYPAD_STABILITY_BITS used for debouncing. */
#define KEYPAD_STABILITY_MASK ((1U << KEYPAD_STABILITY_BITS) - 1U)
/** GPIO identifier for the keypad column multiplexer bit 0. */
#define KEYPAD_COL_MUX_A 0U
/** GPIO identifier for the keypad column multiplexer bit 1. */
#define KEYPAD_COL_MUX_B 1U
/** GPIO identifier for the keypad column multiplexer bit 2. */
#define KEYPAD_COL_MUX_C 2U
/** Chip-select GPIO for the keypad column multiplexer (active low). */
#define KEYPAD_COL_MUX_CS 17U
/** GPIO identifier for the keypad row input used for sampling. */
#define KEYPAD_ROW_INPUT 9U
/** GPIO identifier for the keypad row multiplexer bit 0. */
#define KEYPAD_ROW_MUX_A 6U
/** GPIO identifier for the keypad row multiplexer bit 1. */
#define KEYPAD_ROW_MUX_B 7U
/** GPIO identifier for the keypad row multiplexer bit 2. */
#define KEYPAD_ROW_MUX_C 3U
/** Chip-select GPIO for the keypad row multiplexer (active low). */
#define KEYPAD_ROW_MUX_CS 8U
/** Debounce mask indicating a stable pressed state. */
#define KEY_PRESSED_MASK 0x03U
/** Debounce mask indicating a stable released state. */
#define KEY_RELEASED_MASK 0x04U
/** Encoded state value for a pressed key. */
#define KEY_PRESSED 1U
/** Encoded state value for a released key. */
#define KEY_RELEASED 0U
/** @} */

/**
 * @name ADC multiplexer configuration
 * @{
 */
/** GPIO identifier for ADC multiplexer bit 0. */
#define ADC_MUX_A 20U
/** GPIO identifier for ADC multiplexer bit 1. */
#define ADC_MUX_B 21U
/** GPIO identifier for ADC multiplexer bit 2. */
#define ADC_MUX_C 22U
/** GPIO identifier for ADC multiplexer bit 3. */
#define ADC_MUX_D 11U
/** Number of ADC channels provided by the hardware multiplexer. */
#define ADC_CHANNELS 16U
/** Length of the moving-average filter applied to ADC samples. */
#define ADC_NUM_TAPS 8U
/** Default µs settling delay between channel selections (74HC4067 + ~10 kΩ pot). */
#define ADC_DEFAULT_SETTLING_US 500U
/** Default number of raw samples averaged per channel per scan. */
#define ADC_DEFAULT_OVERSAMPLE 4U
/** Default hysteresis (in raw 12-bit LSBs) applied before emitting a new event. */
#define ADC_DEFAULT_HYSTERESIS 8U
/** Default cooperative yield (ms) inserted between full ADC scans. */
#define ADC_DEFAULT_SCAN_INTERVAL_MS 1U
/** @} */

/**
 * @name Encoder configuration
 * @{
 */
/** Maximum number of rotary encoders supported by the input task. */
#define MAX_NUM_ENCODERS 8U
/** @} */

/**
 * @brief Per-encoder position mapping within the keypad matrix.
 *
 * Each encoder occupies two adjacent columns (channel A at @c col,
 * channel B at @c col + 1) on the specified @c row.  Mapped positions
 * are excluded from keypad scanning automatically.
 */
typedef struct encoder_map_t
{
	uint8_t row;     /**< Row index where this encoder is wired. */
	uint8_t col;     /**< Base column index (channel A); channel B is col + 1. */
	bool enabled;    /**< Whether this encoder mapping is active. */
} encoder_map_t;

/**
 * @brief Run-time configuration for the input subsystem.
 */
typedef struct input_config_t
{
	uint8_t rows;                             /**< Number of keypad rows to scan. */
	uint8_t columns;                          /**< Number of keypad columns to scan. */
	uint16_t key_settling_time_ms;            /**< Scan-cycle interval (ms); debounce window = key_settling_time_ms * required samples. */
	uint8_t adc_channels;                     /**< Number of ADC channels populated on the board (lower the count to scan only the active axes). */
	uint16_t adc_settling_us;                 /**< Delay (µs) between ADC mux selection and the first sample. */
	uint8_t adc_oversample;                   /**< Raw samples averaged per channel per scan (>= 1). */
	uint16_t adc_hysteresis;                  /**< Minimum LSB delta before emitting a new event (0 = always emit on change). */
	uint16_t adc_scan_interval_ms;            /**< Cooperative yield between full ADC scans (>= 1 ms). */
	QueueHandle_t input_event_queue;          /**< Destination queue for generated events. */
	encoder_map_t encoder_map[MAX_NUM_ENCODERS]; /**< Per-encoder position mappings. */
	uint8_t num_encoders;                     /**< Number of configured encoder entries. */
	uint16_t col_mux_settling_us;             /**< 74HC138 column decoder propagation settling delay (µs). */
	uint16_t row_mux_settling_us;             /**< 74HC4051 row MUX enable settling delay (µs). */
} input_config_t;

/**
 * @brief Result codes produced by the input subsystem API.
 */
typedef enum input_result_t {
	INPUT_OK = 0,             /**< Operation completed successfully. */
	INPUT_ERROR = 1,          /**< Unspecified error while processing. */
	INPUT_INVALID_CONFIG = 2, /**< One or more configuration parameters are invalid. */
	INPUT_QUEUE_FULL = 3      /**< Event queue did not accept new data. */
} input_result_t;

/**
 * @brief Bookkeeping data for ADC channels.
 */
typedef struct adc_states_t
{
	uint16_t adc_previous_value[ADC_CHANNELS];            /**< Last filtered reading per channel. */
	uint32_t adc_sum_values[ADC_CHANNELS];                /**< Accumulator used by the moving average filter. */
	uint16_t adc_sample_value[ADC_CHANNELS][ADC_NUM_TAPS];/**< Circular buffer with recent samples. */
	uint16_t samples_index[ADC_CHANNELS];                 /**< Cursor into @ref adc_sample_value. */
} adc_states_t;

/**
 * @brief Quadrature decoder state for each rotary encoder.
 */
typedef struct encoder_states_t
{
	uint8_t old_encoder; /**< Last sampled quadrature state. */
	int8_t count_encoder;/**< Accumulated detent count pending reporting. */
} encoder_states_t;

/** @} */

/**
 * @brief Configure GPIO, ADC and encoder metadata for subsequent input tasks.
 *
 * The function validates the provided configuration and primes the internal
 * state machines.  On success, the calling code can start the keypad and ADC
 * tasks which will make use of the cached settings.  Rotary encoder sampling
 * is performed inline by @ref keypad_task so there is no separate encoder task.
 *
 * @retval INPUT_OK             Configuration accepted and hardware initialised.
 * @retval INPUT_INVALID_CONFIG One or more parameters are outside the
 *                              supported range.
 */
input_result_t input_init(void);

/**
 * @brief FreeRTOS task that scans the keypad matrix and generates key events.
 *
 * @param[in,out] pvParameters Pointer to the owning @ref task_props_t instance.
 */
void keypad_task(void *pvParameters);

/**
 * @brief FreeRTOS task that samples the ADC multiplexer and reports changes.
 *
 * @param[in,out] pvParameters Pointer to the owning @ref task_props_t instance.
 */
void adc_read_task(void *pvParameters);

/**
 * @brief Query whether a keypad matrix position is mapped to an encoder.
 *
 * @param[in] row Row index to check.
 * @param[in] col Column index to check.
 *
 * @retval true  The position is part of an encoder pair and is excluded
 *               from keypad scanning.
 * @retval false The position is a regular key.
 */
bool input_is_encoder_position(uint8_t row, uint8_t col);

/**
 * @brief Decide whether a new ADC reading is significant enough to emit.
 *
 * Compares @p current against the last reported @p previous using a symmetric
 * hysteresis window.  A value of @p hysteresis equal to zero falls back to
 * legacy "report every change" behavior.
 *
 * @param[in] previous   Last reported filtered value for the channel.
 * @param[in] current    Current filtered value to evaluate.
 * @param[in] hysteresis Minimum absolute LSB delta required to emit.
 *
 * @retval true  The delta meets or exceeds the hysteresis threshold.
 * @retval false The reading is within the deadband and should be suppressed.
 */
bool adc_should_emit(uint16_t previous, uint16_t current, uint16_t hysteresis);

#endif // KEYPAD_H
