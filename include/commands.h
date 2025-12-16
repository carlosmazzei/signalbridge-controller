/**
 * @file commands.h
 * @brief Command identifiers exchanged between the host and the controller.
 */

#ifndef COMMAND_LIB_DEFINES
#define COMMAND_LIB_DEFINES

/**
 * @enum pc_commands_t
 * @brief Identifiers for commands handled by the breakout board firmware.
 *
 * The command space is limited to 5-bit values (0x01-0x1F). Values below 16
 * correspond to regular I/O operations while values starting at 16 are
 * reserved for diagnostics and housekeeping tasks.
 */
typedef enum pc_commands_t {
	PC_PWM_CMD = 1,           /**< PWM output update */
	PC_LEDOUT_CMD,            /**< LED matrix control */
	PC_AD_CMD,                /**< Analog-to-digital conversion report */
	PC_KEY_CMD,               /**< Keypad event */
	PC_DISPLAY_CMD,           /**< Seven-segment display update */
	PC_ROTARY_CMD,            /**< Rotary encoder event */
	PC_TRIM_CMD,              /**< Trim wheel event */
	PC_OPTO_CMD,              /**< Opto-coupler input event */
	PC_RELE_CMD,              /**< Relay command */
	PC_DPYCTL_CMD,            /**< Display control command */
	PC_TCAS_CMD,              /**< TCAS indicator update */
	PC_FCU_CMD,               /**< Flight Control Unit update */
	PC_SETVALUE_CMD,          /**< Generic set-value command */

	// System commands
	PC_DEBUG_CMD = 16,        /**< Debug data */
	PC_DEBUG_CTL1_CMD,        /**< Debug control channel 1 */
	PC_DEBUG_CTL2_CMD,        /**< Debug control channel 2 */
	PC_DEBUG_CTL3_CMD,        /**< Debug control channel 3 */
	PC_ECHO_CMD,              /**< Echo request */
	PC_IDTABLE_CMD,           /**< Identification table */
	PC_IO_ERROR_STATUS_CMD,   /**< I/O error summary */
	PC_ERROR_STATUS_CMD,      /**< Error status query */
	PC_TASK_STATUS_CMD,       /**< FreeRTOS task status request */
	PC_USBSTATUS_CMD,         /**< USB link status request */
	PC_ID_CONFIRM_NODE,       /**< Node confirmation command */
	PC_ID_CONFIRM,            /**< Confirmation response */
	PC_ID_REQUEST,            /**< Identification request */
	PC_CONFIG_CMD,            /**< Configuration update */
	PC_ENUMERATE_CMD          /**< Enumeration trigger */
} pc_commands_t;

#endif // COMMAND_LIB_DEFINES
