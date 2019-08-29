/*
 * Copyright 2017 Red Hat Inc.
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

#include "gk20a.h"
#include "vmm.h"

static int
gp10b_vmm_valid(struct nvkm_vmm *vmm, void *argv, u32 argc,
		struct nvkm_vmm_map *map)
{
	struct gk20a_mmu *mmu = gk20a_mmu(vmm->mmu);
	int ret;

	ret = gp100_vmm_valid(vmm, argv, argc, map);
	if (ret < 0)
		return ret;

	map->type |= mmu->iommu_mask >> 4;

	return 0;
}

static const struct nvkm_vmm_func
gp10b_vmm = {
	.join = gp100_vmm_join,
	.part = gf100_vmm_part,
	.valid = gp10b_vmm_valid,
	.flush = gp100_vmm_flush,
	.mthd = gp100_vmm_mthd,
	.invalidate_pdb = gp100_vmm_invalidate_pdb,
	.page = {
		{ 47, &gp100_vmm_desc_16[4], NVKM_VMM_PAGE_Sxxx },
		{ 38, &gp100_vmm_desc_16[3], NVKM_VMM_PAGE_Sxxx },
		{ 29, &gp100_vmm_desc_16[2], NVKM_VMM_PAGE_Sxxx },
		{ 21, &gp100_vmm_desc_16[1], NVKM_VMM_PAGE_SxHC },
		{ 16, &gp100_vmm_desc_16[0], NVKM_VMM_PAGE_SxHC },
		{ 12, &gp100_vmm_desc_12[0], NVKM_VMM_PAGE_SxHx },
		{}
	}
};

int
gp10b_vmm_new(struct nvkm_mmu *mmu, bool managed, u64 addr, u64 size,
	      void *argv, u32 argc, struct lock_class_key *key,
	      const char *name, struct nvkm_vmm **pvmm)
{
	return gp100_vmm_new_(&gp10b_vmm, mmu, managed, addr, size,
			      argv, argc, key, name, pvmm);
}
