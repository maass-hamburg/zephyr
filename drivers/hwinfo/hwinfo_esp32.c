/*
 * Copyright (c) 2019 Leandro A. F. Pereira
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc/efuse_reg.h>

#include <drivers/hwinfo.h>
#include <string.h>

ssize_t z_impl_hwinfo_get_device_id(u8_t *buffer, size_t length)
{
	u32_t rdata1 = sys_read32(EFUSE_BLK0_RDATA1_REG);
	u32_t rdata2 = sys_read32(EFUSE_BLK0_RDATA2_REG);
	u8_t id[6];

	/* The first word provides the lower 32 bits of the MAC
	 * address; the low 16 bits of the second word provide the
	 * upper 16 bytes of the MAC address.  The upper 16 bits are
	 * (apparently) a checksum, and reserved.  See ESP32 Technical
	 * Reference Manual V4.1 section 20.5.
	 */
	id[0] = (u8_t)(rdata2 >> 8);
	id[1] = (u8_t)(rdata2 >> 0);
	id[2] = (u8_t)(rdata1 >> 24);
	id[3] = (u8_t)(rdata1 >> 16);
	id[4] = (u8_t)(rdata1 >> 8);
	id[5] = (u8_t)(rdata1 >> 0);

	if (length > sizeof(id)) {
		length = sizeof(id);
	}
	memcpy(buffer, id, length);

	return length;
}
