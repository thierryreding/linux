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

#ifndef SOC_TEGRA_KFUSE_H
#define SOC_TEGRA_KFUSE_H

struct tegra_kfuse;

#if IS_ENABLED(CONFIG_SOC_TEGRA_KFUSE)
struct tegra_kfuse *tegra_kfuse_get(struct device *dev);
void tegra_kfuse_put(struct tegra_kfuse *kfuse);
ssize_t tegra_kfuse_read(struct tegra_kfuse *kfuse, void *buffer, size_t size);
#else
static inline struct tegra_kfuse *tegra_kfuse_get(struct device *dev)
{
	return NULL;
}

static inline void tegra_kfuse_put(struct tegra_kfuse *kfuse)
{
}

static inline ssize_t tegra_kfuse_read(struct tegra_kfuse *kfuse,
				       void *buffer, size_t size)
{
	return -ENOSYS;
}
#endif

#endif
