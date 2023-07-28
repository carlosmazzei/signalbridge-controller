#ifndef _HOOKS_H_
#define _HOOKS_H_

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
void vApplicationMallocFailedHook( void );
void vApplicationIdleHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
void vApplicationTickHook( void );

#endif