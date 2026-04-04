#pragma once

#include <stdint.h>

// Mock Pico time functions
uint32_t time_us_32(void);
void sleep_us(uint64_t us);
void busy_wait_us_32(uint32_t delay_us);

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) (ms)  // Simple mapping for tests
#endif
