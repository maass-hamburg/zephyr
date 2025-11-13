:orphan:

..
  See
  https://docs.zephyrproject.org/latest/releases/index.html#migration-guides
  for details of what is supposed to go into this document.

.. _migration_4.4:

Migration guide to Zephyr v4.4.0 (Working Draft)
################################################

This document describes the changes required when migrating your application from Zephyr v4.3.0 to
Zephyr v4.4.0.

Any other changes (not directly related to migrating applications) can be found in
the :ref:`release notes<zephyr_4.4>`.

.. contents::
    :local:
    :depth: 2

Build System
************

Kernel
******

Boards
******

Device Drivers and Devicetree
*****************************

GPIO
====

  * The Litex GPIO driver :dtcompatible:`litex,gpio` has been reworked to support changing direction.
    The driver now uses the reg-names property to detect supported modes of the GPIO controller.
    The Devicetree property ``port-is-output`` has been removed.
    The reg-names are now taken directly from Litex.

Bluetooth
*********

Networking
**********

Other subsystems
****************

Modules
*******

Architectures
*************
