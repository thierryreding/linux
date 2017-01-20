/*
 * Copyright (c) 2017 NVIDIA Corporation
 *
 * This file is released under the GPL v2
 */

#ifndef SYSTEM_POWER_H
#define SYSTEM_POWER_H

#include <linux/device.h>
#include <linux/list.h>
#include <linux/reboot.h>

/**
 * enum system_power_level - system-level of the power chip implementation
 * @SYSTEM_POWER_LEVEL_CPU: The chip implements a restart or power-off
 *     mechanism at the CPU level. Not all of the system may be reset by this
 *     implementation. This is a fallback implementation to restart the CPU
 *     in case no better implementation exists.
 * @SYSTEM_POWER_LEVEL_SOC: Restarts or powers off the SoC. This may not make
 *     the whole system reset properly, in cases where for example external
 *     peripherals aren't hooked up to the SoC level reset.
 * @SYSTEM_POWER_LEVEL_SYSTEM: The mechanism implemented by a chip of this
 *     type resets the CPU, the SoC as well as peripherals on a system-wide
 *     level. This is typically implemented by some power-management IC or a
 *     GPIO controlling the main power supply. However this can also apply to
 *     software-defined mechanisms such as firmware or BIOS, which can be
 *     assumed to be system-specific and hence reset or power off the entire
 *     system.
 */
enum system_power_level {
	SYSTEM_POWER_LEVEL_CPU,
	SYSTEM_POWER_LEVEL_SOC,
	SYSTEM_POWER_LEVEL_SYSTEM,
};

struct system_power_chip {
	enum system_power_level level;
	struct list_head list;
	struct device *dev;
	const char *name;

	int (*restart_prepare)(struct system_power_chip *chip,
			       enum reboot_mode mode, char *cmd);
	int (*restart)(struct system_power_chip *chip,
		       enum reboot_mode mode, char *cmd);
	int (*power_off_prepare)(struct system_power_chip *chip);
	int (*power_off)(struct system_power_chip *chip);
};

int system_power_chip_add(struct system_power_chip *chip);
int system_power_chip_remove(struct system_power_chip *chip);

bool system_can_power_off(void);

int system_restart(char *cmd);
int system_power_off_prepare(void);
int system_power_off(void);

#endif
