/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Interface of a RISC-V external interrupt controller
 *
 * Interface implemented by the interrupt controller sitting behind the RISC-V
 * machine or supervisor external interrupt line, which aggregates the SoC's
 * second-level interrupts. Exactly one implementation is built into an image:
 * the PLIC (@kconfig{CONFIG_RISCV_HAS_PLIC}) or the AIA
 * (@kconfig{CONFIG_RISCV_HAS_AIA}).
 *
 * The generic RISC-V interrupt management in arch/riscv/core calls
 * these for second-level IRQs, and handles first-level IRQs itself through the
 * mie/sie CSR.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_INTERRUPT_CONTROLLER_RISCV_EXT_IRQ_H_
#define ZEPHYR_INCLUDE_DRIVERS_INTERRUPT_CONTROLLER_RISCV_EXT_IRQ_H_

#include <zephyr/types.h>

/**
 * @brief Enable interrupt
 *
 * @param irq Multi-level encoded interrupt ID
 */
void riscv_ext_irq_enable(uint32_t irq);

/**
 * @brief Disable interrupt
 *
 * @param irq Multi-level encoded interrupt ID
 */
void riscv_ext_irq_disable(uint32_t irq);

/**
 * @brief Check if an interrupt is enabled
 *
 * @param irq Multi-level encoded interrupt ID
 *
 * @retval 1 If the interrupt is enabled.
 * @retval 0 If the interrupt is disabled.
 */
int riscv_ext_irq_is_enabled(uint32_t irq);

/**
 * @brief Set interrupt priority
 *
 * @param irq Multi-level encoded interrupt ID
 * @param prio Interrupt priority
 * @param flags Interrupt flags, controller specific. Ignored by controllers
 *              that have nothing to configure from them, such as the PLIC.
 */
void riscv_ext_irq_priority_set(uint32_t irq, uint32_t prio, uint32_t flags);

#endif /* ZEPHYR_INCLUDE_DRIVERS_INTERRUPT_CONTROLLER_RISCV_EXT_IRQ_H_ */
