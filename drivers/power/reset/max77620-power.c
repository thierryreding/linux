/*
 * Power off driver for Maxim MAX77620 device.
 *
 * Copyright (c) 2014-2016, NVIDIA CORPORATION.  All rights reserved.
 *
 * Based on work by Chaitanya Bandi <bandik@nvidia.com>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */

#include <linux/errno.h>
#include <linux/mfd/max77620.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/system-power.h>

struct max77620_power {
	struct system_power_chip chip;
	struct regmap *regmap;
};

static struct max77620_power *to_max77620_power(struct system_power_chip *chip)
{
	return container_of(chip, struct max77620_power, chip);
}

static int max77620_restart(struct system_power_chip *chip,
			    enum reboot_mode mode, char *cmd)
{
	struct max77620_power *power = to_max77620_power(chip);
	int err;

	err = regmap_update_bits(power->regmap, MAX77620_REG_ONOFFCNFG2,
				 MAX77620_ONOFFCNFG2_SFT_RST_WK,
				 MAX77620_ONOFFCNFG2_SFT_RST_WK);
	if (err < 0) {
		dev_err(chip->dev, "failed to set SFT_RST_WK: %d\n", err);
		return err;
	}

	err = regmap_update_bits(power->regmap, MAX77620_REG_ONOFFCNFG1,
				 MAX77620_ONOFFCNFG1_SFT_RST,
				 MAX77620_ONOFFCNFG1_SFT_RST);
	if (err < 0) {
		dev_err(chip->dev, "failed to set SFT_RST: %d\n", err);
		return err;
	}

	return 0;
}

static int max77620_power_off(struct system_power_chip *chip)
{
	struct max77620_power *power = to_max77620_power(chip);
	unsigned int value;
	int err;

	/* clear power key interrupts */
	err = regmap_read(power->regmap, MAX77620_REG_ONOFFIRQ, &value);
	if (err < 0) {
		dev_err(chip->dev, "failed to clear power key interrupts: %d\n", err);
		return err;
	}

	/* clear RTC interrupts */
	/*
	err = regmap_read(power->regmap, MAX77620_REG_RTCINT, &value);
	if (err < 0) {
		dev_err(chip->dev, "failed to clear RTC interrupts: %d\n", err);
		return err;
	}
	*/

	/* clear TOP interrupts */
	err = regmap_read(power->regmap, MAX77620_REG_IRQTOP, &value);
	if (err < 0) {
		dev_err(chip->dev, "failed to clear interrupts: %d\n", err);
		return err;
	}

	err = regmap_update_bits(power->regmap, MAX77620_REG_ONOFFCNFG2,
				 MAX77620_ONOFFCNFG2_SFT_RST_WK, 0);
	if (err < 0) {
		dev_err(chip->dev, "failed to clear SFT_RST_WK: %d\n", err);
		return err;
	}

	err = regmap_update_bits(power->regmap, MAX77620_REG_ONOFFCNFG1,
				 MAX77620_ONOFFCNFG1_SFT_RST,
				 MAX77620_ONOFFCNFG1_SFT_RST);
	if (err < 0) {
		dev_err(chip->dev, "failed to set SFT_RST: %d\n", err);
		return err;
	}

	return 0;
}

static int max77620_power_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.parent->of_node;
	struct max77620_power *power;
	unsigned int value;
	int err;

	if (!of_property_read_bool(np, "system-power-controller"))
		return 0;

	power = devm_kzalloc(&pdev->dev, sizeof(*power), GFP_KERNEL);
	if (!power)
		return -ENOMEM;

	power->regmap = dev_get_regmap(pdev->dev.parent, NULL);

	err = regmap_read(power->regmap, MAX77620_REG_NVERC, &value);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to read event recorder: %d\n", err);
		return err;
	}

	dev_dbg(&pdev->dev, "event recorder: %#x\n", value);

	power->chip.level = SYSTEM_POWER_LEVEL_SYSTEM;
	power->chip.dev = &pdev->dev;
	power->chip.power_off = max77620_power_off;

	if (of_property_read_bool(np, "system-reset-controller"))
		power->chip.restart = max77620_restart;

	err = system_power_chip_add(&power->chip);
	if (err < 0)
		return err;

	platform_set_drvdata(pdev, power);

	return 0;
}

static int max77620_power_remove(struct platform_device *pdev)
{
	struct max77620_power *power = platform_get_drvdata(pdev);

	system_power_chip_remove(&power->chip);

	return 0;
}

static struct platform_driver max77620_power_driver = {
	.driver = {
		.name = "max77620-power",
	},
	.probe = max77620_power_probe,
	.remove = max77620_power_remove,
};
module_platform_driver(max77620_power_driver);

MODULE_DESCRIPTION("Maxim MAX77620 PMIC power off and restart driver");
MODULE_AUTHOR("Thierry Reding <treding@nvidia.com>");
MODULE_ALIAS("platform:max77620-power");
MODULE_LICENSE("GPL v2");
