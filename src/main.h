#ifndef _MAIN_H_
#define _MAIN_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include "cobs.h"
#include "stdA320.h"

#include "bsp/board.h"
#include "tusb.h"

#define BUF_SIZE (480)
#define RD_BUF_SIZE 240
#define WR_BUF_SIZE 240
#define ENCODED_QUEUE_SIZE 100
#define MAX_ENCODED_BUFFER_SIZE 12 // n/254 + 1 + Packet Marker
#define DATA_BUFFER_SIZE 10

#define PACKET_MARKER 0x00

int64_t data_received_time = 0;
uint8_t receive_buffer[MAX_ENCODED_BUFFER_SIZE];
size_t receive_buffer_index = 0;
bool receive_buffer_overflow = false;

static void cdc_task(void);
static void decode_reception_task();
void send_data(uint16_t id, uint8_t command, uint8_t *send_data, uint8_t length);
void process_inbound_data(uint8_t *rx_buffer);
void process_outbound_task();

#endif