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

/**
 * @brief Atomically read @p obj using sequentially consistent ordering.
 *
 * @param[in] obj Atomic 32-bit object to read.
 *
 * @return The value observed in @p obj.
 */
uint32_t atomic_u32_load(const _Atomic uint32_t *obj);

/**
 * @brief Atomically write @p value to @p obj using sequentially consistent ordering.
 *
 * @param[in,out] obj Atomic 32-bit object to update.
 * @param[in] value Value to store.
 */
void atomic_u32_store(_Atomic uint32_t *obj, uint32_t value);

/**
 * @brief Atomically add @p value to @p obj modulo 2^32.
 *
 * @param[in,out] obj Atomic 32-bit object to update.
 * @param[in] value Addend.
 */
void atomic_u32_add(_Atomic uint32_t *obj, uint32_t value);

/**
 * @brief Replace @p obj with @p desired if it still holds @p *expected.
 *
 * @param[in,out] obj Atomic 32-bit object to compare and update.
 * @param[in,out] expected Expected old value; receives the observed value if
 *                         the comparison fails.
 * @param[in] desired Value to store when the comparison succeeds.
 *
 * @return true when the exchange happened; otherwise @p *expected receives the current value.
 */
bool atomic_u32_compare_exchange(_Atomic uint32_t *obj, uint32_t *expected, uint32_t desired);

#endif // ATOMIC_U32_H
