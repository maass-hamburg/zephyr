/*
 * Copyright (c) 2017 Jean-Paul Etienne <fractalclone@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief interrupt management code for riscv SOCs supporting the riscv
	  privileged architecture specification
 */
#include <zephyr/irq.h>
#include <zephyr/irq_multilevel.h>

#if defined(CONFIG_RISCV_HAS_PLIC) || defined(CONFIG_RISCV_HAS_AIA)
#include <zephyr/drivers/interrupt_controller/riscv_ext_irq.h>
#define HAS_EXT_IRQ_CONTROLLER 1
#endif

/*
 * A CLIC takes over every interrupt of the SoC, so a CLIC based SoC implements the
 * arch interrupt management in its CLIC driver instead of here.
 */
#if !defined(CONFIG_RISCV_HAS_CLIC)

void arch_irq_enable(unsigned int irq)
{
#ifdef HAS_EXT_IRQ_CONTROLLER
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
#ifdef HAS_EXT_IRQ_CONTROLLER
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

#ifdef HAS_EXT_IRQ_CONTROLLER
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

#ifdef HAS_EXT_IRQ_CONTROLLER
void z_riscv_irq_priority_set(unsigned int irq, unsigned int prio, uint32_t flags)
{
	if (irq_get_level(irq) == 2) {
		riscv_ext_irq_priority_set(irq, prio, flags);
	}
}
#endif /* HAS_EXT_IRQ_CONTROLLER */

#endif /* !CONFIG_RISCV_HAS_CLIC */

#if defined(CONFIG_RISCV_SOC_INTERRUPT_INIT)
__weak void soc_interrupt_init(void)
{
	/* ensure that all interrupts are disabled */
	(void)arch_irq_lock();

#ifdef CONFIG_RISCV_S_MODE
	csr_write(sie, 0);
#if !defined(CONFIG_64BIT) && defined(CONFIG_RISCV_ISA_EXT_SSAIA)
	csr_write(sieh, 0);
#endif
	/* sip.STIP is read-only from S-mode; clearing sie is sufficient */
#else
	csr_write(mie, 0);
	csr_write(mip, 0);
#if !defined(CONFIG_64BIT) && defined(CONFIG_RISCV_ISA_EXT_SMAIA)
	/* mie/mip are 64 bits in AIA; upper 32 bits are accessed using mieh/miph CSR for RV32 */
	csr_write(mieh, 0);
	csr_write(miph, 0);
#endif
#endif
}
#endif
