// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020 NVIDIA Corporation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/watchdog.h>

/* shared registers */
#define TKEIE(x) (0x100 + ((x) * 4))
#define  TKEIE_WDT_MASK(x, y) ((y) << (16 + 4 * (x)))

/* timer registers */
#define TMRCR 0x000
#define  TMRCR_ENABLE BIT(31)
#define  TMRCR_PERIODIC BIT(30)
#define  TMRCR_PTV(x) ((x) & 0x0fffffff)

#define TMRSR 0x004
#define  TMRSR_INTR_CLR BIT(30)

#define TMRCSSR 0x008
#define  TMRCSSR_SRC_USEC (0 << 0)

/* watchdog registers */
#define WDTCR 0x000
#define  WDTCR_SYSTEM_POR_RESET_ENABLE BIT(16)
#define  WDTCR_SYSTEM_DEBUG_RESET_ENABLE BIT(15)
#define  WDTCR_REMOTE_INT_ENABLE BIT(14)
#define  WDTCR_LOCAL_FIQ_ENABLE BIT(13)
#define  WDTCR_LOCAL_INT_ENABLE BIT(12)
#define  WDTCR_PERIOD_MASK (0xff << 4)
#define  WDTCR_PERIOD(x) (((x) & 0xff) << 4)
#define  WDTCR_TIMER_SOURCE_MASK 0xf
#define  WDTCR_TIMER_SOURCE(x) ((x) & 0xf)

#define WDTCMDR 0x008
#define  WDTCMDR_DISABLE_COUNTER BIT(1)
#define  WDTCMDR_START_COUNTER BIT(0)

#define WDTUR 0x00c
#define  WDTUR_UNLOCK_PATTERN 0x0000c45a

struct tegra186_timer_soc {
	unsigned int num_timers;
	unsigned int num_wdts;
};

struct tegra186_tmr {
	struct tegra186_timer *parent;
	void __iomem *regs;
	unsigned int index;
	unsigned int hwirq;
};

struct tegra186_wdt {
	struct watchdog_device base;

	void __iomem *regs;
	unsigned int index;
	bool locked;

	struct tegra186_tmr *tmr;
};

static inline struct tegra186_wdt *to_tegra186_wdt(struct watchdog_device *wdd)
{
	return container_of(wdd, struct tegra186_wdt, base);
}

struct tegra186_timer {
	const struct tegra186_timer_soc *soc;
	struct device *dev;
	void __iomem *regs;
	unsigned int irq;

	struct tegra186_wdt *wdt;
};

static void tmr_writel(struct tegra186_tmr *tmr, u32 value, unsigned int offset)
{
	writel(value, tmr->regs + offset);
}

static void wdt_writel(struct tegra186_wdt *wdt, u32 value, unsigned int offset)
{
	writel(value, wdt->regs + offset);
}

static u32 wdt_readl(struct tegra186_wdt *wdt, unsigned int offset)
{
	return readl(wdt->regs + offset);
}

static struct tegra186_tmr *tegra186_tmr_create(struct tegra186_timer *tegra,
						unsigned int index)
{
	unsigned int offset = 0x10000 + index * 0x10000;
	struct tegra186_tmr *tmr;

	tmr = devm_kzalloc(tegra->dev, sizeof(*tmr), GFP_KERNEL);
	if (!tmr)
		return ERR_PTR(-ENOMEM);

	tmr->parent = tegra;
	tmr->regs = tegra->regs + offset;
	tmr->index = index;
	tmr->hwirq = 0;

	return tmr;
}

static const struct watchdog_info tegra186_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE | WDIOF_KEEPALIVEPING,
	.identity = "NVIDIA Tegra186 WDT",
};

static void tegra186_wdt_disable(struct tegra186_wdt *wdt)
{
	/* unlock and disable the watchdog */
	wdt_writel(wdt, WDTUR_UNLOCK_PATTERN, WDTUR);
	wdt_writel(wdt, WDTCMDR_DISABLE_COUNTER, WDTCMDR);

	/* disable timer */
	tmr_writel(wdt->tmr, 0, TMRCR);
}

static void tegra186_wdt_enable(struct tegra186_wdt *wdt)
{
	struct tegra186_timer *tegra = wdt->tmr->parent;
	u32 value;

	/* unmask hardware IRQ, this may have been lost across powergate */
	value = TKEIE_WDT_MASK(wdt->index, 1);
	writel(value, tegra->regs + TKEIE(wdt->tmr->hwirq));

	/* clear interrupt */
	tmr_writel(wdt->tmr, TMRSR_INTR_CLR, TMRSR);

	/* select microsecond source */
	tmr_writel(wdt->tmr, TMRCSSR_SRC_USEC, TMRCSSR);

	/* configure timer (system reset happens on the fifth expiration) */
	value = TMRCR_PTV(wdt->base.timeout * USEC_PER_SEC / 5) |
		TMRCR_PERIODIC | TMRCR_ENABLE;
	tmr_writel(wdt->tmr, value, TMRCR);

	if (!wdt->locked) {
		value = wdt_readl(wdt, WDTCR);

		/* select the proper timer source */
		value &= ~WDTCR_TIMER_SOURCE_MASK;
		value |= WDTCR_TIMER_SOURCE(wdt->tmr->index);

		/* single timer period since that's already configured */
		value &= ~WDTCR_PERIOD_MASK;
		value |= WDTCR_PERIOD(1);

		/* enable local interrupt for WDT petting */
		value |= WDTCR_LOCAL_INT_ENABLE;

		/* enable local FIQ and remote interrupt for debug dump */
		if (0)
			value |= WDTCR_REMOTE_INT_ENABLE |
				 WDTCR_LOCAL_FIQ_ENABLE;

		/* enable system debug reset (doesn't properly reboot) */
		if (0)
			value |= WDTCR_SYSTEM_DEBUG_RESET_ENABLE;

		/* enable system POR reset */
		value |= WDTCR_SYSTEM_POR_RESET_ENABLE;

		wdt_writel(wdt, value, WDTCR);
	}

	wdt_writel(wdt, WDTCMDR_START_COUNTER, WDTCMDR);
}

static int tegra186_wdt_start(struct watchdog_device *wdd)
{
	struct tegra186_wdt *wdt = to_tegra186_wdt(wdd);

	tegra186_wdt_enable(wdt);

	return 0;
}

static int tegra186_wdt_stop(struct watchdog_device *wdd)
{
	struct tegra186_wdt *wdt = to_tegra186_wdt(wdd);

	tegra186_wdt_disable(wdt);

	return 0;
}

static int tegra186_wdt_ping(struct watchdog_device *wdd)
{
	struct tegra186_wdt *wdt = to_tegra186_wdt(wdd);

	tegra186_wdt_disable(wdt);
	tegra186_wdt_enable(wdt);

	return 0;
}

static int tegra186_wdt_set_timeout(struct watchdog_device *wdd,
				    unsigned int timeout)
{
	struct tegra186_wdt *wdt = to_tegra186_wdt(wdd);

	tegra186_wdt_disable(wdt);
	wdt->base.timeout = timeout;
	tegra186_wdt_enable(wdt);

	return 0;
}

static const struct watchdog_ops tegra186_wdt_ops = {
	.owner = THIS_MODULE,
	.start = tegra186_wdt_start,
	.stop = tegra186_wdt_stop,
	.ping = tegra186_wdt_ping,
	.set_timeout = tegra186_wdt_set_timeout,
};

static struct tegra186_wdt *tegra186_wdt_create(struct tegra186_timer *tegra,
						unsigned int index)
{
	unsigned int offset = 0x10000, source;
	struct tegra186_wdt *wdt;
	u32 value;
	int err;

	offset += tegra->soc->num_timers * 0x10000 + index * 0x10000;

	wdt = devm_kzalloc(tegra->dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return ERR_PTR(-ENOMEM);

	wdt->regs = tegra->regs + offset;
	wdt->index = index;

	/* read the watchdog configuration since it might be locked down */
	value = wdt_readl(wdt, WDTCR);

	if (value & WDTCR_LOCAL_INT_ENABLE)
		wdt->locked = true;

	source = value & WDTCR_TIMER_SOURCE_MASK;

	wdt->tmr = tegra186_tmr_create(tegra, source);
	if (IS_ERR(wdt->tmr))
		return ERR_CAST(wdt->tmr);

	wdt->base.info = &tegra186_wdt_info;
	wdt->base.ops = &tegra186_wdt_ops;
	wdt->base.min_timeout = 1;
	wdt->base.max_timeout = 255;
	wdt->base.parent = tegra->dev;

	err = watchdog_init_timeout(&wdt->base, 5, tegra->dev);
	if (err < 0) {
		dev_err(tegra->dev, "failed to initialize timeout: %d\n", err);
		return ERR_PTR(err);
	}

	err = devm_watchdog_register_device(tegra->dev, &wdt->base);
	if (err < 0) {
		dev_err(tegra->dev, "failed to register WDT: %d\n", err);
		return ERR_PTR(err);
	}

	return wdt;
}

static irqreturn_t tegra186_timer_irq(int irq, void *data)
{
	struct tegra186_timer *tegra = data;

	if (tegra->wdt) {
		tegra186_wdt_disable(tegra->wdt);
		tegra186_wdt_enable(tegra->wdt);
	}

	return IRQ_HANDLED;
}

static int tegra186_timer_probe(struct platform_device *pdev)
{
	struct tegra186_timer *tegra;
	int err;

	tegra = devm_kzalloc(&pdev->dev, sizeof(*tegra), GFP_KERNEL);
	if (!tegra)
		return -ENOMEM;

	tegra->soc = of_device_get_match_data(&pdev->dev);
	dev_set_drvdata(&pdev->dev, tegra);
	tegra->dev = &pdev->dev;

	tegra->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(tegra->regs))
		return PTR_ERR(tegra->regs);

	err = platform_get_irq(pdev, 0);
	if (err < 0) {
		dev_err(tegra->dev, "failed to get interrupt #0: %d\n", err);
		return err;
	}

	tegra->irq = err;

	err = devm_request_irq(tegra->dev, tegra->irq, tegra186_timer_irq,
			       IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
			       "tegra186-timer", tegra);
	if (err < 0) {
		dev_err(tegra->dev, "failed to request IRQ#%u: %d\n",
			tegra->irq, err);
		return err;
	}

	/* create a watchdog using a preconfigured timer */
	tegra->wdt = tegra186_wdt_create(tegra, 0);
	if (IS_ERR(tegra->wdt)) {
		err = PTR_ERR(tegra->wdt);
		dev_err(&pdev->dev, "failed to create WDT: %d\n", err);
		return err;
	}

	return 0;
}

static int __maybe_unused tegra186_timer_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused tegra186_timer_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(tegra186_timer_pm_ops, tegra186_timer_suspend,
			 tegra186_timer_resume);

static const struct tegra186_timer_soc tegra186_timer = {
	.num_timers = 10,
	.num_wdts = 3,
};

static const struct of_device_id tegra186_timer_of_match[] = {
	{ .compatible = "nvidia,tegra186-timer", .data = &tegra186_timer },
	{ }
};

static struct platform_driver tegra186_wdt_driver = {
	.driver = {
		.name = "tegra186-timer",
		.pm = &tegra186_timer_pm_ops,
		.of_match_table = tegra186_timer_of_match,
		.suppress_bind_attrs = true,
	},
	.probe = tegra186_timer_probe,
};
module_platform_driver(tegra186_wdt_driver);

MODULE_AUTHOR("Thierry Reding <treding@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra186 timers driver");
MODULE_LICENSE("GPL v2");
