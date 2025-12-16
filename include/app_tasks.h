/**
 * @file app_tasks.h
 * @brief Creation and teardown of application FreeRTOS tasks.
 */

#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdbool.h>

/**
 * @brief Create the USB CDC communication subsystem tasks and queues.
 */
bool app_tasks_create_comm(void);

/**
 * @brief Create the application tasks and queues that depend on I/O drivers.
 */
bool app_tasks_create_application(void);

/**
 * @brief Delete non-communication tasks and queues.
 */
void app_tasks_cleanup_application(void);

#endif // APP_TASKS_H
