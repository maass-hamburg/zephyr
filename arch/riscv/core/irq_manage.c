/*
 * Copyright (c) 2016 Jean-Paul Etienne <fractalclone@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <kernel_internal.h>
#include <zephyr/logging/log.h>
#include <zephyr/arch/riscv/csr.h>
#include <zephyr/irq_multilevel.h>
#include <zephyr/sw_isr_table.h>
#include <zephyr/pm/pm.h>

#ifdef CONFIG_RISCV_HAS_PLIC
#include <zephyr/drivers/interrupt_controller/riscv_plic.h>
#endif
#ifdef CONFIG_RISCV_APLIC_DIRECT
#include <zephyr/drivers/interrupt_controller/riscv_aplic_direct.h>
#endif

LOG_MODULE_DECLARE(os, CONFIG_KERNEL_LOG_LEVEL);

FUNC_NORETURN void z_irq_spurious(const void *unused)
{
#ifdef CONFIG_EMPTY_IRQ_SPURIOUS
	while (1) {
	}

	CODE_UNREACHABLE;
#else
	unsigned long cause;

	ARG_UNUSED(unused);

#ifdef CONFIG_RISCV_S_MODE
	cause = csr_read(scause);
#else
	cause = csr_read(mcause);
#endif

	cause &= CONFIG_RISCV_MCAUSE_EXCEPTION_MASK;

	LOG_ERR("Spurious interrupt detected! IRQ: %ld", cause);
#if defined(CONFIG_RISCV_HAS_PLIC)
	if (cause == RISCV_IRQ_MEXT) {
		unsigned int save_irq = riscv_plic_get_irq();
		const struct device *save_dev = riscv_plic_get_dev();

		LOG_ERR("PLIC interrupt line causing the IRQ: %d (%p)", save_irq, save_dev);
	}
#elif defined(CONFIG_RISCV_APLIC_DIRECT)
	if (cause == RISCV_IRQ_MEXT) {
		unsigned int save_irq = riscv_aplic_get_saved_irq();
		const struct device *save_dev = riscv_aplic_get_saved_dev();

		LOG_ERR("APLIC interrupt line causing the IRQ: %d (%p)", save_irq, save_dev);
	}
#endif
	z_riscv_fatal_error(K_ERR_SPURIOUS_IRQ, NULL);
	CODE_UNREACHABLE;
#endif /* CONFIG_EMPTY_IRQ_SPURIOUS */
}

#ifdef CONFIG_DYNAMIC_INTERRUPTS
int arch_irq_connect_dynamic(unsigned int irq, unsigned int priority,
			     void (*routine)(const void *parameter),
			     const void *parameter, uint32_t flags)
{
	z_isr_install(irq + CONFIG_RISCV_RESERVED_IRQ_ISR_TABLES_OFFSET, routine, parameter);

#if defined(CONFIG_RISCV_HAS_EXT_IRQ_CONTROLLER) || defined(CONFIG_RISCV_HAS_CLIC)
	z_riscv_irq_priority_set(irq, priority, flags);
#else
	ARG_UNUSED(flags);
	ARG_UNUSED(priority);
#endif
	return irq;
}

#ifdef CONFIG_SHARED_INTERRUPTS
int arch_irq_disconnect_dynamic(unsigned int irq, unsigned int priority,
				void (*routine)(const void *parameter), const void *parameter,
				uint32_t flags)
{
	ARG_UNUSED(priority);
	ARG_UNUSED(flags);

	return z_isr_uninstall(irq + CONFIG_RISCV_RESERVED_IRQ_ISR_TABLES_OFFSET, routine,
			       parameter);
}
#endif /* CONFIG_SHARED_INTERRUPTS */
#endif /* CONFIG_DYNAMIC_INTERRUPTS */

#ifdef CONFIG_PM
void arch_isr_direct_pm(void)
{
	unsigned int key;

	key = irq_lock();

	if (_kernel.idle) {
		_kernel.idle = 0;
		pm_system_resume();
	}

	irq_unlock(key);
}
#endif

#ifdef CONFIG_RISCV_SOC_INTERRUPT_INIT
/*
 * Default SoC interrupt initialization, for SoCs that have nothing to do
 * beyond what the architecture provides.
 */
__weak void soc_interrupt_init(void)
{
	/* ensure that all interrupts are disabled */
	(void)arch_irq_lock();

	/*
	 * A CLIC holds the interrupt enables and pending bits itself, the mie
	 * and mip CSRs are not in use then.
	 */
	if (IS_ENABLED(CONFIG_RISCV_HAS_CLIC)) {
		return;
	}

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
#endif /* CONFIG_RISCV_SOC_INTERRUPT_INIT */
