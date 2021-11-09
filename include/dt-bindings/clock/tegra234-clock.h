/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved. */

#ifndef DT_BINDINGS_CLOCK_TEGRA234_CLOCK_H
#define DT_BINDINGS_CLOCK_TEGRA234_CLOCK_H

/**
 * @file
 * @defgroup bpmp_clock_ids Clock ID's
 * @{
 */
/** @brief output of mux controlled by CLK_RST_CONTROLLER_CLK_SOURCE_AXI_CBB */
#define TEGRA234_CLK_AXI_CBB			8U
/**
 * @brief controls the EMC clock frequency.
 * @details Doing a clk_set_rate on this clock will select the
 * appropriate clock source, program the source rate and execute a
 * specific sequence to switch to the new clock source for both memory
 * controllers. This can be used to control the balance between memory
 * throughput and memory controller power.
 */
#define TEGRA234_CLK_EMC			31U
/** @brief CLK_RST_CONTROLLER_CLK_SOURCE_EQOS_AXI_CLK_0 divider gated output */
#define TEGRA234_CLK_EQOS_AXI			32U
/** @brief CLK_RST_CONTROLLER_CLK_SOURCE_EQOS_PTP_REF_CLK_0 divider gated output */
#define TEGRA234_CLK_EQOS_PTP_REF		33U
/** @brief output of gate CLK_ENB_EQOS_RX */
#define TEGRA234_CLK_EQOS_RX			34U
/** @brief CLK_RST_CONTROLLER_CLK_SOURCE_EQOS_TX_CLK divider gated output */
#define TEGRA234_CLK_EQOS_TX			35U
/** @brief output of gate CLK_ENB_FUSE */
#define TEGRA234_CLK_FUSE			40U
/** @brief output of mux controlled by CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC4 */
#define TEGRA234_CLK_SDMMC4			123U
/** @brief output of mux controlled by CLK_RST_CONTROLLER_CLK_SOURCE_UARTA */
#define TEGRA234_CLK_UARTA			155U
/** @brief CLK_RST_CONTROLLER_CLK_SOURCE_SDMMC_LEGACY_TM switch divider output */
#define TEGRA234_CLK_SDMMC_LEGACY_TM		219U
/** @brief PLL controlled by CLK_RST_CONTROLLER_PLLC4_BASE */
#define TEGRA234_CLK_PLLC4			237U
/** @brief RX clock recovered from MGBE0 lane input */
#define TEGRA234_CLK_MGBE0_RX_INPUT		248U
/** @brief RX clock recovered from MGBE1 lane input */
#define TEGRA234_CLK_MGBE1_RX_INPUT		249U
/** @brief RX clock recovered from MGBE2 lane input */
#define TEGRA234_CLK_MGBE2_RX_INPUT		250U
/** @brief RX clock recovered from MGBE3 lane input */
#define TEGRA234_CLK_MGBE3_RX_INPUT		251U
/** @brief 32K input clock provided by PMIC */
#define TEGRA234_CLK_CLK_32K			289U
/** @brief Monitored branch of MBGE0 RX input clock */
#define TEGRA234_CLK_MGBE0_RX_INPUT_M		357U
/** @brief Monitored branch of MBGE1 RX input clock */
#define TEGRA234_CLK_MGBE1_RX_INPUT_M		358U
/** @brief Monitored branch of MBGE2 RX input clock */
#define TEGRA234_CLK_MGBE2_RX_INPUT_M		359U
/** @brief Monitored branch of MBGE3 RX input clock */
#define TEGRA234_CLK_MGBE3_RX_INPUT_M		360U
/** @brief Monitored branch of MGBE0 RX PCS mux output */
#define TEGRA234_CLK_MGBE0_RX_PCS_M		361U
/** @brief Monitored branch of MGBE1 RX PCS mux output */
#define TEGRA234_CLK_MGBE1_RX_PCS_M		362U
/** @brief Monitored branch of MGBE2 RX PCS mux output */
#define TEGRA234_CLK_MGBE2_RX_PCS_M		363U
/** @brief Monitored branch of MGBE3 RX PCS mux output */
#define TEGRA234_CLK_MGBE3_RX_PCS_M		364U
/** @brief RX PCS clock recovered from MGBE0 lane input */
#define TEGRA234_CLK_MGBE0_RX_PCS_INPUT		369U
/** @brief RX PCS clock recovered from MGBE1 lane input */
#define TEGRA234_CLK_MGBE1_RX_PCS_INPUT		370U
/** @brief RX PCS clock recovered from MGBE2 lane input */
#define TEGRA234_CLK_MGBE2_RX_PCS_INPUT		371U
/** @brief RX PCS clock recovered from MGBE3 lane input */
#define TEGRA234_CLK_MGBE3_RX_PCS_INPUT		372U
/** @brief output of mux controlled by GBE_UPHY_MGBE0_RX_PCS_CLK_SRC_SEL */
#define TEGRA234_CLK_MGBE0_RX_PCS		373U
/** @brief GBE_UPHY_MGBE0_TX_CLK divider gated output */
#define TEGRA234_CLK_MGBE0_TX			374U
/** @brief GBE_UPHY_MGBE0_TX_PCS_CLK divider gated output */
#define TEGRA234_CLK_MGBE0_TX_PCS		375U
/** @brief GBE_UPHY_MGBE0_MAC_CLK divider output */
#define TEGRA234_CLK_MGBE0_MAC_DIVIDER		376U
/** @brief GBE_UPHY_MGBE0_MAC_CLK gate output */
#define TEGRA234_CLK_MGBE0_MAC			377U
/** @brief GBE_UPHY_MGBE0_MACSEC_CLK gate output */
#define TEGRA234_CLK_MGBE0_MACSEC		378U
/** @brief GBE_UPHY_MGBE0_EEE_PCS_CLK gate output */
#define TEGRA234_CLK_MGBE0_EEE_PCS		379U
/** @brief GBE_UPHY_MGBE0_APP_CLK gate output */
#define TEGRA234_CLK_MGBE0_APP			380U
/** @brief GBE_UPHY_MGBE0_PTP_REF_CLK divider gated output */
#define TEGRA234_CLK_MGBE0_PTP_REF		381U
/** @brief output of mux controlled by GBE_UPHY_MGBE1_RX_PCS_CLK_SRC_SEL */
#define TEGRA234_CLK_MGBE1_RX_PCS		382U
/** @brief GBE_UPHY_MGBE1_TX_CLK divider gated output */
#define TEGRA234_CLK_MGBE1_TX			383U
/** @brief GBE_UPHY_MGBE1_TX_PCS_CLK divider gated output */
#define TEGRA234_CLK_MGBE1_TX_PCS		384U
/** @brief GBE_UPHY_MGBE1_MAC_CLK divider output */
#define TEGRA234_CLK_MGBE1_MAC_DIVIDER		385U
/** @brief GBE_UPHY_MGBE1_MAC_CLK gate output */
#define TEGRA234_CLK_MGBE1_MAC			386U
/** @brief GBE_UPHY_MGBE1_EEE_PCS_CLK gate output */
#define TEGRA234_CLK_MGBE1_EEE_PCS		388U
/** @brief GBE_UPHY_MGBE1_APP_CLK gate output */
#define TEGRA234_CLK_MGBE1_APP			389U
/** @brief GBE_UPHY_MGBE1_PTP_REF_CLK divider gated output */
#define TEGRA234_CLK_MGBE1_PTP_REF		390U
/** @brief output of mux controlled by GBE_UPHY_MGBE2_RX_PCS_CLK_SRC_SEL */
#define TEGRA234_CLK_MGBE2_RX_PCS		391U
/** @brief GBE_UPHY_MGBE2_TX_CLK divider gated output */
#define TEGRA234_CLK_MGBE2_TX			392U
/** @brief GBE_UPHY_MGBE2_TX_PCS_CLK divider gated output */
#define TEGRA234_CLK_MGBE2_TX_PCS		393U
/** @brief GBE_UPHY_MGBE2_MAC_CLK divider output */
#define TEGRA234_CLK_MGBE2_MAC_DIVIDER		394U
/** @brief GBE_UPHY_MGBE2_MAC_CLK gate output */
#define TEGRA234_CLK_MGBE2_MAC			395U
/** @brief GBE_UPHY_MGBE2_EEE_PCS_CLK gate output */
#define TEGRA234_CLK_MGBE2_EEE_PCS		397U
/** @brief GBE_UPHY_MGBE2_APP_CLK gate output */
#define TEGRA234_CLK_MGBE2_APP			398U
/** @brief GBE_UPHY_MGBE2_PTP_REF_CLK divider gated output */
#define TEGRA234_CLK_MGBE2_PTP_REF		399U
/** @brief output of mux controlled by GBE_UPHY_MGBE3_RX_PCS_CLK_SRC_SEL */
#define TEGRA234_CLK_MGBE3_RX_PCS		400U
/** @brief GBE_UPHY_MGBE3_TX_CLK divider gated output */
#define TEGRA234_CLK_MGBE3_TX			401U
/** @brief GBE_UPHY_MGBE3_TX_PCS_CLK divider gated output */
#define TEGRA234_CLK_MGBE3_TX_PCS		402U
/** @brief GBE_UPHY_MGBE3_MAC_CLK divider output */
#define TEGRA234_CLK_MGBE3_MAC_DIVIDER		403U
/** @brief GBE_UPHY_MGBE3_MAC_CLK gate output */
#define TEGRA234_CLK_MGBE3_MAC			404U
/** @brief GBE_UPHY_MGBE3_MACSEC_CLK gate output */
#define TEGRA234_CLK_MGBE3_MACSEC		405U
/** @brief GBE_UPHY_MGBE3_EEE_PCS_CLK gate output */
#define TEGRA234_CLK_MGBE3_EEE_PCS		406U
/** @brief GBE_UPHY_MGBE3_APP_CLK gate output */
#define TEGRA234_CLK_MGBE3_APP			407U
/** @brief GBE_UPHY_MGBE3_PTP_REF_CLK divider gated output */
#define TEGRA234_CLK_MGBE3_PTP_REF		408U

#endif
