#ifndef _DATA_EVENT_H_
#define _DATA_EVENT_H_

#include <stdint.h>

/**
 * Structure to hold the keypad key data.
 */
typedef struct
{
    uint8_t command;
    uint8_t data;
} data_events_t;

#endif
