/**
 * @file hooks.c
 * @brief Hooks for FreeRTOS
 * @author Carlos Mazzei
 *
 * This file contains the code of the hooks for FreeRTOS.
 *
 * @copyright (c) 2020-2024 Carlos Mazzei
 * All rights reserved.
 */

#include "hooks.h"

/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook(void)
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	   free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	   internally by FreeRTOS API functions that create tasks, queues, software
	   timers, and semaphores.  The size of the FreeRTOS heap is set by the
	   configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

	/* Force an assert. */
	configASSERT((volatile void *)NULL);
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
	(void)pcTaskName;
	(void)pxTask;

	/* Run time stack overflow checking is performed if
	   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	   function is called if a stack overflow is detected. */

	/* Force an assert. */
	configASSERT((volatile void *)NULL);
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook(void)
{
	/* This is just a trivial example of an idle hook.  It is called on each
	   cycle of the idle task.  It must *NOT* attempt to block.  In this case the
	   idle task just queries the amount of FreeRTOS heap that remains.  See the
	   memory management section on the http://www.FreeRTOS.org web site for memory
	   management options.  If there is a lot of heap memory free then the
	   configTOTAL_HEAP_SIZE value in FreeRTOSConfig.h can be reduced to free up
	   RAM. */

	/** @todo Use the idle task to calculate heap usage and send it if queried */
	free_heap_size = xPortGetFreeHeapSize();
}
/*-----------------------------------------------------------*/

void vApplicationTickHook(void)
{
	/* Tick hook */
}