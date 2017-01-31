/*
 * Toggles a GPIO pin to power down a device
 *
 * Jamie Lentin <jm@lentin.co.uk>
 * Andrew Lunn <andrew@lunn.ch>
 *
 * Copyright (C) 2012 Jamie Lentin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/system-power.h>

#define DEFAULT_TIMEOUT_MS 3000
/*
 * Hold configuration here, cannot be more than one instance of the driver
 * since pm_power_off itself is global.
 */
static u32 timeout = DEFAULT_TIMEOUT_MS;

struct gpio_power_off {
	struct system_power_chip chip;
	struct gpio_desc *gpio;
};

static inline struct gpio_power_off *
to_gpio_power_off(struct system_power_chip *chip)
{
	return container_of(chip, struct gpio_power_off, chip);
}

static int gpio_power_off(struct system_power_chip *chip)
{
	struct gpio_power_off *power = to_gpio_power_off(chip);

	BUG_ON(!power->gpio);

	/* drive it active, also inactive->active edge */
	gpiod_direction_output(power->gpio, 1);
	msleep(100);
	/* drive inactive, also active->inactive edge */
	gpiod_set_value_cansleep(power->gpio, 0);
	msleep(100);

	/* drive it active, also inactive->active edge */
	gpiod_set_value_cansleep(power->gpio, 1);

	/* give it some time */
	mdelay(timeout);

	WARN_ON(1);

	return 0;
}

static int gpio_poweroff_probe(struct platform_device *pdev)
{
	struct gpio_power_off *power;
	bool input = false;
	enum gpiod_flags flags;
	int err;

	power = devm_kzalloc(&pdev->dev, sizeof(*power), GFP_KERNEL);
	if (!power)
		return -ENOMEM;

	input = device_property_read_bool(&pdev->dev, "input");
	if (input)
		flags = GPIOD_IN;
	else
		flags = GPIOD_OUT_LOW;

	device_property_read_u32(&pdev->dev, "timeout-ms", &timeout);

	power->gpio = devm_gpiod_get(&pdev->dev, NULL, flags);
	if (IS_ERR(power->gpio))
		return PTR_ERR(power->gpio);

	power->chip.level = SYSTEM_POWER_LEVEL_SYSTEM;
	power->chip.dev = &pdev->dev;
	power->chip.power_off = gpio_power_off;

	platform_set_drvdata(pdev, power);

	err = system_power_chip_add(&power->chip);
	if (err < 0)
		return err;

	return 0;
}

static int gpio_poweroff_remove(struct platform_device *pdev)
{
	struct gpio_power_off *power = platform_get_drvdata(pdev);

	return system_power_chip_remove(&power->chip);
}

static const struct of_device_id of_gpio_poweroff_match[] = {
	{ .compatible = "gpio-poweroff", },
	{},
};

static struct platform_driver gpio_poweroff_driver = {
	.probe = gpio_poweroff_probe,
	.remove = gpio_poweroff_remove,
	.driver = {
		.name = "poweroff-gpio",
		.of_match_table = of_gpio_poweroff_match,
	},
};

module_platform_driver(gpio_poweroff_driver);

MODULE_AUTHOR("Jamie Lentin <jm@lentin.co.uk>");
MODULE_DESCRIPTION("GPIO poweroff driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:poweroff-gpio");
