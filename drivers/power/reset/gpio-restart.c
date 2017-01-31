/*
 * Toggles a GPIO pin to restart a device
 *
 * Copyright (C) 2014 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Based on the gpio-poweroff driver.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/system-power.h>

struct gpio_restart {
	struct system_power_chip chip;
	struct gpio_desc *reset_gpio;
	u32 active_delay_ms;
	u32 inactive_delay_ms;
	u32 wait_delay_ms;
};

static inline struct gpio_restart *
to_gpio_restart(struct system_power_chip *chip)
{
	return container_of(chip, struct gpio_restart, chip);
}

static int gpio_restart(struct system_power_chip *chip, enum reboot_mode mode,
			char *cmd)
{
	struct gpio_restart *restart = to_gpio_restart(chip);

	/* drive it active, also inactive->active edge */
	gpiod_direction_output(restart->reset_gpio, 1);
	mdelay(restart->active_delay_ms);

	/* drive inactive, also active->inactive edge */
	gpiod_set_value(restart->reset_gpio, 0);
	mdelay(restart->inactive_delay_ms);

	/* drive it active, also inactive->active edge */
	gpiod_set_value(restart->reset_gpio, 1);

	/* give it some time */
	msleep(restart->wait_delay_ms);

	WARN_ON(1);

	return 0;
}

static int gpio_restart_probe(struct platform_device *pdev)
{
	enum gpiod_flags flags = GPIOD_OUT_LOW;
	struct gpio_restart *restart;
	int ret;

	restart = devm_kzalloc(&pdev->dev, sizeof(*restart), GFP_KERNEL);
	if (!restart)
		return -ENOMEM;

	if (of_property_read_bool(pdev->dev.of_node, "open-source"))
		flags = GPIOD_IN;

	restart->reset_gpio = devm_gpiod_get(&pdev->dev, NULL, flags);
	if (IS_ERR(restart->reset_gpio)) {
		dev_err(&pdev->dev, "Could net get reset GPIO\n");
		return PTR_ERR(restart->reset_gpio);
	}

	restart->active_delay_ms = 100;
	restart->inactive_delay_ms = 100;
	restart->wait_delay_ms = 3000;

	of_property_read_u32(pdev->dev.of_node, "active-delay",
			     &restart->active_delay_ms);
	of_property_read_u32(pdev->dev.of_node, "inactive-delay",
			     &restart->inactive_delay_ms);
	of_property_read_u32(pdev->dev.of_node, "wait-delay",
			     &restart->wait_delay_ms);

	restart->chip.level = SYSTEM_POWER_LEVEL_SYSTEM;
	restart->chip.dev = &pdev->dev;
	restart->chip.restart = gpio_restart;

	platform_set_drvdata(pdev, restart);

	ret = system_power_chip_add(&restart->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "cannot register restart chip: %d\n", ret);
		return ret;
	}

	return 0;
}

static int gpio_restart_remove(struct platform_device *pdev)
{
	struct gpio_restart *restart = platform_get_drvdata(pdev);

	return system_power_chip_remove(&restart->chip);
}

static const struct of_device_id of_gpio_restart_match[] = {
	{ .compatible = "gpio-restart", },
	{},
};

static struct platform_driver gpio_restart_driver = {
	.probe = gpio_restart_probe,
	.remove = gpio_restart_remove,
	.driver = {
		.name = "restart-gpio",
		.of_match_table = of_gpio_restart_match,
	},
};

module_platform_driver(gpio_restart_driver);

MODULE_AUTHOR("David Riley <davidriley@chromium.org>");
MODULE_DESCRIPTION("GPIO restart driver");
MODULE_LICENSE("GPL");
