/*
 * Copyright (c) 2017 Jean-Paul Etienne <fractalclone@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief interrupt management of a SoC implementing the RISC-V privileged
 *        architecture specification whose interrupts are handled in CLINT mode:
 *        through the mie/sie CSR, and an external interrupt controller behind
 *        it where the SoC has one. A CLIC takes this over, its driver providing
 *        the interrupt management instead.
 */
#include <zephyr/irq.h>
#include <zephyr/irq_multilevel.h>

#ifdef CONFIG_RISCV_HAS_EXT_IRQ_CONTROLLER
#include <zephyr/drivers/interrupt_controller/riscv_ext_irq.h>
#endif

void arch_irq_enable(unsigned int irq)
{
#ifdef CONFIG_RISCV_HAS_EXT_IRQ_CONTROLLER
	if (irq_get_level(irq) == 2) {
		riscv_ext_irq_enable(irq);
		return;
	}
#endif

	/*
	 * CSR mie/sie register is updated using atomic instruction csrs
	 * (atomic read and set bits in CSR register)
	 */
#ifdef CONFIG_RISCV_S_MODE
#if !defined(CONFIG_64BIT) && defined(CONFIG_RISCV_ISA_EXT_SSAIA)
	/* sie is 64 bits in AIA; upper 32 bits are accessed using sieh CSR for RV32 */
	if (irq >= 32) {
		csr_set(sieh, 1 << (irq - 32));
		return;
	}
#endif
	csr_set(sie, 1UL << irq);
#else
#if !defined(CONFIG_64BIT) && defined(CONFIG_RISCV_ISA_EXT_SMAIA)
	/* mie is 64 bits in AIA; upper 32 bits are accessed using mieh CSR for RV32 */
	if (irq >= 32) {
		csr_set(mieh, 1 << (irq - 32));
		return;
	}
#endif
	csr_set(mie, 1UL << irq);
#endif
}

void arch_irq_disable(unsigned int irq)
{
#ifdef CONFIG_RISCV_HAS_EXT_IRQ_CONTROLLER
	if (irq_get_level(irq) == 2) {
		riscv_ext_irq_disable(irq);
		return;
	}
#endif

	/*
	 * Use atomic instruction csrc to disable device interrupt in mie/sie CSR.
	 * (atomic read and clear bits in CSR register)
	 */
#ifdef CONFIG_RISCV_S_MODE
#if !defined(CONFIG_64BIT) && defined(CONFIG_RISCV_ISA_EXT_SSAIA)
	/* sie is 64 bits in AIA; upper 32 bits are accessed using sieh CSR for RV32 */
	if (irq >= 32) {
		csr_clear(sieh, 1 << (irq - 32));
		return;
	}
#endif
	csr_clear(sie, 1UL << irq);
#else
#if !defined(CONFIG_64BIT) && defined(CONFIG_RISCV_ISA_EXT_SMAIA)
	/* mie is 64 bits in AIA; upper 32 bits are accessed using mieh CSR for RV32 */
	if (irq >= 32) {
		csr_clear(mieh, 1 << (irq - 32));
		return;
	}
#endif
	csr_clear(mie, 1UL << irq);
#endif
}

int arch_irq_is_enabled(unsigned int irq)
{
	unsigned long ie;

#ifdef CONFIG_RISCV_HAS_EXT_IRQ_CONTROLLER
	if (irq_get_level(irq) == 2) {
		return riscv_ext_irq_is_enabled(irq);
	}
#endif

#ifdef CONFIG_RISCV_S_MODE
#if !defined(CONFIG_64BIT) && defined(CONFIG_RISCV_ISA_EXT_SSAIA)
	/* sie is 64 bits in AIA; upper 32 bits are accessed using sieh CSR for RV32. */
	if (irq >= 32) {
		ie = csr_read(sieh);
		return !!(ie & (1 << (irq - 32)));
	}
#endif
	ie = csr_read(sie);
#else
#if !defined(CONFIG_64BIT) && defined(CONFIG_RISCV_ISA_EXT_SMAIA)
	/* mie is 64 bits in AIA; upper 32 bits are accessed using mieh CSR for RV32. */
	if (irq >= 32) {
		ie = csr_read(mieh);
		return !!(ie & (1 << (irq - 32)));
	}
#endif
	ie = csr_read(mie);
#endif

	return !!(ie & (1UL << irq));
}

#ifdef CONFIG_RISCV_HAS_EXT_IRQ_CONTROLLER
void z_riscv_irq_priority_set(unsigned int irq, unsigned int prio, uint32_t flags)
{
	if (irq_get_level(irq) == 2) {
		riscv_ext_irq_priority_set(irq, prio, flags);
	}
}
#endif /* CONFIG_RISCV_HAS_EXT_IRQ_CONTROLLER */
