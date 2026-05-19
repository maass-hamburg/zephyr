/*
 * Copyright (c) 2024 Vogl Electronic GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 * @brief hawkBit DDI codec interface.
 *
 * This header provides the common encode/decode API used by the hawkBit
 * client. Exactly one implementation is compiled at a time:
 * - JSON implementation when CONFIG_HAWKBIT_USE_CBOR is not selected
 * - CBOR implementation when CONFIG_HAWKBIT_USE_CBOR is selected
 */

#ifndef HAWKBIT_CODEC_H__
#define HAWKBIT_CODEC_H__

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "hawkbit_priv.h"

ssize_t hawkbit_encode_cfg(const char *device_id, uint8_t *buf, size_t buf_len);

ssize_t hawkbit_encode_cancel(const struct hawkbit_cancel *cancel, uint8_t *buf, size_t buf_len);

ssize_t hawkbit_encode_dep_fbk(const struct hawkbit_dep_fbk *fbk, uint8_t *buf, size_t buf_len);

int hawkbit_decode_ctl_res(uint8_t *buf, size_t len, struct hawkbit_ctl_res *res);

int hawkbit_decode_dep_res(uint8_t *buf, size_t len, struct hawkbit_dep_res *res);

#endif /* HAWKBIT_CODEC_H__ */
