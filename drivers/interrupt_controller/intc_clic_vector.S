/*
 * Copyright (c) 2017 Jean-Paul Etienne <fractalclone@gmail.com>
 * Contributors: 2018 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Reset vector of a SoC whose interrupts are handled by a CLIC, the counterpart
 * of soc/common/riscv-privileged/vector.S for CLINT mode.
 *
 * The CLIC mode of the trap vector is defined by the CLIC specification rather
 * than by any one CLIC, so this is shared by every CLIC implementation,
 * including SoC level ones without a driver.
 */

#include <zephyr/toolchain.h>

#ifndef CSR_MTVT
#define CSR_MTVT 0x307
#endif

/* exports */
GTEXT(__start)

/* imports */
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
	 * CLIC vectored mode
	 *
	 * CLIC vectored mode uses mtvec exclusively for exception handling and
	 * mtvec.base must be aligned to 64 bytes (this is done using
	 * CONFIG_RISCV_TRAP_HANDLER_ALIGNMENT)
	 */
#if defined(CONFIG_USE_ISR_WRAPPER)
	la t0, _isr_wrapper
#else
	add t0, zero, zero
#endif
	addi t0, t0, 0x03 /* Enable CLIC vectored mode by setting LSB */
	csrw mtvec, t0

	/*
	 * CLIC vectored mode has a similar concept to CLINT vectored mode,
	 * where an interrupt vector table is used for specific interrupts.
	 * However, in CLIC vectored mode, the handler table contains the
	 * address of the interrupt handler instead of an opcode containing a
	 * jump instruction, this is done by leveraging
	 * CONFIG_IRQ_VECTOR_TABLE_JUMP_BY_ADDRESS.
	 * When an interrupt occurs in CLIC vectored mode, the address of the
	 * handler entry from the vector table is loaded and then jumped to in
	 * hardware. This time mtvt is used as the base address for the
	 * interrupt table.
	 */
	la t0, _irq_vector_table
	csrw CSR_MTVT, t0
#elif !defined(CONFIG_LEGACY_CLIC)
	la t0, _isr_wrapper
	addi t0, t0, 0x03 /* Set mode bits to 3, signifying CLIC. Everything else is reserved. */
	csrw mtvec, t0
#else
	/*
	 * Legacy CLIC, direct mode: the mode bits of mtvec are not used, it is
	 * programmed as in CLINT direct mode.
	 */
	la t0, _isr_wrapper
	csrw mtvec, t0
#endif

	/* Jump to __reset */
	tail __reset
