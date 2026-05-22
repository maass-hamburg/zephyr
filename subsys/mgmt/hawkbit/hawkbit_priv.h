/*
 * Copyright (c) 2020 Linumiz
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file
 *
 * @brief This file contains structures representing JSON messages
 * exchanged with a hawkBit server
 */

#ifndef __HAWKBIT_PRIV_H__
#define __HAWKBIT_PRIV_H__

#include <sys/types.h>

#ifdef CONFIG_HAWKBIT_USE_CBOR
#include <zcbor_common.h>
#define HAWKBIT_STR struct zcbor_string
#define HAWKBIT_STR_PRINT_SPEC "%.*s"
#define HAWKBIT_STR_PRINT_ARG(str) (int)(str).len, (str).value
#else
#define HAWKBIT_STR const char *
#define HAWKBIT_STR_PRINT_SPEC "%s"
#define HAWKBIT_STR_PRINT_ARG(str) (str)
#endif

#define HAWKBIT_SLEEP_LENGTH 8

enum hawkbit_http_request {
	HAWKBIT_PROBE,
	HAWKBIT_CONFIG_DEVICE,
	HAWKBIT_CANCEL,
	HAWKBIT_PROBE_DEPLOYMENT_BASE,
	HAWKBIT_REPORT,
	HAWKBIT_DOWNLOAD,
};

enum hawkbit_status_fini {
	HAWKBIT_STATUS_FINISHED_SUCCESS,
	HAWKBIT_STATUS_FINISHED_FAILURE,
	HAWKBIT_STATUS_FINISHED_NONE,
};

enum hawkbit_status_exec {
	HAWKBIT_STATUS_EXEC_CLOSED = 0,
	HAWKBIT_STATUS_EXEC_PROCEEDING,
	HAWKBIT_STATUS_EXEC_CANCELED,
	HAWKBIT_STATUS_EXEC_SCHEDULED,
	HAWKBIT_STATUS_EXEC_REJECTED,
	HAWKBIT_STATUS_EXEC_RESUMED,
	HAWKBIT_STATUS_EXEC_NONE,
};

struct hawkbit_href {
	HAWKBIT_STR href;
};

struct hawkbit_status_result {
	HAWKBIT_STR finished;
};

struct hawkbit_status {
	struct hawkbit_status_result result;
	HAWKBIT_STR execution;
};

struct hawkbit_ctl_res_sleep {
	HAWKBIT_STR sleep;
};

struct hawkbit_ctl_res_polling {
	struct hawkbit_ctl_res_sleep polling;
};

struct hawkbit_ctl_res_links {
	struct hawkbit_href deploymentBase;
	struct hawkbit_href configData;
	struct hawkbit_href cancelAction;
};

struct hawkbit_ctl_res {
	struct hawkbit_ctl_res_polling config;
	struct hawkbit_ctl_res_links _links;
};

struct hawkbit_cfg_data {
	HAWKBIT_STR VIN;
};

struct hawkbit_cfg {
	HAWKBIT_STR mode;
	struct hawkbit_cfg_data data;
};

struct hawkbit_cancel {
	struct hawkbit_status status;
};

/* Maximum number of chunks we support */
#define HAWKBIT_DEP_MAX_CHUNKS 1
/* Maximum number of artifacts per chunk. */
#define HAWKBIT_DEP_MAX_CHUNK_ARTS 1

struct hawkbit_dep_res_hashes {
	HAWKBIT_STR sha1;
	HAWKBIT_STR md5;
	HAWKBIT_STR sha256;
};

struct hawkbit_dep_res_links {
	struct hawkbit_href download_http;
	struct hawkbit_href md5sum_http;
};

struct hawkbit_dep_res_arts {
	HAWKBIT_STR filename;
	struct hawkbit_dep_res_hashes hashes;
	struct hawkbit_dep_res_links _links;
	int size;
};

struct hawkbit_dep_res_chunk {
	HAWKBIT_STR part;
	HAWKBIT_STR name;
	HAWKBIT_STR version;
	struct hawkbit_dep_res_arts artifacts[HAWKBIT_DEP_MAX_CHUNK_ARTS];
	size_t num_artifacts;
};

struct hawkbit_dep_res_deploy {
	HAWKBIT_STR download;
	HAWKBIT_STR update;
	struct hawkbit_dep_res_chunk chunks[HAWKBIT_DEP_MAX_CHUNKS];
	size_t num_chunks;
};

struct hawkbit_dep_res {
	HAWKBIT_STR id;
	struct hawkbit_dep_res_deploy deployment;
};

struct hawkbit_dep_fbk {
	struct hawkbit_status status;
};

ssize_t hawkbit_encode_cfg(const char *device_id, uint8_t *buf, size_t buf_len);

ssize_t hawkbit_encode_cancel(const struct hawkbit_cancel *cancel, uint8_t *buf, size_t buf_len);

ssize_t hawkbit_encode_dep_fbk(const struct hawkbit_dep_fbk *fbk, uint8_t *buf, size_t buf_len);

int hawkbit_decode_ctl_res(uint8_t *buf, size_t len, struct hawkbit_ctl_res *res);

int hawkbit_decode_dep_res(uint8_t *buf, size_t len, struct hawkbit_dep_res *res);

#endif /* __HAWKBIT_PRIV_H__ */
