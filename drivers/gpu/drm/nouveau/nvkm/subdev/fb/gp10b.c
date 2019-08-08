/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <subdev/mmu.h>

#include "gf100.h"

static void
gp10b_fb_init(struct nvkm_fb *base)
{
	struct nvkm_device *device = base->subdev.device;
	struct gf100_fb *fb = gf100_fb(base);
	struct nvkm_mmu *mmu = device->mmu;

	nvkm_info(&base->subdev, "> %s(base=%px)\n", __func__, base);

	gm200_fb_init(base);

	if (1) {
		u32 *data = page_address(fb->r100c10_page);
		u32 value;

		nvkm_info(&fb->base.subdev, "sysmem: %pad\n", &fb->r100c10);

		nvkm_info(&fb->base.subdev, "DATA[0]: %08x\n", data[0]);
		data[0] = 0xdeadbeef;
		nvkm_info(&fb->base.subdev, "DATA[0]: %08x\n", data[0]);

		dma_sync_single_for_device(device->dev, fb->r100c10, SZ_64K,
					   DMA_TO_DEVICE);

		value = (fb->r100c10 >> 16) & 0xffffff;
		value |= mmu->iommu_mask >> 16;

		value |= 0x03000000; /* NCOH */
		nvkm_wr32(device, 0x001700, value);

		value = nvkm_rd32(device, 0x001700);
		nvkm_info(&fb->base.subdev, "PRAM: %08x\n", value);

		value = nvkm_rd32(device, 0x700000);
		nvkm_info(&fb->base.subdev, "PRAM[0]: %08x\n", value);
	}

	nvkm_info(&base->subdev, "< %s()\n", __func__);
}

static const struct nvkm_fb_func
gp10b_fb = {
	.dtor = gf100_fb_dtor,
	.oneinit = gf100_fb_oneinit,
	.init = gp10b_fb_init,
	.init_page = gm200_fb_init_page,
	.intr = gf100_fb_intr,
};

int
gp10b_fb_new(struct nvkm_device *device, int index, struct nvkm_fb **pfb)
{
	return gf100_fb_new_(&gp10b_fb, device, index, pfb);
}
