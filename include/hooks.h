#ifndef HOOKS_H
#define HOOKS_H

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"

extern volatile size_t free_heap_size;

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
 * within this file. */
void vApplicationMallocFailedHook( void );
void vApplicationIdleHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
void vApplicationTickHook( void );

#endif