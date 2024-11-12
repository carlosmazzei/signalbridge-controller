#ifndef _TASK_PROPS_H_
#define _TASK_PROPS_H_

#include <stdint.h>
#include "task.h"

/**
 * Structure to hold task handle and high watermark
 */
typedef struct task_props_t
{
	TaskHandle_t task_handle;
	uint8_t high_watermark;
} task_props_t;

#endif
