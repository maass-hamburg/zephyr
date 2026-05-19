/*
 * Copyright (c) 2020 Linumiz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/mgmt/hawkbit/hawkbit.h>
#include <zephyr/mgmt/hawkbit/config.h>
#include <zephyr/mgmt/hawkbit/autohandler.h>
#include <zephyr/mgmt/hawkbit/event.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>

#include <errno.h>
#include <string.h>

#ifdef CONFIG_HAWKBIT_USE_CBOR
#include <zcbor_encode.h>
#else
#include <zephyr/data/json.h>
#endif

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
#include <zephyr/net/tls_credentials.h>
#include "ca_certificate.h"
#endif

LOG_MODULE_REGISTER(main);

#ifdef CONFIG_HAWKBIT_CUSTOM_ATTRIBUTES
#ifdef CONFIG_HAWKBIT_USE_JSON
struct hawkbit_cfg_data {
	const char *VIN;
	const char *customAttr;
};

struct hawkbit_cfg {
	const char *mode;
	struct hawkbit_cfg_data data;
};

static const struct json_obj_descr json_cfg_data_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct hawkbit_cfg_data, VIN, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct hawkbit_cfg_data, customAttr, JSON_TOK_STRING),
};

static const struct json_obj_descr json_cfg_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct hawkbit_cfg, mode, JSON_TOK_STRING),
	JSON_OBJ_DESCR_OBJECT(struct hawkbit_cfg, data, json_cfg_data_descr),
};
#endif /* CONFIG_HAWKBIT_USE_JSON */

ssize_t hawkbit_new_config_data_cb(const char *device_id, uint8_t *buffer, const size_t buffer_size)
{
	const char *custom_attr = "Hello World!";

#ifdef CONFIG_HAWKBIT_USE_CBOR
	ZCBOR_STATE_E(zse, 3, buffer, buffer_size, 1);

	bool ok = zcbor_map_start_encode(zse, 2) &&
			  zcbor_tstr_put_lit(zse, "mode") &&
			  zcbor_tstr_put_lit(zse, "merge") &&
			  zcbor_tstr_put_lit(zse, "data") &&
			  zcbor_map_start_encode(zse, 2) &&
			  zcbor_tstr_put_lit(zse, "VIN") &&
			  zcbor_tstr_put_term(zse, device_id, strlen(device_id)) &&
			  zcbor_tstr_put_lit(zse, "customAttr") &&
			  zcbor_tstr_put_term(zse, custom_attr, strlen(custom_attr)) &&
			  zcbor_map_end_encode(zse, 2) &&
			  zcbor_map_end_encode(zse, 2);

	if (!ok) {
		return -EINVAL;
	}

	return (ssize_t)(zse->payload - buffer);
#else
	struct hawkbit_cfg cfg = {
		.mode = "merge",
		.data.VIN = device_id,
		.data.customAttr = custom_attr,
	};

	int ret = json_obj_encode_buf(json_cfg_descr, ARRAY_SIZE(json_cfg_descr), &cfg, buffer,
				      buffer_size);

	return ret < 0 ? ret : (ssize_t)strnlen((char *)buffer, buffer_size);
#endif
}
#endif /* CONFIG_HAWKBIT_CUSTOM_ATTRIBUTES */

#ifdef CONFIG_HAWKBIT_EVENT_CALLBACKS
void hawkbit_event_cb(struct hawkbit_event_callback *cb, enum hawkbit_event_type event)
{
	LOG_INF("hawkBit event: %d", event);

	switch (event) {
	case HAWKBIT_EVENT_START_RUN:
		LOG_INF("Run of hawkBit started");
		break;

	case HAWKBIT_EVENT_END_RUN:
		LOG_INF("Run of hawkBit ended");
		break;

	default:
		break;
	}
}

static HAWKBIT_EVENT_CREATE_CALLBACK(hb_event_cb_start, hawkbit_event_cb, HAWKBIT_EVENT_START_RUN);
static HAWKBIT_EVENT_CREATE_CALLBACK(hb_event_cb_end, hawkbit_event_cb, HAWKBIT_EVENT_END_RUN);
#endif /* CONFIG_HAWKBIT_EVENT_CALLBACKS */

int main(void)
{
	int ret = -1;

	LOG_INF("hawkBit sample app started");
	LOG_INF("Image build time: " __DATE__ " " __TIME__);

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
	tls_credential_add(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
			   ca_certificate, sizeof(ca_certificate));
#endif
#ifdef CONFIG_HAWKBIT_CUSTOM_ATTRIBUTES
	ret = hawkbit_set_custom_data_cb(hawkbit_new_config_data_cb);
	if (ret < 0) {
		LOG_ERR("Failed to set custom data callback");
	}
#endif /* CONFIG_HAWKBIT_CUSTOM_ATTRIBUTES */

#ifdef CONFIG_HAWKBIT_EVENT_CALLBACKS
	hawkbit_event_add_callback(&hb_event_cb_start);
	hawkbit_event_add_callback(&hb_event_cb_end);
#endif /* CONFIG_HAWKBIT_EVENT_CALLBACKS */

	ret = hawkbit_init();
	if (ret < 0) {
		LOG_ERR("Failed to init hawkBit");
	}

#if defined(CONFIG_HAWKBIT_SET_SETTINGS_RUNTIME) && !defined(CONFIG_HAWKBIT_SHELL)
	hawkbit_set_server_addr(CONFIG_HAWKBIT_SERVER);
	hawkbit_set_server_port(CONFIG_HAWKBIT_PORT);
#endif

#if defined(CONFIG_HAWKBIT_POLLING)
	LOG_INF("Starting hawkBit polling mode");
	hawkbit_autohandler(true);
#endif

#if defined(CONFIG_HAWKBIT_MANUAL)
	LOG_INF("Starting hawkBit manual mode");

	switch (hawkbit_probe()) {
	case HAWKBIT_UNCONFIRMED_IMAGE:
		LOG_ERR("Image is unconfirmed");
		LOG_ERR("Rebooting to previous confirmed image");
		LOG_ERR("If this image is flashed using a hardware tool");
		LOG_ERR("Make sure that it is a confirmed image");
		k_sleep(K_SECONDS(1));
		sys_reboot(SYS_REBOOT_WARM);
		break;

	case HAWKBIT_NO_UPDATE:
		LOG_INF("No update found");
		break;

	case HAWKBIT_UPDATE_INSTALLED:
		LOG_INF("Update installed");
		break;

	case HAWKBIT_PROBE_IN_PROGRESS:
		LOG_INF("hawkBit is already running");
		break;

	default:
		break;
	}

#endif
	return 0;
}
