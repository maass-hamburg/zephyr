/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <sys/errno.h>

LOG_MODULE_REGISTER(adc_ade1913, CONFIG_ADC_LOG_LEVEL);

#define ADC_CONTEXT_USES_KERNEL_TIMER
#include "adc_context.h"

#define DT_DRV_COMPAT ti_ade7913

#define ADE7913_SPI_READ BIT(2)

#define ADE7913_REG_IWV          0x00 << 3
#define ADE7913_REG_V1WV         0x01 << 3
#define ADE7913_REG_V2WV         0x02 << 3
struct ade7913_config {
	struct spi_dt_spec spi;
};

struct ade7913_data {
	struct adc_context ctx;

	uint32_t *buffer;
	uint32_t *repeat_buffer;
	struct k_thread thread;
	struct k_sem sem;

	K_KERNEL_STACK_MEMBER(stack, CONFIG_ADC_ADE7913_ACQUISITION_THREAD_STACK_SIZE);
};

static int ade7913_channel_setup(const struct device *dev __unused,
				 const struct adc_channel_cfg *channel_cfg)
{
	if (channel_cfg->gain != ADC_GAIN_1) {
		LOG_ERR("unsupported channel gain %d", channel_cfg->gain);
		return -ENOTSUP;
	}

	if (channel_cfg->reference != ADC_REF_VDD_1) {
		LOG_ERR("unsupported channel reference '%d'", channel_cfg->reference);
		return -ENOTSUP;
	}

	if (channel_cfg->acquisition_time != ADC_ACQ_TIME_DEFAULT) {
		LOG_ERR("unsupported acquisition time '%d'", channel_cfg->acquisition_time);
		return -ENOTSUP;
	}

	if (channel_cfg->channel_id >= 3) {
		LOG_ERR("unsupported channel id '%d'", channel_cfg->channel_id);
		return -ENOTSUP;
	}

	if (channel_cfg->differential) {
		LOG_ERR("unsupported differential mode");
		return -ENOTSUP;
	}

	return 0;
}

static int ade7913_validate_sequence(const struct adc_sequence *sequence)
{
	uint8_t channels = POPCOUNT(sequence->channels);
	size_t needed = channels * sizeof(uint32_t);

	if (!IN_RANGE(channels, 1, 3)) {
		return -EINVAL;
	}

	if (sequence->options) {
		needed *= (1 + sequence->options->extra_samplings);
	}

	if (sequence->buffer_size < needed) {
		return -ENOMEM;
	}

	return 0;
}

static void adc_context_update_buffer_pointer(struct adc_context *ctx, bool repeat_sampling)
{
	struct ade7913_data *data = CONTAINER_OF(ctx, struct ade7913_data, ctx);

	if (repeat_sampling) {
		data->buffer = data->repeat_buffer;
	}
}

static void adc_context_start_sampling(struct adc_context *ctx)
{
	struct ade7913_data *data = CONTAINER_OF(ctx, struct ade7913_data, ctx);

	data->repeat_buffer = data->buffer;
	k_sem_give(&data->sem);
	LOG_DBG("start_sampling");
}

static int ade7913_acquisition_one(const struct device *dev)
{
	struct ade7913_data *data = dev->data;
	const struct ade7913_config *cfg = dev->config;
	uint8_t opcode = ADE7913_SPI_READ;
	uint8_t buffer[3];
	int ret;

	struct spi_buf tx_buf = {
		.buf = &opcode,
		.len = sizeof(opcode),
	};
	struct spi_buf_set tx_buf_set = {
		.buffers = &tx_buf,
		.count = 1,
	};
	struct spi_buf rx_buf[] = {
		{
			.buf = NULL,
			.len = sizeof(opcode),
		},
		{
			.buf = buffer,
			.len = sizeof(buffer),
		},
	};
	struct spi_buf_set rx_buf_set = {
		.buffers = rx_buf,
		.count = 2,
	};

	switch (data->ctx.sequence.channels) {
	case BIT(1):
		opcode |= ADE7913_REG_V1WV;
		break;
	case BIT(2):
		opcode |= ADE7913_REG_V2WV;
		break;
	default:
		break;
	}

	ret = spi_transceive_dt(&cfg->spi, &tx_buf_set, &rx_buf_set);
	if (ret < 0) {
		return ret;
	}

	*data->buffer = sys_get_be24(buffer);
	data->buffer++;
	return 0;
}

static int ade7913_acquisition_all(const struct device *dev)
{
	struct ade7913_data *data = dev->data;
	const struct ade7913_config *cfg = dev->config;
	uint8_t opcode = ADE7913_SPI_READ;
	uint8_t buffer[9];
	int ret;

	struct spi_buf tx_buf = {
		.buf = &opcode,
		.len = sizeof(opcode),
	};
	struct spi_buf_set tx_buf_set = {
		.buffers = &tx_buf,
		.count = 1,
	};
	struct spi_buf rx_buf[] = {
		{
			.buf = NULL,
			.len = sizeof(opcode),
		},
		{
			.buf = buffer,
			.len = sizeof(buffer),
		},
	};
	struct spi_buf_set rx_buf_set = {
		.buffers = rx_buf,
		.count = 2,
	};

	ret = spi_transceive_dt(&cfg->spi, &tx_buf_set, &rx_buf_set);
	if (ret < 0) {
		return ret;
	}

	if (IS_BIT_SET(data->ctx.sequence.channels, 0)) {
		*data->buffer = sys_get_be24(&buffer[0]);
		data->buffer++;
	}

	if (IS_BIT_SET(data->ctx.sequence.channels, 1)) {
		*data->buffer = sys_get_be24(&buffer[3]);
		data->buffer++;
	}

	if (IS_BIT_SET(data->ctx.sequence.channels, 2)) {
		*data->buffer = sys_get_be24(&buffer[6]);
		data->buffer++;
	}

	return 0;
}

static int ade7913_acquisition(const struct device *dev)
{
	struct ade7913_data *data = dev->data;

	if (POPCOUNT(data->ctx.sequence.channels) == 1) {
		return ade7913_acquisition_one(dev);
	}

	return ade7913_acquisition_all(dev);
}

static int ade7913_start_read(const struct device *dev, const struct adc_sequence *sequence)
{
	struct ade7913_data *data = dev->data;
	int ret;

	if (sequence->resolution != 24) {
		LOG_ERR("unsupported resolution %d", sequence->resolution);
		return -ENOTSUP;
	}

	if (find_msb_set(sequence->channels) > 3) {
		LOG_ERR("unsupported channels in mask: 0x%08x", sequence->channels);
		return -ENOTSUP;
	}

	if (sequence->calibrate) {
		LOG_ERR("unsupported calibration");
		return -ENOTSUP;
	}

	if (sequence->oversampling) {
		LOG_ERR("oversampling not supported");
		return -ENOTSUP;
	}

	ret = ade7913_validate_sequence(sequence);
	if (ret) {
		LOG_ERR("invalid sequence / buffer too small");
		return ret;
	}

	data->buffer = sequence->buffer;
	data->repeat_buffer = data->buffer;

	adc_context_start_read(&data->ctx, sequence);

	return adc_context_wait_for_completion(&data->ctx);
}

static int ade7913_read_async(const struct device *dev, const struct adc_sequence *sequence,
			      struct k_poll_signal *async)
{
	struct ade7913_data *data = dev->data;
	int ret;

	adc_context_lock(&data->ctx, async ? true : false, async);
	ret = ade7913_start_read(dev, sequence);
	adc_context_release(&data->ctx, ret);

	return ret;
}

static int ade7913_read(const struct device *dev, const struct adc_sequence *sequence)
{
	return ade7913_read_async(dev, sequence, NULL);
}

static void ade7913_acquisition_thread(void *p1, void *p2, void *p3 __unused)
{
	const struct device *dev = p1;
	struct ade7913_data *data = p2;
	int ret;

	while (true) {
		k_sem_take(&data->sem, K_FOREVER);

		ret = ade7913_acquisition(dev);
		if (ret) {
			adc_context_complete(&data->ctx, ret);
			continue;
		}

		adc_context_on_sampling_done(&data->ctx, dev);
	}
}

int ade7913_init(const struct device *dev)
{
	const struct ade7913_config *config = dev->config;
	struct ade7913_data *data = dev->data;

	adc_context_init(&data->ctx);

	if (!spi_is_ready_dt(&config->spi)) {
		LOG_ERR("SPI bus %s not ready", config->spi.bus->name);
		return -ENODEV;
	}
	k_sem_init(&data->sem, 0, 1);

	k_thread_create(&data->thread, data->stack, K_KERNEL_STACK_SIZEOF(data->stack),
			ade7913_acquisition_thread, dev, data, NULL,
			CONFIG_ADC_ADE7913_ACQUISITION_THREAD_PRIORITY, 0, K_NO_WAIT);

	adc_context_unlock_unconditionally(&data->ctx);

	return 0;
}

static DEVICE_API(adc, ade7913_api) = {
	.channel_setup = ade7913_channel_setup,
	.read = ade7913_read,
#ifdef CONFIG_ADC_ASYNC
	.read_async = ade7913_read_async,
#endif
	.ref_internal = 1200,
};

#define ADE7913_SPI_OP (SPI_OP_MODE_MASTER | SPI_MODE_CPOL | SPI_MODE_CPHA | SPI_WORD_SET(8))

#define ADE7913_INIT(n)                                                                            \
	static struct ade7913_data ade7913_data_##n;                                               \
	static const struct ade7913_config ade7913_cfg_##n = {                                     \
		.spi = SPI_DT_SPEC_INST_GET(n, ADE7913_SPI_OP),                                    \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, ade7913_init, NULL, &ade7913_data_##n, &ade7913_cfg_##n,      \
			      POST_KERNEL, CONFIG_ADC_ADE7913_INIT_PRIORITY, &ade7913_api);

DT_INST_FOREACH_STATUS_OKAY(ADE7913_INIT)
