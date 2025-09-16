/*
 * File: stdA320.h
 * Author: Cadu Mazzei
 * Version: 1.0 XC8 Compiler
 */
#ifndef COMMAND_LIB_DEFINES
#define COMMAND_LIB_DEFINES

/*
 * Standard definitions
 */

/**
 * @enum pc_commands_t
 * @brief Enumerates the commands for the breakout board
 * This enum defines the unique identifiers for each command in the board.
 * There is a maximum of 30 commands, starting from 1 to 30 (0x1F).
 */
typedef enum pc_commands_t {
	PC_PWM_CMD = 1,
	PC_LEDOUT_CMD,
	PC_AD_CMD,
	PC_KEY_CMD,
	PC_DISPLAY_CMD,
	PC_ROTARY_CMD,
	PC_TRIM_CMD,
	PC_OPTO_CMD,
	PC_RELE_CMD,
	PC_DPYCTL_CMD,
	PC_TCAS_CMD,
	PC_FCU_CMD,
	PC_SETVALUE_CMD,
	// System commands
	PC_DEBUG_CMD = 16,
	PC_DEBUG_CTL1_CMD,
	PC_DEBUG_CTL2_CMD,
	PC_DEBUG_CTL3_CMD,
	PC_ECHO_CMD,
	PC_IDTABLE_CMD,
	PC_IO_ERROR_STATUS_CMD,
	PC_ERROR_STATUS_CMD,
	PC_TASK_STATUS_CMD,
	PC_USBSTATUS_CMD,
	PC_ID_CONFIRM_NODE,
	PC_ID_CONFIRM,
	PC_ID_REQUEST,
	PC_CONFIG_CMD,
	PC_ENUMERATE_CMD

} pc_commands_t;

#endif