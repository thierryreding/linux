/*
 * Copyright (C) 2012 Avionic Design GmbH
 * Copyright (C) 2012-2016 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/bitops.h>
#include <linux/host1x.h>
#include <linux/idr.h>
#include <linux/iommu.h>
#include <linux/sync_file.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_syncobj.h>

#if IS_ENABLED(CONFIG_ARM_DMA_USE_IOMMU)
#include <asm/dma-iommu.h>
#endif

#include "drm.h"
#include "gem.h"

#define DRIVER_NAME "tegra"
#define DRIVER_DESC "NVIDIA Tegra graphics"
#define DRIVER_DATE "20120330"
#define DRIVER_MAJOR 0
#define DRIVER_MINOR 0
#define DRIVER_PATCHLEVEL 0

#define CARVEOUT_SZ SZ_64M
#define CDMA_GATHER_FETCHES_MAX_NB 16383

struct tegra_drm_file {
	struct idr contexts;
	struct mutex lock;
};

static int tegra_atomic_check(struct drm_device *drm,
			      struct drm_atomic_state *state)
{
	int err;

	err = drm_atomic_helper_check(drm, state);
	if (err < 0)
		return err;

	return tegra_display_hub_atomic_check(drm, state);
}

static const struct drm_mode_config_funcs tegra_drm_mode_config_funcs = {
	.fb_create = tegra_fb_create,
#ifdef CONFIG_DRM_FBDEV_EMULATION
	.output_poll_changed = drm_fb_helper_output_poll_changed,
#endif
	.atomic_check = tegra_atomic_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static void tegra_atomic_commit_tail(struct drm_atomic_state *old_state)
{
	struct drm_device *drm = old_state->dev;
	struct tegra_drm *tegra = drm->dev_private;

	if (tegra->hub) {
		drm_atomic_helper_commit_modeset_disables(drm, old_state);
		tegra_display_hub_atomic_commit(drm, old_state);
		drm_atomic_helper_commit_planes(drm, old_state, 0);
		drm_atomic_helper_commit_modeset_enables(drm, old_state);
		drm_atomic_helper_commit_hw_done(old_state);
		drm_atomic_helper_wait_for_vblanks(drm, old_state);
		drm_atomic_helper_cleanup_planes(drm, old_state);
	} else {
		drm_atomic_helper_commit_tail_rpm(old_state);
	}
}

static const struct drm_mode_config_helper_funcs
tegra_drm_mode_config_helpers = {
	.atomic_commit_tail = tegra_atomic_commit_tail,
};

static int tegra_drm_load(struct drm_device *drm, unsigned long flags)
{
	struct host1x_device *device = to_host1x_device(drm->dev);
	struct tegra_drm *tegra;
	int err;

	tegra = kzalloc(sizeof(*tegra), GFP_KERNEL);
	if (!tegra)
		return -ENOMEM;

	if (iommu_present(&platform_bus_type)) {
		tegra->domain = iommu_domain_alloc(&platform_bus_type);
		if (!tegra->domain) {
			err = -ENOMEM;
			goto free;
		}

		err = iova_cache_get();
		if (err < 0)
			goto domain;
	}

	mutex_init(&tegra->clients_lock);
	INIT_LIST_HEAD(&tegra->clients);

	drm->dev_private = tegra;
	tegra->drm = drm;

	drm_mode_config_init(drm);

	drm->mode_config.min_width = 0;
	drm->mode_config.min_height = 0;

	drm->mode_config.max_width = 4096;
	drm->mode_config.max_height = 4096;

	drm->mode_config.allow_fb_modifiers = true;

	drm->mode_config.normalize_zpos = true;

	drm->mode_config.funcs = &tegra_drm_mode_config_funcs;
	drm->mode_config.helper_private = &tegra_drm_mode_config_helpers;

	err = tegra_drm_fb_prepare(drm);
	if (err < 0)
		goto config;

	drm_kms_helper_poll_init(drm);

	err = host1x_device_init(device);
	if (err < 0)
		goto fbdev;

	if (tegra->domain) {
		u64 carveout_start, carveout_end, gem_start, gem_end;
		u64 dma_mask = dma_get_mask(&device->dev);
		dma_addr_t start, end;
		unsigned long order;

		start = tegra->domain->geometry.aperture_start & dma_mask;
		end = tegra->domain->geometry.aperture_end & dma_mask;

		gem_start = start;
		gem_end = end - CARVEOUT_SZ;
		carveout_start = gem_end + 1;
		carveout_end = end;

		order = __ffs(tegra->domain->pgsize_bitmap);
		init_iova_domain(&tegra->carveout.domain, 1UL << order,
				 carveout_start >> order);

		tegra->carveout.shift = iova_shift(&tegra->carveout.domain);
		tegra->carveout.limit = carveout_end >> tegra->carveout.shift;

		drm_mm_init(&tegra->mm, gem_start, gem_end - gem_start + 1);
		mutex_init(&tegra->mm_lock);

		DRM_DEBUG("IOMMU apertures:\n");
		DRM_DEBUG("  GEM: %#llx-%#llx\n", gem_start, gem_end);
		DRM_DEBUG("  Carveout: %#llx-%#llx\n", carveout_start,
			  carveout_end);
	}

	if (tegra->hub) {
		err = tegra_display_hub_prepare(tegra->hub);
		if (err < 0)
			goto device;
	}

	/*
	 * We don't use the drm_irq_install() helpers provided by the DRM
	 * core, so we need to set this manually in order to allow the
	 * DRM_IOCTL_WAIT_VBLANK to operate correctly.
	 */
	drm->irq_enabled = true;

	/* syncpoints are used for full 32-bit hardware VBLANK counters */
	drm->max_vblank_count = 0xffffffff;

	err = drm_vblank_init(drm, drm->mode_config.num_crtc);
	if (err < 0)
		goto hub;

	drm_mode_config_reset(drm);

	err = tegra_drm_fb_init(drm);
	if (err < 0)
		goto hub;

	return 0;

hub:
	if (tegra->hub)
		tegra_display_hub_cleanup(tegra->hub);
device:
	host1x_device_exit(device);
fbdev:
	drm_kms_helper_poll_fini(drm);
	tegra_drm_fb_free(drm);
config:
	drm_mode_config_cleanup(drm);

	if (tegra->domain) {
		mutex_destroy(&tegra->mm_lock);
		drm_mm_takedown(&tegra->mm);
		put_iova_domain(&tegra->carveout.domain);
		iova_cache_put();
	}
domain:
	if (tegra->domain)
		iommu_domain_free(tegra->domain);
free:
	kfree(tegra);
	return err;
}

static void tegra_drm_unload(struct drm_device *drm)
{
	struct host1x_device *device = to_host1x_device(drm->dev);
	struct tegra_drm *tegra = drm->dev_private;
	int err;

	drm_kms_helper_poll_fini(drm);
	tegra_drm_fb_exit(drm);
	drm_atomic_helper_shutdown(drm);
	drm_mode_config_cleanup(drm);

	err = host1x_device_exit(device);
	if (err < 0)
		return;

	if (tegra->domain) {
		mutex_destroy(&tegra->mm_lock);
		drm_mm_takedown(&tegra->mm);
		put_iova_domain(&tegra->carveout.domain);
		iova_cache_put();
		iommu_domain_free(tegra->domain);
	}

	kfree(tegra);
}

static int tegra_drm_open(struct drm_device *drm, struct drm_file *filp)
{
	struct tegra_drm_file *fpriv;

	fpriv = kzalloc(sizeof(*fpriv), GFP_KERNEL);
	if (!fpriv)
		return -ENOMEM;

	idr_init(&fpriv->contexts);
	mutex_init(&fpriv->lock);
	filp->driver_priv = fpriv;

	return 0;
}

static void tegra_drm_context_free(struct tegra_drm_context *context)
{
	context->client->ops->close_channel(context);
	kfree(context);
}

static struct host1x_bo *
host1x_bo_lookup(struct drm_file *file, u32 handle)
{
	struct drm_gem_object *gem;
	struct tegra_bo *bo;

	gem = drm_gem_object_lookup(file, handle);
	if (!gem)
		return NULL;

	bo = to_tegra_bo(gem);
	return &bo->base;
}

static int host1x_reloc_copy_from_user(struct host1x_job *job,
				       struct host1x_reloc *dest,
				       struct drm_tegra_reloc __user *src)
{
	u32 cmdbuf, target;
	int err;

	err = get_user(cmdbuf, &src->cmdbuf.index);
	if (err < 0)
		return err;

	err = get_user(target, &src->target.index);
	if (err < 0)
		return err;

	if (cmdbuf >= job->num_buffers || target >= job->num_buffers)
		return -EINVAL;

	dest->cmdbuf.bo = job->buffers[cmdbuf];
	dest->target.bo = job->buffers[target];

	err = get_user(dest->cmdbuf.offset, &src->cmdbuf.offset);
	if (err < 0)
		return err;

	err = get_user(dest->target.offset, &src->target.offset);
	if (err < 0)
		return err;

	err = get_user(dest->shift, &src->shift);
	if (err < 0)
		return err;

	return 0;
}

static int host1x_job_get_buffers(struct host1x_job *job,
				  struct drm_file *file,
				  struct drm_tegra_buffer __user *buffers,
				  unsigned int count)
{
	struct drm_tegra_buffer buffer;
	unsigned int i;
	int err;

	for (i = 0; i < count; i++) {
		if (copy_from_user(&buffer, &buffers[i], sizeof(buffer))) {
			err = -EFAULT;
			goto put;
		}

		job->buffers[i] = host1x_bo_lookup(file, buffer.handle);
		if (!job->buffers[i]) {
			err = -ENOENT;
			goto put;
		}
	}

	return 0;

put:
	while (i--)
		host1x_bo_put(job->buffers[i]);

	return err;
}

/*
 * Obtain an existing fence to wait upon before submitting a new command
 * buffer.
 */
static struct dma_fence *tegra_drm_get_fence(struct drm_file *file,
					     struct drm_tegra_fence *fence)
{
	struct drm_syncobj *syncobj;
	struct dma_fence *in_fence;

	if (fence->flags & DRM_TEGRA_FENCE_FD)
		return sync_file_get_fence(fence->handle);

	syncobj = drm_syncobj_find(file, fence->handle);
	if (!syncobj)
		return NULL;

	in_fence = drm_syncobj_fence_get(syncobj);

	drm_syncobj_put(syncobj);
	return in_fence;
}

/*
 * Create a new fence to return to userspace.
 */
static struct dma_fence *tegra_drm_add_fence(struct drm_file *file,
					     struct host1x_syncpt *syncpt,
					     struct drm_tegra_fence *fence)
{
	struct dma_fence *f;
	int err, fd;

	f = host1x_fence_create(syncpt, fence->value);
	if (!f)
		return ERR_PTR(-ENOMEM);

	if (fence->flags & DRM_TEGRA_FENCE_FD) {
		struct sync_file *file;

		file = sync_file_create(f);
		if (!file) {
			err = -ENOMEM;
			goto put_fence;
		}

		fd = get_unused_fd_flags(O_CLOEXEC);
		if (fd < 0) {
			err = fd;
			goto put_fence;
		}

		fd_install(fd, file->file);
		fence->handle = fd;
	} else {
		struct drm_syncobj *syncobj;

		err = drm_syncobj_create(&syncobj, 0, f);
		if (err < 0)
			goto put_fence;

		err = drm_syncobj_get_handle(file, syncobj, &fence->handle);
		drm_syncobj_put(syncobj);
	}

	return f;

put_fence:
	dma_fence_put(f);

	return ERR_PTR(err);
}

static int host1x_job_get_fences(struct host1x_job *job, struct drm_file *file,
				 struct drm_tegra_cmdbuf *cmdbuf,
				 struct drm_tegra_fence *user_fences,
				 struct host1x_job_fence *fences,
				 unsigned int num_fences,
				 unsigned int *count)
{
	struct drm_tegra_fence __user *user;
	unsigned int i;
	size_t size;
	int err;

	if (cmdbuf->num_fences > num_fences)
		return -ENOSPC;

	size = cmdbuf->num_fences * sizeof(*user);
	user = u64_to_user_ptr(cmdbuf->fences);

	if (copy_from_user(user_fences, user, size))
		return -EFAULT;

	for (i = 0; i < cmdbuf->num_fences; i++) {
		struct drm_tegra_fence *fence = &user_fences[i];

		/*
		 * A fence can only be pre- or post-fence, never both at the
		 * same time.
		 */
		if ((fence->flags & DRM_TEGRA_FENCE_WAIT) &&
		    (fence->flags & DRM_TEGRA_FENCE_EMIT)) {
			err = -EINVAL;
			goto put;
		}

		if (fence->flags & DRM_TEGRA_FENCE_WAIT) {
			/*
			 * Patch offset, syncpoint index and value are not
			 * supported for pre-fences.
			 */
			if (fence->offset || fence->index || fence->value) {
				err = -EINVAL;
				goto put;
			}

			fences[i].fence = tegra_drm_get_fence(file, fence);
			if (!fences[i].fence) {
				err = -ENOENT;
				goto put;
			}

			fences[i].syncpt = NULL;
			fences[i].bo = NULL;
			fences[i].offset = 0;
			fences[i].value = 0;
		}

		if (fence->flags & DRM_TEGRA_FENCE_EMIT) {
			/* ensure that the syncpoint index is within range */
			if (fence->index >= job->client->num_syncpts) {
				err = -EINVAL;
				goto put;
			}

			if (fence->value != 1) {
				err = -EINVAL;
				goto put;
			}

			fences[i].syncpt = job->client->syncpts[fence->index];
			fences[i].bo = job->buffers[cmdbuf->index];
			fences[i].offset = fence->offset;
			fences[i].value = fence->value;
		}

		if (!fences[i].fence && !fences[i].syncpt) {
			/* ensure that the syncpoint index is within range */
			if (fence->index >= job->client->num_syncpts) {
				err = -EINVAL;
				goto put;
			}

			if (fence->value != 1) {
				err = -EINVAL;
				goto put;
			}

			fences[i].syncpt = job->client->syncpts[fence->index];
			fences[i].bo = job->buffers[cmdbuf->index];
			fences[i].offset = fence->offset;
			fences[i].value = fence->value;
		}
	}

	if (count)
		*count = cmdbuf->num_fences;

	return 0;

put:
	while (i--)
		dma_fence_put(fences[i].fence);

	return err;
}

static int host1x_job_put_fences(struct host1x_job *job, struct drm_file *file,
				 struct drm_tegra_cmdbuf *cmdbuf,
				 struct drm_tegra_fence *user_fences,
				 struct host1x_job_fence *fences,
				 unsigned int num_fences,
				 unsigned int *count)
{
	struct drm_tegra_fence __user *user;
	unsigned int i;
	size_t size;
	int err;

	if (cmdbuf->num_fences > num_fences)
		return -ENOSPC;

	for (i = 0; i < cmdbuf->num_fences; i++) {
		struct drm_tegra_fence *fence = &user_fences[i];
		struct host1x_syncpt *syncpt = fences[i].syncpt;

		if ((fence->flags & DRM_TEGRA_FENCE_EMIT) == 0)
			continue;

		/* XXX don't leak this to userspace? */
		fence->value = fences[i].value;

		fences[i].fence = tegra_drm_add_fence(file, syncpt, fence);
		if (IS_ERR(fences[i].fence)) {
			err = PTR_ERR(fences[i].fence);
			goto put;
		}
	}

	size = cmdbuf->num_fences * sizeof(*user);
	user = u64_to_user_ptr(cmdbuf->fences);

	if (copy_to_user(user, user_fences, size)) {
		err = -EFAULT;
		goto put;
	}

	if (count)
		*count = cmdbuf->num_fences;

	return 0;

put:
	while (i--)
		dma_fence_put(fences[i].fence);

	return err;
}

int tegra_drm_submit(struct tegra_drm_context *context,
		     struct drm_tegra_submit *args, struct drm_device *drm,
		     struct drm_file *file)
{
	struct host1x_client *client = &context->client->base;
	unsigned int num_buffers = args->num_buffers;
	struct drm_tegra_buffer __user *user_buffers;
	unsigned int num_cmdbufs = args->num_cmdbufs;
	struct drm_tegra_cmdbuf __user *user_cmdbufs;
	struct drm_tegra_reloc __user *user_relocs;
	unsigned int num_relocs = args->num_relocs;
	struct drm_tegra_fence *user_fences, *out;
	struct host1x_job_fence *fences;
	unsigned int num_fences = 0;
	struct host1x_job *job;
	unsigned int i;
	int err;

	user_buffers = u64_to_user_ptr(args->buffers);
	user_cmdbufs = u64_to_user_ptr(args->cmdbufs);
	user_relocs = u64_to_user_ptr(args->relocs);

	/* Check for unrecognized flags */
	if (args->flags & ~DRM_TEGRA_SUBMIT_FLAGS)
		return -EINVAL;

	/* count the number of fences */
	for (i = 0; i < num_cmdbufs; i++) {
		u32 count;

		err = get_user(count, &user_cmdbufs[i].num_fences);
		if (err < 0)
			return err;

		num_fences += count;
	}

	job = host1x_job_alloc(context->channel, num_buffers, num_cmdbufs,
			       num_relocs, client->num_syncpts, num_fences,
			       num_fences * sizeof(*user_fences),
			       (void **)&user_fences);
	if (!job)
		return -ENOMEM;

	job->client = client;
	job->class = client->class;
	job->serialize = true;

	/*
	 * XXX move this into host1x_job_alloc(), there should be no need for
	 * Tegra DRM to know about checkpoints.
	 */
	for (i = 0; i < client->num_syncpts; i++)
		job->checkpoints[i].syncpt = client->syncpts[i];

	err = host1x_job_get_buffers(job, file, user_buffers, num_buffers);
	if (err < 0)
		goto put;

	fences = job->fences;
	out = user_fences;

	for (i = 0; i < num_cmdbufs; i++) {
		struct drm_tegra_cmdbuf __user *user = &user_cmdbufs[i];
		struct drm_tegra_cmdbuf cmdbuf;
		struct drm_gem_object *obj;
		struct host1x_bo *bo;
		unsigned int count;
		u64 limit;

		if (copy_from_user(&cmdbuf, user, sizeof(cmdbuf))) {
			err = -EFAULT;
			goto put;
		}

		if (cmdbuf.index > job->num_buffers) {
			err = -EINVAL;
			goto put;
		}

		bo = job->buffers[cmdbuf.index];

		/*
		 * The maximum number of CDMA gather fetches is 16383, a
		 * higher value means the words count is malformed.
		 */
		if (cmdbuf.words > CDMA_GATHER_FETCHES_MAX_NB) {
			err = -EINVAL;
			goto put;
		}

		limit = (u64)cmdbuf.offset + (u64)cmdbuf.words * sizeof(u32);
		obj = &host1x_to_tegra_bo(bo)->gem;

		/*
		 * Gather buffer base address must be 4-bytes aligned,
		 * unaligned offset is malformed and cause commands stream
		 * corruption on the buffer address relocation.
		 */
		if (limit & 3 || limit >= obj->size) {
			err = -EINVAL;
			goto put;
		}

		err = host1x_job_get_fences(job, file, &cmdbuf, out, fences,
					    num_fences, &count);
		if (err < 0)
			goto put;

		host1x_job_add_gather(job, bo, cmdbuf.words, cmdbuf.offset,
				      fences, count);

		num_fences -= count;
		fences += count;
		out += count;
	}

	/*
	 * Need to reset this for bounds checking when copying fences back
	 * to userspace later on.
	 */
	num_fences = job->num_fences;
	fences = job->fences;
	out = user_fences;

	/* copy and resolve relocations from submit */
	for (i = 0; i < num_relocs; i++) {
		struct drm_tegra_reloc __user *user = &user_relocs[i];
		struct host1x_reloc *reloc = &job->relocs[i];
		struct tegra_bo *obj;

		err = host1x_reloc_copy_from_user(job, reloc, user);
		if (err < 0)
			goto put;

		obj = host1x_to_tegra_bo(reloc->cmdbuf.bo);

		/*
		 * The unaligned cmdbuf offset will cause an unaligned write
		 * during of the relocations patching, corrupting the commands
		 * stream.
		 */
		if (reloc->cmdbuf.offset & 3 ||
		    reloc->cmdbuf.offset >= obj->gem.size) {
			err = -EINVAL;
			goto put;
		}

		obj = host1x_to_tegra_bo(reloc->target.bo);

		if (reloc->target.offset >= obj->gem.size) {
			err = -EINVAL;
			goto put;
		}
	}

	job->is_addr_reg = context->client->ops->is_addr_reg;
	job->is_valid_class = context->client->ops->is_valid_class;
	job->timeout = 10000;

	if (args->timeout && args->timeout < 10000)
		job->timeout = args->timeout;

	err = host1x_job_pin(job, context->client->base.dev);
	if (err) {
		dev_dbg(client->dev, "failed to pin job: %d\n", err);
		goto put;
	}

	err = host1x_job_submit(job);
	if (err) {
		dev_dbg(client->dev, "failed to submit job: %d\n", err);
		host1x_job_unpin(job);
		goto put;
	}

	/* copy fences back to userspace */
	for (i = 0; i < num_cmdbufs; i++) {
		struct drm_tegra_cmdbuf __user *user = &user_cmdbufs[i];
		struct drm_tegra_cmdbuf cmdbuf;
		/*
		 * XXX gcc is unable to infer from host1x_job_put_fences()
		 * that this won't ever be uninitialized.
		 */
		unsigned int count = 0;

		if (copy_from_user(&cmdbuf, user, sizeof(cmdbuf))) {
			err = -EFAULT;
			goto put;
		}

		err = host1x_job_put_fences(job, file, &cmdbuf, out, fences,
					    num_fences, &count);
		if (err < 0) {
			dev_dbg(client->dev, "failed to put fences: %d\n", err);
			goto put;
		}

		num_fences -= count;
		fences += count;
		out += count;
	}

put:
	host1x_job_put(job);
	return err;
}

static int tegra_gem_create(struct drm_device *drm, void *data,
			    struct drm_file *file)
{
	struct drm_tegra_gem_create *args = data;
	struct tegra_bo *bo;

	bo = tegra_bo_create_with_handle(file, drm, args->size, args->flags,
					 &args->handle);
	if (IS_ERR(bo))
		return PTR_ERR(bo);

	return 0;
}

static int tegra_gem_mmap(struct drm_device *drm, void *data,
			  struct drm_file *file)
{
	struct drm_tegra_gem_mmap *args = data;
	struct drm_gem_object *gem;
	struct tegra_bo *bo;

	gem = drm_gem_object_lookup(file, args->handle);
	if (!gem)
		return -EINVAL;

	bo = to_tegra_bo(gem);

	args->offset = drm_vma_node_offset_addr(&bo->gem.vma_node);

	drm_gem_object_put_unlocked(gem);

	return 0;
}

static int tegra_client_open(struct tegra_drm_file *fpriv,
			     struct tegra_drm_client *client,
			     struct tegra_drm_context *context)
{
	int err;

	err = client->ops->open_channel(client, context);
	if (err < 0)
		return err;

	err = idr_alloc(&fpriv->contexts, context, 1, 0, GFP_KERNEL);
	if (err < 0) {
		client->ops->close_channel(context);
		return err;
	}

	context->client = client;
	context->id = err;

	return 0;
}

static int tegra_close_channel(struct drm_device *drm, void *data,
			       struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct drm_tegra_close_channel *args = data;
	struct tegra_drm_context *context;
	int err = 0;

	mutex_lock(&fpriv->lock);

	context = idr_find(&fpriv->contexts, args->context);
	if (!context) {
		err = -EINVAL;
		goto unlock;
	}

	idr_remove(&fpriv->contexts, context->id);
	tegra_drm_context_free(context);

unlock:
	mutex_unlock(&fpriv->lock);
	return err;
}

static int tegra_open_channel(struct drm_device *drm, void *data,
			      struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct tegra_drm *tegra = drm->dev_private;
	struct drm_tegra_open_channel *args = data;
	struct tegra_drm_context *context;
	struct tegra_drm_client *client;
	int err = -ENODEV;

	context = kzalloc(sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;

	mutex_lock(&fpriv->lock);

	list_for_each_entry(client, &tegra->clients, list)
		if (client->base.class == args->client) {
			err = tegra_client_open(fpriv, client, context);
			if (err < 0)
				break;

			args->syncpts = client->base.num_syncpts;
			args->version = client->version;
			args->context = context->id;
			break;
		}

	if (err < 0)
		kfree(context);

	mutex_unlock(&fpriv->lock);
	return err;
}

static int tegra_submit(struct drm_device *drm, void *data,
			struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;
	struct drm_tegra_submit *args = data;
	struct tegra_drm_context *context;
	int err;

	mutex_lock(&fpriv->lock);

	context = idr_find(&fpriv->contexts, args->context);
	if (!context) {
		err = -ENODEV;
		goto unlock;
	}

	err = context->client->ops->submit(context, args, drm, file);

unlock:
	mutex_unlock(&fpriv->lock);
	return err;
}

static const struct drm_ioctl_desc tegra_drm_ioctls[] = {
	DRM_IOCTL_DEF_DRV(TEGRA_GEM_CREATE, tegra_gem_create,
			  DRM_UNLOCKED | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_GEM_MMAP, tegra_gem_mmap,
			  DRM_UNLOCKED | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_CLOSE_CHANNEL, tegra_close_channel,
			  DRM_UNLOCKED | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_OPEN_CHANNEL, tegra_open_channel,
			  DRM_UNLOCKED | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(TEGRA_SUBMIT, tegra_submit,
			  DRM_UNLOCKED | DRM_RENDER_ALLOW),
};

static const struct file_operations tegra_drm_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = tegra_drm_mmap,
	.poll = drm_poll,
	.read = drm_read,
	.compat_ioctl = drm_compat_ioctl,
	.llseek = noop_llseek,
};

static int tegra_drm_context_cleanup(int id, void *p, void *data)
{
	struct tegra_drm_context *context = p;

	tegra_drm_context_free(context);

	return 0;
}

static void tegra_drm_postclose(struct drm_device *drm, struct drm_file *file)
{
	struct tegra_drm_file *fpriv = file->driver_priv;

	mutex_lock(&fpriv->lock);
	idr_for_each(&fpriv->contexts, tegra_drm_context_cleanup, NULL);
	mutex_unlock(&fpriv->lock);

	idr_destroy(&fpriv->contexts);
	mutex_destroy(&fpriv->lock);
	kfree(fpriv);
}

#ifdef CONFIG_DEBUG_FS
static int tegra_debugfs_framebuffers(struct seq_file *s, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)s->private;
	struct drm_device *drm = node->minor->dev;
	struct drm_framebuffer *fb;

	mutex_lock(&drm->mode_config.fb_lock);

	list_for_each_entry(fb, &drm->mode_config.fb_list, head) {
		seq_printf(s, "%3d: user size: %d x %d, depth %d, %d bpp, refcount %d\n",
			   fb->base.id, fb->width, fb->height,
			   fb->format->depth,
			   fb->format->cpp[0] * 8,
			   drm_framebuffer_read_refcount(fb));
	}

	mutex_unlock(&drm->mode_config.fb_lock);

	return 0;
}

static int tegra_debugfs_iova(struct seq_file *s, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)s->private;
	struct drm_device *drm = node->minor->dev;
	struct tegra_drm *tegra = drm->dev_private;
	struct drm_printer p = drm_seq_file_printer(s);

	if (tegra->domain) {
		mutex_lock(&tegra->mm_lock);
		drm_mm_print(&tegra->mm, &p);
		mutex_unlock(&tegra->mm_lock);
	}

	return 0;
}

static struct drm_info_list tegra_debugfs_list[] = {
	{ "framebuffers", tegra_debugfs_framebuffers, 0 },
	{ "iova", tegra_debugfs_iova, 0 },
};

static int tegra_debugfs_init(struct drm_minor *minor)
{
	return drm_debugfs_create_files(tegra_debugfs_list,
					ARRAY_SIZE(tegra_debugfs_list),
					minor->debugfs_root, minor);
}
#endif

static struct drm_driver tegra_drm_driver = {
	.driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_PRIME |
			   DRIVER_ATOMIC | DRIVER_RENDER | DRIVER_SYNCOBJ,
	.load = tegra_drm_load,
	.unload = tegra_drm_unload,
	.open = tegra_drm_open,
	.postclose = tegra_drm_postclose,
	.lastclose = drm_fb_helper_lastclose,

#if defined(CONFIG_DEBUG_FS)
	.debugfs_init = tegra_debugfs_init,
#endif

	.gem_free_object_unlocked = tegra_bo_free_object,
	.gem_vm_ops = &tegra_bo_vm_ops,

	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_export = tegra_gem_prime_export,
	.gem_prime_import = tegra_gem_prime_import,
	.gem_prime_res_obj = tegra_gem_prime_res_obj,

	.dumb_create = tegra_bo_dumb_create,

	.ioctls = tegra_drm_ioctls,
	.num_ioctls = ARRAY_SIZE(tegra_drm_ioctls),
	.fops = &tegra_drm_fops,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
};

int tegra_drm_register_client(struct tegra_drm *tegra,
			      struct tegra_drm_client *client)
{
	mutex_lock(&tegra->clients_lock);
	list_add_tail(&client->list, &tegra->clients);
	client->drm = tegra;
	mutex_unlock(&tegra->clients_lock);

	return 0;
}

int tegra_drm_unregister_client(struct tegra_drm *tegra,
				struct tegra_drm_client *client)
{
	mutex_lock(&tegra->clients_lock);
	list_del_init(&client->list);
	client->drm = NULL;
	mutex_unlock(&tegra->clients_lock);

	return 0;
}

struct iommu_group *host1x_client_iommu_attach(struct host1x_client *client,
					       bool shared)
{
	struct drm_device *drm = dev_get_drvdata(client->parent);
	struct tegra_drm *tegra = drm->dev_private;
	struct iommu_group *group = NULL;
	int err;

	if (tegra->domain) {
		group = iommu_group_get(client->dev);
		if (!group) {
			dev_err(client->dev, "failed to get IOMMU group\n");
			return ERR_PTR(-ENODEV);
		}

		if (!shared || (shared && (group != tegra->group))) {
#if IS_ENABLED(CONFIG_ARM_DMA_USE_IOMMU)
			if (client->dev->archdata.mapping) {
				struct dma_iommu_mapping *mapping =
					to_dma_iommu_mapping(client->dev);
				arm_iommu_detach_device(client->dev);
				arm_iommu_release_mapping(mapping);
			}
#endif
			err = iommu_attach_group(tegra->domain, group);
			if (err < 0) {
				iommu_group_put(group);
				return ERR_PTR(err);
			}

			if (shared && !tegra->group)
				tegra->group = group;
		}
	}

	return group;
}

void host1x_client_iommu_detach(struct host1x_client *client,
				struct iommu_group *group)
{
	struct drm_device *drm = dev_get_drvdata(client->parent);
	struct tegra_drm *tegra = drm->dev_private;

	if (group) {
		if (group == tegra->group) {
			iommu_detach_group(tegra->domain, group);
			tegra->group = NULL;
		}

		iommu_group_put(group);
	}
}

void *tegra_drm_alloc(struct tegra_drm *tegra, size_t size, dma_addr_t *dma)
{
	struct iova *alloc;
	void *virt;
	gfp_t gfp;
	int err;

	if (tegra->domain)
		size = iova_align(&tegra->carveout.domain, size);
	else
		size = PAGE_ALIGN(size);

	gfp = GFP_KERNEL | __GFP_ZERO;
	if (!tegra->domain) {
		/*
		 * Many units only support 32-bit addresses, even on 64-bit
		 * SoCs. If there is no IOMMU to translate into a 32-bit IO
		 * virtual address space, force allocations to be in the
		 * lower 32-bit range.
		 */
		gfp |= GFP_DMA;
	}

	virt = (void *)__get_free_pages(gfp, get_order(size));
	if (!virt)
		return ERR_PTR(-ENOMEM);

	if (!tegra->domain) {
		/*
		 * If IOMMU is disabled, devices address physical memory
		 * directly.
		 */
		*dma = virt_to_phys(virt);
		return virt;
	}

	alloc = alloc_iova(&tegra->carveout.domain,
			   size >> tegra->carveout.shift,
			   tegra->carveout.limit, true);
	if (!alloc) {
		err = -EBUSY;
		goto free_pages;
	}

	*dma = iova_dma_addr(&tegra->carveout.domain, alloc);
	err = iommu_map(tegra->domain, *dma, virt_to_phys(virt),
			size, IOMMU_READ | IOMMU_WRITE);
	if (err < 0)
		goto free_iova;

	return virt;

free_iova:
	__free_iova(&tegra->carveout.domain, alloc);
free_pages:
	free_pages((unsigned long)virt, get_order(size));

	return ERR_PTR(err);
}

void tegra_drm_free(struct tegra_drm *tegra, size_t size, void *virt,
		    dma_addr_t dma)
{
	if (tegra->domain)
		size = iova_align(&tegra->carveout.domain, size);
	else
		size = PAGE_ALIGN(size);

	if (tegra->domain) {
		iommu_unmap(tegra->domain, dma, size);
		free_iova(&tegra->carveout.domain,
			  iova_pfn(&tegra->carveout.domain, dma));
	}

	free_pages((unsigned long)virt, get_order(size));
}

static int host1x_drm_probe(struct host1x_device *dev)
{
	struct drm_driver *driver = &tegra_drm_driver;
	struct drm_device *drm;
	int err;

	drm = drm_dev_alloc(driver, &dev->dev);
	if (IS_ERR(drm))
		return PTR_ERR(drm);

	dev_set_drvdata(&dev->dev, drm);

	err = drm_fb_helper_remove_conflicting_framebuffers(NULL, "tegradrmfb", false);
	if (err < 0)
		goto put;

	err = drm_dev_register(drm, 0);
	if (err < 0)
		goto put;

	return 0;

put:
	drm_dev_put(drm);
	return err;
}

static int host1x_drm_remove(struct host1x_device *dev)
{
	struct drm_device *drm = dev_get_drvdata(&dev->dev);

	drm_dev_unregister(drm);
	drm_dev_put(drm);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int host1x_drm_suspend(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	return drm_mode_config_helper_suspend(drm);
}

static int host1x_drm_resume(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	return drm_mode_config_helper_resume(drm);
}
#endif

static SIMPLE_DEV_PM_OPS(host1x_drm_pm_ops, host1x_drm_suspend,
			 host1x_drm_resume);

static const struct of_device_id host1x_drm_subdevs[] = {
	{ .compatible = "nvidia,tegra20-dc", },
	{ .compatible = "nvidia,tegra20-hdmi", },
	{ .compatible = "nvidia,tegra20-gr2d", },
	{ .compatible = "nvidia,tegra20-gr3d", },
	{ .compatible = "nvidia,tegra30-dc", },
	{ .compatible = "nvidia,tegra30-hdmi", },
	{ .compatible = "nvidia,tegra30-gr2d", },
	{ .compatible = "nvidia,tegra30-gr3d", },
	{ .compatible = "nvidia,tegra114-dsi", },
	{ .compatible = "nvidia,tegra114-hdmi", },
	{ .compatible = "nvidia,tegra114-gr3d", },
	{ .compatible = "nvidia,tegra124-dc", },
	{ .compatible = "nvidia,tegra124-sor", },
	{ .compatible = "nvidia,tegra124-hdmi", },
	{ .compatible = "nvidia,tegra124-dsi", },
	{ .compatible = "nvidia,tegra124-vic", },
	{ .compatible = "nvidia,tegra132-dsi", },
	{ .compatible = "nvidia,tegra210-dc", },
	{ .compatible = "nvidia,tegra210-dsi", },
	{ .compatible = "nvidia,tegra210-sor", },
	{ .compatible = "nvidia,tegra210-sor1", },
	{ .compatible = "nvidia,tegra210-vic", },
	{ .compatible = "nvidia,tegra186-display", },
	{ .compatible = "nvidia,tegra186-dc", },
	{ .compatible = "nvidia,tegra186-sor", },
	{ .compatible = "nvidia,tegra186-sor1", },
	{ .compatible = "nvidia,tegra186-vic", },
	{ .compatible = "nvidia,tegra194-display", },
	{ .compatible = "nvidia,tegra194-dc", },
	{ .compatible = "nvidia,tegra194-sor", },
	{ .compatible = "nvidia,tegra194-vic", },
	{ /* sentinel */ }
};

static struct host1x_driver host1x_drm_driver = {
	.driver = {
		.name = "drm",
		.pm = &host1x_drm_pm_ops,
	},
	.probe = host1x_drm_probe,
	.remove = host1x_drm_remove,
	.subdevs = host1x_drm_subdevs,
};

static struct platform_driver * const drivers[] = {
	&tegra_display_hub_driver,
	&tegra_dc_driver,
	&tegra_hdmi_driver,
	&tegra_dsi_driver,
	&tegra_dpaux_driver,
	&tegra_sor_driver,
	&tegra_gr2d_driver,
	&tegra_gr3d_driver,
	&tegra_vic_driver,
};

static int __init host1x_drm_init(void)
{
	int err;

	err = host1x_driver_register(&host1x_drm_driver);
	if (err < 0)
		return err;

	err = platform_register_drivers(drivers, ARRAY_SIZE(drivers));
	if (err < 0)
		goto unregister_host1x;

	return 0;

unregister_host1x:
	host1x_driver_unregister(&host1x_drm_driver);
	return err;
}
module_init(host1x_drm_init);

static void __exit host1x_drm_exit(void)
{
	platform_unregister_drivers(drivers, ARRAY_SIZE(drivers));
	host1x_driver_unregister(&host1x_drm_driver);
}
module_exit(host1x_drm_exit);

MODULE_AUTHOR("Thierry Reding <thierry.reding@avionic-design.de>");
MODULE_DESCRIPTION("NVIDIA Tegra DRM driver");
MODULE_LICENSE("GPL v2");
