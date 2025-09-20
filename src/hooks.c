/**
 * @file hooks.c
 * @brief Hooks for FreeRTOS
 * @author Carlos Mazzei
 *
 * This file contains the code of the hooks for FreeRTOS.
 *
 * @copyright
 *   (c) 2020-2025 Carlos Mazzei - All rights reserved.
 */

#include "hooks.h"
#include "error_management.h"

static volatile size_t free_heap_size = 0;

//-----------------------------------------------------------

void vApplicationMallocFailedHook(void)
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	 * free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	 * internally by FreeRTOS API functions that create tasks, queues, software
	 * timers, and semaphores.  The size of the FreeRTOS heap is set by the
	 * configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

	// Force an assert.
	set_error_state_persistent(ERROR_FREERTOS_STACK);

	// Disable interrupts using Pico SDK
	(void)save_and_disable_interrupts();

	// Show error pattern forever using busy wait
	while (true) {
		show_error_pattern_blocking(ERROR_FREERTOS_STACK);
	}
}
//-----------------------------------------------------------

// FreeRTOS stack overflow - uses busy wait (scheduler may be corrupted)
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
	(void)pcTaskName;
	(void)pxTask;

	set_error_state_persistent(ERROR_FREERTOS_STACK);

	// Disable interrupts using Pico SDK
	(void)save_and_disable_interrupts();

	// Show error pattern forever using busy wait
	while (true) {
		show_error_pattern_blocking(ERROR_FREERTOS_STACK);
	}
}
//-----------------------------------------------------------

void vApplicationIdleHook(void)
{
	free_heap_size = xPortGetFreeHeapSize();
}
//-----------------------------------------------------------

void vApplicationTickHook(void)
{
	// Tick hook
}
