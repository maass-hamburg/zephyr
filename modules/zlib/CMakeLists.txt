# SPDX-License-Identifier: Apache-2.0

if(CONFIG_ZLIB_INFLATE)
  zephyr_library()

  set(ZLIB_BASE ${ZEPHYR_TRUSTED_FIRMWARE_A_MODULE_DIR}/lib/zlib)

  zephyr_library_compile_definitions(Z_SOLO)

  zephyr_library_include_directories(${ZLIB_BASE})

  zephyr_library_sources(
    ${ZLIB_BASE}/adler32.c
    ${ZLIB_BASE}/crc32.c
    ${ZLIB_BASE}/inffast.c
    ${ZLIB_BASE}/inflate.c
    ${ZLIB_BASE}/inftrees.c
    ${ZLIB_BASE}/zutil.c
  )
endif()
