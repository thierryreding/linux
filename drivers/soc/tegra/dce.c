// SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2023 NVIDIA CORPORATION.  All rights reselved.
 */

#include <linux/completion.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/mailbox_client.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <soc/tegra/ivc.h>

#define DCE_IRQ_STATUS_TYPE		GENMASK(30, 27)
#define DCE_IRQ_STATUS_TYPE_IRQ		0x0
#define DCE_IRQ_STATUS_TYPE_BOOT_CMD	0x1

#define DCE_IRQ_STATUS		GENMASK(23, 0)
#define DCE_IRQ_READY		BIT(23)
#define DCE_IRQ_LOG_OVERFLOW	BIT(22)
#define DCE_IRQ_LOG_READY	BIT(21)
#define DCE_IRQ_CRASH_LOG	BIT(20)
#define DCE_IRQ_ABORT		BIT(19)
#define DCE_IRQ_SC7_ENTERED	BIT(18)

#define DCE_BOOT_STATUS_ERROR		BIT(23)
#define DCE_BOOT_STATUS_ERROR_MASK	GENMASK(22, 0)
#define DCE_BOOT_STATUS_SUCCESS		0x00
#define DCE_BOOT_STATUS_BAD_COMMAND	0x01
#define DCE_BOOT_STATUS_NOT_IMPLEMENTED	0x02
#define DCE_BOOT_STATUS_IPC_SETUP	0x03
#define DCE_BOOT_STATUS_INVALID_NFRAMES	0x04
#define DCE_BOOT_STATUS_IPC_CREATE	0x05
#define DCE_BOOT_STATUS_LOCKED		0x06

#define DCE_BOOT_CMD_GO		BIT(31)
#define DCE_BOOT_CMD_COMMAND	GENMASK(30, 27)
#define DCE_BOOT_CMD_HILO	BIT(25)
#define DCE_BOOT_CMD_RDWR	BIT(24)
#define DCE_BOOT_CMD_PARAM	GENMASK(19, 0)

#define DCE_BOOT_CMD(cmd, hilo, rdwr, param)	\
	FIELD_PREP(DCE_BOOT_CMD_COMMAND, cmd) |	\
	FIELD_PREP(DCE_BOOT_CMD_HILO, hilo) |	\
	FIELD_PREP(DCE_BOOT_CMD_RDWR, rdwr) |	\
	FIELD_PREP(DCE_BOOT_CMD_PARAM, param)

#define DCE_BOOT_CMD_VERSION		0x00
#define DCE_BOOT_CMD_SET_SID		0x01
#define DCE_BOOT_CMD_CHANNEL_INIT	0x02
#define DCE_BOOT_CMD_SET_ADDR		0x03
#define DCE_BOOT_CMD_GET_FRAME_SIZE	0x04
#define DCE_BOOT_CMD_SET_NFRAMES	0x05
#define DCE_BOOT_CMD_RESET		0x06
#define DCE_BOOT_CMD_LOCK		0x07
#define DCE_BOOT_CMD_SET_AST_LENGTH	0x08
#define DCE_BOOT_CMD_SET_AST_IOVA	0x09
#define DCE_BOOT_CMD_SET_FRAME_SIZE	0x0a

struct tegra_dce;
struct tegra_dce_channel;

struct tegra_dce_channel_soc {
	const char *name;
	unsigned int num_frames;
	size_t frame_size;

	void (*prepare)(struct mbox_client *client, void *msg);
	void (*done)(struct mbox_client *client, void *msg, int r);
	void (*callback)(struct mbox_client *client, void *msg);
};

struct tegra_dce_soc {
	const struct tegra_dce_channel_soc *channels;
	unsigned int num_channels;
};

struct tegra_dce_channel {
	struct tegra_dce *dce;
	const struct tegra_dce_channel_soc *soc;
	struct tegra_ivc *ivc;

	struct completion done;
	u32 status;

	struct {
		struct mbox_client client;
		struct mbox_chan *channel;
	} rx;

	struct {
		struct mbox_client client;
		struct mbox_chan *channel;
	} tx;
};

struct tegra_dce {
	const struct tegra_dce_soc *soc;
	struct device *dev;

	struct {
		struct mbox_client client;
		struct mbox_chan *channel;
		int status;
	} boot;

	struct {
		struct mbox_client client;
		struct mbox_chan *channel;
		struct completion done;
	} irq;

	dma_addr_t iova;
	size_t size;
	void *virt;

	struct tegra_dce_channel *channels;
	unsigned int num_channels;
};

static int tegra_dce_channel_reset(struct tegra_dce_channel *channel)
{
	ktime_t timeout = ktime_add_us(ktime_get(), USEC_PER_SEC);

	tegra_ivc_reset(channel->ivc);

	do {
		if (tegra_ivc_notified(channel->ivc) == 0)
			return 0;

		usleep_range(1000, 2000);
	} while (ktime_compare(ktime_get(), timeout) > 0);

	/*
	while (tegra_ivc_notified(channel->ivc)) {
		dev_info(dev, "waiting for IVC to reset\n");
		msleep(250);
	}
	*/

	return -ETIMEDOUT;
}

static int tegra_dce_boot_get_version(struct tegra_dce *dce, u32 *version)
{
	u32 command = FIELD_PREP(DCE_BOOT_CMD_COMMAND, DCE_BOOT_CMD_VERSION);
	void *msg = (void *)(unsigned long)command;
	unsigned long status;
	int err;

	err = mbox_send_message(dce->boot.channel, msg);
	if (err < 0)
		return err;

	status = wait_for_completion_timeout(&dce->irq.done, 1000);
	if (status == 0)
		return -ETIMEDOUT;

	if (dce->boot.status < 0)
		return dce->boot.status;

	*version = dce->boot.status;

	return 0;
}

static int tegra_dce_boot_set_stream_id(struct tegra_dce *dce, u32 stream_id)
{
	u32 command = FIELD_PREP(DCE_BOOT_CMD_COMMAND, DCE_BOOT_CMD_SET_SID) |
		      FIELD_PREP(DCE_BOOT_CMD_PARAM, stream_id);
	void *msg = (void *)(unsigned long)command;
	unsigned long status;
	int err;

	err = mbox_send_message(dce->boot.channel, msg);
	if (err < 0)
		return err;

	status = wait_for_completion_timeout(&dce->irq.done, 1000);
	if (status == 0)
		return -ETIMEDOUT;

	if (dce->boot.status < 0)
		return dce->boot.status;

	dev_info(dce->dev, "stream ID set: %08x\n", dce->boot.status);

	return 0;
}

#define DCE_BOOT_WRITE BIT(0)
#define DCE_BOOT_HIGH  BIT(1)

static int tegra_dce_boot_send(struct tegra_dce *dce, u32 command, u32 value,
			       unsigned int flags)
{
	u32 message;
	void *msg;

	message = FIELD_PREP(DCE_BOOT_CMD_COMMAND, command) |
		  FIELD_PREP(DCE_BOOT_CMD_PARAM, value);

	if (flags & DCE_BOOT_WRITE)
		message |= FIELD_PREP(DCE_BOOT_CMD_RDWR, 1);

	if (flags & DCE_BOOT_HIGH)
		message |= FIELD_PREP(DCE_BOOT_CMD_HILO, 1);

	msg = (void *)(unsigned long)message;

	return mbox_send_message(dce->boot.channel, msg);
}

static int tegra_dce_boot_exec(struct tegra_dce *dce, u32 command, u32 param,
			       u32 *value, unsigned int flags,
			       unsigned long timeout)
{
	unsigned long status;
	int err;

	err = tegra_dce_boot_send(dce, command, param, flags);
	if (err < 0)
		return err;

	status = wait_for_completion_timeout(&dce->irq.done, timeout);
	if (status == 0)
		return -ETIMEDOUT;

	if (dce->boot.status < 0)
		return dce->boot.status;

	if (value)
		*value = dce->boot.status;

	return 0;
}

static int tegra_dce_boot_set_ast_iova_info(struct tegra_dce *dce)
{
	unsigned long status;
	u32 command;
	void *msg;
	int err;

	command = FIELD_PREP(DCE_BOOT_CMD_COMMAND, DCE_BOOT_CMD_SET_AST_LENGTH) |
		  FIELD_PREP(DCE_BOOT_CMD_HILO, 1) |
		  FIELD_PREP(DCE_BOOT_CMD_PARAM, dce->size >> 20);
	msg = (void *)(unsigned long)command;

	err = mbox_send_message(dce->boot.channel, msg);
	if (err < 0)
		return err;

	status = wait_for_completion_timeout(&dce->irq.done, 1000);
	if (status == 0)
		return -ETIMEDOUT;

	if (dce->boot.status < 0)
		return dce->boot.status;

	dev_info(dce->dev, "  SET_AST_LENGTH(HI): %08x\n", dce->boot.status);

	command = FIELD_PREP(DCE_BOOT_CMD_COMMAND, DCE_BOOT_CMD_SET_AST_LENGTH) |
		  FIELD_PREP(DCE_BOOT_CMD_HILO, 0) |
		  FIELD_PREP(DCE_BOOT_CMD_PARAM, dce->size);
	msg = (void *)(unsigned long)command;

	err = mbox_send_message(dce->boot.channel, msg);
	if (err < 0)
		return err;

	status = wait_for_completion_timeout(&dce->irq.done, 1000);
	if (status == 0)
		return -ETIMEDOUT;

	if (dce->boot.status < 0)
		return dce->boot.status;

	dev_info(dce->dev, "  SET_AST_LENGTH(LO): %08x\n", dce->boot.status);

	command = FIELD_PREP(DCE_BOOT_CMD_COMMAND, DCE_BOOT_CMD_SET_AST_IOVA) |
		  FIELD_PREP(DCE_BOOT_CMD_HILO, 1) |
		  FIELD_PREP(DCE_BOOT_CMD_PARAM, dce->iova >> 20);
	msg = (void *)(unsigned long)command;

	err = mbox_send_message(dce->boot.channel, msg);
	if (err < 0)
		return err;

	status = wait_for_completion_timeout(&dce->irq.done, 1000);
	if (status == 0)
		return -ETIMEDOUT;

	if (dce->boot.status < 0)
		return dce->boot.status;

	dev_info(dce->dev, "  SET_AST_IOVA(HI): %08x\n", dce->boot.status);

	command = FIELD_PREP(DCE_BOOT_CMD_COMMAND, DCE_BOOT_CMD_SET_AST_IOVA) |
		  FIELD_PREP(DCE_BOOT_CMD_HILO, 0) |
		  FIELD_PREP(DCE_BOOT_CMD_PARAM, dce->iova);
	msg = (void *)(unsigned long)command;

	err = mbox_send_message(dce->boot.channel, msg);
	if (err < 0)
		return err;

	status = wait_for_completion_timeout(&dce->irq.done, 1000);
	if (status == 0)
		return -ETIMEDOUT;

	if (dce->boot.status < 0)
		return dce->boot.status;

	dev_info(dce->dev, "  SET_AST_IOVA(LO): %08x\n", dce->boot.status);

	return 0;
}

static int tegra_dce_boot_set_addr_read(struct tegra_dce *dce, dma_addr_t phys)
{
	int err;

	err = tegra_dce_boot_exec(dce, DCE_BOOT_CMD_SET_ADDR, phys >> 20, NULL,
				  DCE_BOOT_HIGH, 1000);
	if (err < 0)
		return err;

	err = tegra_dce_boot_exec(dce, DCE_BOOT_CMD_SET_ADDR, phys, NULL, 0,
				  1000);
	if (err < 0)
		return err;

	return 0;
}

static int tegra_dce_boot_set_addr_write(struct tegra_dce *dce, dma_addr_t phys)
{
	int err;

	err = tegra_dce_boot_exec(dce, DCE_BOOT_CMD_SET_ADDR, phys >> 20, NULL,
				  DCE_BOOT_WRITE | DCE_BOOT_HIGH, 1000);
	if (err < 0)
		return err;

	err = tegra_dce_boot_exec(dce, DCE_BOOT_CMD_SET_ADDR, phys, NULL,
				  DCE_BOOT_WRITE, 1000);
	if (err < 0)
		return err;

	return 0;
}

static int tegra_dce_boot_get_frame_size(struct tegra_dce *dce, size_t *frame_size)
{
	u32 value;
	int err;

	err = tegra_dce_boot_exec(dce, DCE_BOOT_CMD_GET_FRAME_SIZE, 0, &value,
				  0, 1000);
	if (err < 0)
		return err;

	if (frame_size)
		*frame_size = value;

	return 0;
}

static int tegra_dce_boot_set_frames(struct tegra_dce *dce, unsigned int frames)
{
	int err;

	err = tegra_dce_boot_exec(dce, DCE_BOOT_CMD_SET_NFRAMES, frames, NULL,
				  0, 1000);
	if (err < 0)
		return err;

	return 0;
}

static int tegra_dce_boot_set_frame_size(struct tegra_dce *dce, size_t frame_size)
{
	int err;

	err = tegra_dce_boot_exec(dce, DCE_BOOT_CMD_SET_FRAME_SIZE, frame_size,
				  NULL, 0, 1000);
	if (err < 0)
		return err;

	return 0;
}

static int tegra_dce_boot_channel_init(struct tegra_dce *dce)
{
	int err;

	err = tegra_dce_boot_exec(dce, DCE_BOOT_CMD_CHANNEL_INIT, 0, NULL, 0,
				  1000);
	if (err < 0)
		return err;

	return err;
}

static int tegra_dce_boot_lock(struct tegra_dce *dce)
{
	int err;

	err = tegra_dce_boot_exec(dce, DCE_BOOT_CMD_LOCK, 0, NULL, 0, 1000);
	if (err < 0)
		return err;

	return err;
}

static void tegra_dce_irq_callback(struct mbox_client *cl, void *msg)
{
	struct tegra_dce *dce = container_of(cl, struct tegra_dce, irq.client);
	u32 value = (u32)(unsigned long)msg, type, status;

	dev_info(cl->dev, "> %s(cl=%px, msg=%px)\n", __func__, cl, msg);
	dev_info(cl->dev, "  dce: %s\n", dev_name(dce->dev));
	dev_info(cl->dev, "  value: %08x\n", value);

	type = FIELD_GET(DCE_IRQ_STATUS_TYPE, value);
	status = FIELD_GET(DCE_IRQ_STATUS, value);

	dev_info(cl->dev, "  type: %02x\n", type);
	dev_info(cl->dev, "  status: %06x\n", status);

	switch (type) {
	case DCE_IRQ_STATUS_TYPE_IRQ:
		dev_info(cl->dev, "  IRQ: %08x\n", status);

		if (status & DCE_IRQ_READY)
			dev_info(cl->dev, "    ready\n");

		if (status & DCE_IRQ_LOG_OVERFLOW)
			dev_info(cl->dev, "    log overflow\n");

		if (status & DCE_IRQ_LOG_READY)
			dev_info(cl->dev, "    log buffers available\n");

		if (status & DCE_IRQ_CRASH_LOG)
			dev_info(cl->dev, "    crash log available\n");

		if (status & DCE_IRQ_ABORT)
			dev_info(cl->dev, "    ucode abort occurred\n");

		if (status & DCE_IRQ_SC7_ENTERED)
			dev_info(cl->dev, "    DCE state saved, can be powered off\n");

		break;

	case DCE_IRQ_STATUS_TYPE_BOOT_CMD:
		dev_info(cl->dev, "  boot command: %08x\n", status);

		if (status & DCE_BOOT_STATUS_ERROR) {
			switch (status & DCE_BOOT_STATUS_ERROR_MASK) {
			case DCE_BOOT_STATUS_BAD_COMMAND:
				dev_info(cl->dev, "error: bad command\n");
				dce->boot.status = -ENXIO;
				break;

			case DCE_BOOT_STATUS_NOT_IMPLEMENTED:
				dev_info(cl->dev, "error: not implemented\n");
				dce->boot.status = -ENXIO;
				break;

			case DCE_BOOT_STATUS_IPC_SETUP:
				dev_info(cl->dev, "error: IPC setup\n");
				dce->boot.status = -ENXIO;
				break;

			case DCE_BOOT_STATUS_INVALID_NFRAMES:
				dev_info(cl->dev, "error: invalid n-frames\n");
				dce->boot.status = -ENXIO;
				break;

			case DCE_BOOT_STATUS_IPC_CREATE:
				dev_info(cl->dev, "error: IPC create\n");
				dce->boot.status = -ENXIO;
				break;

			case DCE_BOOT_STATUS_LOCKED:
				dev_info(cl->dev, "error: locked\n");
				dce->boot.status = -ENXIO;
				break;

			default:
				dev_info(cl->dev, "error: unknown: %08x\n", status);
				dce->boot.status = -ENXIO;
				break;
			}
		} else {
			dce->boot.status = status;
			complete(&dce->irq.done);
		}

		break;

	default:
		dev_info(cl->dev, "  invalid status: %08x\n", value);
		break;
	}

	dev_info(cl->dev, "< %s()\n", __func__);
}

static void tegra_dce_boot_prepare(struct mbox_client *cl, void *msg)
{
	dev_info(cl->dev, "> %s(cl=%px, msg=%px)\n", __func__, cl, msg);
	dev_info(cl->dev, "< %s()\n", __func__);
}

static void tegra_dce_boot_done(struct mbox_client *cl, void *msg, int r)
{
	dev_info(cl->dev, "> %s(cl=%px, msg=%px, r=%d)\n", __func__, cl, msg, r);
	dev_info(cl->dev, "< %s()\n", __func__);
}

static void tegra_dce_channel_notify(struct tegra_ivc *ivc, void *data)
{
	struct tegra_dce_channel *channel = data;
	struct tegra_dce *dce = channel->dce;
	int err;

	dev_info(dce->dev, "> %s(ivc=%px, data=%px)\n", __func__, ivc, data);

	err = mbox_send_message(channel->tx.channel, 0);
	if (err < 0)
		dev_err(channel->dce->dev, "failed to send message: %d\n", err);

	dev_info(dce->dev, "< %s()\n", __func__);
}

struct tegra_dce_frame {
	u32 length;
	u8 data[0];
};

struct tegra_dce_message {
	struct {
		const void *data;
		size_t size;
	} tx;

	struct {
		void *data;
		size_t size;
	} rx;
};

#define DCE_ADMIN_CMD_VERSION 0x00
#define DCE_ADMIN_CMD_HOST_VERSION 0x01
#define DCE_ADMIN_CMD_GET_FW_VERSION 0x02
#define DCE_ADMIN_CMD_ECHO 0x03
#define DCE_ADMIN_CMD_MEM_MAP 0x04
#define DCE_ADMIN_CMD_MEM_INFO 0x05
#define DCE_ADMIN_CMD_IPC_INFO 0x06
#define DCE_ADMIN_CMD_IPC_CREATE 0x07
#define DCE_ADMIN_CMD_PREPARE_SC7 0x08
#define DCE_ADMIN_CMD_ENTER_SC7 0x09
#define DCE_ADMIN_CMD_SET_LOGGING 0x0a
#define DCE_ADMIN_CMD_GET_LOG_INFO 0x0b
#define DCE_ADMIN_CMD_LOCK_CHANGES 0x0c
#define DCE_ADMIN_CMD_CODE_COVERAGE_START 0x0d
#define DCE_ADMIN_CMD_CODE_COVERAGE_STOP 0x0e
#define DCE_ADMIN_CMD_PERF_START 0x0f
#define DCE_ADMIN_CMD_PERF_STOP 0x10
#define DCE_ADMIN_CMD_INT_TEST_START 0x11
#define DCE_ADMIN_CMD_INT_TEST_STOP 0x12
#define DCE_ADMIN_CMD_EXT_TEST 0x13
#define DCE_ADMIN_CMD_DEBUG 0x14
#define DCE_ADMIN_CMD_RM_BOOTSTRAP 0x15
#define DCE_ADMIN_CMD_NEXT 0x16

struct tegra_dce_admin_version_info {
	u32 version;
};

struct tegra_dce_admin_fw_version_info {
	u32 bootstrap_interface;
	u32 admin_interface;
	u32 driver_headers;
	u32 core_interface;
	u8 fw_version[4];
	u32 gcid_revision;
	u8 safertos_major;
	u8 safertos_minor;
};

struct tegra_dce_admin_echo {
	u32 data;
};

enum tegra_dce_admin_ext_test {
	DCE_ADMIN_EXT_TEST_ALU = 0,
	DCE_ADMIN_EXT_TEST_DMA = 1,
};

struct tegra_dce_admin_ext_test_args {
	enum tegra_dce_admin_ext_test test;
};

struct tegra_dce_admin_log_args {
	u32 log_enable;
	u32 log_level;
};

struct tegra_dce_admin_mem_args {
	u32 region;
	u64 iova;
	u32 length;
	u32 sid;
};

struct tegra_dce_admin_ipc_info_args {
	u32 type;
};

struct tegra_dce_admin_ipc_signal {
	u32 type;
	union {
		u32 mailbox;
		struct {
			u32 num;
			u32 bit;
		} doorbell;
	} signal;
	struct {
		u32 num;
		u32 bit;
	} semaphore;
};

struct tegra_dce_admin_ipc_info {
	u32 type;
	u32 flags;
	u32 mem_region;
	u64 rd_iova;
	u64 wr_iova;
	u32 fsize;
	u32 n_frames;
	struct tegra_dce_admin_ipc_signal signal_from_dce;
	struct tegra_dce_admin_ipc_signal signal_to_dce;
};

struct tegra_dce_admin_ipc_create_args {
	u32 type;
	u64 rd_iova;
	u64 wr_iova;
	u32 fsize;
	u32 n_frames;
};

struct tegra_dce_admin_ipc_request {
	u32 cmd;
	union {
		struct tegra_dce_admin_version_info version;
		struct tegra_dce_admin_echo echo;
		struct tegra_dce_admin_ext_test_args ext_test;
		struct tegra_dce_admin_log_args log;
		struct tegra_dce_admin_ipc_info_args ipc_info;
		struct tegra_dce_admin_mem_args mem_map;
		struct tegra_dce_admin_ipc_create_args ipc_create;
	} args;
};

struct tegra_dce_admin_ipc_response {
	u32 error;
	union {
		struct tegra_dce_admin_version_info version;
		struct tegra_dce_admin_echo echo;
		struct tegra_dce_admin_log_args log;
		struct tegra_dce_admin_ipc_info ipc;
		struct tegra_dce_admin_mem_args mem_info;
		struct tegra_dce_admin_fw_version_info fw_version;
	} args;
};

static int tegra_dce_channel_send(struct tegra_dce_channel *channel,
				  const void *data, size_t size)
{
	struct iosys_map map;
	int err;

	err = tegra_ivc_write_get_next_frame(channel->ivc, &map);
	if (err < 0)
		return err;

	iosys_map_wr_field(&map, 0, struct tegra_dce_frame, length, size);
	iosys_map_memcpy_to(&map, offsetof(struct tegra_dce_frame, data),
			    data, size);

	return tegra_ivc_write_advance(channel->ivc);
}

static int tegra_dce_channel_recv(struct tegra_dce_channel *channel,
				  void *data, size_t size)
{
	struct iosys_map map;
	size_t length;
	int err;

	dev_info(channel->dce->dev, "> %s(channel=%px, data=%px, size=%zu)\n",
		 __func__, channel, data, size);

	err = tegra_ivc_read_get_next_frame(channel->ivc, &map);
	if (err < 0)
		return err;

	length = iosys_map_rd_field(&map, 0, struct tegra_dce_frame, length);
	dev_info(channel->dce->dev, "  length: %zu\n", length);
	iosys_map_memcpy_from(data, &map, offsetof(struct tegra_dce_frame, data), size);

	err = tegra_ivc_read_advance(channel->ivc);
	if (err < 0)
		return err;

	dev_info(channel->dce->dev, "< %s()\n", __func__);
	return 0;
}

static int tegra_dce_channel_transfer(struct tegra_dce_channel *channel,
				      struct tegra_dce_message *msg,
				      unsigned long timeout)
{
	unsigned long status;
	int err;

	err = tegra_dce_channel_send(channel, msg->tx.data, msg->tx.size);
	if (err < 0) {
		dev_err(channel->dce->dev, "failed to send request: %d\n", err);
		return err;
	}

	status = wait_for_completion_timeout(&channel->done, timeout);
	if (status == 0) {
		dev_err(channel->dce->dev, "timeout waiting for response\n");
		return -ETIMEDOUT;
	}

	err = tegra_dce_channel_recv(channel, msg->rx.data, msg->rx.size);
	if (err < 0) {
		dev_err(channel->dce->dev, "failed to receive response: %d\n", err);
		return err;
	}

	return 0;
}

static ssize_t tegra_dce_channel_init(struct tegra_dce *dce,
				      struct tegra_dce_channel *channel,
				      const struct tegra_dce_channel_soc *soc,
				      size_t offset)
{
	size_t message_size, size;
	struct iosys_map rx, tx;
	char name[32];
	int err;

	channel->dce = dce;
	channel->soc = soc;

	snprintf(name, sizeof(name), "%s-tx", soc->name);
	channel->tx.client.dev = dce->dev;
	channel->tx.client.tx_prepare = soc->prepare;
	channel->tx.client.tx_done = soc->done;

	channel->tx.channel = mbox_request_channel_byname(&channel->tx.client,
							 name);
	if (IS_ERR(channel->tx.channel)) {
		err = PTR_ERR(channel->tx.channel);
		dev_err(dce->dev, "failed to get %s mailbox: %d\n", name, err);
		return err;
	}

	snprintf(name, sizeof(name), "%s-rx", soc->name);
	channel->rx.client.dev = dce->dev;
	channel->rx.client.rx_callback = soc->callback;

	channel->rx.channel = mbox_request_channel_byname(&channel->rx.client,
							  name);
	if (IS_ERR(channel->rx.channel)) {
		err = PTR_ERR(channel->rx.channel);
		dev_err(dce->dev, "failed to get %s mailbox: %d\n", name, err);
		return err;
	}

	init_completion(&channel->done);

	channel->ivc = devm_kzalloc(dce->dev, sizeof(*channel->ivc),
				    GFP_KERNEL);
	if (!channel->ivc)
		return -ENOMEM;

	message_size = tegra_ivc_align(soc->frame_size);
	size = tegra_ivc_total_queue_size(message_size * soc->num_frames);

	iosys_map_set_vaddr(&rx, dce->virt + offset);
	iosys_map_set_vaddr(&tx, dce->virt + offset + size);

	err = tegra_ivc_init(channel->ivc, NULL, &rx, dce->iova + offset,
			     &tx, dce->iova + offset + size, soc->num_frames,
			     message_size, tegra_dce_channel_notify, channel);
	if (err < 0)
		return err;

	return offset + size * 2;
}

static void tegra_dce_admin_prepare(struct mbox_client *client, void *msg)
{
	dev_info(client->dev, "> %s(client=%px, msg=%px)\n", __func__, client, msg);
	dev_info(client->dev, "< %s()\n", __func__);
}

static void tegra_dce_admin_done(struct mbox_client *client, void *msg, int r)
{
	dev_info(client->dev, "> %s(client=%px, msg=%px, r=%d)\n", __func__, client, msg, r);
	dev_info(client->dev, "< %s()\n", __func__);
}

static void tegra_dce_admin_callback(struct mbox_client *client, void *msg)
{
	struct tegra_dce_channel *channel;

	dev_info(client->dev, "> %s(client=%px, msg=%px)\n", __func__, client, msg);

	channel = container_of(client, struct tegra_dce_channel, rx.client);
	dev_info(client->dev, "  channel: %px (dev: %s)\n", channel, dev_name(channel->dce->dev));
	complete(&channel->done);

	dev_info(client->dev, "< %s()\n", __func__);
}

static void tegra_dce_rm_prepare(struct mbox_client *client, void *msg)
{
	dev_info(client->dev, "> %s(client=%px, msg=%px)\n", __func__, client, msg);
	dev_info(client->dev, "< %s()\n", __func__);
}

static void tegra_dce_rm_done(struct mbox_client *client, void *msg, int r)
{
	dev_info(client->dev, "> %s(client=%px, msg=%px, r=%d)\n", __func__, client, msg, r);
	dev_info(client->dev, "< %s()\n", __func__);
}

static void tegra_dce_rm_callback(struct mbox_client *client, void *msg)
{
	dev_info(client->dev, "> %s(client=%px, msg=%px)\n", __func__, client, msg);
	dev_info(client->dev, "< %s()\n", __func__);
}

#if 0
static void tegra_dce_hdcp_prepare(struct mbox_client *client, void *msg)
{
	dev_info(client->dev, "> %s(client=%px, msg=%px)\n", __func__, client, msg);
	dev_info(client->dev, "< %s()\n", __func__);
}

static void tegra_dce_hdcp_done(struct mbox_client *client, void *msg, int r)
{
	dev_info(client->dev, "> %s(client=%px, msg=%px, r=%d)\n", __func__, client, msg, r);
	dev_info(client->dev, "< %s()\n", __func__);
}

static void tegra_dce_hdcp_callback(struct mbox_client *client, void *msg)
{
	dev_info(client->dev, "> %s(client=%px, msg=%px)\n", __func__, client, msg);
	dev_info(client->dev, "< %s()\n", __func__);
}
#endif

static void tegra_dce_notify_prepare(struct mbox_client *client, void *msg)
{
	dev_info(client->dev, "> %s(client=%px, msg=%px)\n", __func__, client, msg);
	dev_info(client->dev, "< %s()\n", __func__);
}

static void tegra_dce_notify_done(struct mbox_client *client, void *msg, int r)
{
	dev_info(client->dev, "> %s(client=%px, msg=%px, r=%d)\n", __func__, client, msg, r);
	dev_info(client->dev, "< %s()\n", __func__);
}

static void tegra_dce_notify_callback(struct mbox_client *client, void *msg)
{
	dev_info(client->dev, "> %s(client=%px, msg=%px)\n", __func__, client, msg);
	dev_info(client->dev, "< %s()\n", __func__);
}

static int tegra_dce_admin_init(struct tegra_dce *dce)
{
	struct tegra_dce_admin_ipc_response response;
	struct tegra_dce_admin_ipc_request request;
	struct tegra_dce_channel *channel, *rm;
	struct tegra_dce_message msg;
	int err;

	dev_info(dce->dev, "> %s(dce=%px)\n", __func__, dce);

	/* XXX find by name? */
	channel = &dce->channels[0];

	err = tegra_dce_channel_reset(channel);
	if (err < 0) {
		dev_err(dce->dev, "failed to reset admin channel: %d\n", err);
		return err;
	}

	memset(&request, 0, sizeof(request));
	request.cmd = DCE_ADMIN_CMD_VERSION;

	memset(&response, 0, sizeof(response));

	memset(&msg, 0, sizeof(msg));
	msg.tx.data = &request;
	msg.tx.size = sizeof(request);
	msg.rx.data = &response;
	msg.rx.size = sizeof(response);

	err = tegra_dce_channel_transfer(channel, &msg, 1000);
	if (err < 0) {
		dev_err(dce->dev, "failed to get admin version info: %d\n", err);
		return err;
	}

	dev_info(dce->dev, "admin version info: %08x\n", response.args.version.version);

	rm = &dce->channels[1];

	memset(&request, 0, sizeof(request));
	request.cmd = DCE_ADMIN_CMD_IPC_CREATE;
	request.args.ipc_create.type = 0x01; /* DCE_IPC_TYPE_DISPRM */
	request.args.ipc_create.rd_iova = rm->ivc->rx.phys;
	request.args.ipc_create.wr_iova = rm->ivc->tx.phys;
	request.args.ipc_create.fsize = rm->ivc->frame_size;
	request.args.ipc_create.n_frames = rm->ivc->num_frames;

	memset(&response, 0, sizeof(response));

	memset(&msg, 0, sizeof(msg));
	msg.tx.data = &request;
	msg.tx.size = sizeof(request);
	msg.rx.data = &response;
	msg.rx.size = sizeof(response);

	err = tegra_dce_channel_transfer(channel, &msg, 1000);
	if (err < 0) {
		dev_err(dce->dev, "failed to create RM channel: %d\n", err);
		return err;
	}

	err = tegra_dce_channel_reset(rm);
	if (err < 0) {
		dev_err(dce->dev, "failed to reset RM channel: %d\n", err);
		return err;
	}

	dev_info(dce->dev, "< %s()\n", __func__);
	return 0;
}

static int tegra_dce_bind(struct device *dev)
{
	struct tegra_dce *dce = dev_get_drvdata(dev);
	struct tegra_dce_channel *channel;
	size_t size = 0, offset = 0;
	u32 version, stream_id;
	size_t frame_size = 0;
	unsigned int i;
	int err = 0;

	dev_info(dev, "> %s(dev=%px)\n", __func__, dev);

	for (i = 0; i < dce->soc->num_channels; i++) {
		const struct tegra_dce_channel_soc *soc =
			&dce->soc->channels[i];

		size += tegra_ivc_align(soc->frame_size) * 2 * soc->num_frames;
	}

	size = tegra_ivc_total_queue_size(size);
	dce->size = roundup_pow_of_two(size);

	dev_info(dev, "  allocating %zu bytes for IVC channel\n", dce->size);

	dce->virt = dmam_alloc_coherent(dev, dce->size, &dce->iova,
					GFP_KERNEL | __GFP_ZERO);
	if (!dce->virt)
		return -ENOMEM;

	dce->channels = devm_kcalloc(dce->dev, dce->soc->num_channels,
				     sizeof(*channel), GFP_KERNEL);
	if (!dce->channels)
		return -ENOMEM;

	for (i = 0; i < dce->soc->num_channels; i++) {
		struct tegra_dce_channel *channel = &dce->channels[i];
		const struct tegra_dce_channel_soc *soc =
			&dce->soc->channels[i];

		err = tegra_dce_channel_init(dce, channel, soc, offset);
		if (err < 0)
			return err;

		offset += err;
	}

	dce->irq.client.dev = dev;
	dce->irq.client.rx_callback = tegra_dce_irq_callback;
	init_completion(&dce->irq.done);

	dce->irq.channel = mbox_request_channel_byname(&dce->irq.client, "irq");
	if (IS_ERR(dce->irq.channel)) {
		err = PTR_ERR(dce->irq.channel);
		dev_err(dev, "failed to get IRQ mailbox: %d\n", err);
		return err;
	}

	dce->boot.client.dev = dev;
	dce->boot.client.tx_prepare = tegra_dce_boot_prepare;
	dce->boot.client.tx_done = tegra_dce_boot_done;

	dce->boot.channel = mbox_request_channel_byname(&dce->boot.client,
							"boot");
	if (IS_ERR(dce->boot.channel)) {
		err = PTR_ERR(dce->boot.channel);
		dev_err(dev, "failed to get boot mailbox: %d\n", err);
		return err;
	}

	err = tegra_dce_boot_get_version(dce, &version);
	if (err < 0) {
		dev_err(dev, "failed to get DCE version: %d\n", err);
		return err;
	}

	dev_info(dev, "DCE version: %x\n", version);

	if (tegra_dev_iommu_get_stream_id(dev, &stream_id)) {
		err = tegra_dce_boot_set_stream_id(dce, stream_id);
		if (err < 0) {
			dev_err(dev, "failed to set stream ID: %d\n", err);
			return err;
		}

		dev_info(dev, "DCE stream ID: %x\n", stream_id);
	}

	err = tegra_dce_boot_set_ast_iova_info(dce);
	if (err < 0) {
		dev_err(dev, "failed to set IOVA info: %d\n", err);
		return err;
	}

	err = tegra_dce_boot_set_addr_read(dce, dce->channels[0].ivc->tx.phys);
	if (err < 0) {
		dev_err(dev, "failed to set IVC read address: %d\n", err);
		return err;
	}

	err = tegra_dce_boot_set_addr_write(dce, dce->channels[0].ivc->rx.phys);
	if (err < 0) {
		dev_err(dev, "failed to set IVC write addresse: %d\n", err);
		return err;
	}

	err = tegra_dce_boot_get_frame_size(dce, &frame_size);
	if (err < 0) {
		dev_err(dev, "failed to get frame size: %d\n", err);
		return err;
	}

	dev_info(dev, "frame size: %zu\n", frame_size);

	err = tegra_dce_boot_set_frames(dce, dce->channels[0].soc->num_frames);
	if (err < 0) {
		dev_err(dev, "failed to set frame count: %d\n", err);
		return err;
	}

	err = tegra_dce_boot_set_frame_size(dce, dce->channels[0].soc->frame_size);
	if (err < 0) {
		dev_err(dev, "failed to set frame size: %d\n", err);
		return err;
	}

	err = tegra_dce_boot_channel_init(dce);
	if (err < 0) {
		dev_err(dev, "failed to init channel: %d\n", err);
		return err;
	}

	err = tegra_dce_boot_lock(dce);
	if (err < 0) {
		dev_err(dev, "failed to lock DCE configuration: %d\n", err);
		return err;
	}

	err = tegra_dce_admin_init(dce);
	if (err < 0)
		return err;

	err = component_bind_all(dev, dce);

	dev_info(dev, "< %s() = %d\n", __func__, err);
	return err;
}

static void tegra_dce_unbind(struct device *dev)
{
	struct tegra_dce *dce = dev_get_drvdata(dev);

	dev_info(dev, "> %s(dev=%px)\n", __func__, dev);

	component_unbind_all(dev, dce);

	dma_free_coherent(dce->dev, dce->size, dce->virt, dce->iova);

	dev_info(dev, "< %s()\n", __func__);
}

static const struct component_master_ops tegra_dce_master_ops = {
	.bind = tegra_dce_bind,
	.unbind = tegra_dce_unbind,
};

static int tegra_dce_probe(struct platform_device *pdev)
{
	struct component_match *match;
	struct device_node *np;
	struct tegra_dce *dce;
	int err;

	dev_dbg(&pdev->dev, "> %s(pdev=%px)\n", __func__, pdev);

	dce = devm_kzalloc(&pdev->dev, sizeof(*dce), GFP_KERNEL);
	if (!dce)
		return -ENOMEM;

	dce->soc = of_device_get_match_data(&pdev->dev);
	platform_set_drvdata(pdev, dce);
	dce->dev = &pdev->dev;

	err = devm_of_platform_populate(&pdev->dev);
	if (err < 0)
		dev_err(&pdev->dev, "failed to populate child devices\n");

	for_each_child_of_node(pdev->dev.of_node, np) {
		if (of_device_is_compatible(np, "nvidia,tegra234-hsp")) {
			dev_info(&pdev->dev, "found mailboxes: %pOF\n", np);
			component_match_add_release(&pdev->dev, &match,
						    component_release_of,
						    component_compare_of,
						    np);
		}
	}

	err = component_master_add_with_match(&pdev->dev,
					      &tegra_dce_master_ops, match);

	dev_dbg(&pdev->dev, "< %s() = %d\n", __func__, err);
	return err;
}

static int tegra_dce_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "> %s(pdev=%px)\n", __func__, pdev);
	dev_dbg(&pdev->dev, "< %s()\n", __func__);
	return 0;
}

static int __maybe_unused tegra_dce_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "> %s(dev=%px)\n", __func__, dev);
	dev_dbg(dev, "< %s()\n", __func__);
	return 0;
}

static int __maybe_unused tegra_dce_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "> %s(dev=%px)\n", __func__, dev);
	dev_dbg(dev, "< %s()\n", __func__);
	return 0;
}

static int __maybe_unused tegra_dce_suspend(struct device *dev)
{
	dev_dbg(dev, "> %s(dev=%px)\n", __func__, dev);
	dev_dbg(dev, "< %s()\n", __func__);
	return 0;
}

static int __maybe_unused tegra_dce_resume(struct device *dev)
{
	dev_dbg(dev, "> %s(dev=%px)\n", __func__, dev);
	dev_dbg(dev, "< %s()\n", __func__);
	return 0;
}

static const struct dev_pm_ops tegra_dce_pm = {
	SET_RUNTIME_PM_OPS(tegra_dce_runtime_suspend, tegra_dce_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra_dce_suspend, tegra_dce_resume)
};

static const struct tegra_dce_channel_soc tegra234_dce_channels[] = {
	{
		/* admin channel */
		.name = "admin",
		.num_frames = 4,
		.frame_size = 1024,
		.prepare = tegra_dce_admin_prepare,
		.done = tegra_dce_admin_done,
		.callback = tegra_dce_admin_callback,
	}, {
		/* RM channel */
		.name = "rm",
		.num_frames = 1,
		.frame_size = 4096,
		.prepare = tegra_dce_rm_prepare,
		.done = tegra_dce_rm_done,
		.callback = tegra_dce_rm_callback,
	},
#if 0
	{
		/* HDCP channel */
		.name = "hdcp",
		.num_frames = 4,
		.frame_size = 1024,
		.prepare = tegra_dce_hdcp_prepare,
		.done = tegra_dce_hdcp_done,
		.callback = tegra_dce_hdcp_callback,
	},
#endif
	{
		/* RM notify channel */
		.name = "notify",
		.num_frames = 4,
		.frame_size = 4096,
		.prepare = tegra_dce_notify_prepare,
		.done = tegra_dce_notify_done,
		.callback = tegra_dce_notify_callback,
	}
};

static const struct tegra_dce_soc tegra234_dce_soc = {
	.num_channels = ARRAY_SIZE(tegra234_dce_channels),
	.channels = tegra234_dce_channels,
};

static const struct of_device_id tegra_dce_match[] = {
	{ .compatible = "nvidia,tegra234-dce", .data = &tegra234_dce_soc },
	{ }
};

static struct platform_driver tegra_dce_driver = {
	.driver = {
		.name = "tegra-dce",
		.of_match_table = tegra_dce_match,
		.pm = &tegra_dce_pm,
	},
	.probe = tegra_dce_probe,
	.remove = tegra_dce_remove,
};
module_platform_driver(tegra_dce_driver);

MODULE_AUTHOR("Thierry Reding <treding@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra234 DCE driver");
MODULE_LICENSE("GPL v2");
