/*
 * Copyright 2020 NVIDIA Corporation.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "priv.h"

#if IS_ENABLED(CONFIG_ARCH_TEGRA_194_SOC)
MODULE_FIRMWARE("nvidia/gv11b/acr/bl.bin");
MODULE_FIRMWARE("nvidia/gv11b/acr/ucode_load.bin");
#endif

static const struct nvkm_acr_hsf_fwif
gv11b_acr_load_fwif[] = {
	{ 0, nvkm_acr_hsfw_load, &gm20b_acr_load_0 },
	{}
};

static const struct nvkm_acr_func
gv11b_acr = {
	.load = gv11b_acr_load_fwif,
	.wpr_parse = gm200_acr_wpr_parse,
	.wpr_layout = gm200_acr_wpr_layout,
	.wpr_alloc = gm20b_acr_wpr_alloc,
	.wpr_build = gm200_acr_wpr_build,
	.wpr_patch = gm200_acr_wpr_patch,
	.wpr_check = gm200_acr_wpr_check,
	.init = gm200_acr_init,
};

static const struct nvkm_acr_fwif
gv11b_acr_fwif[] = {
	{ 0, gm20b_acr_load, &gv11b_acr },
	{}
};

int
gv11b_acr_new(struct nvkm_device *device, int index, struct nvkm_acr **pacr)
{
	return nvkm_acr_new_(gv11b_acr_fwif, device, index, pacr);
}
