/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef DT_BINDINGS_THERMAL_NVIDIA_TEGRA264_BPMP_H
#define DT_BINDINGS_THERMAL_NVIDIA_TEGRA264_BPMP_H

#define TEGRA264_THERMAL_ZONE_TJ_MAX		0
#define TEGRA264_THERMAL_ZONE_TJ_MIN		1
#define TEGRA264_THERMAL_ZONE_GPU_AVG		2
#define TEGRA264_THERMAL_ZONE_CPU_AVG		3
#define TEGRA264_THERMAL_ZONE_SOC_012_AVG	4 /* Powered by Vdd_SoC */
#define TEGRA264_THERMAL_ZONE_SOC_45_AVG	5 /* Powered by Vdd_MSS */
#define TEGRA264_THERMAL_ZONE_SOC_3_AVG		6 /* Powered by Uphy_Vdd */
#define TEGRA264_THERMAL_ZONE_GPU_MAX		7
#define TEGRA264_THERMAL_ZONE_CPU_MAX		8
#define TEGRA264_THERMAL_ZONE_SOC_012_MAX	9 /* Powered by Vdd_SoC */
#define TEGRA264_THERMAL_ZONE_SOC_345_MAX	10 /* Powered by Uphy_Vdd and Vdd_MSS */
#define TEGRA264_THERMAL_ZONE_TJ_AVG		11

#endif /* DT_BINDINGS_THERMAL_NVIDIA_TEGAR264_BPMP_H */
