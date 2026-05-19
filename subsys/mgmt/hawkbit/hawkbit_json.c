/*
 * Copyright (c) 2024 Vogl Electronic GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <errno.h>

#include <zephyr/data/json.h>
#include <zephyr/logging/log.h>

#include "hawkbit_priv.h"

LOG_MODULE_DECLARE(hawkbit, CONFIG_HAWKBIT_LOG_LEVEL);

/* JSON object descriptors */
const struct json_obj_descr json_href_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct hawkbit_href, href, JSON_TOK_STRING),
};

const struct json_obj_descr json_status_result_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct hawkbit_status_result, finished, JSON_TOK_STRING),
};

const struct json_obj_descr json_status_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct hawkbit_status, execution, JSON_TOK_STRING),
	JSON_OBJ_DESCR_OBJECT(struct hawkbit_status, result, json_status_result_descr),
};

const struct json_obj_descr json_ctl_res_sleep_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct hawkbit_ctl_res_sleep, sleep, JSON_TOK_STRING),
};

const struct json_obj_descr json_ctl_res_polling_descr[] = {
	JSON_OBJ_DESCR_OBJECT(struct hawkbit_ctl_res_polling, polling, json_ctl_res_sleep_descr),
};

const struct json_obj_descr json_ctl_res_links_descr[] = {
	JSON_OBJ_DESCR_OBJECT(struct hawkbit_ctl_res_links, deploymentBase, json_href_descr),
	JSON_OBJ_DESCR_OBJECT(struct hawkbit_ctl_res_links, cancelAction, json_href_descr),
	JSON_OBJ_DESCR_OBJECT(struct hawkbit_ctl_res_links, configData, json_href_descr),
};

const struct json_obj_descr json_ctl_res_descr[] = {
	JSON_OBJ_DESCR_OBJECT(struct hawkbit_ctl_res, config, json_ctl_res_polling_descr),
	JSON_OBJ_DESCR_OBJECT(struct hawkbit_ctl_res, _links, json_ctl_res_links_descr),
};

const struct json_obj_descr json_cfg_data_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct hawkbit_cfg_data, VIN, JSON_TOK_STRING),
};

const struct json_obj_descr json_cfg_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct hawkbit_cfg, mode, JSON_TOK_STRING),
	JSON_OBJ_DESCR_OBJECT(struct hawkbit_cfg, data, json_cfg_data_descr),
};

const struct json_obj_descr json_cancel_descr[] = {
	JSON_OBJ_DESCR_OBJECT(struct hawkbit_cancel, status, json_status_descr),
};

const struct json_obj_descr json_dep_res_hashes_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct hawkbit_dep_res_hashes, sha1, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct hawkbit_dep_res_hashes, md5, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct hawkbit_dep_res_hashes, sha256, JSON_TOK_STRING),
};

const struct json_obj_descr json_dep_res_links_descr[] = {
	JSON_OBJ_DESCR_OBJECT_NAMED(struct hawkbit_dep_res_links, "download-http", download_http,
				    json_href_descr),
	JSON_OBJ_DESCR_OBJECT_NAMED(struct hawkbit_dep_res_links, "md5sum-http", md5sum_http,
				    json_href_descr),
};

const struct json_obj_descr json_dep_res_arts_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct hawkbit_dep_res_arts, filename, JSON_TOK_STRING),
	JSON_OBJ_DESCR_OBJECT(struct hawkbit_dep_res_arts, hashes, json_dep_res_hashes_descr),
	JSON_OBJ_DESCR_PRIM(struct hawkbit_dep_res_arts, size, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_OBJECT(struct hawkbit_dep_res_arts, _links, json_dep_res_links_descr),
};

const struct json_obj_descr json_dep_res_chunk_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct hawkbit_dep_res_chunk, part, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct hawkbit_dep_res_chunk, version, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct hawkbit_dep_res_chunk, name, JSON_TOK_STRING),
	JSON_OBJ_DESCR_OBJ_ARRAY(struct hawkbit_dep_res_chunk, artifacts,
				 HAWKBIT_DEP_MAX_CHUNK_ARTS, num_artifacts, json_dep_res_arts_descr,
				 ARRAY_SIZE(json_dep_res_arts_descr)),
};

const struct json_obj_descr json_dep_res_deploy_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct hawkbit_dep_res_deploy, download, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct hawkbit_dep_res_deploy, update, JSON_TOK_STRING),
	JSON_OBJ_DESCR_OBJ_ARRAY(struct hawkbit_dep_res_deploy, chunks, HAWKBIT_DEP_MAX_CHUNKS,
				 num_chunks, json_dep_res_chunk_descr,
				 ARRAY_SIZE(json_dep_res_chunk_descr)),
};

const struct json_obj_descr json_dep_res_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct hawkbit_dep_res, id, JSON_TOK_STRING),
	JSON_OBJ_DESCR_OBJECT(struct hawkbit_dep_res, deployment, json_dep_res_deploy_descr),
};

const struct json_obj_descr json_dep_fbk_descr[] = {
	JSON_OBJ_DESCR_OBJECT(struct hawkbit_dep_fbk, status, json_status_descr),
};

ssize_t hawkbit_encode_cfg(const char *device_id, uint8_t *buf, size_t buf_len)
{
	struct hawkbit_cfg cfg = {
		.mode = "merge",
		.data.VIN = device_id,
	};

	int ret = json_obj_encode_buf(json_cfg_descr, ARRAY_SIZE(json_cfg_descr), &cfg, buf,
				      buf_len);
	return ret < 0 ? ret : (ssize_t)strnlen((char *)buf, buf_len);
}

ssize_t hawkbit_encode_cancel(const struct hawkbit_cancel *cancel, uint8_t *buf, size_t buf_len)
{
	int ret = json_obj_encode_buf(json_cancel_descr, ARRAY_SIZE(json_cancel_descr), cancel,
				      buf, buf_len);
	return ret < 0 ? ret : (ssize_t)strnlen((char *)buf, buf_len);
}

ssize_t hawkbit_encode_dep_fbk(const struct hawkbit_dep_fbk *fbk, uint8_t *buf, size_t buf_len)
{
	int ret = json_obj_encode_buf(json_dep_fbk_descr, ARRAY_SIZE(json_dep_fbk_descr), fbk, buf,
				      buf_len);
	return ret < 0 ? ret : (ssize_t)strnlen((char *)buf, buf_len);
}

int hawkbit_decode_ctl_res(uint8_t *buf, size_t len, struct hawkbit_ctl_res *res)
{
	int ret;

	memset(res, 0, sizeof(*res));

	ret = json_obj_parse(buf, len, json_ctl_res_descr, ARRAY_SIZE(json_ctl_res_descr), res);
	if (ret < 0) {
		LOG_ERR("JSON parse error (ctl_res): %d", ret);
		return ret;
	}
	return 0;
}

int hawkbit_decode_dep_res(uint8_t *buf, size_t len, struct hawkbit_dep_res *res)
{
	int ret;

	memset(res, 0, sizeof(*res));

	ret = json_obj_parse(buf, len, json_dep_res_descr, ARRAY_SIZE(json_dep_res_descr), res);
	if (ret < 0) {
		LOG_ERR("JSON parse error (dep_res): %d", ret);
		return ret;
	}
	return 0;
}
