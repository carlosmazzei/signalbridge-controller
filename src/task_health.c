/**
 * @file task_health.c
 * @brief Track task progress and intentional waits for watchdog supervision.
 *
 * Timestamps and wait markers are atomic because application tasks on both
 * RP2040 cores publish progress while the supervisor checks their state.
 */

#include "task_health.h"
#include "atomic_u32.h"

static _Atomic uint32_t last_progress[NUM_TASKS]; /**< Most recent progress timestamp per task. */
static _Atomic uint32_t waiting[NUM_TASKS]; /**< Nonzero when a task is in an intentional kernel wait. */

void task_health_reset(uint32_t now_us)
{
	for (uint32_t i = 0U; i < (uint32_t)NUM_TASKS; i++)
	{
		atomic_u32_store(&last_progress[i], now_us);
		atomic_u32_store(&waiting[i], 0U);
	}
}

void task_health_progress(task_enum_t task, uint32_t now_us)
{
	if ((uint32_t)task < (uint32_t)NUM_TASKS)
	{
		atomic_u32_store(&last_progress[task], now_us);
		atomic_u32_store(&waiting[task], 0U);
	}
}

void task_health_wait(task_enum_t task)
{
	if ((uint32_t)task < (uint32_t)NUM_TASKS)
	{
		atomic_u32_store(&waiting[task], 1U);
	}
}

bool task_health_check(uint32_t now_us)
{
	bool healthy = true;

	for (uint32_t i = 0U; (i < (uint32_t)NUM_TASKS) && healthy; i++)
	{
		const bool blocked = (atomic_u32_load(&waiting[i]) != 0U);
		const uint32_t last = atomic_u32_load(&last_progress[i]);
		const uint32_t elapsed = now_us - last;
		// The other core may publish a timestamp just after now_us, which wraps
		// elapsed to a huge value: only a forward gap below 2^31 counts.
		const bool overdue = (elapsed >= TASK_HEALTH_TIMEOUT_US) && (elapsed < 0x80000000U);
		if (!blocked && overdue)
		{
			healthy = false;
		}
	}
	return healthy;
}
