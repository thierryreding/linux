/*
 * Copyright (c) 2016-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mailbox_controller.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <dt-bindings/mailbox/tegra186-hsp.h>

#include "mailbox.h"

#define HSP_INT_IE(x)		(0x100 + ((x) * 4))
#define HSP_INT_IV		0x300
#define HSP_INT_IR		0x304

#define HSP_INT_EMPTY_SHIFT	0
#define HSP_INT_EMPTY_MASK	0xff
#define HSP_INT_FULL_SHIFT	8
#define HSP_INT_FULL_MASK	0xff

#define HSP_INT_DIMENSIONING	0x380
#define HSP_nSM_SHIFT		0
#define HSP_nSS_SHIFT		4
#define HSP_nAS_SHIFT		8
#define HSP_nDB_SHIFT		12
#define HSP_nSI_SHIFT		16
#define HSP_nINT_MASK		0xf

#define HSP_DB_TRIGGER	0x0
#define HSP_DB_ENABLE	0x4
#define HSP_DB_RAW	0x8
#define HSP_DB_PENDING	0xc

#define HSP_SM_SHRD_MBOX	0x0
#define HSP_SM_SHRD_MBOX_FULL	BIT(31)
#define HSP_SM_SHRD_MBOX_FULL_INT_IE	0x04
#define HSP_SM_SHRD_MBOX_EMPTY_INT_IE	0x08

#define HSP_DB_CCPLEX		1
#define HSP_DB_BPMP		3
#define HSP_DB_MAX		7

struct tegra_hsp_channel;
struct tegra_hsp;

struct tegra_hsp_channel {
	unsigned int type;
	struct tegra_hsp *hsp;
	struct mbox_chan *chan;
	void __iomem *regs;
};

struct tegra_hsp_doorbell {
	struct tegra_hsp_channel channel;
	struct list_head list;
	const char *name;
	unsigned int master;
	unsigned int index;
};

static inline struct tegra_hsp_doorbell *
channel_to_doorbell(struct tegra_hsp_channel *channel)
{
	return container_of(channel, struct tegra_hsp_doorbell, channel);
}

struct tegra_hsp_db_map {
	const char *name;
	unsigned int master;
	unsigned int index;
};

struct tegra_hsp_mailbox {
	struct tegra_hsp_channel channel;
	unsigned int index;
	bool sending;
};

static inline struct tegra_hsp_mailbox *
channel_to_mailbox(struct tegra_hsp_channel *channel)
{
	return container_of(channel, struct tegra_hsp_mailbox, channel);
}

struct tegra_hsp_soc {
	const struct tegra_hsp_db_map *map;
};

struct tegra_hsp {
	struct device *dev;
	const struct tegra_hsp_soc *soc;
	struct mbox_controller mbox;
	void __iomem *regs;
	unsigned int doorbell_irq;
	unsigned int *shared_irqs;
	unsigned int num_sm;
	unsigned int num_as;
	unsigned int num_ss;
	unsigned int num_db;
	unsigned int num_si;
	spinlock_t lock;

	unsigned int si_empty;
	unsigned int si_full;

	struct list_head doorbells;
	struct tegra_hsp_mailbox *mailboxes;

	struct dentry *debugfs;

	struct {
		struct {
			unsigned int count[8];
			unsigned int empty[8];
			unsigned int full[8];
			unsigned int unhandled;
			unsigned int invalid;
			unsigned int total;
		} interrupts;
	} stats;
};

static inline struct tegra_hsp *
to_tegra_hsp(struct mbox_controller *mbox)
{
	return container_of(mbox, struct tegra_hsp, mbox);
}

static inline u32 tegra_hsp_readl(struct tegra_hsp *hsp, unsigned int offset)
{
	return readl(hsp->regs + offset);
}

static inline void tegra_hsp_writel(struct tegra_hsp *hsp, u32 value,
				    unsigned int offset)
{
	writel(value, hsp->regs + offset);
}

static inline u32 tegra_hsp_channel_readl(struct tegra_hsp_channel *channel,
					  unsigned int offset)
{
	return readl(channel->regs + offset);
}

static inline void tegra_hsp_channel_writel(struct tegra_hsp_channel *channel,
					    u32 value, unsigned int offset)
{
	writel(value, channel->regs + offset);
}

static bool tegra_hsp_doorbell_can_ring(struct tegra_hsp_doorbell *db)
{
	u32 value;

	value = tegra_hsp_channel_readl(&db->channel, HSP_DB_ENABLE);

	return (value & BIT(TEGRA_HSP_DB_MASTER_CCPLEX)) != 0;
}

static struct tegra_hsp_doorbell *
__tegra_hsp_doorbell_get(struct tegra_hsp *hsp, unsigned int master)
{
	struct tegra_hsp_doorbell *entry;

	list_for_each_entry(entry, &hsp->doorbells, list)
		if (entry->master == master)
			return entry;

	return NULL;
}

static struct tegra_hsp_doorbell *
tegra_hsp_doorbell_get(struct tegra_hsp *hsp, unsigned int master)
{
	struct tegra_hsp_doorbell *db;
	unsigned long flags;

	spin_lock_irqsave(&hsp->lock, flags);
	db = __tegra_hsp_doorbell_get(hsp, master);
	spin_unlock_irqrestore(&hsp->lock, flags);

	return db;
}

static irqreturn_t tegra_hsp_doorbell_irq(int irq, void *data)
{
	struct tegra_hsp *hsp = data;
	struct tegra_hsp_doorbell *db;
	unsigned long master, value;

	db = tegra_hsp_doorbell_get(hsp, TEGRA_HSP_DB_MASTER_CCPLEX);
	if (!db)
		return IRQ_NONE;

	value = tegra_hsp_channel_readl(&db->channel, HSP_DB_PENDING);
	tegra_hsp_channel_writel(&db->channel, value, HSP_DB_PENDING);

	spin_lock(&hsp->lock);

	for_each_set_bit(master, &value, hsp->mbox.num_chans) {
		struct tegra_hsp_doorbell *db;

		db = __tegra_hsp_doorbell_get(hsp, master);
		/*
		 * Depending on the bootloader chain, the CCPLEX doorbell will
		 * have some doorbells enabled, which means that requesting an
		 * interrupt will immediately fire.
		 *
		 * In that case, db->channel.chan will still be NULL here and
		 * cause a crash if not properly guarded.
		 *
		 * It remains to be seen if ignoring the doorbell in that case
		 * is the correct solution.
		 */
		if (db && db->channel.chan)
			mbox_chan_received_data(db->channel.chan, NULL);
	}

	spin_unlock(&hsp->lock);

	return IRQ_HANDLED;
}

static irqreturn_t tegra_hsp_shared_irq(int irq, void *data)
{
	struct tegra_hsp_mailbox *mb;
	struct tegra_hsp *hsp = data;
	unsigned int i, shared_irq;
	unsigned long bit, mask;
	u32 status, value;

	dev_info(hsp->dev, "> %s(irq=%d, data=%px)\n", __func__, irq, data);

	for (i = 0; i < hsp->num_si; i++) {
		if (hsp->shared_irqs[i] == irq) {
			hsp->stats.interrupts.count[i]++;
			break;
		}
	}

	shared_irq = i;

	hsp->stats.interrupts.total++;

	status = tegra_hsp_readl(hsp, HSP_INT_IR);

	for (i = 0; i < hsp->num_sm; i++) {
		if (status & BIT(HSP_INT_EMPTY_SHIFT + i))
			hsp->stats.interrupts.empty[i]++;
	}

	for (i = 0; i < hsp->num_sm; i++) {
		if (status & BIT(HSP_INT_FULL_SHIFT + i))
			hsp->stats.interrupts.full[i]++;
	}

	if (shared_irq == hsp->si_full) {
		/* only interested in FULL interrupts */
		mask = (status >> HSP_INT_FULL_SHIFT) & HSP_INT_FULL_MASK;

		dev_info(hsp->dev, "  FULL: %08lx\n", mask);

		for_each_set_bit(bit, &mask, hsp->num_sm) {
			dev_info(hsp->dev, "    %lu:\n", bit);

			mb = &hsp->mailboxes[bit];

			if (!mb->sending) {
				value = tegra_hsp_channel_readl(&mb->channel,
								HSP_SM_SHRD_MBOX);
				value &= ~HSP_SM_SHRD_MBOX_FULL;
				mbox_chan_received_data(mb->channel.chan, &value);

				/*
				 * Need to clear all bits here since some
				 * producers, such as TCU, depend on fields in
				 * the register getting cleared by the
				 * consumer.
				 *
				 * The mailbox API doesn't give the consumers
				 * a way of doing that explicitly, so we have
				 * to make sure we cover all possible cases.
				 */
				//tegra_hsp_channel_writel(&mb->channel, 0x0,
				tegra_hsp_channel_writel(&mb->channel, value,
							 HSP_SM_SHRD_MBOX);
			}
		}

		if (mask) {
			dev_info(hsp->dev, "< %s()\n", __func__);
			return IRQ_HANDLED;
		}
	}

	if (shared_irq == hsp->si_empty) {
		/* only interested in EMPTY interrupts */
		mask = (status >> HSP_INT_EMPTY_SHIFT) & HSP_INT_EMPTY_MASK;

		dev_info(hsp->dev, "  EMPTY: %08lx\n", mask);

		for_each_set_bit(bit, &mask, hsp->num_sm) {
			mb = &hsp->mailboxes[bit];

			if (mb->sending) {
				dev_info(hsp->dev, "    %lu: processing\n", bit);

				value = tegra_hsp_channel_readl(&mb->channel, HSP_SM_SHRD_MBOX);
				dev_info(hsp->dev, "HSP_SM_SHRD_MBOX > %08x\n", value);

				value = tegra_hsp_readl(hsp, HSP_INT_IE(hsp->si_empty));
				value &= ~BIT(HSP_INT_EMPTY_SHIFT + mb->index);
				dev_info(hsp->dev, "HSP_INT_IE(%u) < %08x\n", hsp->si_empty, value);
				tegra_hsp_writel(hsp, value, HSP_INT_IE(hsp->si_empty));
			}
#if 0
			tegra_hsp_channel_writel(&mb->channel, 0x0, HSP_SM_SHRD_MBOX_EMPTY_INT_IE);
#endif

			if (mb->channel.chan->txdone_method == TXDONE_BY_IRQ)
				mbox_chan_txdone(mb->channel.chan, 0);
		}

		if (mask) {
			dev_info(hsp->dev, "< %s()\n", __func__);
			return IRQ_HANDLED;
		}
	}

	if (shared_irq != 0 && shared_irq != 1)
		hsp->stats.interrupts.invalid++;

	hsp->stats.interrupts.unhandled++;

	dev_info(hsp->dev, "< %s()\n", __func__);
	return IRQ_NONE;
}

static struct tegra_hsp_channel *
tegra_hsp_doorbell_create(struct tegra_hsp *hsp, const char *name,
			  unsigned int master, unsigned int index)
{
	struct tegra_hsp_doorbell *db;
	unsigned int offset;
	unsigned long flags;

	db = kzalloc(sizeof(*db), GFP_KERNEL);
	if (!db)
		return ERR_PTR(-ENOMEM);

	offset = (1 + (hsp->num_sm / 2) + hsp->num_ss + hsp->num_as) * SZ_64K;
	offset += index * 0x100;

	db->channel.regs = hsp->regs + offset;
	db->channel.hsp = hsp;

	db->name = kstrdup_const(name, GFP_KERNEL);
	db->master = master;
	db->index = index;

	spin_lock_irqsave(&hsp->lock, flags);
	list_add_tail(&db->list, &hsp->doorbells);
	spin_unlock_irqrestore(&hsp->lock, flags);

	return &db->channel;
}

static void __tegra_hsp_doorbell_destroy(struct tegra_hsp_doorbell *db)
{
	list_del(&db->list);
	kfree_const(db->name);
	kfree(db);
}

static int tegra_hsp_doorbell_startup(struct tegra_hsp_doorbell *db)
{
	struct tegra_hsp *hsp = db->channel.hsp;
	struct tegra_hsp_doorbell *ccplex;
	unsigned long flags;
	u32 value;

	if (db->master >= hsp->mbox.num_chans) {
		dev_err(hsp->mbox.dev,
			"invalid master ID %u for HSP channel\n",
			db->master);
		return -EINVAL;
	}

	ccplex = tegra_hsp_doorbell_get(hsp, TEGRA_HSP_DB_MASTER_CCPLEX);
	if (!ccplex)
		return -ENODEV;

	if (!tegra_hsp_doorbell_can_ring(db))
		return -ENODEV;

	spin_lock_irqsave(&hsp->lock, flags);

	value = tegra_hsp_channel_readl(&ccplex->channel, HSP_DB_ENABLE);
	value |= BIT(db->master);
	tegra_hsp_channel_writel(&ccplex->channel, value, HSP_DB_ENABLE);

	spin_unlock_irqrestore(&hsp->lock, flags);

	return 0;
}

static void tegra_hsp_doorbell_shutdown(struct tegra_hsp_doorbell *db)
{
	struct tegra_hsp *hsp = db->channel.hsp;
	struct tegra_hsp_doorbell *ccplex;
	unsigned long flags;
	u32 value;

	ccplex = tegra_hsp_doorbell_get(hsp, TEGRA_HSP_DB_MASTER_CCPLEX);
	if (!ccplex)
		return;

	spin_lock_irqsave(&hsp->lock, flags);

	value = tegra_hsp_channel_readl(&ccplex->channel, HSP_DB_ENABLE);
	value &= ~BIT(db->master);
	tegra_hsp_channel_writel(&ccplex->channel, value, HSP_DB_ENABLE);

	spin_unlock_irqrestore(&hsp->lock, flags);
}

static int tegra_hsp_mailbox_startup(struct tegra_hsp_mailbox *mb)
{
	struct tegra_hsp_channel *channel = &mb->channel;
	struct tegra_hsp *hsp = channel->hsp;
	u32 value;

	mb->channel.chan->txdone_method = TXDONE_BY_IRQ;

#if 0
	tegra_hsp_channel_writel(channel, 0x1, HSP_SM_SHRD_MBOX_FULL_INT_IE);
	/* shared mailboxes start out as consumers by default */
	tegra_hsp_channel_writel(channel, 0x1, HSP_SM_SHRD_MBOX_EMPTY_INT_IE);
#endif

	/* shared mailboxes start out as consumers by default */

	/* route FULL interrupt to external IRQ 0 */
	value = tegra_hsp_readl(hsp, HSP_INT_IE(hsp->si_full));
	value |= BIT(HSP_INT_FULL_SHIFT + mb->index);
	dev_info(hsp->dev, "HSP_INT_IE(%u) < %08x\n", hsp->si_full, value);
	tegra_hsp_writel(hsp, value, HSP_INT_IE(hsp->si_full));

	/* route EMPTY interrupt to external IRQ 1 */
	value = tegra_hsp_readl(hsp, HSP_INT_IE(hsp->si_empty));
	value &= ~BIT(HSP_INT_EMPTY_SHIFT + mb->index);
	dev_info(hsp->dev, "HSP_INT_IE(%u) < %08x\n", hsp->si_empty, value);
	tegra_hsp_writel(hsp, value, HSP_INT_IE(hsp->si_empty));

	return 0;
}

static int tegra_hsp_mailbox_shutdown(struct tegra_hsp_mailbox *mb)
{
	struct tegra_hsp_channel *channel = &mb->channel;
	struct tegra_hsp *hsp = channel->hsp;
	u32 value;

#if 0
	tegra_hsp_channel_writel(channel, 0x0, HSP_SM_SHRD_MBOX_EMPTY_INT_IE);
	tegra_hsp_channel_writel(channel, 0x0, HSP_SM_SHRD_MBOX_FULL_INT_IE);
#endif

	value = tegra_hsp_readl(hsp, HSP_INT_IE(hsp->si_empty));
	value &= ~BIT(HSP_INT_EMPTY_SHIFT + mb->index + 8);
	tegra_hsp_writel(hsp, value, HSP_INT_IE(hsp->si_empty));

	value = tegra_hsp_readl(hsp, HSP_INT_IE(hsp->si_full));
	value &= ~BIT(HSP_INT_FULL_SHIFT + mb->index + 8);
	tegra_hsp_writel(hsp, value, HSP_INT_IE(hsp->si_full));

	return 0;
}

static int tegra_hsp_send_data(struct mbox_chan *chan, void *data)
{
	struct tegra_hsp_channel *channel = chan->con_priv;
	struct device *dev = channel->hsp->dev;
	struct tegra_hsp *hsp = channel->hsp;
	struct tegra_hsp_mailbox *mailbox;
	uint32_t value;

	switch (channel->type) {
	case TEGRA_HSP_MBOX_TYPE_DB:
		tegra_hsp_channel_writel(channel, 1, HSP_DB_TRIGGER);
		return 0;

	case TEGRA_HSP_MBOX_TYPE_SM:
		dev_info(dev, "> %s(chan=%px, data=%px)\n", __func__, chan, data);
		dev_info(dev, "  value: %08x\n", (u32)data);

		mailbox = channel_to_mailbox(channel);
		mailbox->sending = true;

		dev_info(dev, "  disabling FULL interrupt\n");
#if 0
		tegra_hsp_channel_writel(channel, 0x0, HSP_SM_SHRD_MBOX_FULL_INT_IE);
#else
		/*
		value = tegra_hsp_readl(hsp, HSP_INT_IE(hsp->si_full));
		dev_info(hsp->dev, "HSP_INT_IE(%u) < %08x\n", hsp->si_full, value);
		value &= ~BIT(HSP_INT_FULL_SHIFT + mailbox->index);
		dev_info(hsp->dev, "HSP_INT_IE(%u) < %08x\n", hsp->si_full, value);
		tegra_hsp_writel(hsp, value, HSP_INT_IE(hsp->si_full));
		*/
#endif

		value = (u32)data;
		/* Mark mailbox full */
		value |= HSP_SM_SHRD_MBOX_FULL;

		dev_info(hsp->dev, "  HSP_SM_SHRD_MBOX < %08x\n", value);
		tegra_hsp_channel_writel(channel, value, HSP_SM_SHRD_MBOX);
		dev_info(dev, "  enabling EMPTY interrupt\n");
#if 0
		tegra_hsp_channel_writel(channel, 0x1, HSP_SM_SHRD_MBOX_EMPTY_INT_IE);
#else
		value = tegra_hsp_readl(hsp, HSP_INT_IE(hsp->si_empty));
		dev_info(hsp->dev, "HSP_INT_IE(%u) < %08x\n", hsp->si_empty, value);
		value |= BIT(HSP_INT_EMPTY_SHIFT + mailbox->index);
		dev_info(hsp->dev, "HSP_INT_IE(%u) < %08x\n", hsp->si_empty, value);
		tegra_hsp_writel(hsp, value, HSP_INT_IE(hsp->si_empty));
#endif

		if (chan->txdone_method != TXDONE_BY_IRQ) {
			unsigned long timeout = jiffies + msecs_to_jiffies(100);

			dev_info(dev, "  waiting for mailbox to drain...\n");

			while (time_before(jiffies, timeout)) {
				value = tegra_hsp_channel_readl(channel, HSP_SM_SHRD_MBOX);
				if ((value & HSP_SM_SHRD_MBOX_FULL) == 0)
					break;

				udelay(10);
			}

			if (time_after(jiffies, timeout)) {
				dev_info(dev, "  timeout\n");
			} else {
				dev_info(dev, "  done\n");
			}
		}

		dev_info(dev, "< %s()\n", __func__);
		return 0;
	}

	return -EINVAL;
}

static int tegra_hsp_startup(struct mbox_chan *chan)
{
	struct tegra_hsp_channel *channel = chan->con_priv;

	switch (channel->type) {
	case TEGRA_HSP_MBOX_TYPE_DB:
		return tegra_hsp_doorbell_startup(channel_to_doorbell(channel));
	case TEGRA_HSP_MBOX_TYPE_SM:
		return tegra_hsp_mailbox_startup(channel_to_mailbox(channel));
	}

	return -EINVAL;
}

static void tegra_hsp_shutdown(struct mbox_chan *chan)
{
	struct tegra_hsp_channel *channel = chan->con_priv;

	switch (channel->type) {
	case TEGRA_HSP_MBOX_TYPE_DB:
		tegra_hsp_doorbell_shutdown(channel_to_doorbell(channel));
		break;
	case TEGRA_HSP_MBOX_TYPE_SM:
		tegra_hsp_mailbox_shutdown(channel_to_mailbox(channel));
		break;
	}
}

static const struct mbox_chan_ops tegra_hsp_ops = {
	.send_data = tegra_hsp_send_data,
	.startup = tegra_hsp_startup,
	.shutdown = tegra_hsp_shutdown,
};

static struct mbox_chan *tegra_hsp_doorbell_xlate(struct tegra_hsp *hsp,
						  unsigned int master)
{
	struct tegra_hsp_channel *channel = ERR_PTR(-ENODEV);
	struct tegra_hsp_doorbell *db;
	struct mbox_chan *chan;
	unsigned long flags;
	unsigned int i;

	db = tegra_hsp_doorbell_get(hsp, master);
	if (db)
		channel = &db->channel;

	if (IS_ERR(channel))
		return ERR_CAST(channel);

	spin_lock_irqsave(&hsp->lock, flags);

	for (i = 0; i < hsp->mbox.num_chans; i++) {
		chan = &hsp->mbox.chans[i];
		if (!chan->con_priv) {
			chan->con_priv = channel;
			channel->chan = chan;
			channel->type = TEGRA_HSP_MBOX_TYPE_DB;
			break;
		}

		chan = NULL;
	}

	spin_unlock_irqrestore(&hsp->lock, flags);

	return chan ?: ERR_PTR(-EBUSY);
}

static struct mbox_chan *of_tegra_hsp_xlate(struct mbox_controller *mbox,
					    const struct of_phandle_args *args)
{
	struct tegra_hsp *hsp = to_tegra_hsp(mbox);
	unsigned int type = args->args[0];
	unsigned int param = args->args[1];

	switch (type) {
	case TEGRA_HSP_MBOX_TYPE_DB:
		if (hsp->doorbell_irq)
			return tegra_hsp_doorbell_xlate(hsp, param);
		else
			return ERR_PTR(-EINVAL);

	case TEGRA_HSP_MBOX_TYPE_SM:
		if (hsp->shared_irqs && param < hsp->num_sm)
			return hsp->mailboxes[param].channel.chan;
		else
			return ERR_PTR(-EINVAL);

	default:
		return ERR_PTR(-EINVAL);
	}
}

static void tegra_hsp_remove_doorbells(struct tegra_hsp *hsp)
{
	struct tegra_hsp_doorbell *db, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&hsp->lock, flags);

	list_for_each_entry_safe(db, tmp, &hsp->doorbells, list)
		__tegra_hsp_doorbell_destroy(db);

	spin_unlock_irqrestore(&hsp->lock, flags);
}

static int tegra_hsp_add_doorbells(struct tegra_hsp *hsp)
{
	const struct tegra_hsp_db_map *map = hsp->soc->map;
	struct tegra_hsp_channel *channel;

	while (map->name) {
		channel = tegra_hsp_doorbell_create(hsp, map->name,
						    map->master, map->index);
		if (IS_ERR(channel)) {
			tegra_hsp_remove_doorbells(hsp);
			return PTR_ERR(channel);
		}

		map++;
	}

	return 0;
}

static int tegra_hsp_add_mailboxes(struct tegra_hsp *hsp, struct device *dev)
{
	int i;

	hsp->mailboxes = devm_kcalloc(dev, hsp->num_sm, sizeof(*hsp->mailboxes),
				      GFP_KERNEL);
	if (!hsp->mailboxes)
		return -ENOMEM;

	for (i = 0; i < hsp->num_sm; i++) {
		struct tegra_hsp_mailbox *mb = &hsp->mailboxes[i];

		mb->index = i;
		mb->sending = false;

		mb->channel.hsp = hsp;
		mb->channel.type = TEGRA_HSP_MBOX_TYPE_SM;
		mb->channel.regs = hsp->regs + SZ_64K + i * SZ_32K;
		mb->channel.chan = &hsp->mbox.chans[i];
		mb->channel.chan->con_priv = &mb->channel;
	}

	return 0;
}

static int tegra_hsp_interrupts_show(struct seq_file *s, void *data)
{
	struct tegra_hsp *hsp = s->private;
	unsigned int i;

	seq_printf(s, "interrupts: %u\n", hsp->num_si);

	for (i = 0; i < hsp->num_si; i++)
		seq_printf(s, "  %u: %3u: %u\n", i, hsp->shared_irqs[i],
			   hsp->stats.interrupts.count[i]);

	seq_printf(s, "unhandled: %u\n", hsp->stats.interrupts.unhandled);
	seq_printf(s, "invalid: %u\n", hsp->stats.interrupts.invalid);
	seq_printf(s, "total: %u\n", hsp->stats.interrupts.total);

	seq_printf(s, "shared mailboxes: %u\n", hsp->num_sm);

	for (i = 0; i < hsp->num_sm; i++) {
		seq_printf(s, "  %u: empty %u full %u\n", i,
			   hsp->stats.interrupts.empty[i],
			   hsp->stats.interrupts.full[i]);
	}

	return 0;
}

static int tegra_hsp_interrupts_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra_hsp_interrupts_show, inode->i_private);
}

static const struct file_operations tegra_hsp_interrupts_fops = {
	.open = tegra_hsp_interrupts_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int tegra_hsp_probe(struct platform_device *pdev)
{
	struct tegra_hsp *hsp;
	struct resource *res;
	unsigned int i;
	u32 value;
	int err;

	hsp = devm_kzalloc(&pdev->dev, sizeof(*hsp), GFP_KERNEL);
	if (!hsp)
		return -ENOMEM;

	hsp->dev = &pdev->dev;
	hsp->soc = of_device_get_match_data(&pdev->dev);
	INIT_LIST_HEAD(&hsp->doorbells);
	spin_lock_init(&hsp->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	hsp->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(hsp->regs))
		return PTR_ERR(hsp->regs);

	value = tegra_hsp_readl(hsp, HSP_INT_DIMENSIONING);
	hsp->num_sm = (value >> HSP_nSM_SHIFT) & HSP_nINT_MASK;
	hsp->num_ss = (value >> HSP_nSS_SHIFT) & HSP_nINT_MASK;
	hsp->num_as = (value >> HSP_nAS_SHIFT) & HSP_nINT_MASK;
	hsp->num_db = (value >> HSP_nDB_SHIFT) & HSP_nINT_MASK;
	hsp->num_si = (value >> HSP_nSI_SHIFT) & HSP_nINT_MASK;

	for (i = 0; i < hsp->num_si; i++) {
		u32 value = tegra_hsp_readl(hsp, HSP_INT_IE(i));
		dev_info(&pdev->dev, "IE%u > %08x\n", i, value);
		/*
		value = 0;
		dev_info(&pdev->dev, "IE%u < %08x\n", i, value);
		tegra_hsp_writel(hsp, 0, HSP_INT_IE(i));
		*/
	}

	hsp->si_empty = 1;
	hsp->si_full = 0;

	err = platform_get_irq_byname(pdev, "doorbell");
	if (err >= 0)
		hsp->doorbell_irq = err;

	if (hsp->num_si > 0) {
		unsigned int count = 0;

		hsp->shared_irqs = devm_kcalloc(&pdev->dev, hsp->num_si,
						sizeof(*hsp->shared_irqs),
						GFP_KERNEL);
		if (!hsp->shared_irqs)
			return -ENOMEM;

		for (i = 0; i < hsp->num_si; i++) {
			char *name;

			name = kasprintf(GFP_KERNEL, "shared%u", i);
			if (!name)
				return -ENOMEM;

			err = platform_get_irq_byname(pdev, name);
			if (err >= 0) {
				hsp->shared_irqs[i] = err;
				count++;
			}

			kfree(name);
		}

		if (count == 0) {
			devm_kfree(&pdev->dev, hsp->shared_irqs);
			hsp->shared_irqs = NULL;
		}
	}

	hsp->mbox.of_xlate = of_tegra_hsp_xlate;
	/* First nSM are reserved for mailboxes */
	hsp->mbox.num_chans = 32;
	hsp->mbox.dev = &pdev->dev;
	hsp->mbox.txdone_irq = false;
	hsp->mbox.txdone_poll = false;
	hsp->mbox.ops = &tegra_hsp_ops;

	hsp->mbox.chans = devm_kcalloc(&pdev->dev, hsp->mbox.num_chans,
					sizeof(*hsp->mbox.chans),
					GFP_KERNEL);
	if (!hsp->mbox.chans)
		return -ENOMEM;

	if (hsp->doorbell_irq) {
		err = tegra_hsp_add_doorbells(hsp);
		if (err < 0) {
			dev_err(&pdev->dev, "failed to add doorbells: %d\n",
			        err);
			return err;
		}
	}

	if (hsp->shared_irqs) {
		err = tegra_hsp_add_mailboxes(hsp, &pdev->dev);
		if (err < 0) {
			dev_err(&pdev->dev, "failed to add mailboxes: %d\n",
			        err);
			goto remove_doorbells;
		}
	}

	platform_set_drvdata(pdev, hsp);

	err = mbox_controller_register(&hsp->mbox);
	if (err) {
		dev_err(&pdev->dev, "failed to register mailbox: %d\n", err);
		goto remove_doorbells;
	}

	if (hsp->doorbell_irq) {
		err = devm_request_irq(&pdev->dev, hsp->doorbell_irq,
				       tegra_hsp_doorbell_irq, IRQF_NO_SUSPEND,
				       dev_name(&pdev->dev), hsp);
		if (err < 0) {
			dev_err(&pdev->dev,
			        "failed to request doorbell IRQ#%u: %d\n",
				hsp->doorbell_irq, err);
			goto unregister_mbox_controller;
		}
	}

	if (hsp->shared_irqs) {
#if 1
		for (i = 0; i < hsp->num_si; i++) {
			if (hsp->shared_irqs[i] > 0) {
				err = devm_request_irq(&pdev->dev, hsp->shared_irqs[i],
						       tegra_hsp_shared_irq, 0,
						       dev_name(&pdev->dev), hsp);
				if (err < 0) {
					dev_err(&pdev->dev,
						"failed to request shared%u IRQ%u: %d\n",
						i, hsp->shared_irqs[i], err);
					goto unregister_mbox_controller;
				}

				dev_info(&pdev->dev, "interrupt shared%u requested: %u\n", i, hsp->shared_irqs[i]);
			}
		}
#else
		if (hsp->shared_irqs[hsp->si_empty] > 0) {
			err = devm_request_irq(&pdev->dev, hsp->shared_irqs[hsp->si_empty],
					       tegra_hsp_empty_irq, 0, dev_name(&pdev->dev), hsp);
			if (err < 0) {
				dev_err(&pdev->dev,
					"failed to request shared%u IRQ%u: %d\n",
					hsp->si_empty, hsp->shared_irqs[hsp->si_empty], err);
				goto unregister_mbox_controller;
			}

			dev_info(&pdev->dev, "EMPTY interrupt shared%u requested: %u\n", hsp->si_empty, hsp->shared_irqs[hsp->si_empty]);
		}

		if (hsp->shared_irqs[hsp->si_full] > 0) {
			err = devm_request_irq(&pdev->dev, hsp->shared_irqs[hsp->si_full],
					       tegra_hsp_full_irq, 0, dev_name(&pdev->dev), hsp);
			if (err < 0) {
				dev_err(&pdev->dev,
					"failed to request shared%u IRQ%u: %d\n",
					hsp->si_empty, hsp->shared_irqs[i], err);
				goto unregister_mbox_controller;
			}

			dev_info(&pdev->dev, "EMPTY interrupt shared%u requested: %u\n", hsp->si_empty, hsp->shared_irqs[hsp->si_empty]);
		}
#endif
	}

	hsp->debugfs = debugfs_create_dir(dev_name(&pdev->dev), NULL);
	if (hsp->debugfs) {
		debugfs_create_file("stats", S_IRUGO, hsp->debugfs, hsp,
				    &tegra_hsp_interrupts_fops);
	}

	return 0;

unregister_mbox_controller:
	mbox_controller_unregister(&hsp->mbox);
remove_doorbells:
	if (hsp->doorbell_irq)
		tegra_hsp_remove_doorbells(hsp);

	return err;
}

static int tegra_hsp_remove(struct platform_device *pdev)
{
	struct tegra_hsp *hsp = platform_get_drvdata(pdev);

	mbox_controller_unregister(&hsp->mbox);
	if (hsp->doorbell_irq)
		tegra_hsp_remove_doorbells(hsp);

	return 0;
}

static const struct tegra_hsp_db_map tegra186_hsp_db_map[] = {
	{ "ccplex", TEGRA_HSP_DB_MASTER_CCPLEX, HSP_DB_CCPLEX, },
	{ "bpmp",   TEGRA_HSP_DB_MASTER_BPMP,   HSP_DB_BPMP,   },
	{ /* sentinel */ }
};

static const struct tegra_hsp_soc tegra186_hsp_soc = {
	.map = tegra186_hsp_db_map,
};

static const struct of_device_id tegra_hsp_match[] = {
	{ .compatible = "nvidia,tegra186-hsp", .data = &tegra186_hsp_soc },
	{ }
};

static struct platform_driver tegra_hsp_driver = {
	.driver = {
		.name = "tegra-hsp",
		.of_match_table = tegra_hsp_match,
	},
	.probe = tegra_hsp_probe,
	.remove = tegra_hsp_remove,
};

static int __init tegra_hsp_init(void)
{
	return platform_driver_register(&tegra_hsp_driver);
}
core_initcall(tegra_hsp_init);
