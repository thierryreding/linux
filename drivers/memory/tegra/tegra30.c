/*
 * Copyright (C) 2014 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of.h>
#include <linux/mm.h>

#include <dt-bindings/memory/tegra30-mc.h>

#include "mc.h"

static const struct tegra_mc_client tegra30_mc_clients[] = {
	{
		.id = 0x00,
		.name = "ptcr",
		.swgroup = TEGRA30_SWGROUP_PTC,
	}, {
		.id = 0x01,
		.name = "display0a",
		.swgroup = TEGRA30_SWGROUP_DC,
		.smmu = {
			.reg = 0x228,
			.bit = 1,
		},
		.la = {
			.reg = 0x2e8,
			.shift = 0,
			.mask = 0xff,
			.def = 0x4e,
		},
	}, {
		.id = 0x02,
		.name = "display0ab",
		.swgroup = TEGRA30_SWGROUP_DCB,
		.smmu = {
			.reg = 0x228,
			.bit = 2,
		},
		.la = {
			.reg = 0x2f4,
			.shift = 0,
			.mask = 0xff,
			.def = 0x4e,
		},
	}, {
		.id = 0x03,
		.name = "display0b",
		.swgroup = TEGRA30_SWGROUP_DC,
		.smmu = {
			.reg = 0x228,
			.bit = 3,
		},
		.la = {
			.reg = 0x2e8,
			.shift = 16,
			.mask = 0xff,
			.def = 0x4e,
		},
	}, {
		.id = 0x04,
		.name = "display0bb",
		.swgroup = TEGRA30_SWGROUP_DCB,
		.smmu = {
			.reg = 0x228,
			.bit = 4,
		},
		.la = {
			.reg = 0x2f4,
			.shift = 16,
			.mask = 0xff,
			.def = 0x4e,
		},
	}, {
		.id = 0x05,
		.name = "display0c",
		.swgroup = TEGRA30_SWGROUP_DC,
		.smmu = {
			.reg = 0x228,
			.bit = 5,
		},
		.la = {
			.reg = 0x2ec,
			.shift = 0,
			.mask = 0xff,
			.def = 0x4e,
		},
	}, {
		.id = 0x06,
		.name = "display0cb",
		.swgroup = TEGRA30_SWGROUP_DCB,
		.smmu = {
			.reg = 0x228,
			.bit = 6,
		},
		.la = {
			.reg = 0x2f8,
			.shift = 0,
			.mask = 0xff,
			.def = 0x4e,
		},
	}, {
		.id = 0x07,
		.name = "display1b",
		.swgroup = TEGRA30_SWGROUP_DC,
		.smmu = {
			.reg = 0x228,
			.bit = 7,
		},
		.la = {
			.reg = 0x2ec,
			.shift = 16,
			.mask = 0xff,
			.def = 0x4e,
		},
	}, {
		.id = 0x08,
		.name = "display1bb",
		.swgroup = TEGRA30_SWGROUP_DCB,
		.smmu = {
			.reg = 0x228,
			.bit = 8,
		},
		.la = {
			.reg = 0x2f8,
			.shift = 16,
			.mask = 0xff,
			.def = 0x4e,
		},
	}, {
		.id = 0x09,
		.name = "eppup",
		.swgroup = TEGRA30_SWGROUP_EPP,
		.smmu = {
			.reg = 0x228,
			.bit = 9,
		},
		.la = {
			.reg = 0x300,
			.shift = 0,
			.mask = 0xff,
			.def = 0x17,
		},
	}, {
		.id = 0x0a,
		.name = "g2pr",
		.swgroup = TEGRA30_SWGROUP_G2,
		.smmu = {
			.reg = 0x228,
			.bit = 10,
		},
		.la = {
			.reg = 0x308,
			.shift = 0,
			.mask = 0xff,
			.def = 0x09,
		},
	}, {
		.id = 0x0b,
		.name = "g2sr",
		.swgroup = TEGRA30_SWGROUP_G2,
		.smmu = {
			.reg = 0x228,
			.bit = 11,
		},
		.la = {
			.reg = 0x308,
			.shift = 16,
			.mask = 0xff,
			.def = 0x09,
		},
	}, {
		.id = 0x0c,
		.name = "mpeunifbr",
		.swgroup = TEGRA30_SWGROUP_MPE,
		.smmu = {
			.reg = 0x228,
			.bit = 12,
		},
		.la = {
			.reg = 0x328,
			.shift = 0,
			.mask = 0xff,
			.def = 0x50,
		},
	}, {
		.id = 0x0d,
		.name = "viruv",
		.swgroup = TEGRA30_SWGROUP_VI,
		.smmu = {
			.reg = 0x228,
			.bit = 13,
		},
		.la = {
			.reg = 0x364,
			.shift = 0,
			.mask = 0xff,
			.def = 0x2c,
		},
	}, {
		.id = 0x0e,
		.name = "afir",
		.swgroup = TEGRA30_SWGROUP_AFI,
		.smmu = {
			.reg = 0x228,
			.bit = 14,
		},
		.la = {
			.reg = 0x2e0,
			.shift = 0,
			.mask = 0xff,
			.def = 0x10,
		},
	}, {
		.id = 0x0f,
		.name = "avpcarm7r",
		.swgroup = TEGRA30_SWGROUP_AVPC,
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
		.swgroup = TEGRA30_SWGROUP_DC,
		.smmu = {
			.reg = 0x228,
			.bit = 16,
		},
		.la = {
			.reg = 0x2f0,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
		},
	}, {
		.id = 0x11,
		.name = "displayhcb",
		.swgroup = TEGRA30_SWGROUP_DCB,
		.smmu = {
			.reg = 0x228,
			.bit = 17,
		},
		.la = {
			.reg = 0x2fc,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
		},
	}, {
		.id = 0x12,
		.name = "fdcdrd",
		.swgroup = TEGRA30_SWGROUP_NV,
		.smmu = {
			.reg = 0x228,
			.bit = 18,
		},
		.la = {
			.reg = 0x334,
			.shift = 0,
			.mask = 0xff,
			.def = 0x0a,
		},
	}, {
		.id = 0x13,
		.name = "fdcdrd2",
		.swgroup = TEGRA30_SWGROUP_NV2,
		.smmu = {
			.reg = 0x228,
			.bit = 19,
		},
		.la = {
			.reg = 0x33c,
			.shift = 0,
			.mask = 0xff,
			.def = 0x0a,
		},
	}, {
		.id = 0x14,
		.name = "g2dr",
		.swgroup = TEGRA30_SWGROUP_G2,
		.smmu = {
			.reg = 0x228,
			.bit = 20,
		},
		.la = {
			.reg = 0x30c,
			.shift = 0,
			.mask = 0xff,
			.def = 0x0a,
		},
	}, {
		.id = 0x15,
		.name = "hdar",
		.swgroup = TEGRA30_SWGROUP_HDA,
		.smmu = {
			.reg = 0x228,
			.bit = 21,
		},
		.la = {
			.reg = 0x318,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
		},
	}, {
		.id = 0x16,
		.name = "host1xdmar",
		.swgroup = TEGRA30_SWGROUP_HC,
		.smmu = {
			.reg = 0x228,
			.bit = 22,
		},
		.la = {
			.reg = 0x310,
			.shift = 0,
			.mask = 0xff,
			.def = 0x05,
		},
	}, {
		.id = 0x17,
		.name = "host1xr",
		.swgroup = TEGRA30_SWGROUP_HC,
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
		.id = 0x18,
		.name = "idxsrd",
		.swgroup = TEGRA30_SWGROUP_NV,
		.smmu = {
			.reg = 0x228,
			.bit = 24,
		},
		.la = {
			.reg = 0x334,
			.shift = 16,
			.mask = 0xff,
			.def = 0x13,
		},
	}, {
		.id = 0x19,
		.name = "idxsrd2",
		.swgroup = TEGRA30_SWGROUP_NV2,
		.smmu = {
			.reg = 0x228,
			.bit = 25,
		},
		.la = {
			.reg = 0x33c,
			.shift = 16,
			.mask = 0xff,
			.def = 0x13,
		},
	}, {
		.id = 0x1a,
		.name = "mpe_ipred",
		.swgroup = TEGRA30_SWGROUP_MPE,
		.smmu = {
			.reg = 0x228,
			.bit = 26,
		},
		.la = {
			.reg = 0x328,
			.shift = 16,
			.mask = 0xff,
			.def = 0x80,
		},
	}, {
		.id = 0x1b,
		.name = "mpeamemrd",
		.swgroup = TEGRA30_SWGROUP_MPE,
		.smmu = {
			.reg = 0x228,
			.bit = 27,
		},
		.la = {
			.reg = 0x32c,
			.shift = 0,
			.mask = 0xff,
			.def = 0x42,
		},
	}, {
		.id = 0x1c,
		.name = "mpecsrd",
		.swgroup = TEGRA30_SWGROUP_MPE,
		.smmu = {
			.reg = 0x228,
			.bit = 28,
		},
		.la = {
			.reg = 0x32c,
			.shift = 16,
			.mask = 0xff,
			.def = 0xff,
		},
	}, {
		.id = 0x1d,
		.name = "ppcsahbdmar",
		.swgroup = TEGRA30_SWGROUP_PPCS,
		.smmu = {
			.reg = 0x228,
			.bit = 29,
		},
		.la = {
			.reg = 0x344,
			.shift = 0,
			.mask = 0xff,
			.def = 0x10,
		},
	}, {
		.id = 0x1e,
		.name = "ppcsahbslvr",
		.swgroup = TEGRA30_SWGROUP_PPCS,
		.smmu = {
			.reg = 0x228,
			.bit = 30,
		},
		.la = {
			.reg = 0x344,
			.shift = 16,
			.mask = 0xff,
			.def = 0x12,
		},
	}, {
		.id = 0x1f,
		.name = "satar",
		.swgroup = TEGRA30_SWGROUP_SATA,
		.smmu = {
			.reg = 0x228,
			.bit = 31,
		},
		.la = {
			.reg = 0x350,
			.shift = 0,
			.mask = 0xff,
			.def = 0x33,
		},
	}, {
		.id = 0x20,
		.name = "texsrd",
		.swgroup = TEGRA30_SWGROUP_NV,
		.smmu = {
			.reg = 0x22c,
			.bit = 0,
		},
		.la = {
			.reg = 0x338,
			.shift = 0,
			.mask = 0xff,
			.def = 0x13,
		},
	}, {
		.id = 0x21,
		.name = "texsrd2",
		.swgroup = TEGRA30_SWGROUP_NV2,
		.smmu = {
			.reg = 0x22c,
			.bit = 1,
		},
		.la = {
			.reg = 0x340,
			.shift = 0,
			.mask = 0xff,
			.def = 0x13,
		},
	}, {
		.id = 0x22,
		.name = "vdebsevr",
		.swgroup = TEGRA30_SWGROUP_VDE,
		.smmu = {
			.reg = 0x22c,
			.bit = 2,
		},
		.la = {
			.reg = 0x354,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
		},
	}, {
		.id = 0x23,
		.name = "vdember",
		.swgroup = TEGRA30_SWGROUP_VDE,
		.smmu = {
			.reg = 0x22c,
			.bit = 3,
		},
		.la = {
			.reg = 0x354,
			.shift = 16,
			.mask = 0xff,
			.def = 0xd0,
		},
	}, {
		.id = 0x24,
		.name = "vdemcer",
		.swgroup = TEGRA30_SWGROUP_VDE,
		.smmu = {
			.reg = 0x22c,
			.bit = 4,
		},
		.la = {
			.reg = 0x358,
			.shift = 0,
			.mask = 0xff,
			.def = 0x2a,
		},
	}, {
		.id = 0x25,
		.name = "vdetper",
		.swgroup = TEGRA30_SWGROUP_VDE,
		.smmu = {
			.reg = 0x22c,
			.bit = 5,
		},
		.la = {
			.reg = 0x358,
			.shift = 16,
			.mask = 0xff,
			.def = 0x74,
		},
	}, {
		.id = 0x26,
		.name = "mpcorelpr",
		.swgroup = TEGRA30_SWGROUP_MPCORELP,
		.la = {
			.reg = 0x324,
			.shift = 0,
			.mask = 0xff,
			.def = 0x04,
		},
	}, {
		.id = 0x27,
		.name = "mpcorer",
		.swgroup = TEGRA30_SWGROUP_MPCORE,
		.la = {
			.reg = 0x320,
			.shift = 0,
			.mask = 0xff,
			.def = 0x04,
		},
	}, {
		.id = 0x28,
		.name = "eppu",
		.swgroup = TEGRA30_SWGROUP_EPP,
		.smmu = {
			.reg = 0x22c,
			.bit = 8,
		},
		.la = {
			.reg = 0x300,
			.shift = 16,
			.mask = 0xff,
			.def = 0x6c,
		},
	}, {
		.id = 0x29,
		.name = "eppv",
		.swgroup = TEGRA30_SWGROUP_EPP,
		.smmu = {
			.reg = 0x22c,
			.bit = 9,
		},
		.la = {
			.reg = 0x304,
			.shift = 0,
			.mask = 0xff,
			.def = 0x6c,
		},
	}, {
		.id = 0x2a,
		.name = "eppy",
		.swgroup = TEGRA30_SWGROUP_EPP,
		.smmu = {
			.reg = 0x22c,
			.bit = 10,
		},
		.la = {
			.reg = 0x304,
			.shift = 16,
			.mask = 0xff,
			.def = 0x6c,
		},
	}, {
		.id = 0x2b,
		.name = "mpeunifbw",
		.swgroup = TEGRA30_SWGROUP_MPE,
		.smmu = {
			.reg = 0x22c,
			.bit = 11,
		},
		.la = {
			.reg = 0x330,
			.shift = 0,
			.mask = 0xff,
			.def = 0x13,
		},
	}, {
		.id = 0x2c,
		.name = "viwsb",
		.swgroup = TEGRA30_SWGROUP_VI,
		.smmu = {
			.reg = 0x22c,
			.bit = 12,
		},
		.la = {
			.reg = 0x364,
			.shift = 16,
			.mask = 0xff,
			.def = 0x12,
		},
	}, {
		.id = 0x2d,
		.name = "viwu",
		.swgroup = TEGRA30_SWGROUP_VI,
		.smmu = {
			.reg = 0x22c,
			.bit = 13,
		},
		.la = {
			.reg = 0x368,
			.shift = 0,
			.mask = 0xff,
			.def = 0xb2,
		},
	}, {
		.id = 0x2e,
		.name = "viwv",
		.swgroup = TEGRA30_SWGROUP_VI,
		.smmu = {
			.reg = 0x22c,
			.bit = 14,
		},
		.la = {
			.reg = 0x368,
			.shift = 16,
			.mask = 0xff,
			.def = 0xb2,
		},
	}, {
		.id = 0x2f,
		.name = "viwy",
		.swgroup = TEGRA30_SWGROUP_VI,
		.smmu = {
			.reg = 0x22c,
			.bit = 15,
		},
		.la = {
			.reg = 0x36c,
			.shift = 0,
			.mask = 0xff,
			.def = 0x12,
		},
	}, {
		.id = 0x30,
		.name = "g2dw",
		.swgroup = TEGRA30_SWGROUP_G2,
		.smmu = {
			.reg = 0x22c,
			.bit = 16,
		},
		.la = {
			.reg = 0x30c,
			.shift = 16,
			.mask = 0xff,
			.def = 0x9,
		},
	}, {
		.id = 0x31,
		.name = "afiw",
		.swgroup = TEGRA30_SWGROUP_AFI,
		.smmu = {
			.reg = 0x22c,
			.bit = 17,
		},
		.la = {
			.reg = 0x2e0,
			.shift = 16,
			.mask = 0xff,
			.def = 0x0c,
		},
	}, {
		.id = 0x32,
		.name = "avpcarm7w",
		.swgroup = TEGRA30_SWGROUP_AVPC,
		.smmu = {
			.reg = 0x22c,
			.bit = 18,
		},
		.la = {
			.reg = 0x2e4,
			.shift = 16,
			.mask = 0xff,
			.def = 0x0e,
		},
	}, {
		.id = 0x33,
		.name = "fdcdwr",
		.swgroup = TEGRA30_SWGROUP_NV,
		.smmu = {
			.reg = 0x22c,
			.bit = 19,
		},
		.la = {
			.reg = 0x338,
			.shift = 16,
			.mask = 0xff,
			.def = 0x0a,
		},
	}, {
		.id = 0x34,
		.name = "fdcwr2",
		.swgroup = TEGRA30_SWGROUP_NV2,
		.smmu = {
			.reg = 0x22c,
			.bit = 20,
		},
		.la = {
			.reg = 0x340,
			.shift = 16,
			.mask = 0xff,
			.def = 0x0a,
		},
	}, {
		.id = 0x35,
		.name = "hdaw",
		.swgroup = TEGRA30_SWGROUP_HDA,
		.smmu = {
			.reg = 0x22c,
			.bit = 21,
		},
		.la = {
			.reg = 0x318,
			.shift = 16,
			.mask = 0xff,
			.def = 0xff,
		},
	}, {
		.id = 0x36,
		.name = "host1xw",
		.swgroup = TEGRA30_SWGROUP_HC,
		.smmu = {
			.reg = 0x22c,
			.bit = 22,
		},
		.la = {
			.reg = 0x314,
			.shift = 0,
			.mask = 0xff,
			.def = 0x10,
		},
	}, {
		.id = 0x37,
		.name = "ispw",
		.swgroup = TEGRA30_SWGROUP_ISP,
		.smmu = {
			.reg = 0x22c,
			.bit = 23,
		},
		.la = {
			.reg = 0x31c,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
		},
	}, {
		.id = 0x38,
		.name = "mpcorelpw",
		.swgroup = TEGRA30_SWGROUP_MPCORELP,
		.la = {
			.reg = 0x324,
			.shift = 16,
			.mask = 0xff,
			.def = 0x0e,
		},
	}, {
		.id = 0x39,
		.name = "mpcorew",
		.swgroup = TEGRA30_SWGROUP_MPCORE,
		.la = {
			.reg = 0x320,
			.shift = 16,
			.mask = 0xff,
			.def = 0x0e,
		},
	}, {
		.id = 0x3a,
		.name = "mpecswr",
		.swgroup = TEGRA30_SWGROUP_MPE,
		.smmu = {
			.reg = 0x22c,
			.bit = 26,
		},
		.la = {
			.reg = 0x330,
			.shift = 16,
			.mask = 0xff,
			.def = 0xff,
		},
	}, {
		.id = 0x3b,
		.name = "ppcsahbdmaw",
		.swgroup = TEGRA30_SWGROUP_PPCS,
		.smmu = {
			.reg = 0x22c,
			.bit = 27,
		},
		.la = {
			.reg = 0x348,
			.shift = 0,
			.mask = 0xff,
			.def = 0x10,
		},
	}, {
		.id = 0x3c,
		.name = "ppcsahbslvw",
		.swgroup = TEGRA30_SWGROUP_PPCS,
		.smmu = {
			.reg = 0x22c,
			.bit = 28,
		},
		.la = {
			.reg = 0x348,
			.shift = 16,
			.mask = 0xff,
			.def = 0x06,
		},
	}, {
		.id = 0x3d,
		.name = "sataw",
		.swgroup = TEGRA30_SWGROUP_SATA,
		.smmu = {
			.reg = 0x22c,
			.bit = 29,
		},
		.la = {
			.reg = 0x350,
			.shift = 16,
			.mask = 0xff,
			.def = 0x33,
		},
	}, {
		.id = 0x3e,
		.name = "vdebsevw",
		.swgroup = TEGRA30_SWGROUP_VDE,
		.smmu = {
			.reg = 0x22c,
			.bit = 30,
		},
		.la = {
			.reg = 0x35c,
			.shift = 0,
			.mask = 0xff,
			.def = 0xff,
		},
	}, {
		.id = 0x3f,
		.name = "vdedbgw",
		.swgroup = TEGRA30_SWGROUP_VDE,
		.smmu = {
			.reg = 0x22c,
			.bit = 31,
		},
		.la = {
			.reg = 0x35c,
			.shift = 16,
			.mask = 0xff,
			.def = 0xff,
		},
	}, {
		.id = 0x40,
		.name = "vdembew",
		.swgroup = TEGRA30_SWGROUP_VDE,
		.smmu = {
			.reg = 0x230,
			.bit = 0,
		},
		.la = {
			.reg = 0x360,
			.shift = 0,
			.mask = 0xff,
			.def = 0x42,
		},
	}, {
		.id = 0x41,
		.name = "vdetpmw",
		.swgroup = TEGRA30_SWGROUP_VDE,
		.smmu = {
			.reg = 0x230,
			.bit = 1,
		},
		.la = {
			.reg = 0x360,
			.shift = 16,
			.mask = 0xff,
			.def = 0x2a,
		},
	},
};

#define TEGRA_SMMU_SWGROUP(_name, _swgroup, _offset) \
	{ .name = _name, .swgroup = _swgroup, .reg = _offset }

static const struct tegra_smmu_swgroup tegra30_swgroups[] = {
	TEGRA_SMMU_SWGROUP("dc",   TEGRA30_SWGROUP_DC,   0x240),
	TEGRA_SMMU_SWGROUP("dcb",  TEGRA30_SWGROUP_DCB,  0x244),
	TEGRA_SMMU_SWGROUP("epp",  TEGRA30_SWGROUP_EPP,  0x248),
	TEGRA_SMMU_SWGROUP("g2",   TEGRA30_SWGROUP_G2,   0x24c),
	TEGRA_SMMU_SWGROUP("mpe",  TEGRA30_SWGROUP_MPE,  0x264),
	TEGRA_SMMU_SWGROUP("vi",   TEGRA30_SWGROUP_VI,   0x280),
	TEGRA_SMMU_SWGROUP("afi",  TEGRA30_SWGROUP_AFI,  0x238),
	TEGRA_SMMU_SWGROUP("avpc", TEGRA30_SWGROUP_AVPC, 0x23c),
	TEGRA_SMMU_SWGROUP("nv",   TEGRA30_SWGROUP_NV,   0x268),
	TEGRA_SMMU_SWGROUP("nv2",  TEGRA30_SWGROUP_NV2,  0x26c),
	TEGRA_SMMU_SWGROUP("hda",  TEGRA30_SWGROUP_HDA,  0x254),
	TEGRA_SMMU_SWGROUP("hc",   TEGRA30_SWGROUP_HC,   0x250),
	TEGRA_SMMU_SWGROUP("ppcs", TEGRA30_SWGROUP_PPCS, 0x270),
	TEGRA_SMMU_SWGROUP("sata", TEGRA30_SWGROUP_SATA, 0x278),
	TEGRA_SMMU_SWGROUP("vde",  TEGRA30_SWGROUP_VDE,  0x27c),
	TEGRA_SMMU_SWGROUP("isp",  TEGRA30_SWGROUP_ISP,  0x258),
};

static const unsigned int tegra30_group_display[] = {
	TEGRA30_SWGROUP_DC,
	TEGRA30_SWGROUP_DCB,
};

static const struct tegra_smmu_group_soc tegra30_groups[] = {
	{
		.name = "display",
		.swgroups = tegra30_group_display,
		.num_swgroups = ARRAY_SIZE(tegra30_group_display),
	},
};

static const struct tegra_smmu_soc tegra30_smmu_soc = {
	.clients = tegra30_mc_clients,
	.num_clients = ARRAY_SIZE(tegra30_mc_clients),
	.swgroups = tegra30_swgroups,
	.num_swgroups = ARRAY_SIZE(tegra30_swgroups),
	.groups = tegra30_groups,
	.num_groups = ARRAY_SIZE(tegra30_groups),
	.supports_round_robin_arbitration = false,
	.supports_request_limit = false,
	.num_tlb_lines = 16,
	.num_asids = 4,
};

const struct tegra_mc_soc tegra30_mc_soc = {
	.clients = tegra30_mc_clients,
	.num_clients = ARRAY_SIZE(tegra30_mc_clients),
	.num_address_bits = 32,
	.atom_size = 16,
	.client_id_mask = 0x7f,
	.smmu = &tegra30_smmu_soc,
};
