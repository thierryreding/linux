/*
 * Copyright (c) 2019 NVIDIA Corporation
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

#include <nvif/class.h>

static const struct nvkm_fault_func
gv11b_fault = {
	.oneinit = gv100_fault_oneinit,
	.init = gv100_fault_init,
	.fini = gv100_fault_fini,
	.intr = gv100_fault_intr,
	.buffer.nr = 2,
	.buffer.entry_size = 32,
	.buffer.info = gv100_fault_buffer_info,
	.buffer.pin = gp10b_fault_buffer_pin,
	.buffer.init = gv100_fault_buffer_init,
	.buffer.fini = gv100_fault_buffer_fini,
	.buffer.intr = gv100_fault_buffer_intr,
	/*TODO: Figure out how to expose non-replayable fault buffer, which,
	 *      for some reason, is where recoverable CE faults appear...
	 *
	 * 	It's a bit tricky, as both NVKM and SVM will need access to
	 * 	the non-replayable fault buffer.
	 */
	.user = { { 0, 0, VOLTA_FAULT_BUFFER_A }, 1 },
};

int
gv11b_fault_new(struct nvkm_device *device, int index,
		struct nvkm_fault **pfault)
{
	return nvkm_fault_new_(&gv11b_fault, device, index, pfault);
}
