:orphan:

..
  See
  https://docs.zephyrproject.org/latest/releases/index.html#migration-guides
  for details of what is supposed to go into this document.

.. _migration_4.6:

Migration guide to Zephyr v4.6.0 (Working Draft)
################################################

This document describes the changes required when migrating your application from Zephyr v4.5.0 to
Zephyr v4.6.0.

Any other changes (not directly related to migrating applications) can be found in
the :ref:`release notes<zephyr_4.6>`.

.. contents::
    :local:
    :depth: 2

Common
******

Build System
************

Kernel
******

Boards
******

Device Drivers and Devicetree
*****************************

.. Only place contents common to all device drivers here. Contents specific to one driver subsystem
   goes into its own subsection, below.

.. Group contents in this section by subsystem, e.g.:
..
.. ADC
.. ===
..
.. ...

.. zephyr-keep-sorted-start re(^\w) ignorecase

.. zephyr-keep-sorted-stop

Bluetooth
*********

Networking
**********

* :c:func:`net_if_set_link_addr` now passes the new address to the L2 before storing it. For an
  Ethernet interface this calls the driver's :c:member:`ethernet_api.set_config` with
  ``ETHERNET_CONFIG_TYPE_MAC_ADDRESS``, also when the driver registers its address from
  ``iface_init``. Drivers must therefore accept that call once the hardware is set up. Drivers
  only call :c:func:`net_if_set_link_addr` to register the initial address, never at runtime and
  never from that handler. Drivers that cannot change the MAC
  address return ``-ENOTSUP``, and the address is then only stored in software. The link address
  is not changed if the handler returns any other error.

* :c:func:`net_if_set_link_addr` rejects an Ethernet interface address with a link type other than
  ``NET_LINK_ETHERNET`` or a length other than 6 bytes with ``-EINVAL``.

* :c:macro:`NET_REQUEST_ETHERNET_SET_MAC_ADDRESS` is a wrapper around
  :c:func:`net_if_set_link_addr`. It no longer fails with ``-ENOTSUP`` for drivers without
  :c:member:`ethernet_api.set_config`; for those, the address is only stored in software.

Other subsystems
****************

Modules
*******

Architectures
*************
