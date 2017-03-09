// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2019 NVIDIA Corporation.
 */

#ifndef __HOST1X_FENCE_H
#define __HOST1X_FENCE_H

struct host1x;
struct host1x_channel;
struct dma_fence;

bool host1x_fence_is_waitable(struct dma_fence *fence);
int host1x_fence_wait(struct dma_fence *fence, struct host1x *host,
		      struct host1x_channel *ch);

#endif
