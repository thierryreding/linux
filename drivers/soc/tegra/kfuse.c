/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>

#include <soc/tegra/kfuse.h>

#define KFUSE_PD 0x24
#define  KFUSE_PD_STATUS		(1 << 1)
#define  KFUSE_PD_CTRL_POWERDOWN	(1 << 0)
#define  KFUSE_PD_CTRL_POWERUP		(0 << 0)

#define KFUSE_STATE 0x80
#define  KFUSE_STATE_CRCPASS	(1 << 17)
#define  KFUSE_STATE_DONE	(1 << 16)

#define KFUSE_ERRCOUNT 0x84

#define KFUSE_KEYADDR 0x88
#define  KFUSE_KEYADDR_AUTOINC (1 << 16)
#define  KFUSE_KEYADDR_ADDR(x) (((x) & 0xff) << 0)

#define KFUSE_KEYS 0x8c

#define KFUSE_CG1 0x90
#define  KFUSE_CG1_SLCG_CTRL_ENABLE (1 << 0)
#define  KFUSE_CG1_SLCG_CTRL_DISABLE (0 << 0)

struct tegra_kfuse_soc {
	bool supports_sensing;
};

struct tegra_kfuse {
	struct device *dev;
	const struct tegra_kfuse_soc *soc;

	void __iomem *base;
	struct clk *clk;
	struct reset_control *rst;

	size_t size;
};

static int tegra_kfuse_wait_for_decode(struct tegra_kfuse *kfuse,
				       unsigned long timeout)
{
	u32 value;

	timeout = jiffies + msecs_to_jiffies(timeout);

	while (time_before(jiffies, timeout)) {
		value = readl(kfuse->base + KFUSE_STATE);
		if (value & KFUSE_STATE_DONE)
			return 0;

		usleep_range(100, 1000);
	}

	return -ETIMEDOUT;
}

static int tegra_kfuse_wait_for_crc(struct tegra_kfuse *kfuse,
				    unsigned long timeout)
{
	u32 value;

	timeout = jiffies + msecs_to_jiffies(timeout);

	while (time_before(jiffies, timeout)) {
		value = readl(kfuse->base + KFUSE_STATE);
		if (value & KFUSE_STATE_CRCPASS)
			return 0;

		usleep_range(100, 1000);
	}

	return -ETIMEDOUT;
}

static int tegra_kfuse_probe(struct platform_device *pdev)
{
	struct tegra_kfuse *kfuse;
	struct resource *regs;
	int err;

	kfuse = devm_kzalloc(&pdev->dev, sizeof(*kfuse), GFP_KERNEL);
	if (!kfuse)
		return -ENOMEM;

	kfuse->soc = of_device_get_match_data(&pdev->dev);
	kfuse->dev = &pdev->dev;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	kfuse->base = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(kfuse->base))
		return PTR_ERR(kfuse->base);

	kfuse->clk = devm_clk_get(&pdev->dev, "kfuse");
	if (IS_ERR(kfuse->clk)) {
		err = PTR_ERR(kfuse->clk);
		dev_err(&pdev->dev, "failed to get clock: %d\n", err);
		return err;
	}

	kfuse->rst = devm_reset_control_get(&pdev->dev, "kfuse");
	if (IS_ERR(kfuse->rst)) {
		err = PTR_ERR(kfuse->rst);
		dev_err(&pdev->dev, "failed to get reset control: %d\n", err);
		return err;
	}

	platform_set_drvdata(pdev, kfuse);
	pm_runtime_enable(kfuse->dev);
	pm_runtime_get_sync(kfuse->dev);
	pm_runtime_get_sync(kfuse->dev);

	err = tegra_kfuse_wait_for_decode(kfuse, 100);
	if (err < 0) {
		dev_err(kfuse->dev, "error waiting for decode: %d\n", err);
		goto disable;
	}

	err = tegra_kfuse_wait_for_crc(kfuse, 100);
	if (err < 0) {
		dev_err(kfuse->dev, "error waiting for CRC check: %d\n", err);
		goto disable;
	}

	pm_runtime_put(kfuse->dev);

	/*
	 * The ECC-decoded keyglob data is 144 32-bit words (576 bytes).
	 */
	kfuse->size = 576;

	return 0;

disable:
	pm_runtime_put(kfuse->dev);
	return err;
}

static int tegra_kfuse_remove(struct platform_device *pdev)
{
	struct tegra_kfuse *kfuse = platform_get_drvdata(pdev);

	pm_runtime_put(kfuse->dev);

	return 0;
}

static int tegra_kfuse_suspend(struct device *dev)
{
	struct tegra_kfuse *kfuse = dev_get_drvdata(dev);
	int err;

	if (kfuse->soc->supports_sensing)
		writel(KFUSE_CG1_SLCG_CTRL_DISABLE, kfuse->base + KFUSE_CG1);

	err = reset_control_assert(kfuse->rst);
	if (err < 0) {
		dev_err(dev, "failed to assert reset: %d\n", err);
		return err;
	}

	usleep_range(2000, 4000);

	clk_disable_unprepare(kfuse->clk);

	return 0;
}

static int tegra_kfuse_resume(struct device *dev)
{
	struct tegra_kfuse *kfuse = dev_get_drvdata(dev);
	int err;

	err = clk_prepare_enable(kfuse->clk);
	if (err < 0) {
		dev_err(dev, "failed to enable clock: %d\n", err);
		return err;
	}

	usleep_range(1000, 2000);

	err = reset_control_deassert(kfuse->rst);
	if (err < 0) {
		dev_err(dev, "failed to assert reset: %d\n", err);
		goto disable;
	}

	usleep_range(1000, 2000);

	if (kfuse->soc->supports_sensing)
		writel(KFUSE_CG1_SLCG_CTRL_ENABLE, kfuse->base + KFUSE_CG1);

	return 0;

disable:
	clk_disable_unprepare(kfuse->clk);
	return err;
}

static const struct dev_pm_ops tegra_kfuse_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra_kfuse_suspend, tegra_kfuse_resume, NULL)
};

static const struct tegra_kfuse_soc tegra210_kfuse = {
	.supports_sensing = false,
};

static const struct tegra_kfuse_soc tegra186_kfuse = {
	.supports_sensing = true,
};

static const struct of_device_id tegra_kfuse_match[] = {
	{ .compatible = "nvidia,tegra186-kfuse", .data = &tegra186_kfuse },
	{ .compatible = "nvidia,tegra210-kfuse", .data = &tegra210_kfuse },
	{ /* sentinel */ }
};

static struct platform_driver tegra_kfuse_driver = {
	.driver = {
		.name = "tegra-kfuse",
		.of_match_table = tegra_kfuse_match,
		.pm = &tegra_kfuse_pm_ops,
	},
	.probe = tegra_kfuse_probe,
	.remove = tegra_kfuse_remove,
};
module_platform_driver(tegra_kfuse_driver);

static int of_device_match(struct device *dev, void *data)
{
	return dev->of_node == data;
}

struct tegra_kfuse *tegra_kfuse_get(struct device *dev)
{
	struct device_node *np;
	struct device *kfuse;

	np = of_parse_phandle(dev->of_node, "nvidia,kfuse", 0);
	if (!np || !of_device_is_available(np))
		return NULL;

	kfuse = driver_find_device(&tegra_kfuse_driver.driver, NULL, np,
				   of_device_match);
	of_node_put(np);

	if (!kfuse)
		return ERR_PTR(-EPROBE_DEFER);

	return dev_get_drvdata(kfuse);
}
EXPORT_SYMBOL(tegra_kfuse_get);

void tegra_kfuse_put(struct tegra_kfuse *kfuse)
{
	if (kfuse)
		put_device(kfuse->dev);
}
EXPORT_SYMBOL(tegra_kfuse_put);

ssize_t tegra_kfuse_read(struct tegra_kfuse *kfuse, void *buffer, size_t size)
{
	size_t offset;
	u32 value;
	int err;

	if (!buffer && size == 0)
		return kfuse->size;

	if (size > kfuse->size)
		size = kfuse->size;

	pm_runtime_get_sync(kfuse->dev);

	err = tegra_kfuse_wait_for_decode(kfuse, 100);
	if (err < 0) {
		dev_err(kfuse->dev, "error waiting for decode: %d\n", err);
		goto disable;
	}

	err = tegra_kfuse_wait_for_crc(kfuse, 100);
	if (err < 0) {
		dev_err(kfuse->dev, "error waiting for CRC check: %d\n", err);
		goto disable;
	}

	value = KFUSE_KEYADDR_AUTOINC | KFUSE_KEYADDR_ADDR(0);
	writel(value, kfuse->base + KFUSE_KEYADDR);

	for (offset = 0; offset < size; offset += 4) {
		value = readl(kfuse->base + KFUSE_KEYS);
		memcpy(buffer + offset, &value, 4);
	}

	err = offset;

disable:
	pm_runtime_put(kfuse->dev);
	return err;
}
EXPORT_SYMBOL(tegra_kfuse_read);

MODULE_DESCRIPTION("NVIDIA Tegra KFUSE driver");
MODULE_AUTHOR("Thierry Reding <treding@nvidia.com>");
MODULE_LICENSE("GPL v2");
