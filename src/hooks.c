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

//-----------------------------------------------------------

void vApplicationMallocFailedHook(void)
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	 * free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	 * internally by FreeRTOS API functions that create tasks, queues, software
	 * timers, and semaphores.  The size of the FreeRTOS heap is set by the
	 * configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

	// Disable interrupts using Pico SDK
	(void)save_and_disable_interrupts();
	fatal_halt(ERROR_FREERTOS_STACK);
}
//-----------------------------------------------------------

// FreeRTOS stack overflow - uses busy wait (scheduler may be corrupted)
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName) // NOSONAR; FreeRTOS prototype
{
	(void)pcTaskName;
	(void)pxTask;

	// Disable interrupts using Pico SDK
	(void)save_and_disable_interrupts();
	fatal_halt(ERROR_FREERTOS_STACK);
}
//-----------------------------------------------------------

void vApplicationIdleHook(void)
{
	static volatile size_t free_heap_size = 0;
	free_heap_size = xPortGetFreeHeapSize();
}
//-----------------------------------------------------------

void vApplicationTickHook(void)
{
	// Tick hook
}
