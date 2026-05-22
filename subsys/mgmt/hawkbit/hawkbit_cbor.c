/*
 * Copyright (c) 2024 Vogl Electronic GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <errno.h>

#include <zcbor_decode.h>
#include <zcbor_encode.h>

#include <zephyr/logging/log.h>

#include "hawkbit_priv.h"

LOG_MODULE_DECLARE(hawkbit, CONFIG_HAWKBIT_LOG_LEVEL);

/* ---------------------------------------------------------------------------
 * Generic CBOR map decoder
 *
 * Iterates over all key-value pairs in a CBOR map.  For each text-key that
 * matches an entry in the @p entries table the corresponding decoder function
 * is called with the CBOR state positioned at the value.  Unknown keys are
 * skipped.
 * ---------------------------------------------------------------------------
 */
struct cbor_map_entry {
	const char *key;
	size_t      key_len;
	bool      (*dec)(zcbor_state_t *state, void *out);
	void       *out;
	bool        found;
};

/* Helper macro: cast dec_fn to the generic signature accepted by the table. */
#define ENTRY(k, dec_fn, ptr)                                                  \
	{                                                                      \
		.key     = (k),                                                \
		.key_len = sizeof(k) - 1,                                      \
		.dec     = (bool (*)(zcbor_state_t *, void *))(dec_fn),        \
		.out     = (ptr),                                              \
		.found   = false,                                              \
	}

static int cbor_map_decode(zcbor_state_t *state, struct cbor_map_entry *entries, size_t n)
{
	bool ok;

	if (!zcbor_map_start_decode(state)) {
		return -EBADMSG;
	}

	do {
		struct zcbor_string key;
		bool matched = false;

		ok = zcbor_tstr_decode(state, &key);
		if (!ok) {
			break; /* end of map */
		}

		for (size_t i = 0; i < n; i++) {
			if (key.len == entries[i].key_len &&
			    memcmp(key.value, entries[i].key, key.len) == 0) {
				if (!entries[i].dec(state, entries[i].out)) {
					return -ENOMSG;
				}
				entries[i].found = true;
				matched = true;
				break;
			}
		}

		if (!matched) {
			ok = zcbor_any_skip(state, NULL);
		}
	} while (ok);

	return zcbor_map_end_decode(state) ? 0 : -EBADMSG;
}

/* ---------------------------------------------------------------------------
 * hawkbit_ctl_res decoders
 *
 * Expected JSON/CBOR shape:
 *   {
 *     "config": { "polling": { "sleep": "HH:MM:SS" } },
 *     "_links": {
 *       "deploymentBase": { "href": "..." },
 *       "cancelAction":   { "href": "..." },
 *       "configData":     { "href": "..." }
 *     }
 *   }
 * ---------------------------------------------------------------------------
 */

/** Decode {"href": <tstr>} into struct hawkbit_href. */
static bool decode_href(zcbor_state_t *state, struct hawkbit_href *out)
{
	struct cbor_map_entry entries[] = {
		ENTRY("href", zcbor_tstr_decode, &out->href),
	};

	if (cbor_map_decode(state, entries, ARRAY_SIZE(entries)) != 0) {
		return false;
	}
	return true;
}

/** Decode {"sleep": <tstr>} into struct hawkbit_ctl_res_sleep. */
static bool decode_sleep_map(zcbor_state_t *state, struct hawkbit_ctl_res_sleep *out)
{
	struct cbor_map_entry entries[] = {
		ENTRY("sleep", zcbor_tstr_decode, &out->sleep),
	};

	if (cbor_map_decode(state, entries, ARRAY_SIZE(entries)) != 0) {
		return false;
	}
	return true;
}

/** Decode {"polling": {...}} into struct hawkbit_ctl_res_polling. */
static bool decode_ctl_config_map(zcbor_state_t *state, struct hawkbit_ctl_res_polling *out)
{
	struct cbor_map_entry entries[] = {
		ENTRY("polling", decode_sleep_map, &out->polling),
	};

	return cbor_map_decode(state, entries, ARRAY_SIZE(entries)) == 0;
}

/** Decode the "_links" map into struct hawkbit_ctl_res_links. */
static bool decode_ctl_links_map(zcbor_state_t *state, struct hawkbit_ctl_res_links *out)
{
	struct cbor_map_entry entries[] = {
		ENTRY("deploymentBase", decode_href, &out->deploymentBase),
		ENTRY("cancelAction",   decode_href, &out->cancelAction),
		ENTRY("configData",     decode_href, &out->configData),
	};

	return cbor_map_decode(state, entries, ARRAY_SIZE(entries)) == 0;
}

/* Decode one list item: {"rel": "...", "href": "..."}. */
static bool decode_ctl_link_rel_item(zcbor_state_t *state, struct hawkbit_ctl_res_links *out)
{
	struct zcbor_string rel = {0};
	struct zcbor_string href = {0};
	struct cbor_map_entry entries[] = {
		ENTRY("rel",  zcbor_tstr_decode, &rel),
		ENTRY("href", zcbor_tstr_decode, &href),
	};

	if (cbor_map_decode(state, entries, ARRAY_SIZE(entries)) != 0) {
		return false;
	}

	if (href.value == NULL || rel.value == NULL) {
		return true;
	}

	if (rel.len == sizeof("cancelAction") - 1 &&
	    memcmp(rel.value, "cancelAction", rel.len) == 0) {
		out->cancelAction.href = href;
	} else if (rel.len == sizeof("configData") - 1 &&
		   memcmp(rel.value, "configData", rel.len) == 0) {
		out->configData.href = href;
	} else if (rel.len == sizeof("deploymentBase") - 1 &&
		   memcmp(rel.value, "deploymentBase", rel.len) == 0) {
		out->deploymentBase.href = href;
	}

	return true;
}

/* Decode hawkBit CBOR link array: "links": [{"rel":"...","href":"..."}, ...] */
static bool decode_ctl_links_array(zcbor_state_t *state, struct hawkbit_ctl_res_links *out)
{
	if (!zcbor_list_start_decode(state)) {
		return false;
	}

	while (decode_ctl_link_rel_item(state, out)) {
	}

	return zcbor_list_end_decode(state);
}

/* Accept both object-form links and rel/href list-form links. */
static bool decode_ctl_links_any(zcbor_state_t *state, struct hawkbit_ctl_res_links *out)
{
	if (decode_ctl_links_map(state, out)) {
		return true;
	}

	return decode_ctl_links_array(state, out);
}

int hawkbit_decode_ctl_res(uint8_t *buf, size_t len, struct hawkbit_ctl_res *res)
{
	/*
	 * Maximum simultaneous nesting depth for ctl_res:
	 *   top-map → config → polling  (or top-map → _links → <href-map>)
	 * → 3 levels, use 4 backup slots for safety.
	 */
	ZCBOR_STATE_D(zsd, 4, buf, len, 1, 0);
	memset(res, 0, sizeof(*res));

	struct cbor_map_entry entries[] = {
		ENTRY("config",  decode_ctl_config_map, &res->config),
		ENTRY("_links",  decode_ctl_links_any,  &res->_links),
		ENTRY("links",   decode_ctl_links_any,  &res->_links),
	};

	return cbor_map_decode(zsd, entries, ARRAY_SIZE(entries));
}

/* ---------------------------------------------------------------------------
 * hawkbit_dep_res decoders
 *
 * Expected JSON/CBOR shape (only the first chunk / first artifact supported):
 *   {
 *     "id": "...",
 *     "deployment": {
 *       "download": "forced",
 *       "update":   "forced",
 *       "chunks": [ {
 *         "part":    "bApp",
 *         "name":    "...",
 *         "version": "...",
 *         "artifacts": [ {
 *           "filename": "...",
 *           "hashes":   { "sha1": "...", "md5": "...", "sha256": "..." },
 *           "size":     12345,
 *           "_links":   {
 *             "download-http": { "href": "..." },
 *             "md5sum-http":   { "href": "..." }
 *           }
 *         } ]
 *       } ]
 *     }
 *   }
 * ---------------------------------------------------------------------------
 */

/** Decode {"sha1":..,"md5":..,"sha256":..} into struct hawkbit_dep_res_hashes. */
static bool decode_dep_hashes_map(zcbor_state_t *state, struct hawkbit_dep_res_hashes *out)
{
	struct cbor_map_entry entries[] = {
		ENTRY("sha1",   zcbor_tstr_decode, &out->sha1),
		ENTRY("md5",    zcbor_tstr_decode, &out->md5),
		ENTRY("sha256", zcbor_tstr_decode, &out->sha256),
	};

	if (cbor_map_decode(state, entries, ARRAY_SIZE(entries)) != 0) {
		return false;
	}
	return true;
}

/** Decode the artifact "_links" map into struct hawkbit_dep_res_links. */
static bool decode_dep_artifact_links(zcbor_state_t *state, struct hawkbit_dep_res_links *out)
{
	struct cbor_map_entry entries[] = {
		ENTRY("download-http", decode_href, &out->download_http),
		ENTRY("download",      decode_href, &out->download_http),
		ENTRY("md5sum-http",   decode_href, &out->md5sum_http),
		ENTRY("md5sum",        decode_href, &out->md5sum_http),
	};

	return cbor_map_decode(state, entries, ARRAY_SIZE(entries)) == 0;
}

/* Decode one artifact-link list item: {"rel":"...","href":"..."}. */
static bool decode_dep_artifact_link_rel_item(zcbor_state_t *state,
					       struct hawkbit_dep_res_links *out)
{
	struct zcbor_string rel = {0};
	struct zcbor_string href = {0};
	struct cbor_map_entry entries[] = {
		ENTRY("rel",  zcbor_tstr_decode, &rel),
		ENTRY("href", zcbor_tstr_decode, &href),
	};

	if (cbor_map_decode(state, entries, ARRAY_SIZE(entries)) != 0) {
		return false;
	}

	if (href.value == NULL || rel.value == NULL) {
		return true;
	}

	if (((rel.len == sizeof("download-http") - 1) &&
	     (memcmp(rel.value, "download-http", rel.len) == 0)) ||
	    ((rel.len == sizeof("download") - 1) &&
	     (memcmp(rel.value, "download", rel.len) == 0))) {
		out->download_http.href = href;
	} else if (((rel.len == sizeof("md5sum-http") - 1) &&
		    (memcmp(rel.value, "md5sum-http", rel.len) == 0)) ||
		   ((rel.len == sizeof("md5sum") - 1) &&
		    (memcmp(rel.value, "md5sum", rel.len) == 0))) {
		out->md5sum_http.href = href;
	}

	return true;
}

static bool decode_dep_artifact_links_array(zcbor_state_t *state, struct hawkbit_dep_res_links *out)
{
	if (!zcbor_list_start_decode(state)) {
		return false;
	}

	while (decode_dep_artifact_link_rel_item(state, out)) {
	}

	return zcbor_list_end_decode(state);
}

/* Accept both object-form and rel/href list-form artifact links. */
static bool decode_dep_artifact_links_any(zcbor_state_t *state, struct hawkbit_dep_res_links *out)
{
	if (decode_dep_artifact_links(state, out)) {
		return true;
	}

	return decode_dep_artifact_links_array(state, out);
}

/** Decode a single artifact map into struct hawkbit_dep_res_arts. */
static bool decode_dep_artifact(zcbor_state_t *state, struct hawkbit_dep_res_arts *out)
{
	int32_t size_val = 0;
	struct cbor_map_entry entries[] = {
		ENTRY("filename", zcbor_tstr_decode,      &out->filename),
		ENTRY("hashes",   decode_dep_hashes_map,  &out->hashes),
		ENTRY("size",     zcbor_int32_decode,      &size_val),
		ENTRY("_links",   decode_dep_artifact_links_any, &out->_links),
		ENTRY("links",    decode_dep_artifact_links_any, &out->_links),
	};

	if (cbor_map_decode(state, entries, ARRAY_SIZE(entries)) != 0) {
		return false;
	}
	out->size     = (int)size_val;
	return true;
}

/**
 * Decode the artifacts CBOR array.
 * Only the first artifact is decoded; additional ones are skipped.
 * @p chunk is updated with the decoded artifact.
 */
static bool decode_artifacts_array(zcbor_state_t *state, struct hawkbit_dep_res_chunk *chunk)
{
	if (!zcbor_list_start_decode(state)) {
		return false;
	}

	if (!decode_dep_artifact(state, &chunk->artifacts[0])) {
		return false;
	}
	chunk->num_artifacts = 1;

	/* Skip any additional artifacts */
	while (zcbor_any_skip(state, NULL)) {
	}

	return zcbor_list_end_decode(state);
}

/** Decode a single chunk map into struct hawkbit_dep_res_chunk. */
static bool decode_dep_chunk(zcbor_state_t *state, struct hawkbit_dep_res_chunk *out)
{
	struct cbor_map_entry entries[] = {
		ENTRY("part",      zcbor_tstr_decode,     &out->part),
		ENTRY("name",      zcbor_tstr_decode,     &out->name),
		ENTRY("version",   zcbor_tstr_decode,     &out->version),
		ENTRY("artifacts", decode_artifacts_array, out),
	};

	if (cbor_map_decode(state, entries, ARRAY_SIZE(entries)) != 0) {
		return false;
	}
	return true;
}

/**
 * Decode the chunks CBOR array.
 * Only the first chunk is decoded; additional ones are skipped.
 * @p deploy is updated with the decoded chunk.
 */
static bool decode_chunks_array(zcbor_state_t *state, struct hawkbit_dep_res_deploy *deploy)
{
	if (!zcbor_list_start_decode(state)) {
		return false;
	}

	if (!decode_dep_chunk(state, &deploy->chunks[0])) {
		return false;
	}
	deploy->num_chunks = 1;

	/* Skip any additional chunks */
	while (zcbor_any_skip(state, NULL)) {
	}

	return zcbor_list_end_decode(state);
}

/** Decode the "deployment" map into struct hawkbit_dep_res_deploy. */
static bool decode_dep_deploy_map(zcbor_state_t *state, struct hawkbit_dep_res_deploy *out)
{
	struct cbor_map_entry entries[] = {
		ENTRY("download", zcbor_tstr_decode,  &out->download),
		ENTRY("update",   zcbor_tstr_decode,  &out->update),
		ENTRY("chunks",   decode_chunks_array, out),
	};

	if (cbor_map_decode(state, entries, ARRAY_SIZE(entries)) != 0) {
		return false;
	}
	return true;
}

int hawkbit_decode_dep_res(uint8_t *buf, size_t len, struct hawkbit_dep_res *res)
{
	/*
	 * Maximum simultaneous nesting depth for dep_res:
	 *   top → deployment → chunks(list) → chunk → artifacts(list)
	 *   → artifact → _links → download-http  (8 levels)
	 * Use 9 backup slots for safety.
	 */
	ZCBOR_STATE_D(zsd, 9, buf, len, 1, 0);
	memset(res, 0, sizeof(*res));

	struct cbor_map_entry entries[] = {
		ENTRY("id",         zcbor_tstr_decode,   &res->id),
		ENTRY("deployment", decode_dep_deploy_map, &res->deployment),
	};

	if (cbor_map_decode(zsd, entries, ARRAY_SIZE(entries)) != 0) {
		return -EINVAL;
	}
	return 0;
}

/* ---------------------------------------------------------------------------
 * CBOR encoding helpers for outgoing messages
 * ---------------------------------------------------------------------------
 */

/**
 * Encode a hawkBit status structure:
 *   {"status":{"execution":<exec>,"result":{"finished":<fin>}}}
 */
static ssize_t encode_status(const struct hawkbit_status *status, uint8_t *buf, size_t buf_len)
{
	ZCBOR_STATE_E(zse, 4, buf, buf_len, 1);

	bool ok = zcbor_map_start_encode(zse, 1) &&
		  zcbor_tstr_put_lit(zse, "status") &&
		  zcbor_map_start_encode(zse, 2) &&
		  zcbor_tstr_put_lit(zse, "execution") &&
		  zcbor_tstr_encode(zse, &status->execution) &&
		  zcbor_tstr_put_lit(zse, "result") &&
		  zcbor_map_start_encode(zse, 1) &&
		  zcbor_tstr_put_lit(zse, "finished") &&
		  zcbor_tstr_encode(zse, &status->result.finished) &&
		  zcbor_map_end_encode(zse, 1) &&
		  zcbor_map_end_encode(zse, 2) &&
		  zcbor_map_end_encode(zse, 1);

	if (!ok) {
		return -EINVAL;
	}
	return (ssize_t)(zse->payload - buf);
}

ssize_t hawkbit_encode_cfg(const char *device_id, uint8_t *buf, size_t buf_len)
{
	ZCBOR_STATE_E(zse, 3, buf, buf_len, 1);

	bool ok = zcbor_map_start_encode(zse, 2) &&
		  zcbor_tstr_put_lit(zse, "mode") &&
		  zcbor_tstr_put_lit(zse, "merge") &&
		  zcbor_tstr_put_lit(zse, "data") &&
		  zcbor_map_start_encode(zse, 1) &&
		  zcbor_tstr_put_lit(zse, "VIN") &&
		  zcbor_tstr_put_term(zse, device_id, CONFIG_ZCBOR_MAX_STR_LEN) &&
		  zcbor_map_end_encode(zse, 1) &&
		  zcbor_map_end_encode(zse, 2);

	if (!ok) {
		return -EINVAL;
	}
	return (ssize_t)(zse->payload - buf);
}

ssize_t hawkbit_encode_cancel(const struct hawkbit_cancel *cancel, uint8_t *buf, size_t buf_len)
{
	return encode_status(&cancel->status, buf, buf_len);
}

ssize_t hawkbit_encode_dep_fbk(const struct hawkbit_dep_fbk *fbk, uint8_t *buf, size_t buf_len)
{
	return encode_status(&fbk->status, buf, buf_len);
}
