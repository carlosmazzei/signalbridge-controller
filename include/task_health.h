#ifndef TASK_HEALTH_H
#define TASK_HEALTH_H

#include <stdbool.h>
#include <stdint.h>
#include "app_config.h"

#define TASK_HEALTH_TIMEOUT_US 4000000U

void task_health_reset(uint32_t now_us);
void task_health_progress(task_enum_t task, uint32_t now_us);
// Only for an intentional, indefinite kernel wait (not hardware work).
void task_health_wait(task_enum_t task);
bool task_health_check(uint32_t now_us);

#endif
