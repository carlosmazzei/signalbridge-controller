#include "task_health.h"
#include <stdatomic.h>

static _Atomic uint32_t last_progress[NUM_TASKS];
static atomic_bool waiting[NUM_TASKS];

void task_health_reset(uint32_t now_us)
{
	for (unsigned i = 0U; i < NUM_TASKS; i++)
	{
		atomic_store(&last_progress[i], now_us);
		atomic_store(&waiting[i], false);
	}
}

void task_health_progress(task_enum_t task, uint32_t now_us)
{
	if ((unsigned)task >= NUM_TASKS)
		return;
	atomic_store_explicit(&last_progress[task], now_us, memory_order_relaxed);
	atomic_store_explicit(&waiting[task], false, memory_order_release);
}

void task_health_wait(task_enum_t task)
{
	if ((unsigned)task < NUM_TASKS)
		atomic_store_explicit(&waiting[task], true, memory_order_release);
}

bool task_health_check(uint32_t now_us)
{
	for (unsigned i = 0U; i < NUM_TASKS; i++)
	{
		bool blocked = atomic_load_explicit(&waiting[i], memory_order_acquire);
		uint32_t last = atomic_load_explicit(&last_progress[i], memory_order_relaxed);
		// The other core may publish a timestamp just after now_us.
		if (!blocked && ((int32_t)(now_us - last) >= (int32_t)TASK_HEALTH_TIMEOUT_US))
			return false;
	}
	return true;
}
