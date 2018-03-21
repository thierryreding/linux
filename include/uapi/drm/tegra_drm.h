/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _UAPI_TEGRA_DRM_H_
#define _UAPI_TEGRA_DRM_H_

#include "drm.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define DRM_TEGRA_GEM_CREATE_CONTIGUOUS	(1 << 0)
#define DRM_TEGRA_GEM_CREATE_FLAGS	(DRM_TEGRA_GEM_CREATE_CONTIGUOUS)

struct drm_tegra_gem_create {
	__u64 size;
	__u32 flags;
	__u32 handle;
};

struct drm_tegra_gem_mmap {
	__u32 handle;
	__u32 pad;
	__u64 offset;
};

#define DRM_TEGRA_CHANNEL_FLAGS (0)

struct drm_tegra_open_channel {
	__u32 client;
	__u32 syncpts;
	__u64 context;
	__u32 flags;
	__u32 pad;
	__u64 reserved;
};

struct drm_tegra_close_channel {
	__u64 context;
};

#define DRM_TEGRA_BUFFER_FLAGS	(0)

struct drm_tegra_buffer {
	__u32 handle;
	__u32 flags;
};

#define DRM_TEGRA_FENCE_WAIT	(1 << 0)
#define DRM_TEGRA_FENCE_EMIT	(1 << 1)
#define DRM_TEGRA_FENCE_FD	(1 << 2)
#define DRM_TEGRA_FENCE_FLAGS	(DRM_TEGRA_FENCE_WAIT | \
				 DRM_TEGRA_FENCE_EMIT | \
				 DRM_TEGRA_FENCE_FD)

struct drm_tegra_fence {
	/**
	 * @handle:
	 *
	 * Handle (syncobj) or file descriptor (sync FD) of the fence. It is
	 * interpreted based on the DRM_TEGRA_FENCE_FD flag (see below).
	 */
	__u32 handle;

	/**
	 * @flags:
	 *
	 * DRM_TEGRA_FENCE_WAIT - Wait for this fence before the new command
	 *	buffer is submitted.
	 * DRM_TEGRA_FENCE_EMIT - Emit this fence when the command buffer is
	 *	done being processed.
	 * DRM_TEGRA_FENCE_FD - This fence is a sync FD. If not specified, a
	 *	syncobj will be used.
	 */
	__u32 flags;

	/**
	 * @offset:
	 *
	 * Offset in the command stream for this fence. This is used to patch
	 * the command stream with the resolved syncpoint ID.
	 */
	__u32 offset;

	/**
	 * @index:
	 *
	 * Syncpoint to use for this fence. This is an index into the list of
	 * syncpoints of the channel. It will be resolved to a real syncpoint
	 * ID upon job submission.
	 */
	__u32 index;

	/**
	 * @value:
	 *
	 * Number of times to increment the syncpoint.
	 */
	__u32 value;

	__u32 pad[3];
};

#define DRM_TEGRA_CMDBUF_FLAGS	(0)

struct drm_tegra_cmdbuf {
	__u32 index;
	__u32 offset;
	__u32 words;
	__u32 flags;
	__u32 pad;
	__u32 num_fences;
	__u64 fences;
};

#define DRM_TEGRA_RELOC_FLAGS	(0)

struct drm_tegra_reloc {
	struct {
		__u32 index;
		__u32 offset;
	} cmdbuf;
	struct {
		__u32 index;
		__u32 offset;
	} target;
	__u32 shift;
	__u32 flags;
	__u64 pad;
};

#define DRM_TEGRA_SUBMIT_FLAGS	(0)

struct drm_tegra_submit {
	__u64 context;
	__u32 num_buffers;
	__u32 num_cmdbufs;
	__u32 num_relocs;
	__u32 timeout;
	__u64 buffers;
	__u64 cmdbufs;
	__u64 relocs;
	__u32 flags;
	__u32 pad;
	__u64 reserved[9]; /* future expansion */
};

#define DRM_TEGRA_GEM_CREATE		0x00
#define DRM_TEGRA_GEM_MMAP		0x01
#define DRM_TEGRA_OPEN_CHANNEL		0x02
#define DRM_TEGRA_CLOSE_CHANNEL		0x03
#define DRM_TEGRA_SUBMIT		0x04

#define DRM_IOCTL_TEGRA_GEM_CREATE DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_GEM_CREATE, struct drm_tegra_gem_create)
#define DRM_IOCTL_TEGRA_GEM_MMAP DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_GEM_MMAP, struct drm_tegra_gem_mmap)
#define DRM_IOCTL_TEGRA_OPEN_CHANNEL DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_OPEN_CHANNEL, struct drm_tegra_open_channel)
#define DRM_IOCTL_TEGRA_CLOSE_CHANNEL DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_CLOSE_CHANNEL, struct drm_tegra_open_channel)
#define DRM_IOCTL_TEGRA_SUBMIT DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_SUBMIT, struct drm_tegra_submit)

#if defined(__cplusplus)
}
#endif

#endif
