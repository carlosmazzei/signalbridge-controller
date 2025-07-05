#ifndef TASK_PROPS_H
#define TASK_PROPS_H

#include <stdint.h>
#include "task.h"

/**
 * Structure to hold task handle and high watermark
 */
typedef struct task_props_t
{
	TaskHandle_t task_handle;
	uint32_t high_watermark;
} task_props_t;

#endif
