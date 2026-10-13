/*
 * FreeRTOS V202107.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/**
 * @file FreeRTOSConfig.h
 * @brief FreeRTOS kernel, SMP, memory, and runtime-statistics configuration.
 *
 * These application-specific settings configure the RP2040 FreeRTOS port and
 * the task APIs used by the controller firmware.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>
#include <assert.h>

/*-----------------------------------------------------------
* Application specific definitions.
*
* These definitions should be adjusted for your particular hardware and
* application requirements.
*
* THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
* FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE.
*
* See http://www.freertos.org/a00110.html
*----------------------------------------------------------*/

// Scheduler Related
/** Enable preemptive task scheduling. */
#define configUSE_PREEMPTION                    1
/** Disable tickless idle mode. */
#define configUSE_TICKLESS_IDLE                 0
/** Enable the application idle hook. */
#define configUSE_IDLE_HOOK                     1
/** Disable the application tick hook. */
#define configUSE_TICK_HOOK                     0
/** FreeRTOS tick rate in ticks per second. */
#define configTICK_RATE_HZ                      ( ( TickType_t ) 2000 )
/** Number of task priority levels. */
#define configMAX_PRIORITIES                    10
/** Minimum task stack depth, in stack units. */
#define configMINIMAL_STACK_SIZE                ( configSTACK_DEPTH_TYPE ) 256
/** Use 32-bit rather than 16-bit tick counters. */
#define configUSE_16_BIT_TICKS                  0

/** Allow the idle task to yield when another task is ready. */
#define configIDLE_SHOULD_YIELD                 1

// Synchronization Related
/** Enable mutex support. */
#define configUSE_MUTEXES                       1
/** Enable recursive mutex support. */
#define configUSE_RECURSIVE_MUTEXES             1
/** Disable application task tags. */
#define configUSE_APPLICATION_TASK_TAG          0
/** Enable counting semaphores. */
#define configUSE_COUNTING_SEMAPHORES           1
/** Number of queue names retained in the registry. */
#define configQUEUE_REGISTRY_SIZE               8
/** Enable queue sets. */
#define configUSE_QUEUE_SETS                    1
/** Enable time slicing between equal-priority ready tasks. */
#define configUSE_TIME_SLICING                  1
/** Disable Newlib reentrancy support. */
#define configUSE_NEWLIB_REENTRANT              0
/** Disable backward-compatible FreeRTOS API aliases. */
#define configENABLE_BACKWARD_COMPATIBILITY     0
/** Number of thread-local storage pointers per task. */
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 5

// System
/** Type used to represent task stack depths. */
#define configSTACK_DEPTH_TYPE                  uint32_t
/** Type used to represent message-buffer lengths. */
#define configMESSAGE_BUFFER_LENGTH_TYPE        size_t

// Memory allocation related definitions.
/** Disable static object allocation. */
#define configSUPPORT_STATIC_ALLOCATION         0
/** Enable dynamic object allocation from the FreeRTOS heap. */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
/** Size of the FreeRTOS heap in bytes. */
#define configTOTAL_HEAP_SIZE                   (128*1024)
/** Use the FreeRTOS-managed heap rather than an application-provided heap. */
#define configAPPLICATION_ALLOCATED_HEAP        0

// Hook function related definitions.
/** Enable the stack-overflow check mode selected below. */
#define configCHECK_FOR_STACK_OVERFLOW          2
/** Enable the malloc-failure hook. */
#define configUSE_MALLOC_FAILED_HOOK            1
/** Enable the stack-overflow hook. */
#define configUSE_STACK_OVERFLOW_HOOK           1
/** Disable the daemon-task startup hook. */
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

// Run time and task stats gathering related definitions.
// Required: the PC_TASK_STATUS_CMD handler (send_heap_status in app_comm.c)
// reports per-task and idle CPU usage to the host via
// ulTaskGetRunTimeCounter/Percent and ulTaskGetIdleRunTimeCounter/Percent,
// all of which need configGENERATE_RUN_TIME_STATS=1.
/** Enable task and idle runtime statistics. */
#define configGENERATE_RUN_TIME_STATS           1
/** Enable FreeRTOS trace facilities. */
#define configUSE_TRACE_FACILITY                1
/** Enable task-statistics formatting helpers. */
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

// Runtime stats configurations
extern uint32_t ulPortGetRunTime( void );
/** Runtime-statistics timer configuration hook; no separate setup is required. */
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()
/** Read the RP2040 microsecond runtime counter. */
#define portGET_RUN_TIME_COUNTER_VALUE()            ulPortGetRunTime()

// Co-routine related definitions.
/** Disable legacy co-routine support. */
#define configUSE_CO_ROUTINES                   0
/** Maximum priority count reserved for co-routines. */
#define configMAX_CO_ROUTINE_PRIORITIES         1

// Software timer related definitions.
/** Enable software timers. */
#define configUSE_TIMERS                        1
/** Run the timer service task one level below the maximum priority. */
#define configTIMER_TASK_PRIORITY               ( configMAX_PRIORITIES - 1 )
/** Number of commands held by the timer command queue. */
#define configTIMER_QUEUE_LENGTH                10
/** Timer service task stack depth. */
#define configTIMER_TASK_STACK_DEPTH            2048

// SMP port only
/** Number of RP2040 cores managed by the SMP port. */
#define configNUMBER_OF_CORES                   2
/** Enable task core-affinity support. */
#define configUSE_CORE_AFFINITY                 1
/** Core that runs the periodic tick interrupt. */
#define configTICK_CORE                         0
/** Allow tasks at multiple priorities to run concurrently on separate cores. */
#define configRUN_MULTIPLE_PRIORITIES           1
/** Disable the passive idle hook. */
#define configUSE_PASSIVE_IDLE_HOOK             0

// RP2040 specific
/** Enable Pico SDK synchronization-object interoperability. */
#define configSUPPORT_PICO_SYNC_INTEROP         1
/** Enable Pico SDK time API interoperability. */
#define configSUPPORT_PICO_TIME_INTEROP         1

// MEMORY OPTIMIZATION: Reduce context switch overhead with larger stacks
/** Minimum secure stack allocation used by the port. */
#define configMINIMAL_SECURE_STACK_SIZE         1024    // Increased for security
/** Maximum number of characters in a task name. */
#define configMAX_TASK_NAME_LEN                 16      // Keep reasonable

// STACK MONITORING: Enable for 8KB stack debugging
/** Record the high address of each task stack for diagnostics. */
#define configRECORD_STACK_HIGH_ADDRESS         1
//#define configINCLUDE_FREERTOS_TASK_C_ADDITIONS_H   1

// PERFORMANCE TUNING for larger stacks
/** Enable direct-to-task notifications. */
#define configUSE_TASK_NOTIFICATIONS            1

// Define to trap errors during development.
/** Route FreeRTOS assertions to the standard assert handler. */
#define configASSERT(x)                         assert(x)

/* Set the following definitions to 1 to include the API function, or zero
 * to exclude the API function. */
/** Include the API for changing a task priority. */
#define INCLUDE_vTaskPrioritySet                1
/** Include the API for reading a task priority. */
#define INCLUDE_uxTaskPriorityGet               1
/** Include the task deletion API. */
#define INCLUDE_vTaskDelete                     1
/** Include the task suspend and resume APIs. */
#define INCLUDE_vTaskSuspend                    1
/** Include the delay-until API. */
#define INCLUDE_vTaskDelayUntil                 1
/** Include the relative task delay API. */
#define INCLUDE_vTaskDelay                      1
/** Include the scheduler-state query API. */
#define INCLUDE_xTaskGetSchedulerState          1
/** Include the current-task handle query API. */
#define INCLUDE_xTaskGetCurrentTaskHandle       1
/** Include the stack high-water-mark query API. */
#define INCLUDE_uxTaskGetStackHighWaterMark     1
/** Include the idle-task handle query API. */
#define INCLUDE_xTaskGetIdleTaskHandle          1
/** Include the task-state query API. */
#define INCLUDE_eTaskGetState                   1
/** Include the timer-daemon callback queue API. */
#define INCLUDE_xTimerPendFunctionCall          1
/** Exclude the API that aborts a task delay. */
#define INCLUDE_xTaskAbortDelay                 0
/** Include the API that looks up a task by name. */
#define INCLUDE_xTaskGetHandle                  1
/** Exclude the API that resumes a task from an ISR. */
#define INCLUDE_xTaskResumeFromISR              0
/** Exclude the API that queries a mutex holder. */
#define INCLUDE_xQueueGetMutexHolder            0

// A header file that defines trace macro can be included here.

#endif // FREERTOS_CONFIG_H
