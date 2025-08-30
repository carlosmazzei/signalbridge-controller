#ifndef MOCK_SEMPHR_H
#define MOCK_SEMPHR_H

#include <stdint.h>
#include <stdbool.h>

// Mock FreeRTOS semaphore types and functions
typedef void* SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateMutex(void);

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1

#endif // MOCK_SEMPHR_H