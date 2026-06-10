/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Header file for GPIO hog functions
 * @ingroup gpio_interface
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_GPIO_GPIO_HOGS_H_
#define ZEPHYR_INCLUDE_DRIVERS_GPIO_GPIO_HOGS_H_

/**
 * @addtogroup gpio_interface
 * @{
 */

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/slist.h>
#include <zephyr/tracing/tracing.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @cond INTERNAL_HIDDEN */

struct gpio_hog_dt_spec {
	gpio_pin_t pin;
	gpio_flags_t flags;
};

struct gpio_hogs {
	const struct gpio_hog_dt_spec *specs;
	size_t num_specs;
};

/* Static initializer for a struct gpio_hog_dt_spec */
#define GPIO_HOG_DT_SPEC_GET_BY_IDX(node_id, idx)						\
	{											\
		.pin = DT_GPIO_HOG_PIN_BY_IDX(node_id, idx),					\
		.flags = DT_GPIO_HOG_FLAGS_BY_IDX(node_id, idx) |				\
			 COND_CODE_1(DT_PROP(node_id, input), (GPIO_INPUT),			\
				     (COND_CODE_1(DT_PROP(node_id, output_low),			\
						 (GPIO_OUTPUT_INACTIVE),			\
						 (COND_CODE_1(DT_PROP(node_id, output_high),	\
							     (GPIO_OUTPUT_ACTIVE), (0)))))),	\
	}

/* Expands to 1 if node_id is a GPIO hog, empty otherwise */
#define GPIO_HOGS_NODE_IS_GPIO_HOG(node_id)			\
	IF_ENABLED(DT_PROP_OR(node_id, gpio_hog, 0), 1)

/* Expands to 1 if GPIO controller node_id has GPIO hog children, 0 otherwise */
#define GPIO_HOGS_GPIO_CTLR_HAS_HOGS(node_id)			\
	COND_CODE_0(						\
		IS_EMPTY(DT_FOREACH_CHILD_STATUS_OKAY(node_id,	\
			GPIO_HOGS_NODE_IS_GPIO_HOG)),		\
		(1), (0))

/* Called for GPIO hog indexes */
#define GPIO_HOGS_INIT_GPIO_HOG_BY_IDX(idx, node_id)		\
	GPIO_HOG_DT_SPEC_GET_BY_IDX(node_id, idx)

/* Called for GPIO hog dts nodes */
#define GPIO_HOGS_INIT_GPIO_HOGS(node_id)			\
	LISTIFY(DT_NUM_GPIO_HOGS(node_id),			\
		GPIO_HOGS_INIT_GPIO_HOG_BY_IDX, (,), node_id),

/* Called for GPIO controller dts node children */
#define GPIO_HOGS_COND_INIT_GPIO_HOGS(node_id)			\
	COND_CODE_0(IS_EMPTY(GPIO_HOGS_NODE_IS_GPIO_HOG(node_id)),	\
		    (GPIO_HOGS_INIT_GPIO_HOGS(node_id)), ())

/* Called for each GPIO controller dts node which has GPIO hog children */
#define GPIO_HOGS_INIT_GPIO_CTLR(node_id)				\
	.gpio_hogs = {							\
		.specs = (const struct gpio_hog_dt_spec []) {		\
			DT_FOREACH_CHILD_STATUS_OKAY(node_id,		\
				GPIO_HOGS_COND_INIT_GPIO_HOGS)		\
		},							\
		.num_specs =						\
			DT_FOREACH_CHILD_STATUS_OKAY_SEP(node_id,	\
				DT_NUM_GPIO_HOGS, (+)),			\
	},

#ifdef CONFIG_GPIO_HOGS
/* Called for each GPIO controller dts node */
#define GPIO_HOGS_COND_INIT_GPIO_CTLR(node_id)			\
	IF_ENABLED(GPIO_HOGS_GPIO_CTLR_HAS_HOGS(node_id),	\
		   (GPIO_HOGS_INIT_GPIO_CTLR(node_id)))
#else
#define GPIO_HOGS_COND_INIT_GPIO_CTLR(node_id)
#endif /* CONFIG_GPIO_HOGS */

/** @endcond */

/**
 * @brief Initialize GPIO hogs for a given device
 *
 * This function is called by GPIO drivers to initialize GPIO hogs defined in
 * devicetree for the device. It is expected to be called from gpio_common_init().
 *
 * @param dev GPIO device for which to initialize hogs
 * @return 0 if successful, or a negative error code if initialization failed
 */
int gpio_hogs_init(const struct device *dev);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_DRIVERS_GPIO_GPIO_HOGS_H_ */
