/**
 * @file app_setup.h
 * @brief Hardware and peripheral initialisation helpers.
 */

#ifndef APP_SETUP_H
#define APP_SETUP_H

#include <stdbool.h>

/**
 * @brief Perform one-time hardware and queue initialisation.
 *
 * @return `true` when initialisation succeeded.
 */
bool app_setup_hardware(void);

#endif // APP_SETUP_H
