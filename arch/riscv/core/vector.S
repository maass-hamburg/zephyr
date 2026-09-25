/*
 * Copyright (c) 2017 Jean-Paul Etienne <fractalclone@gmail.com>
 * Contributors: 2018 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/toolchain.h>

/* exports */
GTEXT(__start)

/* imports */
GTEXT(__initialize)
#if defined(CONFIG_USE_ISR_WRAPPER)
GTEXT(_isr_wrapper)
#endif

SECTION_FUNC(vectors, __start)
#if defined(CONFIG_RISCV_GP)
	/* Initialize global pointer */
	.option push
	.option norelax
	la gp, __global_pointer$
	.option pop
#endif

	.option norvc;

#if defined(CONFIG_SOC_RESET_HOOK)
	call soc_reset_hook
#endif

#if defined(CONFIG_RISCV_VECTORED_MODE)
	/*
	 * CLINT vectored mode
	 *
	 * Set mtvec (Machine Trap-Vector Base-Address Register)
	 * to _irq_vector_table (interrupt vector table). Add 1 to base
	 * address of _irq_vector_table to indicate that vectored mode
	 * is used (LSB = 0x1). CPU will mask the LSB out of
	 * the address so that base address of _irq_vector_table is used.
	 *
	 * NOTE: _irq_vector_table is 256-byte aligned. Incorrect alignment
	 *        of _irq_vector_table breaks this code.
	 */
	la t0, _irq_vector_table /* Load address of interrupt vector table */
	addi t0, t0, 0x01 /* Enable vectored mode by setting LSB */
#ifdef CONFIG_RISCV_S_MODE
	csrw stvec, t0
#else
	csrw mtvec, t0
#endif

#else /* !CONFIG_RISCV_VECTORED_MODE */

	/*
	 * CLINT direct mode
	 *
	 * Set mtvec (Machine Trap-Vector Base-Address Register)
	 * to _isr_wrapper.
	 */
	la t0, _isr_wrapper
#ifdef CONFIG_RISCV_S_MODE
	csrw stvec, t0
#else
	csrw mtvec, t0
#endif

#endif /* CONFIG_RISCV_VECTORED_MODE */

	/* Jump to __reset */
	tail __reset
