/*
 * Copyright (c) 2019 NVIDIA Corporation.
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

#ifndef __NVKM_MMU_GK20A_H__
#define __NVKM_MMU_GK20A_H__

#include "priv.h"

struct gk20a_mmu {
	struct nvkm_mmu base;

	/*
	 * If an IOMMU is used, indicates which address bit will trigger an
	 * IOMMU translation when set (when this bit is not set, the IOMMU is
	 * bypassed). A value of 0 means an IOMMU is never used.
	 */
	u64 iommu_mask;
};

#define gk20a_mmu(mmu) container_of(mmu, struct gk20a_mmu, base)

int gk20a_mmu_new_(const struct nvkm_mmu_func *, struct nvkm_device *,
		   int index, struct nvkm_mmu **);

#endif
