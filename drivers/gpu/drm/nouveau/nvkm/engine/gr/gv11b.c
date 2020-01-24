/*
 * Copyright 2018 Red Hat Inc.
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
#include "gf100.h"
#include "ctxgf100.h"

#include <subdev/acr.h>

#include <nvif/class.h>

#include <nvfw/flcn.h>

static const struct nvkm_acr_lsf_func
gv11b_gr_gpccs_acr = {
	.flags = NVKM_ACR_LSF_FORCE_PRIV_LOAD,
	.bld_size = sizeof(struct flcn_bl_dmem_desc),
	.bld_write = gm20b_gr_acr_bld_write,
	.bld_patch = gm20b_gr_acr_bld_patch,
};

static const struct gf100_gr_func
gv11b_gr = {
	.oneinit_tiles = gm200_gr_oneinit_tiles,
	.oneinit_sm_id = gm200_gr_oneinit_sm_id,
	.init = gf100_gr_init,
	.init_419bd8 = gv100_gr_init_419bd8,
	.init_gpc_mmu = gm200_gr_init_gpc_mmu,
	.init_vsc_stream_master = gk104_gr_init_vsc_stream_master,
	.init_zcull = gf117_gr_init_zcull,
	.init_num_active_ltcs = gf100_gr_init_num_active_ltcs,
	.init_rop_active_fbps = gp100_gr_init_rop_active_fbps,
	.init_swdx_pes_mask = gp102_gr_init_swdx_pes_mask,
	.init_fecs_exceptions = gp100_gr_init_fecs_exceptions,
	.init_ds_hww_esr_2 = gm200_gr_init_ds_hww_esr_2,
	.init_sked_hww_esr = gk104_gr_init_sked_hww_esr,
	.init_ppc_exceptions = gk104_gr_init_ppc_exceptions,
	.init_504430 = gv100_gr_init_504430,
	.init_shader_exceptions = gv100_gr_init_shader_exceptions,
	.init_4188a4 = gv100_gr_init_4188a4,
	.trap_mp = gv100_gr_trap_mp,
	.rops = gm200_gr_rops,
	.gpc_nr = 6,
	.tpc_nr = 5,
	.ppc_nr = 2,
	.grctx = &gv100_grctx,
	.zbc = &gp102_gr_zbc,
	.sclass = {
		{ -1, -1, FERMI_TWOD_A },
		{ -1, -1, KEPLER_INLINE_TO_MEMORY_B },
		{ -1, -1, VOLTA_A, &gf100_fermi },
		{ -1, -1, VOLTA_COMPUTE_A },
		{}
	}
};

#if IS_ENABLED(CONFIG_ARCH_TEGRA_194_SOC)
MODULE_FIRMWARE("nvidia/gv11b/acr/bl.bin");
MODULE_FIRMWARE("nvidia/gv11b/acr/ucode_load.bin");
MODULE_FIRMWARE("nvidia/gv11b/gr/fecs_bl.bin");
MODULE_FIRMWARE("nvidia/gv11b/gr/fecs_inst.bin");
MODULE_FIRMWARE("nvidia/gv11b/gr/fecs_data.bin");
MODULE_FIRMWARE("nvidia/gv11b/gr/fecs_sig.bin");
MODULE_FIRMWARE("nvidia/gv11b/gr/gpccs_bl.bin");
MODULE_FIRMWARE("nvidia/gv11b/gr/gpccs_inst.bin");
MODULE_FIRMWARE("nvidia/gv11b/gr/gpccs_data.bin");
MODULE_FIRMWARE("nvidia/gv11b/gr/gpccs_sig.bin");
MODULE_FIRMWARE("nvidia/gv11b/gr/sw_ctx.bin");
MODULE_FIRMWARE("nvidia/gv11b/gr/sw_nonctx.bin");
MODULE_FIRMWARE("nvidia/gv11b/gr/sw_bundle_init.bin");
MODULE_FIRMWARE("nvidia/gv11b/gr/sw_method_init.bin");
MODULE_FIRMWARE("nvidia/gv11b/pmu/desc.bin");
MODULE_FIRMWARE("nvidia/gv11b/pmu/image.bin");
MODULE_FIRMWARE("nvidia/gv11b/pmu/sig.bin");
#endif

static const struct gf100_gr_fwif
gv11b_gr_fwif[] = {
	{ 0, gm200_gr_load, &gv11b_gr, &gm20b_gr_fecs_acr, &gv11b_gr_gpccs_acr },
	{}
};

int
gv11b_gr_new(struct nvkm_device *device, int index, struct nvkm_gr **pgr)
{
	return gf100_gr_new_(gv11b_gr_fwif, device, index, pgr);
}
