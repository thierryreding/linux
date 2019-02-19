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

#define DRM_TEGRA_GEM_CREATE_TILED     (1 << 0)
#define DRM_TEGRA_GEM_CREATE_BOTTOM_UP (1 << 1)

/**
 * struct drm_tegra_gem_create - parameters for the GEM object creation IOCTL
 */
struct drm_tegra_gem_create {
	/**
	 * @size:
	 *
	 * The size, in bytes, of the buffer object to be created.
	 */
	__u64 size;

	/**
	 * @flags:
	 *
	 * A bitmask of flags that influence the creation of GEM objects:
	 *
	 * DRM_TEGRA_GEM_CREATE_TILED
	 *   Use the 16x16 tiling format for this buffer.
	 *
	 * DRM_TEGRA_GEM_CREATE_BOTTOM_UP
	 *   The buffer has a bottom-up layout.
	 */
	__u32 flags;

	/**
	 * @handle:
	 *
	 * The handle of the created GEM object. Set by the kernel upon
	 * successful completion of the IOCTL.
	 */
	__u32 handle;
};

/**
 * struct drm_tegra_gem_mmap - parameters for the GEM mmap IOCTL
 */
struct drm_tegra_gem_mmap {
	/**
	 * @handle:
	 *
	 * Handle of the GEM object to obtain an mmap offset for.
	 */
	__u32 handle;

	/**
	 * @pad:
	 *
	 * Structure padding that may be used in the future. Must be 0.
	 */
	__u32 pad;

	/**
	 * @offset:
	 *
	 * The mmap offset for the given GEM object. Set by the kernel upon
	 * successful completion of the IOCTL.
	 */
	__u64 offset;
};

/**
 * struct drm_tegra_close_channel - parameters for the close channel IOCTL
 */
struct drm_tegra_close_channel {
	/**
	 * @context:
	 *
	 * The application context of this channel. This is obtained from the
	 * DRM_TEGRA_OPEN_CHANNEL IOCTL.
	 */
	__u64 context;
};

/**
 * struct drm_tegra_syncpt - syncpoint increment operation
 */
struct drm_tegra_syncpt {
	/**
	 * @id:
	 *
	 * ID of the syncpoint to operate on.
	 */
	__u32 id;

	/**
	 * @incrs:
	 *
	 * Number of increments to perform for the syncpoint.
	 */
	__u32 incrs;
};

#define DRM_TEGRA_CHANNEL_FLAGS (0)

/**
 * struct drm_tegra_open_channel - parameters for the open channel IOCTL
 */
struct drm_tegra_open_channel {
	/**
	 * @client:
	 *
	 * The client ID for this channel.
	 */
	__u32 client;

	/**
	 * @flags:
	 *
	 * A bitmask of flags that influence the channel creation. Currently
	 * no flags are defined, so this must be 0.
	 */
	__u32 flags;

	/**
	 * @syncpts:
	 *
	 * Return location for the number of syncpoints used by this channel.
	 */
	__u32 syncpts;

	/**
	 * @version:
	 *
	 * Return location for the implementation version of this channel.
	 */
	__u32 version;

	/**
	 * @context:
	 *
	 * Return location for the application context of this channel. This
	 * context needs to be passed to the DRM_TEGRA_CHANNEL_CLOSE or the
	 * DRM_TEGRA_SUBMIT IOCTLs.
	 */
	__u64 context;

	/**
	 * @reserved:
	 *
	 * This field is reserved for future use. Must be 0.
	 */
	__u64 reserved;
};

#define DRM_TEGRA_BUFFER_FLAGS	(0)

struct drm_tegra_buffer {
	/**
	 * @handle:
	 *
	 * Handle of the buffer.
	 */
	__u32 handle;

	/**
	 * @flags:
	 *
	 * A bitmask of flags specifying the usage of the buffer. Currently no
	 * flags are defined, so this must be 0.
	 */
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
	 * A bitmask of flags that specify this fence.
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

	/**
	 * @reserved:
	 *
	 * This field is reserved for future use. Must be 0.
	 */
	__u32 reserved[3];
};

#define DRM_TEGRA_CMDBUF_FLAGS	(0)

/**
 * struct drm_tegra_cmdbuf - structure describing a command buffer
 */
struct drm_tegra_cmdbuf {
	/**
	 * @index:
	 *
	 * Index into the job's buffer handle list, pointing to the handle of
	 * the GEM object that contains this command buffer.
	 */
	__u32 index;

	/**
	 * @offset:
	 *
	 * Offset, in bytes, into the GEM object at which the command buffer
	 * starts. Needs to be a multiple of 4.
	 */
	__u32 offset;

	/**
	 * @words:
	 *
	 * Number of 32-bit words in this command buffer.
	 */
	__u32 words;

	/**
	 * @flags:
	 *
	 * A bitmask of flags that influence the processing of this command
	 * buffer. Currently no flags are defined, so this must be 0.
	 */
	__u32 flags;

	/**
	 * @pad:
	 *
	 * Structure padding that may be used in the future. Must be 0.
	 */
	__u32 pad;

	/**
	 * @num_fences:
	 *
	 * The number of fences attached to this command buffer.
	 */
	__u32 num_fences;

	/**
	 * @fences:
	 *
	 * Pointer to an array of @num_fences &struct drm_tegra_fence objects.
	 */
	__u64 fences;
};

#define DRM_TEGRA_RELOC_FLAGS	(0)

/**
 * struct drm_tegra_reloc - GEM object relocation structure
 */
struct drm_tegra_reloc {
	struct {
		/**
		 * @cmdbuf.index:
		 *
		 * Index into the job's buffer handle list pointing to the
		 * handle of the GEM object containing the command buffer for
		 * which to perform this GEM object relocation.
		 */
		__u32 index;

		/**
		 * @cmdbuf.offset:
		 *
		 * Offset into the command buffer at which to insert the the
		 * relocated address.
		 */
		__u32 offset;
	} cmdbuf;

	struct {
		/**
		 * @target.index:
		 *
		 * Index into the job's buffer handle list pointing to the
		 * handle of the GEM object to be relocated.
		 */
		__u32 index;

		/**
		 * @target.offset:
		 *
		 * Offset into the target GEM object at which the relocated
		 * data starts.
		 */
		__u32 offset;
	} target;

	/**
	 * @shift:
	 *
	 * The number of bits by which to shift relocated addresses.
	 */
	__u32 shift;

	/**
	 * @flags:
	 *
	 * A bitmask of flags that determine how the GEM object should be
	 * relocated.
	 */
	__u32 flags;

	/**
	 * @reserved:
	 *
	 * This field is reserved for future use. Must be 0.
	 */
	__u64 reserved;
};

#define DRM_TEGRA_SUBMIT_FLAGS	(0)

/**
 * struct drm_tegra_submit - job submission structure
 */
struct drm_tegra_submit {
	/**
	 * @context:
	 *
	 * The application context identifying the channel to use for the
	 * execution of this job.
	 */
	__u64 context;

	/**
	 * @num_buffers:
	 *
	 * The number of GEM objects used during the execution of this job.
	 */
	__u32 num_buffers;

	/**
	 * @num_cmdbufs:
	 *
	 * The number of command buffers to execute as part of this job.
	 */
	__u32 num_cmdbufs;

	/**
	 * @num_relocs:
	 *
	 * The number of relocations to perform before executing this job.
	 */
	__u32 num_relocs;

	/**
	 * @timeout:
	 *
	 * The maximum amount of time, in milliseconds, to allow for the
	 * execution of this job.
	 */
	__u32 timeout;

	/**
	 * @buffers:
	 *
	 * A pointer to @num_buffers &struct drm_tegra_buffer structures that
	 * specify the GEM objects used during the execution of this job.
	 */
	__u64 buffers;

	/**
	 * @cmdbufs:
	 *
	 * A pointer to @num_cmdbufs &struct drm_tegra_cmdbuf structures that
	 * define the command buffers to execute as part of this job.
	 */
	__u64 cmdbufs;

	/**
	 * @relocs:
	 *
	 * A pointer to @num_relocs &struct drm_tegra_reloc structures that
	 * specify the relocations that need to be performed before executing
	 * this job.
	 */
	__u64 relocs;

	/**
	 * @flags:
	 *
	 * A bitmask of flags that specify how to execute this job. Currently
	 * no flags are defined, so this must be 0.
	 */
	__u32 flags;

	/**
	 * @pad:
	 *
	 * Structure padding that may be used in the future. Must be 0.
	 */
	__u32 pad;

	/**
	 * @reserved:
	 *
	 * This field is reserved for future use. Must be 0.
	 */
	__u64 reserved[9]; /* future expansion */
};

#define DRM_TEGRA_GEM_CREATE		0x00
#define DRM_TEGRA_GEM_MMAP		0x01
#define DRM_TEGRA_CLOSE_CHANNEL		0x06
#define DRM_TEGRA_OPEN_CHANNEL		0x0e
#define DRM_TEGRA_SUBMIT		0x0f

#define DRM_IOCTL_TEGRA_GEM_CREATE DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_GEM_CREATE, struct drm_tegra_gem_create)
#define DRM_IOCTL_TEGRA_GEM_MMAP DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_GEM_MMAP, struct drm_tegra_gem_mmap)
#define DRM_IOCTL_TEGRA_CLOSE_CHANNEL DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_CLOSE_CHANNEL, struct drm_tegra_close_channel)
#define DRM_IOCTL_TEGRA_OPEN_CHANNEL DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_OPEN_CHANNEL, struct drm_tegra_open_channel)
#define DRM_IOCTL_TEGRA_SUBMIT DRM_IOWR(DRM_COMMAND_BASE + DRM_TEGRA_SUBMIT, struct drm_tegra_submit)

#if defined(__cplusplus)
}
#endif

#endif
