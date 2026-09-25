/*
 * Copyright (c) 2022 Carlo Caione <ccaione@baylibre.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Driver for Platform Level Interrupt Controller (PLIC)
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_INTERRUPT_CONTROLLER_RISCV_PLIC_H_
#define ZEPHYR_INCLUDE_DRIVERS_INTERRUPT_CONTROLLER_RISCV_PLIC_H_

#include <zephyr/device.h>
#include <zephyr/toolchain.h>
#include <zephyr/drivers/interrupt_controller/riscv_ext_irq.h>

/**
 * @brief Mark interrupt as completed
 *
 * @param irq IRQ number to claim as completed
 */
void riscv_plic_irq_complete(uint32_t irq);

/**
 * @brief Enable interrupt
 *
 * @deprecated Use riscv_ext_irq_enable() instead.
 *
 * @param irq Multi-level encoded interrupt ID
 */
__deprecated static inline void riscv_plic_irq_enable(uint32_t irq)
{
	riscv_ext_irq_enable(irq);
}

/**
 * @brief Disable interrupt
 *
 * @deprecated Use riscv_ext_irq_disable() instead.
 *
 * @param irq Multi-level encoded interrupt ID
 */
__deprecated static inline void riscv_plic_irq_disable(uint32_t irq)
{
	riscv_ext_irq_disable(irq);
}

/**
 * @brief Check if an interrupt is enabled
 *
 * @deprecated Use riscv_ext_irq_is_enabled() instead.
 *
 * @param irq Multi-level encoded interrupt ID
 * @return Returns true if interrupt is enabled, false otherwise
 */
__deprecated static inline int riscv_plic_irq_is_enabled(uint32_t irq)
{
	return riscv_ext_irq_is_enabled(irq);
}

/**
 * @brief Set interrupt priority
 *
 * @deprecated Use riscv_ext_irq_priority_set() instead.
 *
 * @param irq Multi-level encoded interrupt ID
 * @param prio interrupt priority
 */
__deprecated static inline void riscv_plic_set_priority(uint32_t irq, uint32_t prio)
{
	riscv_ext_irq_priority_set(irq, prio, 0);
}

/**
 * @brief Set IRQ affinity.
 *
 * @param irq IRQ line.
 * @param cpumask CPU bit mask.
 *
 * @return 0 if success, negative errno value otherwise
 */
int riscv_plic_irq_set_affinity(uint32_t irq, uint32_t cpumask);

/**
 * @brief Set interrupt as pending
 *
 * @param irq Multi-level encoded interrupt ID
 */
void riscv_plic_irq_set_pending(uint32_t irq);

/**
 * @brief Get active interrupt ID
 *
 * @note Should be called with interrupt locked
 *
 * @return Returns the ID of an active interrupt
 */
unsigned int riscv_plic_get_irq(void);

/**
 * @brief Get active interrupt controller device
 *
 * @note Should be called with interrupt locked
 *
 * @return Returns device pointer of the active interrupt device
 */
const struct device *riscv_plic_get_dev(void);

#endif /* ZEPHYR_INCLUDE_DRIVERS_INTERRUPT_CONTROLLER_RISCV_PLIC_H_ */
