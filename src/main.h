#ifndef _MAIN_H_
#define _MAIN_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "pico/binary_info.h"

#include "cobs.h"
#include "stdA320.h"
#include "led.h"
#include "keypad.h"
#include "data_event.h"

#include "bsp/board.h"
#include "tusb.h"

#define BUF_SIZE (480)
#define RD_BUF_SIZE 240
#define WR_BUF_SIZE 240
#define ENCODED_QUEUE_SIZE 100
#define MAX_ENCODED_BUFFER_SIZE 12 // n/254 + 1 + Packet Marker
#define DATA_BUFFER_SIZE 10
#define DATA_EVENT_QUEUE_SIZE 20

#define PACKET_MARKER 0x00

#define SPI_FREQUENCY 1000 * 1000
#define SPI_BUF_LEN 10

#define mainPROCESS_QUEUE_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )
#define mainCDC_TASK_PRIORITY		        ( tskIDLE_PRIORITY + 2 )

/* 
 * Function prototypes 
*/
static void uart_event_task(void *pvParameters);
static void decode_reception_task(void *pvParameters);
static void send_data(uint16_t id, uint8_t command, uint8_t *send_data, uint8_t length);
static void process_inbound_data(uint8_t *rx_buffer);
static void process_outbound_task(void *pvParameters);



/*
 * Configure the hardware as necessary to run this demo.
 */
static void prvSetupHardware( void );

#endif