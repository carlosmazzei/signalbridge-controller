/**
 * @file task_health.h
 * @brief Progress and intentional-wait tracking for watchdog supervision.
 *
 * The supervisor checks each application task's last progress timestamp.
 * Tasks blocked in an intentional, indefinite kernel wait are exempt until
 * their next progress report.
 */

#ifndef TASK_HEALTH_H
#define TASK_HEALTH_H

#include <stdbool.h>
#include <stdint.h>
#include "app_config.h"

/** Maximum allowed gap between task progress reports, in microseconds. */
#define TASK_HEALTH_TIMEOUT_US 4000000U

/**
 * @brief Initialise all task progress timestamps and clear wait markers.
 *
 * @param[in] now_us Current 32-bit microsecond timestamp.
 */
void task_health_reset(uint32_t now_us);

/**
 * @brief Record progress for a task and clear its intentional-wait marker.
 *
 * @param[in] task Task identifier to update.
 * @param[in] now_us Current 32-bit microsecond timestamp.
 */
void task_health_progress(task_enum_t task, uint32_t now_us);

/**
 * @brief Mark a task as intentionally blocked in an indefinite kernel wait.
 *
 * The supervisor ignores the task's elapsed progress time until
 * @ref task_health_progress() clears this marker. Do not use this for hardware
 * work or bounded waits.
 *
 * @param[in] task Task identifier to mark as waiting.
 */
void task_health_wait(task_enum_t task);

/**
 * @brief Check whether every non-waiting task has reported recent progress.
 *
 * Elapsed time is compared with @ref TASK_HEALTH_TIMEOUT_US using wrap-safe
 * 32-bit timestamp arithmetic.
 *
 * @param[in] now_us Current 32-bit microsecond timestamp.
 *
 * @return `true` when no task is overdue; otherwise `false`.
 */
bool task_health_check(uint32_t now_us);

#endif
