/*
 * Copyright (c) 2024, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <hal/nrf_vpr_clic.h>

void arch_irq_enable(unsigned int irq)
{
	nrf_vpr_clic_int_enable_set(NRF_VPRCLIC, irq, true);
}

void arch_irq_disable(unsigned int irq)
{
	nrf_vpr_clic_int_enable_set(NRF_VPRCLIC, irq, false);
}

int arch_irq_is_enabled(unsigned int irq)
{
	return nrf_vpr_clic_int_enable_check(NRF_VPRCLIC, irq);
}

void z_riscv_irq_priority_set(unsigned int irq, unsigned int pri, uint32_t flags)
{
	nrf_vpr_clic_int_priority_set(NRF_VPRCLIC, irq, NRF_VPR_CLIC_INT_TO_PRIO(pri));
}

void riscv_clic_irq_set_pending(uint32_t irq)
{
	nrf_vpr_clic_int_pending_set(NRF_VPRCLIC, irq);
}
