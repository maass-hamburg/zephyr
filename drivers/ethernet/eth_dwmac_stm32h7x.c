/*
 * Driver for Synopsys DesignWare MAC
 *
 * Copyright (c) 2021 BayLibre SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * STM32 platform-specific glue for the Synopsys DesignWare Ethernet MAC
 * (DWMAC / DWC EthernetQoS) driver.
 *
 * Supported STM32 SoC series (all integrate DWC EthernetQoS IP v4.x/v5.x):
 *   - STM32H5X  (H563/H573)              - SBS ETH MII/RMII selection
 *   - STM32H7X  (H743/H753/H735/H750)    - SYSCFG ETH MII/RMII selection
 *   - STM32H7RSX (H7S7/H7R7)             - SBS ETH PHYSEL MII/RMII selection
 *   - STM32MP13X (MP135)                 - SYSCFG ETH MII/RMII selection
 *   - STM32N6X  (N657)                   - SBS ETH MII/RMII selection
 *
 * Not yet supported (older Synopsys MAC IP, requires core driver porting):
 *   - STM32F1X, STM32F2X, STM32F4X, STM32F7X
 */

#define LOG_MODULE_NAME dwmac_plat
#define LOG_LEVEL CONFIG_ETHERNET_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

/* be compatible with the HAL-based driver here */
#define DT_DRV_COMPAT st_stm32_ethernet

#include <sys/types.h>
#include <zephyr/kernel.h>
#include <zephyr/net/ethernet.h>
#include <ethernet/eth.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/sys/crc.h>
#include <zephyr/irq.h>
#include <stm32_ll_system.h>

#include "eth_dwmac_priv.h"

#define ST_OUI_B0 0x00
#define ST_OUI_B1 0x80
#define ST_OUI_B2 0xE1

/*
 * STM32-specific per-instance configuration.
 *
 * Extends the generic dwmac_config by embedding it as the first member so
 * that a pointer to this struct is safely cast-compatible with a pointer to
 * dwmac_config (the type stored in dev->config by the core driver).
 */
struct dwmac_stm32_config {
	struct dwmac_config     common;     /* MUST be first */
	const struct pinctrl_dev_config *pcfg;
	const struct stm32_pclken       *pclken;
	size_t                           pclken_cnt;
	const struct net_eth_mac_config *mac_cfg;
	uint32_t                         phy_mode; /* series-specific LL constant */
	bool                             is_eth2;  /* true for STM32MP13 ETH2 instance */
	void (*irq_config_fn)(void);
	struct dwmac_dma_desc           *tx_descs; /* points to __desc_mem array */
	struct dwmac_dma_desc           *rx_descs;
};

static inline const struct dwmac_stm32_config *get_stm32_config(const struct device *dev)
{
	return (const struct dwmac_stm32_config *)dev->config;
}

/*
 * Per-series PHY interface mode configuration.
 *
 * stm32_configure_phy_mode() is called at init time with the phy_mode value
 * stored in the per-instance config.  The series is selected at compile time;
 * the MII/RMII/RGMII constant comes from the DT phy-connection-type property
 * and is resolved per DT instance at build time via STM32_PHY_MODE_FOR(n).
 *
 * On STM32MP13, which has two independent Ethernet MACs (ETH1 and ETH2), each
 * MAC has its own SYSCFG PHY mode selector.  The is_eth2 parameter selects
 * between LL_SYSCFG_SetPHYInterface() (ETH1) and LL_SYSCFG_SetPHYInterface2()
 * (ETH2) accordingly.  For all other series the parameter is unused.
 */
static inline void stm32_configure_phy_mode(uint32_t phy_mode, bool is_eth2)
{
#if defined(CONFIG_SOC_SERIES_STM32H5X)
	ARG_UNUSED(is_eth2);
	/*
	 * STM32H5: SBS block, LL_SBS_SetPHYInterface().
	 * Constants: LL_SBS_ETH_MII / LL_SBS_ETH_RMII
	 */
	__HAL_RCC_SBS_CLK_ENABLE();
	LL_SBS_SetPHYInterface(phy_mode);
#elif defined(CONFIG_SOC_SERIES_STM32H7X)
	ARG_UNUSED(is_eth2);
	/*
	 * STM32H7: SYSCFG_PMCR register, LL_SYSCFG_SetPHYInterface().
	 * Constants: LL_SYSCFG_ETH_MII / LL_SYSCFG_ETH_RMII
	 */
	__HAL_RCC_SYSCFG_CLK_ENABLE();
	LL_SYSCFG_SetPHYInterface(phy_mode);
#elif defined(CONFIG_SOC_SERIES_STM32MP13X)
	/*
	 * STM32MP13: SYSCFG_PMCSETR register, two independent selectors.
	 *   ETH1: LL_SYSCFG_SetPHYInterface()   (bits [23:21])
	 *   ETH2: LL_SYSCFG_SetPHYInterface2()  (bits [26:24])
	 * Constants per instance: LL_SYSCFG_ETH_{MII,RGMII,RMII} (ETH1)
	 *                      or LL_SYSCFG_ETH2_{MII,RGMII,RMII} (ETH2)
	 */
	__HAL_RCC_SYSCFG_CLK_ENABLE();
	if (is_eth2) {
		LL_SYSCFG_SetPHYInterface2(phy_mode);
	} else {
		LL_SYSCFG_SetPHYInterface(phy_mode);
	}
#elif defined(CONFIG_SOC_SERIES_STM32H7RSX)
	ARG_UNUSED(is_eth2);
	/*
	 * STM32H7RS: SBS block, LL_SBS_SetEthernetPhy().
	 * Constants: LL_SBS_ETH_PHYSEL_GMII_MII / LL_SBS_ETH_PHYSEL_RMII
	 */
	__HAL_RCC_SBS_CLK_ENABLE();
	LL_SBS_SetEthernetPhy(phy_mode);
#elif defined(CONFIG_SOC_SERIES_STM32N6X)
	ARG_UNUSED(is_eth2);
	/*
	 * STM32N6: SBS block, LL_SBS_SetPHYInterface() — same as STM32H5.
	 * Constants: LL_SBS_ETH_MII / LL_SBS_ETH_RMII
	 */
	__HAL_RCC_SBS_CLK_ENABLE();
	LL_SBS_SetPHYInterface(phy_mode);
#else
#error "Unsupported STM32 SoC series for DWMAC driver"
#endif
}

/*
 * STM32_PHY_MODE_FOR(n) — compile-time LL constant for DT instance n.
 *
 * Resolves the phy-connection-type DT property of instance n to the
 * series-appropriate LL constant.
 *
 * For STM32MP13 the SYSCFG_PMCSETR register has two independent 3-bit fields:
 *   bits [23:21] for ETH1 (LL_SYSCFG_ETH_{MII,RGMII,RMII})
 *   bits [26:24] for ETH2 (LL_SYSCFG_ETH2_{MII,RGMII,RMII})
 * ETH2 is identified at build time by its base address (0x5800_E000).
 *
 * For all other series only MII and RMII are supported (RGMII connections
 * are not available on H5/H7/H7RS/N6 hardware).
 */
#if defined(CONFIG_SOC_SERIES_STM32H5X) || \
    defined(CONFIG_SOC_SERIES_STM32N6X)
#define STM32_PHY_MODE_FOR(n) \
	(DT_INST_ENUM_HAS_VALUE(n, phy_connection_type, mii) \
		? LL_SBS_ETH_MII \
		: LL_SBS_ETH_RMII)
#elif defined(CONFIG_SOC_SERIES_STM32H7X)
#define STM32_PHY_MODE_FOR(n) \
	(DT_INST_ENUM_HAS_VALUE(n, phy_connection_type, mii) \
		? LL_SYSCFG_ETH_MII \
		: LL_SYSCFG_ETH_RMII)
#elif defined(CONFIG_SOC_SERIES_STM32MP13X)
#define STM32_PHY_MODE_FOR(n) \
	(STM32MP13_IS_ETH2(n) \
		? (DT_INST_ENUM_HAS_VALUE(n, phy_connection_type, mii)   ? LL_SYSCFG_ETH2_MII   : \
		   DT_INST_ENUM_HAS_VALUE(n, phy_connection_type, rgmii) ? LL_SYSCFG_ETH2_RGMII : \
		                                                            LL_SYSCFG_ETH2_RMII)  \
		: (DT_INST_ENUM_HAS_VALUE(n, phy_connection_type, mii)   ? LL_SYSCFG_ETH_MII    : \
		   DT_INST_ENUM_HAS_VALUE(n, phy_connection_type, rgmii) ? LL_SYSCFG_ETH_RGMII  : \
		                                                            LL_SYSCFG_ETH_RMII))
#elif defined(CONFIG_SOC_SERIES_STM32H7RSX)
#define STM32_PHY_MODE_FOR(n) \
	(DT_INST_ENUM_HAS_VALUE(n, phy_connection_type, mii) \
		? LL_SBS_ETH_PHYSEL_GMII_MII \
		: LL_SBS_ETH_PHYSEL_RMII)
#endif /* series selection for STM32_PHY_MODE_FOR */

/*
 * STM32MP13_IS_ETH2(n): resolve to a compile-time constant indicating whether
 * DT instance n is the ETH2 peripheral (base address 0x5800_E000).
 * The STM32MP13 Reference Manual (RM0475) Table 2 assigns:
 *   ETH1 MAC: 0x5800_A000 — matches the eth0 node in stm32mp13.dtsi
 *   ETH2 MAC: 0x5800_E000 — matches the eth2 node in stm32mp13.dtsi
 *
 * On non-STM32MP13 series this always evaluates to 0 so the compiler folds
 * away the dead is_eth2 branch and the macro has no observable effect.
 */
#if defined(CONFIG_SOC_SERIES_STM32MP13X)
#define STM32MP13_IS_ETH2(n)  (DT_INST_REG_ADDR(n) == 0x5800e000U)
#else
#define STM32MP13_IS_ETH2(n)  0
#endif

int dwmac_bus_init(const struct device *dev)
{
	const struct dwmac_stm32_config *cfg = get_stm32_config(dev);
	int ret;

	for (size_t i = 0; i < cfg->pclken_cnt; i++) {
		if (IN_RANGE(cfg->pclken[i].bus,
			     STM32_PERIPH_BUS_MIN, STM32_PERIPH_BUS_MAX)) {
			ret = clock_control_on(cfg->common.clock,
					       (clock_control_subsys_t)&cfg->pclken[i]);
		} else {
			ret = clock_control_configure(cfg->common.clock,
						      (clock_control_subsys_t)&cfg->pclken[i],
						      NULL);
		}

		if (ret != 0) {
			LOG_ERR("Failed to setup ethernet clock #%zu", i);
			return -EIO;
		}
	}

	ret = pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		LOG_ERR("Could not configure ethernet pins");
		return ret;
	}

	stm32_configure_phy_mode(cfg->phy_mode, cfg->is_eth2);

	return 0;
}

#if defined(CONFIG_NOCACHE_MEMORY)
#define __desc_mem __nocache __aligned(4)
#else
#define __desc_mem __aligned(4)
#endif

int dwmac_platform_init(const struct device *dev)
{
	const struct dwmac_stm32_config *cfg = get_stm32_config(dev);
	struct dwmac_priv *p = dev->data;
	int ret;

	p->tx_descs = cfg->tx_descs;
	p->rx_descs = cfg->rx_descs;

	/* basic configuration for this platform */
	REG_WRITE(MAC_CONF,
		  MAC_CONF_PS |
		  MAC_CONF_FES |
		  MAC_CONF_DM);
	REG_WRITE(DMA_SYSBUS_MODE,
		  DMA_SYSBUS_MODE_AAL |
		  DMA_SYSBUS_MODE_FB);

	/* set up IRQs (still masked for now) */
	cfg->irq_config_fn();

	/* retrieve MAC address */
	ret = net_eth_mac_load(cfg->mac_cfg, p->mac_addr);
	if (ret == -ENODATA) {
		uint8_t unique_device_ID_12_bytes[12];
		uint32_t result_mac_32_bits;

		/**
		 * Set MAC address locally administered bit (LAA) as this is not assigned by the
		 * manufacturer
		 */
		p->mac_addr[0] = ST_OUI_B0 | 0x02;
		p->mac_addr[1] = ST_OUI_B1;
		p->mac_addr[2] = ST_OUI_B2;

		/* Nothing defined by the user, use device id */
		hwinfo_get_device_id(unique_device_ID_12_bytes, 12);
		result_mac_32_bits = crc32_ieee((uint8_t *)unique_device_ID_12_bytes, 12);
		memcpy(&p->mac_addr[3], &result_mac_32_bits, 3);

		ret = 0;
	}

	if (ret < 0) {
		LOG_ERR("Failed to load MAC address (%d)", ret);
		return ret;
	}

	return 0;
}

/*
 * STM32_DWMAC_ASSERT_PHY_MODE(n) — compile-time validation of phy-connection-type.
 *
 * STM32MP13 supports MII, RMII and RGMII; all other series support only MII
 * and RMII (RGMII PHY connections are not available on H5/H7/H7RS/N6).
 */
#if defined(CONFIG_SOC_SERIES_STM32MP13X)
#define STM32_DWMAC_ASSERT_PHY_MODE(n) \
	BUILD_ASSERT( \
		DT_INST_ENUM_HAS_VALUE(n, phy_connection_type, mii)  || \
		DT_INST_ENUM_HAS_VALUE(n, phy_connection_type, rmii) || \
		DT_INST_ENUM_HAS_VALUE(n, phy_connection_type, rgmii), \
		"STM32 DWMAC[" #n "]: phy-connection-type must be \"mii\", \"rmii\", or \"rgmii\"")
#else
#define STM32_DWMAC_ASSERT_PHY_MODE(n) \
	BUILD_ASSERT( \
		DT_INST_ENUM_HAS_VALUE(n, phy_connection_type, mii) || \
		DT_INST_ENUM_HAS_VALUE(n, phy_connection_type, rmii), \
		"STM32 DWMAC[" #n "]: phy-connection-type must be \"mii\" or \"rmii\"")
#endif

/*
 * DWMAC_STM32_DEVICE(n) — per-instance boilerplate.
 *
 * Generates all per-instance state (pinctrl, clocks, MAC config, DMA
 * descriptor rings, IRQ handler, config struct, private data struct, and
 * device registration) for DT instance n.
 *
 * The IRQ config function is forward-declared before the device so the
 * function pointer can be stored in the config struct.  The body is defined
 * after ETH_NET_DEVICE_DT_INST_DEFINE so that DEVICE_DT_INST_GET(n) refers
 * to an already-declared device object.
 */
#define DWMAC_STM32_DEVICE(n)                                                              \
	STM32_DWMAC_ASSERT_PHY_MODE(n);                                                    \
	                                                                                   \
	/* forward declaration so the function pointer can be stored in the config */      \
	static void dwmac_stm32_irq_config_##n(void);                                      \
	                                                                                   \
	PINCTRL_DT_INST_DEFINE(n);                                                         \
	                                                                                   \
	static const struct stm32_pclken pclken_##n[] = STM32_DT_INST_CLOCKS(n);          \
	static const struct net_eth_mac_config mac_cfg_##n =                               \
		NET_ETH_MAC_DT_INST_CONFIG_INIT(n);                                        \
	static struct dwmac_dma_desc tx_descs_##n[NB_TX_DESCS] __desc_mem;                \
	static struct dwmac_dma_desc rx_descs_##n[NB_RX_DESCS] __desc_mem;                \
	                                                                                   \
	static const struct dwmac_stm32_config dwmac_stm32_cfg_##n = {                    \
		.common = {                                                                \
			DEVICE_MMIO_ROM_INIT(DT_DRV_INST(n)),                              \
			.phy_dev = DEVICE_DT_GET_OR_NULL(DT_INST_PHANDLE(n, phy_handle)), \
			.clock   = DEVICE_DT_GET(STM32_CLOCK_CONTROL_NODE),               \
			.mac_clk = (clock_control_subsys_t)&pclken_##n[0],                \
		},                                                                         \
		.pcfg          = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                        \
		.pclken        = pclken_##n,                                               \
		.pclken_cnt    = DT_INST_NUM_CLOCKS(n),                                    \
		.mac_cfg       = &mac_cfg_##n,                                             \
		.phy_mode      = STM32_PHY_MODE_FOR(n),                                    \
		.is_eth2       = IS_ENABLED(CONFIG_SOC_SERIES_STM32MP13X) &&               \
			         STM32MP13_IS_ETH2(n),                                     \
		.irq_config_fn = dwmac_stm32_irq_config_##n,                               \
		.tx_descs      = tx_descs_##n,                                             \
		.rx_descs      = rx_descs_##n,                                             \
	};                                                                                 \
	                                                                                   \
	static struct dwmac_priv dwmac_priv_##n;                                           \
	                                                                                   \
	ETH_NET_DEVICE_DT_INST_DEFINE(n,                                                   \
				      dwmac_probe,                                         \
				      NULL,                                                \
				      &dwmac_priv_##n,                                     \
				      &dwmac_stm32_cfg_##n.common,                         \
				      CONFIG_ETH_INIT_PRIORITY,                            \
				      &dwmac_api,                                          \
				      NET_ETH_MTU);                                        \
	                                                                                   \
	/* defined after ETH_NET_DEVICE_DT_INST_DEFINE so DEVICE_DT_INST_GET(n) is valid */\
	static void dwmac_stm32_irq_config_##n(void)                                       \
	{                                                                                  \
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority),                    \
			    dwmac_isr, DEVICE_DT_INST_GET(n), 0);                          \
		irq_enable(DT_INST_IRQN(n));                                               \
	}

DT_INST_FOREACH_STATUS_OKAY(DWMAC_STM32_DEVICE)
