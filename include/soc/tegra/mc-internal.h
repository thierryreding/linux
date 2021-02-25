/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2014 NVIDIA Corporation
 * Copyright (C) 2021 NVIDIA Corporation
 */

#ifndef __SOC_TEGRA_MC_INTERNAL_H__
#define __SOC_TEGRA_MC_INTERNAL_H__

#include <soc/tegra/mc.h>

struct tegra_mc_soc {
	const struct tegra_mc_client *clients;
	unsigned int num_clients;

	const unsigned long *emem_regs;
	unsigned int num_emem_regs;

	unsigned int num_address_bits;
	unsigned int atom_size;

	u8 client_id_mask;

	const struct tegra_smmu_soc *smmu;

	u32 intmask;

	const struct tegra_mc_reset_ops *reset_ops;
	const struct tegra_mc_reset *resets;
	unsigned int num_resets;

	const struct tegra_mc_icc_ops *icc_ops;
};

struct tegra_mc {
	struct device *dev;
	struct tegra_smmu *smmu;
	struct gart_device *gart;
	void __iomem *regs;
	struct clk *clk;
	int irq;

	const struct tegra_mc_soc *soc;
	unsigned long tick;

	struct tegra_mc_timing *timings;
	unsigned int num_timings;

	struct reset_controller_dev reset;

	struct icc_provider provider;

	spinlock_t lock;
};

#endif /* __SOC_TEGRA_MC_INTERNAL_H__ */
