// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 NVIDIA Corporation. All rights reserved.
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

/* timer registers */
#define TMRCR 0x000
#define  TMRCR_ENABLE BIT(31)
#define  TMRCR_PERIODIC BIT(30)
#define  TMRCR_PTV(x) ((x) & 0x0fffffff)

#define TMRCSSR 0x008
#define  TMRCSSR_SRC_USEC (0 << 0)

/* watchdog registers */
#define WDTCR 0x000
#define  WDTCR_SYSTEM_POR_ENABLE BIT(16)
#define  WDTCR_LOCAL_INT_ENABLE BIT(12)
#define  WDTCR_PERIOD_MASK (0xff << 4)
#define  WDTCR_PERIOD(x) (((x) & 0xff) << 4)
#define  WDTCR_TIMER_SOURCE_MASK 0xf
#define  WDTCR_TIMER_SOURCE(x) ((x) & 0xf)

#define WDTSR 0x004

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
	struct device *parent;
	void __iomem *regs;
	unsigned int index;
	unsigned int irq;

	irqreturn_t (*handler)(int irq, void *data);
	void *handler_data;
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

	struct tegra186_wdt *wdt;
};

static inline struct tegra186_timer *
to_tegra186_timer(struct watchdog_device *wdt)
{
	return container_of(wdt, struct tegra186_timer, wdt);
}

static void tmr_writel(struct tegra186_tmr *tmr, u32 value, unsigned int offset)
{
	writel(value, tmr->regs + offset);
}

/*
static u32 tmr_readl(struct tegra186_tmr *tmr, unsigned int offset)
{
	return readl(tmr->regs + offset);
}
*/

static void wdt_writel(struct tegra186_wdt *wdt, u32 value, unsigned int offset)
{
	writel(value, wdt->regs + offset);
}

static u32 wdt_readl(struct tegra186_wdt *wdt, unsigned int offset)
{
	return readl(wdt->regs + offset);
}

static irqreturn_t tegra186_timer_irq(int irq, void *data)
{
	struct tegra186_tmr *tmr = data;
	irqreturn_t ret = IRQ_NONE;

	dev_info(tmr->parent, "> %s(irq=%d, data=%px)\n", __func__, irq, data);

	if (tmr->handler)
		ret = tmr->handler(irq, tmr->handler_data);

	dev_info(tmr->parent, "< %s() = %d\n", __func__, ret);
	return ret;
}

static struct tegra186_tmr *tegra186_tmr_create(struct tegra186_timer *tegra,
						unsigned int index)
{
	struct platform_device *pdev = to_platform_device(tegra->dev);
	unsigned int offset = 0x10000 + index * 0x10000;
	struct tegra186_tmr *tmr;
	int err;

	tmr = devm_kzalloc(tegra->dev, sizeof(*tmr), GFP_KERNEL);
	if (!tmr)
		return ERR_PTR(-ENOMEM);

	tmr->parent = tegra->dev;
	tmr->regs = tegra->regs + offset;
	tmr->index = index;

	err = platform_get_irq(pdev, index);
	if (err < 0) {
		dev_err(tegra->dev, "failed to get interrupt #%u: %d\n",
			index, err);
		return ERR_PTR(err);
	}

	tmr->irq = err;

	err = devm_request_irq(tegra->dev, tmr->irq, tegra186_timer_irq,
			       IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
			       "tegra186-timer", tmr);
	if (err < 0) {
		dev_err(tegra->dev, "failed to request IRQ#%u for timer %u: %d\n",
			tmr->irq, index, err);
		return ERR_PTR(err);
	}

	return tmr;
}

static const struct watchdog_info tegra186_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE | WDIOF_KEEPALIVEPING,
	.identity = "NVIDIA Tegra186 WDT",
};

static void tegra186_wdt_disable(struct tegra186_wdt *wdt)
{
	wdt_writel(wdt, WDTUR_UNLOCK_PATTERN, WDTUR);
	wdt_writel(wdt, WDTCMDR_DISABLE_COUNTER, WDTCMDR);

	/* disable timer */
	tmr_writel(wdt->tmr, 0, TMRCR);
}

static void tegra186_wdt_enable(struct tegra186_wdt *wdt)
{
	u32 value;

	/* select microsecond source */
	tmr_writel(wdt->tmr, TMRCSSR_SRC_USEC, TMRCSSR);

	/* configure timer */
	value = TMRCR_PTV(wdt->base.timeout * USEC_PER_SEC) |
		TMRCR_PERIODIC | TMRCR_ENABLE;
	tmr_writel(wdt->tmr, value, TMRCR);

	if (!wdt->locked) {
		/* XXX */
		value = wdt_readl(wdt, WDTCR);
		value &= ~WDTCR_TIMER_SOURCE_MASK;
		value |= WDTCR_TIMER_SOURCE(wdt->tmr->index);
		value &= ~WDTCR_PERIOD_MASK;
		value |= WDTCR_PERIOD(1);
		value |= WDTCR_LOCAL_INT_ENABLE;
		wdt_writel(wdt, value, WDTCR);
	}

	wdt_writel(wdt, WDTCMDR_START_COUNTER, WDTCMDR);
}

static int tegra186_wdt_start(struct watchdog_device *wdd)
{
	struct tegra186_wdt *wdt = to_tegra186_wdt(wdd);

	dev_info(wdd->parent, "> %s(wdd=%px)\n", __func__, wdd);

	tegra186_wdt_enable(wdt);

	dev_info(wdd->parent, "< %s()\n", __func__);
	return 0;
}

static int tegra186_wdt_stop(struct watchdog_device *wdd)
{
	struct tegra186_wdt *wdt = to_tegra186_wdt(wdd);

	dev_info(wdd->parent, "> %s(wdd=%px)\n", __func__, wdd);

	tegra186_wdt_disable(wdt);

	dev_info(wdd->parent, "< %s()\n", __func__);
	return 0;
}

static int tegra186_wdt_ping(struct watchdog_device *wdd)
{
	struct tegra186_wdt *wdt = to_tegra186_wdt(wdd);

	dev_info(wdd->parent, "> %s(wdd=%px)\n", __func__, wdd);

	tegra186_wdt_disable(wdt);
	tegra186_wdt_enable(wdt);

	dev_info(wdd->parent, "< %s()\n", __func__);
	return 0;
}

static int tegra186_wdt_set_timeout(struct watchdog_device *wdd,
				    unsigned int timeout)
{
	struct tegra186_wdt *wdt = to_tegra186_wdt(wdd);

	dev_info(wdd->parent, "> %s(wdd=%px, timeout=%u)\n", __func__, wdd,
		 timeout);

	tegra186_wdt_disable(wdt);
	wdt->base.timeout = timeout;
	tegra186_wdt_enable(wdt);

	dev_info(wdd->parent, "< %s()\n", __func__);

	return 0;
}

static const struct watchdog_ops tegra186_wdt_ops = {
	.owner = THIS_MODULE,
	.start = tegra186_wdt_start,
	.stop = tegra186_wdt_stop,
	.ping = tegra186_wdt_ping,
	.set_timeout = tegra186_wdt_set_timeout,
};

static irqreturn_t tegra186_wdt_irq(int irq, void *data)
{
	struct tegra186_wdt *wdt = data;

	dev_info(wdt->base.parent, "> %s(irq=%d, data=%px)\n", __func__, irq,
		 data);

	tegra186_wdt_disable(wdt);
	tegra186_wdt_enable(wdt);

	dev_info(wdt->base.parent, "< %s()\n", __func__);
	return IRQ_HANDLED;
}

static struct tegra186_wdt *tegra186_wdt_create(struct tegra186_timer *tegra,
						unsigned int index)
{
	unsigned int offset = 0x10000, source;
	struct tegra186_wdt *wdt;
	u32 value;
	int err;

	dev_info(tegra->dev, "> %s(tegra=%px, index=%u)\n", __func__, tegra,
		 index);

	offset += tegra->soc->num_timers * 0x10000 + index * 0x10000;
	dev_info(tegra->dev, "  soc: %ps\n", tegra->soc);
	dev_info(tegra->dev, "  offset: %x\n", offset);

	wdt = devm_kzalloc(tegra->dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return ERR_PTR(-ENOMEM);

	dev_info(tegra->dev, "  wdt: %px\n", wdt);

	wdt->regs = tegra->regs + offset;
	wdt->index = index;

	value = wdt_readl(wdt, WDTCR);

	if (value & WDTCR_LOCAL_INT_ENABLE)
		wdt->locked = true;

	source = value & WDTCR_TIMER_SOURCE_MASK;

	wdt->tmr = tegra186_tmr_create(tegra, source);
	if (IS_ERR(wdt->tmr))
		return ERR_CAST(wdt->tmr);

	dev_info(tegra->dev, "  tmr: %px\n", wdt->tmr);

	wdt->tmr->handler = tegra186_wdt_irq;
	wdt->tmr->handler_data = wdt;

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

	dev_info(tegra->dev, "< %s() = %px\n", __func__, wdt);
	return wdt;
}

static int tegra186_timer_probe(struct platform_device *pdev)
{
	struct tegra186_timer *tegra;
	int err;

	dev_info(&pdev->dev, "> %s(pdev=%px)\n", __func__, pdev);

	tegra = devm_kzalloc(&pdev->dev, sizeof(*tegra), GFP_KERNEL);
	if (!tegra)
		return -ENOMEM;

	tegra->soc = of_device_get_match_data(&pdev->dev);
	dev_set_drvdata(&pdev->dev, tegra);
	tegra->dev = &pdev->dev;

	tegra->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(tegra->regs))
		return PTR_ERR(tegra->regs);

	tegra->wdt = tegra186_wdt_create(tegra, 0);
	if (IS_ERR(tegra->wdt)) {
		err = PTR_ERR(tegra->wdt);
		dev_err(&pdev->dev, "failed to create WDT: %d\n", err);
		return err;
	}

	dev_info(&pdev->dev, "< %s()\n", __func__);
	return 0;
}

static int tegra186_timer_suspend(struct device *dev)
{
	dev_info(dev, "> %s(dev=%px)\n", __func__, dev);
	dev_info(dev, "< %s()\n", __func__);
	return 0;
}

static int tegra186_timer_resume(struct device *dev)
{
	dev_info(dev, "> %s(dev=%px)\n", __func__, dev);
	dev_info(dev, "< %s()\n", __func__);
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
