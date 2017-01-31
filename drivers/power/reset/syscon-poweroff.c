/*
 * Generic Syscon Poweroff Driver
 *
 * Copyright (c) 2015, National Instruments Corp.
 * Author: Moritz Fischer <moritz.fischer@ettus.com>
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

#include <linux/kallsyms.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/mfd/syscon.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/regmap.h>
#include <linux/system-power.h>

struct syscon {
	struct system_power_chip chip;
	struct regmap *map;
	u32 offset;
	u32 value;
	u32 mask;
};

static inline struct syscon *to_syscon(struct system_power_chip *chip)
{
	return container_of(chip, struct syscon, chip);
}

static int syscon_power_off(struct system_power_chip *chip)
{
	struct syscon *syscon = to_syscon(chip);

	/* Issue the poweroff */
	regmap_update_bits(syscon->map, syscon->offset, syscon->mask,
			   syscon->value);

	msleep(1000);

	pr_emerg("Unable to poweroff system\n");

	return 0;
}

static int syscon_poweroff_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int value_err, mask_err, err;
	struct syscon *syscon;

	syscon = devm_kzalloc(&pdev->dev, sizeof(*syscon), GFP_KERNEL);
	if (!syscon)
		return -ENOMEM;

	syscon->map = syscon_regmap_lookup_by_phandle(np, "regmap");
	if (IS_ERR(syscon->map)) {
		dev_err(&pdev->dev, "unable to get syscon");
		return PTR_ERR(syscon->map);
	}

	if (of_property_read_u32(np, "offset", &syscon->offset)) {
		dev_err(&pdev->dev, "unable to read 'offset'");
		return -EINVAL;
	}

	value_err = of_property_read_u32(np, "value", &syscon->value);
	mask_err = of_property_read_u32(np, "mask", &syscon->mask);
	if (value_err && mask_err) {
		dev_err(&pdev->dev, "unable to read 'value' and 'mask'");
		return -EINVAL;
	}

	if (value_err) {
		/* support old binding */
		syscon->value = syscon->mask;
		syscon->mask = 0xFFFFFFFF;
	} else if (mask_err) {
		/* support value without mask*/
		syscon->mask = 0xFFFFFFFF;
	}

	syscon->chip.level = SYSTEM_POWER_LEVEL_SOC;
	syscon->chip.dev = &pdev->dev;
	syscon->chip.power_off = syscon_power_off;

	err = system_power_chip_add(&syscon->chip);
	if (err < 0)
		return err;

	platform_set_drvdata(pdev, syscon);

	return 0;
}

static int syscon_poweroff_remove(struct platform_device *pdev)
{
	struct syscon *syscon = platform_get_drvdata(pdev);

	return system_power_chip_remove(&syscon->chip);
}

static const struct of_device_id syscon_poweroff_of_match[] = {
	{ .compatible = "syscon-poweroff" },
	{}
};

static struct platform_driver syscon_poweroff_driver = {
	.probe = syscon_poweroff_probe,
	.remove = syscon_poweroff_remove,
	.driver = {
		.name = "syscon-poweroff",
		.of_match_table = syscon_poweroff_of_match,
	},
};

static int __init syscon_poweroff_register(void)
{
	return platform_driver_register(&syscon_poweroff_driver);
}
device_initcall(syscon_poweroff_register);
