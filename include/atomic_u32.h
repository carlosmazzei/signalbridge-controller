/**
 * @file atomic_u32.h
 * @brief Sequentially consistent 32-bit atomic operations shared by both cores and IRQs.
 *
 * Every use of <stdatomic.h> generic functions is confined to atomic_u32.c,
 * the only translation unit covered by MISRA C:2012 Rule 1.2 deviation D1
 * (docs/MISRA_DEVIATIONS.md): the toolchain implements the C11 generics with
 * compiler built-ins.
 */

#ifndef ATOMIC_U32_H
#define ATOMIC_U32_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

/** @brief Atomically read @p obj. */
uint32_t atomic_u32_load(const _Atomic uint32_t *obj);

/** @brief Atomically write @p value to @p obj. */
void atomic_u32_store(_Atomic uint32_t *obj, uint32_t value);

/** @brief Atomically add @p value to @p obj (modulo 2^32). */
void atomic_u32_add(_Atomic uint32_t *obj, uint32_t value);

/**
 * @brief Replace @p obj with @p desired if it still holds @p *expected.
 *
 * @return true when the exchange happened; otherwise @p *expected receives the current value.
 */
bool atomic_u32_compare_exchange(_Atomic uint32_t *obj, uint32_t *expected, uint32_t desired);

#endif // ATOMIC_U32_H
