/**
 * @file app_tasks.h
 * @brief Creation and teardown of application FreeRTOS tasks.
 */

#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdbool.h>

/**
 * @brief Create all application tasks and supporting queues.
 *
 * @return `true` if every task and queue was created successfully.
 */
bool app_tasks_create_all(void);

/**
 * @brief Delete all application tasks and destroy associated queues.
 */
void app_tasks_cleanup(void);

#endif // APP_TASKS_H
