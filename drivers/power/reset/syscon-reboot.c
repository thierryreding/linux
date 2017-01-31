/*
 * Generic Syscon Reboot Driver
 *
 * Copyright (c) 2013, Applied Micro Circuits Corporation
 * Author: Feng Kan <fkan@apm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/system-power.h>

struct syscon_reboot_context {
	struct system_power_chip chip;
	struct regmap *map;
	u32 offset;
	u32 mask;
};

static inline struct syscon_reboot_context *
to_syscon(struct system_power_chip *chip)
{
	return container_of(chip, struct syscon_reboot_context, chip);
}

static int syscon_restart(struct system_power_chip *chip,
			  enum reboot_mode mode, char *cmd)
{
	struct syscon_reboot_context *ctx = to_syscon(chip);

	/* Issue the reboot */
	regmap_write(ctx->map, ctx->offset, ctx->mask);

	msleep(1000);

	pr_emerg("Unable to restart system\n");

	return 0;
}

static int syscon_reboot_probe(struct platform_device *pdev)
{
	struct syscon_reboot_context *ctx;
	struct device *dev = &pdev->dev;
	int err;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->map = syscon_regmap_lookup_by_phandle(dev->of_node, "regmap");
	if (IS_ERR(ctx->map))
		return PTR_ERR(ctx->map);

	if (of_property_read_u32(pdev->dev.of_node, "offset", &ctx->offset))
		return -EINVAL;

	if (of_property_read_u32(pdev->dev.of_node, "mask", &ctx->mask))
		return -EINVAL;

	ctx->chip.level = SYSTEM_POWER_LEVEL_SOC;
	ctx->chip.dev = &pdev->dev;
	ctx->chip.restart = syscon_restart;

	err = system_power_chip_add(&ctx->chip);
	if (err < 0) {
		dev_err(dev, "failed to register restart chip: %d\n", err);
		return err;
	}

	return 0;
}

static const struct of_device_id syscon_reboot_of_match[] = {
	{ .compatible = "syscon-reboot" },
	{}
};

static struct platform_driver syscon_reboot_driver = {
	.probe = syscon_reboot_probe,
	.driver = {
		.name = "syscon-reboot",
		.of_match_table = syscon_reboot_of_match,
	},
};
builtin_platform_driver(syscon_reboot_driver);
