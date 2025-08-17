#ifndef DATA_EVENT_H
#define DATA_EVENT_H

#include <stdint.h>

#define MAX_DATA_SIZE 10

/**
 * Event of the data to be sent to the host.
 */
typedef struct data_events_t
{
	uint8_t command; // Command to be sent
	uint8_t data_length; // Length of the data to be sent
	uint8_t data[MAX_DATA_SIZE]; // Data to be sent
} data_events_t;

#endif
