/*
 * Copyright (c) 2017 NVIDIA Corporation
 *
 * This file is released under the GPL v2
 */

#define DEBUG

#define pr_fmt(fmt) "system-power: " fmt

#include <linux/delay.h>
#include <linux/system-power.h>

static DEFINE_MUTEX(system_power_lock);
static LIST_HEAD(system_power_chips);

static const char *spc_get_name(struct system_power_chip *chip)
{
	if (chip->name)
		return chip->name;

	if (chip->dev)
		return dev_name(chip->dev);

	return "";
}

#define spc_warn(chip, fmt, ...)				\
	pr_warn("%s: " fmt, spc_get_name(chip), ##__VA_ARGS__)

#define spc_dbg(chip, fmt, ...)					\
	pr_debug("%s: " fmt, spc_get_name(chip), ##__VA_ARGS__)

int system_power_chip_add(struct system_power_chip *chip)
{
	struct system_power_chip *node;

	pr_debug("> %s(chip=%p)\n", __func__, chip);

	if (!chip->restart && !chip->power_off) {
		WARN(1, pr_fmt("must implement restart or power off\n"));
		return -EINVAL;
	}

	INIT_LIST_HEAD(&chip->list);

	mutex_lock(&system_power_lock);

	list_for_each_entry(node, &system_power_chips, list)
		if (chip->level > node->level)
			break;

	list_add_tail(&chip->list, &node->list);

	mutex_unlock(&system_power_lock);

	pr_debug("< %s()\n", __func__);

	return 0;
}
EXPORT_SYMBOL_GPL(system_power_chip_add);

int system_power_chip_remove(struct system_power_chip *chip)
{
	pr_debug("> %s(chip=%p)\n", __func__, chip);

	mutex_lock(&system_power_lock);

	list_del_init(&chip->list);

	mutex_unlock(&system_power_lock);

	pr_debug("< %s()\n", __func__);
	return 0;
}
EXPORT_SYMBOL_GPL(system_power_chip_remove);

bool system_can_power_off(void)
{
	struct system_power_chip *chip;

	mutex_lock(&system_power_lock);

	list_for_each_entry(chip, &system_power_chips, list) {
		if (chip->power_off) {
			mutex_unlock(&system_power_lock);
			return true;
		}
	}

	mutex_unlock(&system_power_lock);

	/* XXX for backwards compatibility */
	return pm_power_off != NULL;
}

int system_restart(char *cmd)
{
	struct system_power_chip *chip;
	int err;

	pr_debug("> %s(cmd=%s)\n", __func__, cmd);

	mutex_lock(&system_power_lock);

	list_for_each_entry(chip, &system_power_chips, list) {
		if (!chip->restart_prepare)
			continue;

		spc_dbg(chip, "preparing to restart...\n");

		err = chip->restart_prepare(chip, reboot_mode, cmd);
		if (err < 0)
			spc_warn(chip, "failed to prepare restart: %d\n", err);
	}

	list_for_each_entry(chip, &system_power_chips, list) {
		if (!chip->restart)
			continue;

		spc_dbg(chip, "restarting...\n");
		msleep(250);

		err = chip->restart(chip, reboot_mode, cmd);
		if (err < 0)
			spc_warn(chip, "failed to restart: %d\n", err);
	}

	mutex_unlock(&system_power_lock);

	/* XXX for backwards compatibility */
	do_kernel_restart(cmd);

	pr_debug("< %s()\n", __func__);
	return 0;
}

int system_power_off_prepare(void)
{
	pr_debug("> %s()\n", __func__);

	/* XXX for backwards compatibility */
	if (pm_power_off_prepare)
		pm_power_off_prepare();

	pr_debug("< %s()\n", __func__);

	return 0;
}

int system_power_off(void)
{
	struct system_power_chip *chip;
	int err;

	pr_debug("> %s()\n", __func__);

	mutex_lock(&system_power_lock);

	list_for_each_entry(chip, &system_power_chips, list) {
		if (!chip->power_off_prepare)
			continue;

		spc_dbg(chip, "preparing to power off...\n");
		msleep(250);

		err = chip->power_off_prepare(chip);
		if (err < 0)
			spc_warn(chip, "failed to prepare power off: %d\n",
				 err);
	}

	list_for_each_entry(chip, &system_power_chips, list) {
		if (!chip->power_off)
			continue;

		spc_dbg(chip, "powering off...\n");
		msleep(250);

		err = chip->power_off(chip);
		if (err < 0)
			spc_warn(chip, "failed to power off: %d\n", err);
	}

	mutex_unlock(&system_power_lock);

	/* XXX for backwards compatibility */
	if (pm_power_off)
		pm_power_off();

	pr_debug("< %s()\n", __func__);
	return 0;
}
