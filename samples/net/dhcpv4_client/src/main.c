/* Networking DHCPv4 client */

/*
 * Copyright (c) 2017 ARM Ltd.
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_dhcpv4_client_sample, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/linker/sections.h>
#include <errno.h>
#include <stdio.h>

#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>

#define DHCP_OPTION_NTP (42)

static uint8_t ntp_server[4];

static struct net_mgmt_event_callback mgmt_cb;

static struct net_dhcpv4_option_callback dhcp_cb;

static void start_dhcpv4_client(struct net_if *iface, void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_INF("Start on %s: index=%d", net_if_get_device(iface)->name,
		net_if_get_by_iface(iface));
	net_dhcpv4_start(iface);
}

static void handler(struct net_mgmt_event_callback *cb,
		    uint64_t mgmt_event,
		    struct net_if *iface)
{
	int i = 0;

	if (mgmt_event != NET_EVENT_IPV4_ADDR_ADD) {
		return;
	}

	for (i = 0; i < NET_IF_MAX_IPV4_ADDR; i++) {
		char buf[NET_IPV4_ADDR_LEN];

		if (iface->config.ip.ipv4->unicast[i].ipv4.addr_type !=
							NET_ADDR_DHCP) {
			continue;
		}

		LOG_INF("   Address[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(NET_AF_INET,
			    &iface->config.ip.ipv4->unicast[i].ipv4.address.in_addr,
						  buf, sizeof(buf)));
		LOG_INF("    Subnet[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(NET_AF_INET,
				       &iface->config.ip.ipv4->unicast[i].netmask,
				       buf, sizeof(buf)));
		LOG_INF("    Router[%d]: %s", net_if_get_by_iface(iface),
			net_addr_ntop(NET_AF_INET,
						 &iface->config.ip.ipv4->gw,
						 buf, sizeof(buf)));
		LOG_INF("Lease time[%d]: %u seconds", net_if_get_by_iface(iface),
			iface->config.dhcpv4.lease_time);
	}
}

static void option_handler(struct net_dhcpv4_option_callback *cb,
			   size_t length,
			   enum net_dhcpv4_msg_type msg_type,
			   struct net_if *iface)
{
	char buf[NET_IPV4_ADDR_LEN];

	LOG_INF("DHCP Option %d: %s", cb->option,
		net_addr_ntop(NET_AF_INET, cb->data, buf, sizeof(buf)));
}

int main(void)
{
	LOG_INF("Run dhcpv4 client");

	net_mgmt_init_event_callback(&mgmt_cb, handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&mgmt_cb);

	net_dhcpv4_init_option_callback(&dhcp_cb, option_handler,
					DHCP_OPTION_NTP, ntp_server,
					sizeof(ntp_server));

	net_dhcpv4_add_option_callback(&dhcp_cb);

	net_if_foreach(start_dhcpv4_client, NULL);

#define CSR_PMACFG0  0xBC0
#define CSR_PMAADDR0 0xBD0

#define CSR_PMACFG(i)  (CSR_PMACFG0 + (i))
#define CSR_PMAADDR(i) (CSR_PMAADDR0 + (i))

#define CSR_PMACFG_A GENMASK(31, 30)

#define PMA_EN BIT(0)
#define PMA_R  BIT(4)
#define PMA_W  BIT(3)
#define PMA_X  BIT(2)
#define PMA_L  BIT(29)

#define PMA_NONCACHEABLE     BIT(27)
#define PMA_WRITETHROUGH     BIT(26)
#define PMA_WRITEMISSNOALLOC BIT(25)
#define PMA_READMISSNOALLOC  BIT(24)

	uint32_t pma_cfg[16];
	uint32_t pma_addr[16];

	pma_addr[0] = csr_read(CSR_PMAADDR(0));
	pma_addr[1] = csr_read(CSR_PMAADDR(1));
	pma_addr[2] = csr_read(CSR_PMAADDR(2));
	pma_addr[3] = csr_read(CSR_PMAADDR(3));
	pma_addr[4] = csr_read(CSR_PMAADDR(4));
	pma_addr[5] = csr_read(CSR_PMAADDR(5));
	pma_addr[6] = csr_read(CSR_PMAADDR(6));
	pma_addr[7] = csr_read(CSR_PMAADDR(7));
	pma_addr[8] = csr_read(CSR_PMAADDR(8));
	pma_addr[9] = csr_read(CSR_PMAADDR(9));
	pma_addr[10] = csr_read(CSR_PMAADDR(10));
	pma_addr[11] = csr_read(CSR_PMAADDR(11));
	pma_addr[12] = csr_read(CSR_PMAADDR(12));
	pma_addr[13] = csr_read(CSR_PMAADDR(13));
	pma_addr[14] = csr_read(CSR_PMAADDR(14));
	pma_addr[15] = csr_read(CSR_PMAADDR(15));
	pma_cfg[0] = csr_read(CSR_PMACFG(0));
	pma_cfg[1] = csr_read(CSR_PMACFG(1));
	pma_cfg[2] = csr_read(CSR_PMACFG(2));
	pma_cfg[3] = csr_read(CSR_PMACFG(3));
	pma_cfg[4] = csr_read(CSR_PMACFG(4));
	pma_cfg[5] = csr_read(CSR_PMACFG(5));
	pma_cfg[6] = csr_read(CSR_PMACFG(6));
	pma_cfg[7] = csr_read(CSR_PMACFG(7));
	pma_cfg[8] = csr_read(CSR_PMACFG(8));
	pma_cfg[9] = csr_read(CSR_PMACFG(9));
	pma_cfg[10] = csr_read(CSR_PMACFG(10));
	pma_cfg[11] = csr_read(CSR_PMACFG(11));
	pma_cfg[12] = csr_read(CSR_PMACFG(12));
	pma_cfg[13] = csr_read(CSR_PMACFG(13));
	pma_cfg[14] = csr_read(CSR_PMACFG(14));
	pma_cfg[15] = csr_read(CSR_PMACFG(15));

	for (uint32_t i = 0; i < 16; i++) {
		LOG_INF("PMA[%u]: CFG=0x%08x, ADDR=0x%08x", i, pma_cfg[i], pma_addr[i]);
		switch (FIELD_GET(CSR_PMACFG_A, pma_cfg[i])) {
		case 0x0:
			LOG_INF("  Type: OFF");
			break;
		case 0x1:
			LOG_INF("  Type: TOR");
			LOG_INF("  TOR: start=0x%08x, end=0x%08x", (i == 0 ? 0 : (pma_addr[i - 1] << 2)), (pma_addr[i] << 2));
			break;
		case 0x2:
			LOG_INF("  Type: NA4");
			break;
		case 0x3:
			LOG_INF("  Type: NAPOT");
			break;
		default:
			LOG_INF("  Type: UNKNOWN");
			break;
		}

		LOG_INF("  PMA: EN=%d, R=%d, W=%d, X=%d, L=%d", (pma_cfg[i] & PMA_EN) != 0,
			(pma_cfg[i] & PMA_R) != 0, (pma_cfg[i] & PMA_W) != 0,
			(pma_cfg[i] & PMA_X) != 0, (pma_cfg[i] & PMA_L) != 0);

		LOG_INF("  PMA: NONCACHEABLE=%d, WRITETHROUGH=%d, WRITEMISSNOALLOC=%d, "
			"READMISSNOALLOC=%d",
			(pma_cfg[i] & PMA_NONCACHEABLE) != 0, (pma_cfg[i] & PMA_WRITETHROUGH) != 0,
			(pma_cfg[i] & PMA_WRITEMISSNOALLOC) != 0,
			(pma_cfg[i] & PMA_READMISSNOALLOC) != 0);
		k_msleep(100);
	}

	return 0;
}
