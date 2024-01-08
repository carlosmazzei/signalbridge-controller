/*
 * File: stdA320.h
 * Author: Cadu Mazzei
 * Version: 1.0 XC8 Compiler
*/
#ifndef __A320STD_LIB_DEFINES__
#define __A320STD_LIB_DEFINES__

/*
 * Panel definitions 
 */

// Panel ID TABLE
#define MASTER_ID			1
#define MCDU1_ID			2
#define MCDU2_ID			3
#define ECAM_ID				4
#define RMP1_ID				5
#define RMP2_ID				6		
#define RMP3_ID				7
#define AUDIO1_ID			8
#define AUDIO2_ID			9
#define AUDIO3_ID			10
#define TCAS_ID             11
#define WRX_ID				12
#define THROTTLE_ID			13	
#define RUDDER_ID			14	
#define PFDNDBRIGHT_ID      15
#define AP_ID				16
#define MIP_ID				17
#define BARO1_ID			18
#define BARO2_ID			19
#define OVHL1_ID			20
#define OVHL2_ID			21
#define OVHL3_ID			22
#define OVHL4_ID			23
#define ADIRS_ID            24
#define SIDESTICK_PEDALS_ID 25

// ID address
#define PANEL_ID_LOW               1
#define PANEL_ID_HIGH              0

// Functional commands (Input/Output)
#define PC_PWM_CMD              1
#define PC_LEDOUT_CMD           2
#define PC_AD_CMD               3
#define PC_KEY_CMD              4
#define PC_DISPLAY_CMD          5
#define PC_ROTARY_CMD           6
#define PC_TRIM_CMD             7
#define PC_OPTO_CMD             8
#define PC_RELE_CMD             9
#define PC_DPYCTL_CMD           10
#define PC_TCAS_CMD             11
#define PC_FCU_CMD              12
#define PC_SETVALUE_CMD         13 //USED WITH IDENTIFIER AND VALUE TO BE SET

//System commands
//#define ERROR_STREAM			0x55
#define PC_DEBUG_CMD            16
#define PC_DEBUG_CTL1_CMD       17
#define PC_DEBUG_CTL2_CMD       18
#define PC_DEBUG_CTL3_CMD       19
#define PC_ECHO_CMD             20
#define PC_IDTABLE_CMD          21
#define PC_RESET_CMD            22
#define PC_STATUS_CMD           23
#define PC_HEAP_STATUS_CMD      24
#define PC_USBSTATUS_CMD        25
#define PC_ID_CONFIRM_NODE      26
#define PC_ID_CONFIRM           27
#define PC_ID_REQUEST           28
#define PC_CONFIG_CMD           29
#define PC_ENUMERATE_CMD        30
// MAXIMUM HEX COMMAND = DECIMAL 30 (1F)

// DEVICE_TYPE Definitions
#define MASTER              1
#define MCDU_PANEL          2
#define ECAM_PANEL          3
#define AUDIO_PANEL 		4
#define RMP_PANEL           5
#define TCAS_PANEL          6
#define RUDDER_PANEL		7
#define THROTTLE_PANEL      8
#define AP_PANEL            9
#define MIP_PANEL           10
#define OHL_PANEL           11
#define BARO_PANEL          12

#define ADDRESS_BYTE		255

#endif