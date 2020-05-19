/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved. */

#ifndef DT_BINDINGS_CLOCK_TEGRA234_CLOCK_H
#define DT_BINDINGS_CLOCK_TEGRA234_CLOCK_H

/** @brief output of mux controlled by CLK_RST_CONTROLLER_CLK_SOURCE_AXI_CBB */
#define TEGRA234_CLK_AXI_CBB			8U
/** @brief CLK_RST_CONTROLLER_CLK_SOURCE_EQOS_AXI_CLK_0 divider gated output */
#define TEGRA234_CLK_EQOS_AXI			32U
/** @brief CLK_RST_CONTROLLER_CLK_SOURCE_EQOS_PTP_REF_CLK_0 divider gated output */
#define TEGRA234_CLK_EQOS_PTP_REF		33U
/** @brief output of gate CLK_ENB_EQOS_RX */
#define TEGRA234_CLK_EQOS_RX			34U
/** @brief CLK_RST_CONTROLLER_CLK_SOURCE_EQOS_TX_CLK divider gated output */
#define TEGRA234_CLK_EQOS_TX			35U
/** @brief output of mux controlled by CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC4 */
#define TEGRA234_CLK_SDMMC4			123U
/** @brief output of mux controlled by CLK_RST_CONTROLLER_CLK_SOURCE_UARTA */
#define TEGRA234_CLK_UARTA			155U

#endif
