#ifndef _KEYPAD_H_
#define _KEYPAD_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "pico/stdlib.h"
#include "stdA320.h"

#define KEYPAD_ROWS 8
#define KEYPAD_COLUMNS 8

#define KEYPAD_MAX_COLS 8
#define KEYPAD_MAX_ROWS 8

#define KEYPAD_STABILITY_BITS 3
#define KEYPAD_STABILITY_MASK ((1 << KEYPAD_STABILITY_BITS) - 1)

#define KEYPAD_COL_MUX_A 1
#define KEYPAD_COL_MUX_B 2
#define KEYPAD_COL_MUX_C 3

#define KEYPAD_ROW_MUX_A 1
#define KEYPAD_ROW_MUX_B 2
#define KEYPAD_ROW_MUX_C 3
#define KEYPAD_ROW_INPUT 4

#define KEY_PRESSED_MASK 0x03 // 011: two consecutive actives
#define KEY_RELEASED_MASK 0x04 // 100: two consecutive inactives
#define KEY_PRESSED 1
#define KEY_RELEASED 0 

/* 
 * Structure to hold the keypad configuration.
*/
typedef struct
{
    uint8_t rows;
    uint8_t columns;
    uint16_t settling_time_ms;
    QueueHandle_t keypad_event_queue;

} keypad_config_t;



/* Prototypes */
bool keypad_init(keypad_config_t *config);
void keypad_task(void *pvParameters);
void keypad_set_columns(uint8_t columns);
void keypad_set_rows(uint8_t rows);
void keypad_generate_event(uint8_t command, uint8_t row, uint8_t column, uint8_t state);

#endif