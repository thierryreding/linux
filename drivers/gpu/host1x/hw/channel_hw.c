/*
 * Tegra host1x Channel
 *
 * Copyright (C) 2010-2016 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/dma-fence.h>
#include <linux/host1x.h>
#include <linux/iommu.h>
#include <linux/slab.h>

#include <trace/events/host1x.h>

#include "../channel.h"
#include "../dev.h"
#include "../fence.h"
#include "../intr.h"
#include "../job.h"

#define TRACE_MAX_LENGTH 128U

static int host1x_job_gather_wait_fences(struct host1x_job *job,
					 struct host1x_job_gather *gather)
{
	struct host1x *host = dev_get_drvdata(job->channel->dev->parent);
	unsigned int i;
	int err = 0;

	if (gather->num_fences == 0)
		return 0;

	for (i = 0; i < gather->num_fences; i++) {
		struct dma_fence *fence = gather->fences[i].fence;

		/* skip emit fences */
		if (gather->fences[i].syncpt)
			continue;

		if (host1x_fence_is_waitable(fence)) {
			err = host1x_fence_wait(fence, host, job->channel);
		} else {
			err = dma_fence_wait(fence, true);
		}

		if (err < 0)
			break;
	}

	return err;
}

static void trace_write_gather(struct host1x_cdma *cdma, struct host1x_bo *bo,
			       u32 offset, u32 words)
{
	struct device *dev = cdma_to_channel(cdma)->dev;
	void *mem = NULL;

	if (host1x_debug_trace_cmdbuf)
		mem = host1x_bo_mmap(bo);

	if (mem) {
		u32 i;
		/*
		 * Write in batches of 128 as there seems to be a limit
		 * of how much you can output to ftrace at once.
		 */
		for (i = 0; i < words; i += TRACE_MAX_LENGTH) {
			u32 num_words = min(words - i, TRACE_MAX_LENGTH);

			offset += i * sizeof(u32);

			trace_host1x_cdma_push_gather(dev_name(dev), bo,
						      num_words, offset,
						      mem);
		}

		host1x_bo_munmap(bo, mem);
	}
}

static void submit_gathers(struct host1x_job *job)
{
	struct host1x_cdma *cdma = &job->channel->cdma;
	struct device *dev = job->channel->dev;
	unsigned int i;

	for (i = 0; i < job->num_gathers; i++) {
		struct host1x_job_gather *g = &job->gathers[i];
		dma_addr_t addr = g->base + g->offset;
		u32 op2, op3;
		int err;

		op2 = lower_32_bits(addr);
		op3 = upper_32_bits(addr);

		err = host1x_job_gather_wait_fences(job, g);
		if (err < 0) {
			dev_err(dev, "failed to wait for fences: %d\n", err);
			continue;
		}

		/* add a setclass for modules that require it */
		if (job->class)
			host1x_cdma_push(cdma,
				 host1x_opcode_setclass(job->class, 0, 0),
				 HOST1X_OPCODE_NOP);

		trace_write_gather(cdma, g->bo, g->offset, g->words);

		if (op3 != 0) {
#if HOST1X_HW >= 6
			u32 op1 = host1x_opcode_gather_wide(g->words);
			u32 op4 = HOST1X_OPCODE_NOP;

			host1x_cdma_push_wide(cdma, op1, op2, op3, op4);
#else
			dev_err(dev, "invalid gather for push buffer %pad\n",
				&addr);
			continue;
#endif
		} else {
			u32 op1 = host1x_opcode_gather(g->words);

			host1x_cdma_push(cdma, op1, op2);
		}
	}
}

static void channel_push_wait(struct host1x_channel *channel,
			     u32 id, u32 thresh)
{
	host1x_cdma_push(&channel->cdma,
			 host1x_opcode_setclass(HOST1X_CLASS_HOST1X,
				host1x_uclass_wait_syncpt_r(), 1),
			 host1x_class_host_wait_syncpt(id, thresh));
}

static inline void host1x_syncpt_sync_base(struct host1x_syncpt *syncpt,
					   struct host1x_cdma *cdma)
{
	u32 opcode, value;

	if (!syncpt->base)
		return;

	value = host1x_syncpt_read_max(syncpt);

	opcode = host1x_opcode_setclass(HOST1X_CLASS_HOST1X,
					HOST1X_UCLASS_LOAD_SYNCPT_BASE, 1);
	value = HOST1X_UCLASS_LOAD_SYNCPT_BASE_BASE_INDX_F(syncpt->id) |
		HOST1X_UCLASS_LOAD_SYNCPT_BASE_VALUE_F(value);

	host1x_cdma_push(cdma, opcode, value);
}

static void channel_serialize(struct host1x_job *job)
{
	unsigned int i;

	if (!job->serialize)
		return;

	/*
	 * Force serialization by inserting a host wait for the
	 * previous job to finish before this one can commence.
	 */
	for (i = 0; i < job->num_checkpoints; i++) {
		struct host1x_syncpt *syncpt = job->checkpoints[i].syncpt;
		u32 opcode, value = host1x_syncpt_read_max(syncpt);

		opcode = host1x_opcode_setclass(HOST1X_CLASS_HOST1X,
						host1x_uclass_wait_syncpt_r(),
						1);
		value = host1x_class_host_wait_syncpt(syncpt->id, value);

		host1x_cdma_push(&job->channel->cdma, opcode, value);
	}
}

static struct host1x_waitlist **alloc_waiters(unsigned int count)
{
	struct host1x_waitlist *waiter, **waiters;
	unsigned int i;

	waiters = kmalloc_array(count, sizeof(*waiters), GFP_KERNEL);
	if (!waiters)
		return NULL;

	for (i = 0; i < count; i++) {
		waiter = kzalloc(sizeof(*waiter), GFP_KERNEL);
		if (!waiter)
			goto free;

		waiters[i] = waiter;
	}

	return waiters;

free:
	while (i--)
		kfree(waiters[i]);

	kfree(waiters);
	return NULL;
}

static int submit_waiters(struct host1x_job *job,
			  struct host1x_waitlist **waiters, unsigned int count)
{
	struct host1x *host = dev_get_drvdata(job->channel->dev->parent);
	unsigned int i;
	int err;

	for (i = 0; i < job->num_checkpoints; i++) {
		struct host1x_checkpoint *cp = &job->checkpoints[i];

		/* schedule a submit complete interrupt */
		err = host1x_intr_add_action(host, cp->syncpt, cp->threshold,
					     HOST1X_INTR_ACTION_SUBMIT_COMPLETE,
					     job->channel, waiters[i], NULL);
		WARN(err < 0, "failed to set submit complete interrupt: %d\n",
		     err);
	}

	return 0;
}

static void host1x_channel_set_streamid(struct host1x_channel *channel)
{
#if HOST1X_HW >= 6
	struct iommu_fwspec *spec = dev_iommu_fwspec_get(channel->dev->parent);
	u32 sid = spec ? spec->ids[0] & 0xffff : 0x7f;

	host1x_ch_writel(channel, sid, HOST1X_CHANNEL_SMMU_STREAMID);
#endif
}

static int channel_submit(struct host1x_job *job)
{
	struct host1x *host = dev_get_drvdata(job->channel->dev->parent);
	struct host1x_channel *channel = job->channel;
	struct host1x_waitlist **waiters;
	unsigned int i, j;
	int err;

	trace_host1x_channel_submit(dev_name(channel->dev),
				    job->num_gathers, job->num_relocs,
				    job->num_checkpoints);

	for (i = 0; i < job->num_checkpoints; i++) {
		struct host1x_checkpoint *cp = &job->checkpoints[i];

		/* before error checks, return current max */
		cp->threshold = host1x_syncpt_read_max(cp->syncpt);
	}

	/* get submit lock */
	err = mutex_lock_interruptible(&channel->submitlock);
	if (err)
		return err;

	waiters = alloc_waiters(job->num_checkpoints);
	if (!waiters) {
		mutex_unlock(&channel->submitlock);
		return -ENOMEM;
	}

	host1x_channel_set_streamid(channel);

	/* begin a CDMA submit */
	err = host1x_cdma_begin(&channel->cdma, job);
	if (err) {
		mutex_unlock(&channel->submitlock);
		goto free;
	}

	channel_serialize(job);

	/* rebase fences on current threshold */
	for (i = 0; i < job->num_fences; i++) {
		struct host1x_job_fence *fence = &job->fences[i];

		for (j = 0; j < job->num_checkpoints; j++) {
			if (job->checkpoints[j].syncpt == fence->syncpt)
				fence->value += job->checkpoints[j].threshold;
		}
	}

	/* bump threshold */
	for (i = 0; i < job->num_checkpoints; i++) {
		struct host1x_checkpoint *cp = &job->checkpoints[i];

		/*
		 * Synchronize base register to allow using it for relative
		 * waiting.
		 */
		host1x_syncpt_sync_base(cp->syncpt, &channel->cdma);

		cp->threshold = host1x_syncpt_incr_max(cp->syncpt, cp->value);
		host1x_hw_syncpt_assign_to_channel(host, cp->syncpt, channel);
	}

	submit_gathers(job);

	/* end CDMA submit & stash pinned hMems into sync queue */
	host1x_cdma_end(&channel->cdma, job);

	trace_host1x_channel_submitted(dev_name(channel->dev));
	submit_waiters(job, waiters, job->num_checkpoints);

	mutex_unlock(&channel->submitlock);

	return 0;

free:
	for (i = 0; i < job->num_checkpoints; i++)
		kfree(waiters[i]);

	kfree(waiters[i]);
	return err;
}

static void enable_gather_filter(struct host1x *host,
				 struct host1x_channel *ch)
{
#if HOST1X_HW >= 6
	u32 val;

	if (!host->hv_regs)
		return;

	val = host1x_hypervisor_readl(
		host, HOST1X_HV_CH_KERNEL_FILTER_GBUFFER(ch->id / 32));
	val |= BIT(ch->id % 32);
	host1x_hypervisor_writel(
		host, val, HOST1X_HV_CH_KERNEL_FILTER_GBUFFER(ch->id / 32));
#elif HOST1X_HW >= 4
	host1x_ch_writel(ch,
			 HOST1X_CHANNEL_CHANNELCTRL_KERNEL_FILTER_GBUFFER(1),
			 HOST1X_CHANNEL_CHANNELCTRL);
#endif
}

static int host1x_channel_init(struct host1x_channel *ch, struct host1x *dev,
			       unsigned int index)
{
#if HOST1X_HW < 6
	ch->regs = dev->regs + index * 0x4000;
#else
	ch->regs = dev->regs + index * 0x100;
#endif
	enable_gather_filter(dev, ch);
	return 0;
}

static const struct host1x_channel_ops host1x_channel_ops = {
	.init = host1x_channel_init,
	.submit = channel_submit,
	.push_wait = channel_push_wait
};
