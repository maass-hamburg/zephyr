# Copyright (c) 2024 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

# A CLIC takes over the reset vector and the interrupt management of the SoC,
# and the CLIC drivers provide both, so the privileged layer is not used then.
if(CONFIG_RISCV_SOC_COMMON_PRIVILEGED AND NOT CONFIG_RISCV_HAS_CLIC)
  add_subdirectory(riscv-privileged)
endif()
