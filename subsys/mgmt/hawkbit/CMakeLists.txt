#
# Copyright (c) 2020 Linumiz
#
# SPDX-License-Identifier: Apache-2.0
#

zephyr_library()

zephyr_library_sources(hawkbit.c)
zephyr_library_sources(hawkbit_device.c)
zephyr_library_sources(hawkbit_firmware.c)
zephyr_library_sources_ifdef(CONFIG_HAWKBIT_AUTOHANDLER hawkbit_autohandler.c)
zephyr_library_sources_ifdef(CONFIG_HAWKBIT_SHELL shell.c)

if(CONFIG_HAWKBIT_USE_CBOR)
  zephyr_library_sources(hawkbit_cbor.c)
else()
  zephyr_library_sources(hawkbit_json.c)
endif()

zephyr_library_link_libraries(MCUBOOT_BOOTUTIL)
