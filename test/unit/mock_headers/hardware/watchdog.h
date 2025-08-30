#pragma once
#include <stdint.h>
#include <stdbool.h>

void watchdog_enable(uint32_t delay_ms, bool pause_on_debug);
void watchdog_update(void);

// Mock watchdog hardware structure
typedef struct {
    uint32_t scratch[8];
} watchdog_hw_t;

extern watchdog_hw_t *watchdog_hw;
