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
#include "mem.h"
#include "vmm.h"

#include <nvkm/core/tegra.h>
#include <nvif/class.h>

static void
gk20a_mmu_ctor(const struct nvkm_mmu_func *func, struct nvkm_device *device,
	       int index, struct gk20a_mmu *mmu)
{
	struct iommu_domain *domain = iommu_get_domain_for_dev(device->dev);
	struct nvkm_device_tegra *tegra = device->func->tegra(device);

	nvkm_mmu_ctor(func, device, index, &mmu->base);

	/*
	 * If the DMA API is backed by an IOMMU, make sure the IOMMU bit is
	 * set for all buffer accesses. If the IOMMU is explicitly used, it
	 * is only used for instance blocks and the MMU doesn't care, since
	 * buffer objects are only mapped through the MMU, not through the
	 * IOMMU.
	 *
	 * Big page support could be implemented using explicit IOMMU usage,
	 * but the DMA API already provides that for free, so we don't worry
	 * about it for now.
	 */
	if (domain && !tegra->iommu.domain) {
		mmu->iommu_mask = BIT_ULL(tegra->func->iommu_bit);
		nvkm_debug(&mmu->base.subdev, "IOMMU mask: %llx\n",
			   mmu->iommu_mask);
	}
}

int
gk20a_mmu_new_(const struct nvkm_mmu_func *func, struct nvkm_device *device,
	       int index, struct nvkm_mmu **pmmu)
{
	struct gk20a_mmu *mmu;

	mmu = kzalloc(sizeof(*mmu), GFP_KERNEL);
	if (!mmu)
		return -ENOMEM;

	gk20a_mmu_ctor(func, device, index, mmu);

	if (pmmu)
		*pmmu = &mmu->base;

	return 0;
}

static const struct nvkm_mmu_func
gk20a_mmu = {
	.dma_bits = 40,
	.mmu = {{ -1, -1, NVIF_CLASS_MMU_GF100}},
	.mem = {{ -1, -1, NVIF_CLASS_MEM_GF100}, .umap = gf100_mem_map },
	.vmm = {{ -1, -1, NVIF_CLASS_VMM_GF100}, gk20a_vmm_new },
	.kind = gf100_mmu_kind,
	.kind_sys = true,
};

int
gk20a_mmu_new(struct nvkm_device *device, int index, struct nvkm_mmu **pmmu)
{
	return gk20a_mmu_new_(&gk20a_mmu, device, index, pmmu);
}
