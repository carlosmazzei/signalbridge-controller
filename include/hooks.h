/**
 * @file hooks.h
 * @brief FreeRTOS application hook prototypes used by the firmware.
 */

#ifndef HOOKS_H
#define HOOKS_H

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"

/**
 * @brief Invoked when pvPortMalloc() fails due to insufficient heap space.
 */
void vApplicationMallocFailedHook(void);

/**
 * @brief Executed when the scheduler is idle.
 */
void vApplicationIdleHook(void);

/**
 * @brief Called when FreeRTOS detects a stack overflow.
 *
 * @param[in] pxTask      Handle of the task that overflowed its stack.
 * @param[in] pcTaskName  Name of the offending task.
 */
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName);

/**
 * @brief Periodic callback executed from the system tick interrupt.
 */
void vApplicationTickHook(void);

#endif /* HOOKS_H */
