/*
 * PTP clock driver for Synopsys DesignWare MAC
 *
 * Copyright (c) 2026 Zephyr
 * SPDX-License-Identifier: Apache-2.0
 *
 * This driver provides PTP clock support for Synopsys DesignWare MAC
 * controllers (dwmac) found in various SoCs.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/ptp_clock.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/device_mmio.h>

#include <stdbool.h>

LOG_MODULE_REGISTER(ptp_clock_dwmac, CONFIG_ETHERNET_LOG_LEVEL);

#define DT_DRV_COMPAT snps_dwmac_ptp_clock

/*
 * DW MAC PTP Timestamp Register Offsets
 * These are based on the DesignWare Cores Ethernet Quality-of-Service Databook
 */

/* MAC Timestamp Control Register (offset 0x0B00 for most implementations) */
#define MAC_TIMESTAMP_CTRL		0x0B00
#define MAC_TIMESTAMP_CTRL_TSENA	BIT(0)	/* Timestamp Enable */
#define MAC_TIMESTAMP_CTRL_TSCFUPDT	BIT(1)	/* Timestamp Fine/Coarse Update */
#define MAC_TIMESTAMP_CTRL_TSINIT	BIT(2)	/* Timestamp Initialize */
#define MAC_TIMESTAMP_CTRL_TSUPDT	BIT(3)	/* Timestamp Update */
#define MAC_TIMESTAMP_CTRL_TSADDREG	BIT(5)	/* Timestamp Addend Register Update */
#define MAC_TIMESTAMP_CTRL_TSENALL	BIT(8)	/* Timestamp Enable for All Frames */
#define MAC_TIMESTAMP_CTRL_TSEVNTENA	BIT(9)	/* Timestamp Event Enable */
#define MAC_TIMESTAMP_CTRL_TSIPV4ENA	BIT(10)	/* Timestamp IPv4 Enable */
#define MAC_TIMESTAMP_CTRL_TSIPV6ENA	BIT(11)	/* Timestamp IPv6 Enable */

/* MAC Subsecond Increment Register (offset 0x0B04) */
#define MAC_SUB_SECOND_INCREMENT	0x0B04
#define MAC_SUB_SECOND_INCREMENT_SSINC_MASK	GENMASK(7, 0)

/* MAC System Time Seconds Register (offset 0x0B08) */
#define MAC_SYSTEM_TIME_SECONDS		0x0B08

/* MAC System Time Nanoseconds Register (offset 0x0B0C) */
#define MAC_SYSTEM_TIME_NANOSECONDS	0x0B0C
#define MAC_SYSTEM_TIME_NANOSECONDS_TSSS_MASK	GENMASK(30, 0)
#define MAC_SYSTEM_TIME_NANOSECONDS_ADDSUB	BIT(31)

/* MAC System Time Seconds Update Register (offset 0x0B10) */
#define MAC_SYSTEM_TIME_SECONDS_UPDATE	0x0B10

/* MAC System Time Nanoseconds Update Register (offset 0x0B14) */
#define MAC_SYSTEM_TIME_NANOSECONDS_UPDATE	0x0B14
#define MAC_SYSTEM_TIME_NANOSECONDS_UPDATE_TSSS_MASK	GENMASK(30, 0)
#define MAC_SYSTEM_TIME_NANOSECONDS_UPDATE_ADDSUB	BIT(31)

/* MAC Timestamp Addend Register (offset 0x0B18) */
#define MAC_TIMESTAMP_ADDEND		0x0B18

/* MAC Timestamp Status Register (offset 0x0B1C) */
#define MAC_TIMESTAMP_STATUS		0x0B1C

/* Default PTP clock frequency (50 MHz) */
#define PTP_CLOCK_DWMAC_PTPCLK_HZ	50000000U

/* PTP context structure */
struct ptp_context_dwmac {
	DEVICE_MMIO_ROM;
};

struct ptp_data_dwmac {
	DEVICE_MMIO_RAM;
	struct k_mutex ptp_mutex;
	uint32_t nominal_addend;
	uint32_t ref_clk_hz;
};

static int ptp_clock_dwmac_set(const struct device *dev, struct net_ptp_time *tm)
{
	struct ptp_data_dwmac *data = dev->data;
	uintptr_t base = DEVICE_MMIO_GET(dev);

	LOG_DBG("PTP set time: %u s, %u ns", (unsigned int)tm->second, tm->nanosecond);

	k_mutex_lock(&data->ptp_mutex, K_FOREVER);

	/* Set the seconds value */
	sys_write32(tm->second, base + MAC_SYSTEM_TIME_SECONDS_UPDATE);

	/* Set the nanoseconds value (clear ADDSUB for absolute load) */
	sys_write32(tm->nanosecond & MAC_SYSTEM_TIME_NANOSECONDS_UPDATE_TSSS_MASK,
		    base + MAC_SYSTEM_TIME_NANOSECONDS_UPDATE);

	/* Trigger the update with TSINIT (initialize/load command) */
	uint32_t ctrl = sys_read32(base + MAC_TIMESTAMP_CTRL);
	sys_write32(ctrl | MAC_TIMESTAMP_CTRL_TSINIT, base + MAC_TIMESTAMP_CTRL);

	/* Wait for the operation to complete */
	while (sys_read32(base + MAC_TIMESTAMP_CTRL) & MAC_TIMESTAMP_CTRL_TSINIT) {
		/* Spin lock until TSINIT self-clears */
	}

	k_mutex_unlock(&data->ptp_mutex);

	return 0;
}

static int ptp_clock_dwmac_get(const struct device *dev, struct net_ptp_time *tm)
{
	uintptr_t base = DEVICE_MMIO_GET(dev);
	uint32_t ns1, ns2, sec;

	/*
	 * Guard against a seconds roll-over between the two nanosecond reads:
	 * re-read if nanoseconds decreased (wrap occurred).
	 */
	do {
		ns1 = sys_read32(base + MAC_SYSTEM_TIME_NANOSECONDS) &
		      MAC_SYSTEM_TIME_NANOSECONDS_TSSS_MASK;
		sec = sys_read32(base + MAC_SYSTEM_TIME_SECONDS);
		ns2 = sys_read32(base + MAC_SYSTEM_TIME_NANOSECONDS) &
		      MAC_SYSTEM_TIME_NANOSECONDS_TSSS_MASK;
	} while (ns2 < ns1);

	tm->second = sec;
	tm->nanosecond = ns2;

	LOG_DBG("PTP get time: %u s, %u ns", (unsigned int)tm->second, tm->nanosecond);

	return 0;
}

static int ptp_clock_dwmac_adjust(const struct device *dev, int increment)
{
	struct ptp_data_dwmac *data = dev->data;
	uintptr_t base = DEVICE_MMIO_GET(dev);
	uint32_t ns_update;

	LOG_DBG("PTP adjust increment: %d", increment);

	if ((increment <= (-(int32_t)NSEC_PER_SEC)) || (increment >= (int32_t)NSEC_PER_SEC)) {
		return -EINVAL;
	}

	k_mutex_lock(&data->ptp_mutex, K_FOREVER);

	/* Clear seconds update register */
	sys_write32(0, base + MAC_SYSTEM_TIME_SECONDS_UPDATE);

	if (increment >= 0) {
		/* ADDSUB = 0: add */
		ns_update = (uint32_t)increment & MAC_SYSTEM_TIME_NANOSECONDS_UPDATE_TSSS_MASK;
	} else {
		/* ADDSUB = 1: subtract */
		ns_update = ((uint32_t)(-increment) &
			     MAC_SYSTEM_TIME_NANOSECONDS_UPDATE_TSSS_MASK) |
			    MAC_SYSTEM_TIME_NANOSECONDS_UPDATE_ADDSUB;
	}

	sys_write32(ns_update, base + MAC_SYSTEM_TIME_NANOSECONDS_UPDATE);

	/* Trigger the update with TSUPDT */
	uint32_t ctrl = sys_read32(base + MAC_TIMESTAMP_CTRL);
	sys_write32(ctrl | MAC_TIMESTAMP_CTRL_TSUPDT, base + MAC_TIMESTAMP_CTRL);

	/* Wait for the operation to complete */
	while (sys_read32(base + MAC_TIMESTAMP_CTRL) & MAC_TIMESTAMP_CTRL_TSUPDT) {
		/* Spin lock until TSUPDT self-clears */
	}

	k_mutex_unlock(&data->ptp_mutex);

	return 0;
}

static int ptp_clock_dwmac_rate_adjust(const struct device *dev, double ratio)
{
	struct ptp_data_dwmac *data = dev->data;
	uintptr_t base = DEVICE_MMIO_GET(dev);
	uint32_t new_addend;

	LOG_DBG("PTP rate adjust ratio: %f", ratio);

	/* No meaningful change */
	if ((ratio > 1.0 && ratio - 1.0 < 1e-9) || (ratio < 1.0 && 1.0 - ratio < 1e-9)) {
		return 0;
	}

	new_addend = (uint32_t)((double)data->nominal_addend * ratio);

	k_mutex_lock(&data->ptp_mutex, K_FOREVER);

	/* Write the new addend value */
	sys_write32(new_addend, base + MAC_TIMESTAMP_ADDEND);

	/* Trigger the addend update */
	uint32_t ctrl = sys_read32(base + MAC_TIMESTAMP_CTRL);
	sys_write32(ctrl | MAC_TIMESTAMP_CTRL_TSADDREG, base + MAC_TIMESTAMP_CTRL);

	/* Wait for the operation to complete */
	while (sys_read32(base + MAC_TIMESTAMP_CTRL) & MAC_TIMESTAMP_CTRL_TSADDREG) {
		/* Spin lock until TSADDREG self-clears */
	}

	k_mutex_unlock(&data->ptp_mutex);

	return 0;
}

static int ptp_clock_dwmac_init(const struct device *dev)
{
	LOG_INF("Initializing DW MAC PTP clock on device %s", dev->name);

	struct ptp_data_dwmac *data = dev->data;
	uintptr_t base = DEVICE_MMIO_GET(dev);
	const uint32_t snsinc = NSEC_PER_SEC / PTP_CLOCK_DWMAC_PTPCLK_HZ;

	k_mutex_init(&data->ptp_mutex);

	/* For now, assume a fixed reference clock frequency */
	data->ref_clk_hz = PTP_CLOCK_DWMAC_PTPCLK_HZ;
	LOG_INF("PTP reference clock %u Hz", data->ref_clk_hz);

	/*
	 * Calculate the nominal addend for the accumulator.
	 * addend = 2^32 * ptp_clk / ref_clk
	 */
	data->nominal_addend =
		(uint32_t)((double)(1ULL << 32) * (double)PTP_CLOCK_DWMAC_PTPCLK_HZ /
			   (double)data->ref_clk_hz);
	LOG_INF("PTP accumulator addend %u", data->nominal_addend);

	LOG_INF("Starting DW MAC PTP hardware");

	/*
	 * Step 1: Enable timestamping in coarse update mode (no TSCFUPDT yet).
	 */
	uint32_t ctrl = MAC_TIMESTAMP_CTRL_TSENA |		/* Enable timestamp */
			MAC_TIMESTAMP_CTRL_TSENALL |		/* Timestamp all frames */
			MAC_TIMESTAMP_CTRL_TSEVNTENA |		/* Enable timestamp event */
			MAC_TIMESTAMP_CTRL_TSIPV4ENA |		/* Enable IPv4 timestamp */
			MAC_TIMESTAMP_CTRL_TSIPV6ENA;		/* Enable IPv6 timestamp */
	sys_write32(ctrl, base + MAC_TIMESTAMP_CTRL);

	/*
	 * Step 2: Set sub-second increment (nanoseconds per clock tick).
	 */
	sys_write32(snsinc & MAC_SUB_SECOND_INCREMENT_SSINC_MASK,
		    base + MAC_SUB_SECOND_INCREMENT);

	/*
	 * Step 3: Initialize system time to zero
	 */
	sys_write32(0, base + MAC_SYSTEM_TIME_SECONDS_UPDATE);
	sys_write32(0, base + MAC_SYSTEM_TIME_NANOSECONDS_UPDATE);

	ctrl = sys_read32(base + MAC_TIMESTAMP_CTRL);
	sys_write32(ctrl | MAC_TIMESTAMP_CTRL_TSINIT, base + MAC_TIMESTAMP_CTRL);

	/* Wait for initialization to complete */
	while (sys_read32(base + MAC_TIMESTAMP_CTRL) & MAC_TIMESTAMP_CTRL_TSINIT) {
		/* Spin lock */
	}

	/*
	 * Step 4: Switch to fine update mode
	 */
	ctrl = sys_read32(base + MAC_TIMESTAMP_CTRL);
	sys_write32(ctrl | MAC_TIMESTAMP_CTRL_TSCFUPDT, base + MAC_TIMESTAMP_CTRL);

	/*
	 * Step 5: Load the nominal addend into the fine accumulator
	 */
	sys_write32(data->nominal_addend, base + MAC_TIMESTAMP_ADDEND);

	ctrl = sys_read32(base + MAC_TIMESTAMP_CTRL);
	sys_write32(ctrl | MAC_TIMESTAMP_CTRL_TSADDREG, base + MAC_TIMESTAMP_CTRL);

	/* Wait for addend load to complete */
	while (sys_read32(base + MAC_TIMESTAMP_CTRL) & MAC_TIMESTAMP_CTRL_TSADDREG) {
		/* Spin lock */
	}

	LOG_INF("DW MAC PTP clock initialized successfully");

	return 0;
}

static const struct ptp_clock_driver_api ptp_clock_dwmac_api = {
	.set = ptp_clock_dwmac_set,
	.get = ptp_clock_dwmac_get,
	.adjust = ptp_clock_dwmac_adjust,
	.rate_adjust = ptp_clock_dwmac_rate_adjust,
};

#define PTP_CLOCK_DWMAC_INIT(n)							\
	static const struct ptp_context_dwmac ptp_clock_dwmac_##n##_config = { \
		DEVICE_MMIO_ROM_INIT(DT_INST_PARENT(n)),		\
	};								\
								\
	static struct ptp_data_dwmac ptp_clock_dwmac_##n##_data;	\
								\
	DEVICE_DT_INST_DEFINE(n, ptp_clock_dwmac_init, NULL,	\
			      &ptp_clock_dwmac_##n##_data,		\
			      &ptp_clock_dwmac_##n##_config,		\
			      POST_KERNEL,				\
			      CONFIG_PTP_CLOCK_INIT_PRIORITY,		\
			      &ptp_clock_dwmac_api);

DT_INST_FOREACH_STATUS_OKAY(PTP_CLOCK_DWMAC_INIT)
