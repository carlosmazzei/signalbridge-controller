/**
 * @file task_props.h
 * @brief Helper structure for tracking FreeRTOS task bookkeeping data.
 */

#ifndef TASK_PROPS_H
#define TASK_PROPS_H

#include <stdint.h>
#include "task.h"

/**
 * @struct task_props_t
 * @brief Captures runtime metrics for a FreeRTOS task.
 */
typedef struct task_props_t
{
	TaskHandle_t task_handle; /**< Task handle assigned by the scheduler. */
	uint32_t high_watermark;  /**< Minimum remaining stack depth recorded. */
} task_props_t;

#endif // TASK_PROPS_H
