/**
 * @file atomic_u32.c
 * @brief Sequentially consistent 32-bit atomic operations.
 *
 * MISRA C:2012 Rule 1.2 deviation D1 (docs/MISRA_DEVIATIONS.md):
 * the C11 <stdatomic.h> generic functions expand to GCC __atomic built-ins.
 * They are the only portable primitive that is safe across both RP2040 cores
 * and IRQ context, so their use is confined to this file.
 */

#include "atomic_u32.h"

uint32_t atomic_u32_load(const _Atomic uint32_t *obj)
{
	// cppcheck-suppress misra-c2012-1.2 ; Deviation D1: C11 atomic generic expands to compiler built-ins
	return atomic_load(obj);
}

void atomic_u32_store(_Atomic uint32_t *obj, uint32_t value)
{
	// cppcheck-suppress misra-c2012-1.2 ; Deviation D1: C11 atomic generic expands to compiler built-ins
	atomic_store(obj, value);
}

void atomic_u32_add(_Atomic uint32_t *obj, uint32_t value)
{
	// cppcheck-suppress misra-c2012-1.2 ; Deviation D1: C11 atomic generic expands to compiler built-ins
	(void)atomic_fetch_add(obj, value);
}

bool atomic_u32_compare_exchange(_Atomic uint32_t *obj, uint32_t *expected, uint32_t desired)
{
	// cppcheck-suppress misra-c2012-1.2 ; Deviation D1: C11 atomic generic expands to compiler built-ins
	return atomic_compare_exchange_strong(obj, expected, desired);
}
