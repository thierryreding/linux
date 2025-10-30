// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Paul Cercueil <paul@crapouillou.net>
 */

#include "cgu.h"
#include "pm.h"

#include <linux/io.h>

#define CGU_REG_LCR		0x04

#define LCR_LOW_POWER_MODE	BIT(0)

static int __maybe_unused ingenic_cgu_pm_suspend(void *data)
{
	struct ingenic_cgu *cgu = data;
	u32 val = readl(cgu->base + CGU_REG_LCR);

	writel(val | LCR_LOW_POWER_MODE, cgu->base + CGU_REG_LCR);

	return 0;
}

static void __maybe_unused ingenic_cgu_pm_resume(void *data)
{
	struct ingenic_cgu *cgu = data;
	u32 val = readl(cgu->base + CGU_REG_LCR);

	writel(val & ~LCR_LOW_POWER_MODE, cgu->base + CGU_REG_LCR);
}

static const struct syscore_ops __maybe_unused ingenic_cgu_pm_ops = {
	.suspend = ingenic_cgu_pm_suspend,
	.resume = ingenic_cgu_pm_resume,
};

void ingenic_cgu_register_syscore(struct ingenic_cgu *cgu)
{
	if (IS_ENABLED(CONFIG_PM_SLEEP)) {
		cgu->syscore.ops = &ingenic_cgu_pm_ops;
		cgu->syscore.data = cgu;
		register_syscore(&cgu->syscore);
	}
}
