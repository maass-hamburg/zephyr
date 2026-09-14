.. _ethernet_eee:

Energy Efficient Ethernet
#########################

Overview
********

Energy Efficient Ethernet (EEE), defined in IEEE 802.3az, lets an Ethernet MAC signal Low Power
Idle (LPI) to its PHY while there is no data to transmit. The PHYs then turn off parts of the
link until there is data again. Both link partners advertise EEE during auto-negotiation, and it
is only used if both support it.

EEE needs support from the PHY, which advertises and negotiates it, and from the MAC, which
signals LPI:

* The Ethernet driver passes the link speeds it supports, and the ones for which it supports
  LPI, to the PHY with :c:func:`phy_set_mac_caps` from its init function. The PHY is initialized
  after the Ethernet driver, and only advertises link speeds supported by both. EEE is only
  advertised for link speeds with LPI support in the MAC.
* After auto-negotiation, the PHY reports in the ``eee_active`` member of
  :c:struct:`phy_link_state` whether EEE is active on the link. The Ethernet driver enables or
  disables LPI in its link state callback accordingly.

A PHY, that does not get the capabilities of the MAC, does not advertise EEE.

Configuration
*************

EEE can be enabled or disabled at run-time with :c:func:`phy_set_eee_cfg`. The PHY of an
interface is returned by :c:func:`net_eth_get_phy`.

.. code-block:: c

	#include <zephyr/net/ethernet.h>
	#include <zephyr/net/phy.h>

	const struct device *phy = net_eth_get_phy(iface);
	struct phy_eee_cfg eee_cfg;
	int ret;

	ret = phy_get_eee_cfg(phy, &eee_cfg);
	if (ret == 0) {
		eee_cfg.enable = false;
		ret = phy_set_eee_cfg(phy, &eee_cfg);
	}

The time the transmit path has to be idle, before the MAC signals LPI, is set by
:kconfig:option:`CONFIG_ETH_LPI_TIMER_DEFAULT_US`. With
:kconfig:option:`CONFIG_NET_L2_ETHERNET_LPI_MGMT` it can be changed at run-time with the
``NET_REQUEST_ETHERNET_SET_LPI_PARAM`` network management request, which also allows to disable
signalling LPI in the MAC, while EEE stays advertised.

The ``net iface eee`` shell command shows the EEE status of an interface and changes its
configuration.
