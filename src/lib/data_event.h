#ifndef _DATA_EVENT_H_
#define _DATA_EVENT_H_

#include <stdint.h>

/**
 * Event of the data to be sent to the host.
 */
typedef struct
{
    uint8_t command;
    uint8_t data;
} data_events_t;

#endif
