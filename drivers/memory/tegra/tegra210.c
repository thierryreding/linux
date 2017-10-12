/*
 * Copyright (C) 2015 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of.h>
#include <linux/mm.h>

#include <asm/cacheflush.h>

#include <dt-bindings/memory/tegra210-mc.h>

#include "mc.h"

static const struct tegra_mc_client tegra210_mc_clients[] = {
	{
		.id = 0x00,
		.name = "ptcr",
		.swgroup = TEGRA210_SWGROUP_PTC,
	}, {
		.id = 0x01,
		.name = "display0a",
		.swgroup = TEGRA210_SWGROUP_DC,
		.smmu = {
			.reg = 0x228,
			.bit = 1,
		},
		.la = {
			.reg = 0x2e8,
			.shift = 0,
			.mask = 0xff,
			.def = 0xc2,
		},
	}, {
		.id = 0x02,
		.name = "display0ab",
		.swgroup = TEGRA210_SWGROUP_DCB,
		.smmu = {
			.reg = 0x228,
			.bit = 2,
		},
		.la = {
			.reg = 0x2f4,
			.shift = 0,
			.mask = 0xff,
			.def = 0xc6,
		},
	}, {
		.id = 0x03,
		.name = "display0b",
		.swgroup = TEGRA210_SWGROUP_DC,
		.smmu = {
			.reg = 0x228,
			.bit = 3,
		},
		.la = {
			.reg = 0x2e8,
			.shift = 16,
			.mask = 0xff,
			.def = 0x50,
		},
	}, {
		.id = 0x04,
		.name = "display0bb",
		.swgroup = TEGRA210_SWGROUP_DCB,
		.smmu = {
			.reg = 0x228,
			.bit = 4,
		},
		.la = {
			.reg = 0x2f4,
			.shift = 16,
			.mask = 0xff,
			.def = 0x50,
		},
	}, {
		.id = 0x05,
		.name = "display0c",
		.swgroup = TEGRA210_SWGROUP_DC,
		.smmu = {
			.reg = 0x228,
			.bit = 5,
		},
		.la = {
			.reg = 0x2ec,
			.shift = 0,
			.mask = 0xff,
			.def = 0x50,
		},
	}, {
		.id = 0x06,
		.name = "display0cb",
		.swgroup = TEGRA210_SWGROUP_DCB,
		.smmu = {
			.reg = 0x228,
			.bit = 6,
		},
		.la = {
			.reg = 0x2f8,
			.shift = 0,
			.mask = 0xff,
			.def = 0x50,
		},
	}, {
		.id = 0x0e,
		.name = "afir",
		.swgroup = TEGRA210_SWGROUP_AFI,
		.smmu = {
			.reg = 0x228,
			.bit = 14,
		},
		.la = {
			.reg = 0x2e0,
			.shift = 0,
			.mask = 0xff,
			.def = 0x13,
		},
	}, {
		.id = 0x0f,
		.name = "avpcarm7r",
		.swgroup = TEGRA210_SWGROUP_AVPC,
		.smmu = {
			.reg = 0x228,
			.bit = 15,
		},
		.la = {
			.reg = 0x2e4,
			.shift = 0,
			.mask = 0xff,
			.def = 0x04,
		},
	}, {
		.id = 0x10,
		.name = "displayhc",
		.swgroup = TEGRA210_SWGROUP_DC,
		.smmu = {
			.reg = 0x228,
			.bit = 16,
		},
		.la = {
			.reg = 0x2f0,
			.shift = 0,
			.mask = 0xff,
			.def = 0x50,
		},
	}, {
		.id = 0x11,
		.name = "displayhcb",
		.swgroup = TEGRA210_SWGROUP_DCB,
		.smmu = {
			.reg = 0x228,
			.bit = 17,
		},
		.la = {
			.reg = 0x2fc,
			.shift = 0,
			.mask = 0xff,
			.def = 0x50,
		},
	}, {
		.id = 0x15,
		.name = "hdar",
		.swgroup = TEGRA210_SWGROUP_HDA,
		.smmu = {
			.reg = 0x228,
			.bit = 21,
		},
		.la = {
			.reg = 0x318,
			.shift = 0,
			.mask = 0xff,
			.def = 0x24,
		},
	}, {
		.id = 0x16,
		.name = "host1xdmar",
		.swgroup = TEGRA210_SWGROUP_HC,
		.smmu = {
			.reg = 0x228,
			.bit = 22,
		},
		.la = {
			.reg = 0x310,
			.shift = 0,
			.mask = 0xff,
			.def = 0x1e,
		},
	}, {
		.id = 0x17,
		.name = "host1xr",
		.swgroup = TEGRA210_SWGROUP_HC,
		.smmu = {
			.reg = 0x228,
			.bit = 23,
		},
		.la = {
			.reg = 0x310,
			.shift = 16,
			.mask = 0xff,
			.def = 0x50,
		},
	}, {
		.id = 0x1c,
		.name = "nvencsrd",
		.swgroup = TEGRA210_SWGROUP_NVENC,
		.smmu = {
			.reg = 0x228,
			.bit = 28,
		},
		.la = {
			.reg = 0x328,
			.shift = 0,
			.mask = 0xff,
			.def = 0x23,
		},
	}, {
		.id = 0x1d,
		.name = "ppcsahbdmar",
		.swgroup = TEGRA210_SWGROUP_PPCS,
		.smmu = {
			.reg = 0x228,
			.bit = 29,
		},
		.la = {
			.reg = 0x344,
			.shift = 0,
			.mask = 0xff,
			.def = 0x49,
		},
	}, {
		.id = 0x1e,
		.name = "ppcsahbslvr",
		.swgroup = TEGRA210_SWGROUP_PPCS,
		.smmu = {
			.reg = 0x228,
			.bit = 30,
		},
		.la = {
			.reg = 0x344,
			.shift = 16,
			.mask = 0xff,
			.def = 0x1a,
		},
	}, {
		.id = 0x1f,
		.name = "satar",
		.swgroup = TEGRA210_SWGROUP_SATA,
		.smmu = {
			.reg = 0x228,
			.bit = 31,
		},
		.la = {
			.reg = 0x350,
			.shift = 0,
			.mask = 0xff,
			.def = 0x65,
		},
	}, {
		.id = 0x27,
		.name = "mpcorer",
		.swgroup = TEGRA210_SWGROUP_MPCORE,
		.la = {
			.reg = 0x320,
			.shift = 0,
			.mask = 0xff,
			.def = 0x04,
		},
	}, {
		.id = 0x2b,
		.name = "nvencswr",
		.swgroup = TEGRA210_SWGROUP_NVENC,
		.smmu = {
			.reg = 0x22c,
			.bit = 11,
		},
		.la = {
			.reg = 0x328,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x31,
		.name = "afiw",
		.swgroup = TEGRA210_SWGROUP_AFI,
		.smmu = {
			.reg = 0x22c,
			.bit = 17,
		},
		.la = {
			.reg = 0x2e0,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x32,
		.name = "avpcarm7w",
		.swgroup = TEGRA210_SWGROUP_AVPC,
		.smmu = {
			.reg = 0x22c,
			.bit = 18,
		},
		.la = {
			.reg = 0x2e4,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x35,
		.name = "hdaw",
		.swgroup = TEGRA210_SWGROUP_HDA,
		.smmu = {
			.reg = 0x22c,
			.bit = 21,
		},
		.la = {
			.reg = 0x318,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x36,
		.name = "host1xw",
		.swgroup = TEGRA210_SWGROUP_HC,
		.smmu = {
			.reg = 0x22c,
			.bit = 22,
		},
		.la = {
			.reg = 0x314,
			.shift = 0,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x39,
		.name = "mpcorew",
		.swgroup = TEGRA210_SWGROUP_MPCORE,
		.la = {
			.reg = 0x320,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x3b,
		.name = "ppcsahbdmaw",
		.swgroup = TEGRA210_SWGROUP_PPCS,
		.smmu = {
			.reg = 0x22c,
			.bit = 27,
		},
		.la = {
			.reg = 0x348,
			.shift = 0,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x3c,
		.name = "ppcsahbslvw",
		.swgroup = TEGRA210_SWGROUP_PPCS,
		.smmu = {
			.reg = 0x22c,
			.bit = 28,
		},
		.la = {
			.reg = 0x348,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x3d,
		.name = "sataw",
		.swgroup = TEGRA210_SWGROUP_SATA,
		.smmu = {
			.reg = 0x22c,
			.bit = 29,
		},
		.la = {
			.reg = 0x350,
			.shift = 16,
			.mask = 0xff,
			.def = 0x65,
		},
	}, {
		.id = 0x44,
		.name = "ispra",
		.swgroup = TEGRA210_SWGROUP_ISP2,
		.smmu = {
			.reg = 0x230,
			.bit = 4,
		},
		.la = {
			.reg = 0x370,
			.shift = 0,
			.mask = 0xff,
			.def = 0x18,
		},
	}, {
		.id = 0x46,
		.name = "ispwa",
		.swgroup = TEGRA210_SWGROUP_ISP2,
		.smmu = {
			.reg = 0x230,
			.bit = 6,
		},
		.la = {
			.reg = 0x374,
			.shift = 0,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x47,
		.name = "ispwb",
		.swgroup = TEGRA210_SWGROUP_ISP2,
		.smmu = {
			.reg = 0x230,
			.bit = 7,
		},
		.la = {
			.reg = 0x374,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x4a,
		.name = "xusb_hostr",
		.swgroup = TEGRA210_SWGROUP_XUSB_HOST,
		.smmu = {
			.reg = 0x230,
			.bit = 10,
		},
		.la = {
			.reg = 0x37c,
			.shift = 0,
			.mask = 0xff,
			.def = 0x39,
		},
	}, {
		.id = 0x4b,
		.name = "xusb_hostw",
		.swgroup = TEGRA210_SWGROUP_XUSB_HOST,
		.smmu = {
			.reg = 0x230,
			.bit = 11,
		},
		.la = {
			.reg = 0x37c,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x4c,
		.name = "xusb_devr",
		.swgroup = TEGRA210_SWGROUP_XUSB_DEV,
		.smmu = {
			.reg = 0x230,
			.bit = 12,
		},
		.la = {
			.reg = 0x380,
			.shift = 0,
			.mask = 0xff,
			.def = 0x39,
		},
	}, {
		.id = 0x4d,
		.name = "xusb_devw",
		.swgroup = TEGRA210_SWGROUP_XUSB_DEV,
		.smmu = {
			.reg = 0x230,
			.bit = 13,
		},
		.la = {
			.reg = 0x380,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x4e,
		.name = "isprab",
		.swgroup = TEGRA210_SWGROUP_ISP2B,
		.smmu = {
			.reg = 0x230,
			.bit = 14,
		},
		.la = {
			.reg = 0x384,
			.shift = 0,
			.mask = 0xff,
			.def = 0x18,
		},
	}, {
		.id = 0x50,
		.name = "ispwab",
		.swgroup = TEGRA210_SWGROUP_ISP2B,
		.smmu = {
			.reg = 0x230,
			.bit = 16,
		},
		.la = {
			.reg = 0x388,
			.shift = 0,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x51,
		.name = "ispwbb",
		.swgroup = TEGRA210_SWGROUP_ISP2B,
		.smmu = {
			.reg = 0x230,
			.bit = 17,
		},
		.la = {
			.reg = 0x388,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x54,
		.name = "tsecsrd",
		.swgroup = TEGRA210_SWGROUP_TSEC,
		.smmu = {
			.reg = 0x230,
			.bit = 20,
		},
		.la = {
			.reg = 0x390,
			.shift = 0,
			.mask = 0xff,
			.def = 0x9b,
		},
	}, {
		.id = 0x55,
		.name = "tsecswr",
		.swgroup = TEGRA210_SWGROUP_TSEC,
		.smmu = {
			.reg = 0x230,
			.bit = 21,
		},
		.la = {
			.reg = 0x390,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x56,
		.name = "a9avpscr",
		.swgroup = TEGRA210_SWGROUP_A9AVP,
		.smmu = {
			.reg = 0x230,
			.bit = 22,
		},
		.la = {
			.reg = 0x3a4,
			.shift = 0,
			.mask = 0xff,
			.def = 0x04,
		},
	}, {
		.id = 0x57,
		.name = "a9avpscw",
		.swgroup = TEGRA210_SWGROUP_A9AVP,
		.smmu = {
			.reg = 0x230,
			.bit = 23,
		},
		.la = {
			.reg = 0x3a4,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x58,
		.name = "gpusrd",
		.swgroup = TEGRA210_SWGROUP_GPU,
		.smmu = {
			/* read-only */
			.reg = 0x230,
			.bit = 24,
		},
		.la = {
			.reg = 0x3c8,
			.shift = 0,
			.mask = 0xff,
			.def = 0x1a,
		},
	}, {
		.id = 0x59,
		.name = "gpuswr",
		.swgroup = TEGRA210_SWGROUP_GPU,
		.smmu = {
			/* read-only */
			.reg = 0x230,
			.bit = 25,
		},
		.la = {
			.reg = 0x3c8,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x5a,
		.name = "displayt",
		.swgroup = TEGRA210_SWGROUP_DC,
		.smmu = {
			.reg = 0x230,
			.bit = 26,
		},
		.la = {
			.reg = 0x2f0,
			.shift = 16,
			.mask = 0xff,
			.def = 0x50,
		},
	}, {
		.id = 0x60,
		.name = "sdmmcra",
		.swgroup = TEGRA210_SWGROUP_SDMMC1A,
		.smmu = {
			.reg = 0x234,
			.bit = 0,
		},
		.la = {
			.reg = 0x3b8,
			.shift = 0,
			.mask = 0xff,
			.def = 0x49,
		},
	}, {
		.id = 0x61,
		.name = "sdmmcraa",
		.swgroup = TEGRA210_SWGROUP_SDMMC2A,
		.smmu = {
			.reg = 0x234,
			.bit = 1,
		},
		.la = {
			.reg = 0x3bc,
			.shift = 0,
			.mask = 0xff,
			.def = 0x49,
		},
	}, {
		.id = 0x62,
		.name = "sdmmcr",
		.swgroup = TEGRA210_SWGROUP_SDMMC3A,
		.smmu = {
			.reg = 0x234,
			.bit = 2,
		},
		.la = {
			.reg = 0x3c0,
			.shift = 0,
			.mask = 0xff,
			.def = 0x49,
		},
	}, {
		.id = 0x63,
		.swgroup = TEGRA210_SWGROUP_SDMMC4A,
		.name = "sdmmcrab",
		.smmu = {
			.reg = 0x234,
			.bit = 3,
		},
		.la = {
			.reg = 0x3c4,
			.shift = 0,
			.mask = 0xff,
			.def = 0x49,
		},
	}, {
		.id = 0x64,
		.name = "sdmmcwa",
		.swgroup = TEGRA210_SWGROUP_SDMMC1A,
		.smmu = {
			.reg = 0x234,
			.bit = 4,
		},
		.la = {
			.reg = 0x3b8,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x65,
		.name = "sdmmcwaa",
		.swgroup = TEGRA210_SWGROUP_SDMMC2A,
		.smmu = {
			.reg = 0x234,
			.bit = 5,
		},
		.la = {
			.reg = 0x3bc,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x66,
		.name = "sdmmcw",
		.swgroup = TEGRA210_SWGROUP_SDMMC3A,
		.smmu = {
			.reg = 0x234,
			.bit = 6,
		},
		.la = {
			.reg = 0x3c0,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x67,
		.name = "sdmmcwab",
		.swgroup = TEGRA210_SWGROUP_SDMMC4A,
		.smmu = {
			.reg = 0x234,
			.bit = 7,
		},
		.la = {
			.reg = 0x3c4,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x6c,
		.name = "vicsrd",
		.swgroup = TEGRA210_SWGROUP_VIC,
		.smmu = {
			.reg = 0x234,
			.bit = 12,
		},
		.la = {
			.reg = 0x394,
			.shift = 0,
			.mask = 0xff,
			.def = 0x1a,
		},
	}, {
		.id = 0x6d,
		.name = "vicswr",
		.swgroup = TEGRA210_SWGROUP_VIC,
		.smmu = {
			.reg = 0x234,
			.bit = 13,
		},
		.la = {
			.reg = 0x394,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x72,
		.name = "viw",
		.swgroup = TEGRA210_SWGROUP_VI,
		.smmu = {
			.reg = 0x234,
			.bit = 18,
		},
		.la = {
			.reg = 0x398,
			.shift = 0,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x73,
		.name = "displayd",
		.swgroup = TEGRA210_SWGROUP_DC,
		.smmu = {
			.reg = 0x234,
			.bit = 19,
		},
		.la = {
			.reg = 0x3c8,
			.shift = 0,
			.mask = 0xff,
			.def = 0x50,
		},
	}, {
		.id = 0x78,
		.name = "nvdecsrd",
		.swgroup = TEGRA210_SWGROUP_NVDEC,
		.smmu = {
			.reg = 0x234,
			.bit = 24,
		},
		.la = {
			.reg = 0x3d8,
			.shift = 0,
			.mask = 0xff,
			.def = 0x23,
		},
	}, {
		.id = 0x79,
		.name = "nvdecswr",
		.swgroup = TEGRA210_SWGROUP_NVDEC,
		.smmu = {
			.reg = 0x234,
			.bit = 25,
		},
		.la = {
			.reg = 0x3d8,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x7a,
		.name = "aper",
		.swgroup = TEGRA210_SWGROUP_APE,
		.smmu = {
			.reg = 0x234,
			.bit = 26,
		},
		.la = {
			.reg = 0x3dc,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
		},
	}, {
		.id = 0x7b,
		.name = "apew",
		.swgroup = TEGRA210_SWGROUP_APE,
		.smmu = {
			.reg = 0x234,
			.bit = 27,
		},
		.la = {
			.reg = 0x3dc,
			.shift = 0,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x7e,
		.name = "nvjpgsrd",
		.swgroup = TEGRA210_SWGROUP_NVJPG,
		.smmu = {
			.reg = 0x234,
			.bit = 30,
		},
		.la = {
			.reg = 0x3e4,
			.shift = 0,
			.mask = 0xff,
			.def = 0x23,
		},
	}, {
		.id = 0x7f,
		.name = "nvjpgswr",
		.swgroup = TEGRA210_SWGROUP_NVJPG,
		.smmu = {
			.reg = 0x234,
			.bit = 31,
		},
		.la = {
			.reg = 0x3e4,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x80,
		.name = "sesrd",
		.swgroup = TEGRA210_SWGROUP_SE,
		.smmu = {
			.reg = 0xb98,
			.bit = 0,
		},
		.la = {
			.reg = 0x3e0,
			.shift = 0,
			.mask = 0xff,
			.def = 0x2e,
		},
	}, {
		.id = 0x81,
		.name = "seswr",
		.swgroup = TEGRA210_SWGROUP_SE,
		.smmu = {
			.reg = 0xb98,
			.bit = 1,
		},
		.la = {
			.reg = 0xb98,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x82,
		.name = "axiapr",
		.swgroup = TEGRA210_SWGROUP_AXIAP,
		.smmu = {
			.reg = 0xb98,
			.bit = 2,
		},
		.la = {
			.reg = 0x3a0,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
		},
	}, {
		.id = 0x83,
		.name = "axiapw",
		.swgroup = TEGRA210_SWGROUP_AXIAP,
		.smmu = {
			.reg = 0xb98,
			.bit = 3,
		},
		.la = {
			.reg = 0x3a0,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x84,
		.name = "etrr",
		.swgroup = TEGRA210_SWGROUP_ETR,
		.smmu = {
			.reg = 0xb98,
			.bit = 4,
		},
		.la = {
			.reg = 0x3ec,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
		},
	}, {
		.id = 0x85,
		.name = "etrw",
		.swgroup = TEGRA210_SWGROUP_ETR,
		.smmu = {
			.reg = 0xb98,
			.bit = 5,
		},
		.la = {
			.reg = 0x3ec,
			.shift = 16,
			.mask = 0xff,
			.def = 0xff,
		},
	}, {
		.id = 0x86,
		.name = "tsecsrdb",
		.swgroup = TEGRA210_SWGROUP_TSECB,
		.smmu = {
			.reg = 0xb98,
			.bit = 6,
		},
		.la = {
			.reg = 0x3f0,
			.shift = 0,
			.mask = 0xff,
			.def = 0x9b,
		},
	}, {
		.id = 0x87,
		.name = "tsecswrb",
		.swgroup = TEGRA210_SWGROUP_TSECB,
		.smmu = {
			.reg = 0xb98,
			.bit = 7,
		},
		.la = {
			.reg = 0x3f0,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x88,
		.name = "gpusrd2",
		.swgroup = TEGRA210_SWGROUP_GPU,
		.smmu = {
			/* read-only */
			.reg = 0xb98,
			.bit = 8,
		},
		.la = {
			.reg = 0x3e8,
			.shift = 0,
			.mask = 0xff,
			.def = 0x1a,
		},
	}, {
		.id = 0x89,
		.name = "gpuswr2",
		.swgroup = TEGRA210_SWGROUP_GPU,
		.smmu = {
			/* read-only */
			.reg = 0xb98,
			.bit = 9,
		},
		.la = {
			.reg = 0x3e8,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	},
};

#define TEGRA_SMMU_SWGROUP(_name, _swgroup, _offset) \
	{ .name = _name, .swgroup = _swgroup, .reg = _offset }

static const struct tegra_smmu_swgroup tegra210_swgroups[] = {
	TEGRA_SMMU_SWGROUP("dc",        TEGRA210_SWGROUP_DC,        0x240),
	TEGRA_SMMU_SWGROUP("dcb",       TEGRA210_SWGROUP_DCB,       0x244),
	TEGRA_SMMU_SWGROUP("afi",       TEGRA210_SWGROUP_AFI,       0x238),
	TEGRA_SMMU_SWGROUP("avpc",      TEGRA210_SWGROUP_AVPC,      0x23c),
	TEGRA_SMMU_SWGROUP("hda",       TEGRA210_SWGROUP_HDA,       0x254),
	TEGRA_SMMU_SWGROUP("hc",        TEGRA210_SWGROUP_HC,        0x250),
	TEGRA_SMMU_SWGROUP("nvenc",     TEGRA210_SWGROUP_NVENC,     0x264),
	TEGRA_SMMU_SWGROUP("ppcs",      TEGRA210_SWGROUP_PPCS,      0x270),
	TEGRA_SMMU_SWGROUP("sata",      TEGRA210_SWGROUP_SATA,      0x274),
	TEGRA_SMMU_SWGROUP("isp2",      TEGRA210_SWGROUP_ISP2,      0x258),
	TEGRA_SMMU_SWGROUP("xusb_host", TEGRA210_SWGROUP_XUSB_HOST, 0x288),
	TEGRA_SMMU_SWGROUP("xusb_dev",  TEGRA210_SWGROUP_XUSB_DEV,  0x28c),
	TEGRA_SMMU_SWGROUP("isp2b",     TEGRA210_SWGROUP_ISP2B,     0xaa4),
	TEGRA_SMMU_SWGROUP("tsec",      TEGRA210_SWGROUP_TSEC,      0x294),
	TEGRA_SMMU_SWGROUP("a9avp",     TEGRA210_SWGROUP_A9AVP,     0x290),
	TEGRA_SMMU_SWGROUP("gpu",       TEGRA210_SWGROUP_GPU,       0xaac),
	TEGRA_SMMU_SWGROUP("sdmmc1a",   TEGRA210_SWGROUP_SDMMC1A,   0xa94),
	TEGRA_SMMU_SWGROUP("sdmmc2a",   TEGRA210_SWGROUP_SDMMC2A,   0xa98),
	TEGRA_SMMU_SWGROUP("sdmmc3a",   TEGRA210_SWGROUP_SDMMC3A,   0xa9c),
	TEGRA_SMMU_SWGROUP("sdmmc4a",   TEGRA210_SWGROUP_SDMMC4A,   0xaa0),
	TEGRA_SMMU_SWGROUP("vic",       TEGRA210_SWGROUP_VIC,       0x284),
	TEGRA_SMMU_SWGROUP("vi",        TEGRA210_SWGROUP_VI,        0x280),
	TEGRA_SMMU_SWGROUP("nvdec",     TEGRA210_SWGROUP_NVDEC,     0xab4),
	TEGRA_SMMU_SWGROUP("ape",       TEGRA210_SWGROUP_APE,       0xab8),
	TEGRA_SMMU_SWGROUP("nvjpg",     TEGRA210_SWGROUP_NVJPG,     0xac0),
	TEGRA_SMMU_SWGROUP("se",        TEGRA210_SWGROUP_SE,        0xabc),
	TEGRA_SMMU_SWGROUP("axiap",     TEGRA210_SWGROUP_AXIAP,     0xacc),
	TEGRA_SMMU_SWGROUP("etr",       TEGRA210_SWGROUP_ETR,       0xad0),
	TEGRA_SMMU_SWGROUP("tsecb",     TEGRA210_SWGROUP_TSECB,     0xad4),
};

static const unsigned int tegra210_group_display[] = {
	TEGRA210_SWGROUP_DC,
	TEGRA210_SWGROUP_DCB,
};

static const struct tegra_smmu_group_soc tegra210_groups[] = {
	{
		.name = "display",
		.swgroups = tegra210_group_display,
		.num_swgroups = ARRAY_SIZE(tegra210_group_display),
	},
};

static const struct tegra_smmu_soc tegra210_smmu_soc = {
	.clients = tegra210_mc_clients,
	.num_clients = ARRAY_SIZE(tegra210_mc_clients),
	.swgroups = tegra210_swgroups,
	.num_swgroups = ARRAY_SIZE(tegra210_swgroups),
	.groups = tegra210_groups,
	.num_groups = ARRAY_SIZE(tegra210_groups),
	.supports_round_robin_arbitration = true,
	.supports_request_limit = true,
	.num_tlb_lines = 32,
	.num_asids = 128,
};

const struct tegra_mc_soc tegra210_mc_soc = {
	.clients = tegra210_mc_clients,
	.num_clients = ARRAY_SIZE(tegra210_mc_clients),
	.num_address_bits = 34,
	.atom_size = 64,
	.client_id_mask = 0xff,
	.smmu = &tegra210_smmu_soc,
};
