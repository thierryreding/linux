/*
 * Copyright (C) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <linux/err.h>
#include <linux/module.h>
#include <linux/registry.h>

#include <drm/drm_crtc.h>
#include <drm/drm_panel.h>

/**
 * DOC: drm panel
 *
 * The DRM panel helpers allow drivers to register panel objects with a
 * central registry and provide functions to retrieve those panels in display
 * drivers.
 */

/*
 * DRM panel registry
 */
static struct registry panels = {
	.lock = __MUTEX_INITIALIZER(panels.lock),
	.list = LIST_HEAD_INIT(panels.list),
	.owner = THIS_MODULE,
};

static inline struct drm_panel *to_drm_panel(struct registry_record *record)
{
	return container_of(record, struct drm_panel, record);
}

static void drm_panel_release(struct registry_record *record)
{
	struct drm_panel *panel = to_drm_panel(record);

	/*
	 * The .release() callback is optional because drivers may not need
	 * to manually release any resources (e.g. if they've used devm_*()
	 * helper functions).
	 */
	if (panel->funcs && panel->funcs->release)
		panel->funcs->release(panel);
}

/**
 * drm_panel_init - initialize a panel
 * @panel: DRM panel
 *
 * Sets up internal fields of the panel so that it can subsequently be added
 * to the registry.
 */
void drm_panel_init(struct drm_panel *panel)
{
	registry_record_init(&panel->record);
	panel->record.release = drm_panel_release;
}
EXPORT_SYMBOL(drm_panel_init);

/**
 * drm_panel_ref - acquire a reference to a panel
 * @panel: DRM panel
 *
 * Increases the reference on a panel and returns a pointer to it.
 *
 * Return: A reference to the panel on success or NULL on failure.
 */
struct drm_panel *drm_panel_ref(struct drm_panel *panel)
{
	if (panel) {
		struct registry_record *record;

		record = registry_record_ref(&panel->record);
		if (!record)
			panel = NULL;
	}

	return panel;
}
EXPORT_SYMBOL(drm_panel_ref);

/**
 * drm_panel_unref - release a reference to a panel
 * @panel: DRM panel
 *
 * Decreases the reference count on a panel. If the reference count reaches 0
 * the panel is destroyed.
 */
void drm_panel_unref(struct drm_panel *panel)
{
	if (panel)
		registry_record_unref(&panel->record);
}
EXPORT_SYMBOL(drm_panel_unref);

/**
 * drm_panel_add - add a panel to the global registry
 * @panel: panel to add
 *
 * Add a panel to the global registry so that it can be looked up by display
 * drivers.
 *
 * Return: 0 on success or a negative error code on failure.
 */
int drm_panel_add(struct drm_panel *panel)
{
	panel->record.owner = panel->dev->driver->owner;
	panel->record.dev = panel->dev;

	return registry_add(&panels, &panel->record);
}
EXPORT_SYMBOL(drm_panel_add);

/**
 * drm_panel_remove - remove a panel from the global registry
 * @panel: DRM panel
 *
 * Removes a panel from the global registry. References to the object can
 * still exist, but drivers won't be able to look the panel up again.
 */
void drm_panel_remove(struct drm_panel *panel)
{
	registry_remove(&panels, &panel->record);
}
EXPORT_SYMBOL(drm_panel_remove);

/**
 * drm_panel_attach - attach a panel to a connector
 * @panel: DRM panel
 * @connector: DRM connector
 *
 * After obtaining a pointer to a DRM panel a display driver calls this
 * function to attach a panel to a connector.
 *
 * An error is returned if the panel is already attached to another connector.
 *
 * Return: 0 on success or a negative error code on failure.
 */
int drm_panel_attach(struct drm_panel *panel, struct drm_connector *connector)
{
	if (panel->connector)
		return -EBUSY;

	panel->connector = connector;
	panel->drm = connector->dev;

	return 0;
}
EXPORT_SYMBOL(drm_panel_attach);

/**
 * drm_panel_detach - detach a panel from a connector
 * @panel: DRM panel
 *
 * Detaches a panel from the connector it is attached to. If a panel is not
 * attached to any connector this is effectively a no-op.
 *
 * Return: 0 on success or a negative error code on failure.
 */
int drm_panel_detach(struct drm_panel *panel)
{
	panel->connector = NULL;
	panel->drm = NULL;

	return 0;
}
EXPORT_SYMBOL(drm_panel_detach);

#ifdef CONFIG_OF
/**
 * of_drm_find_panel - look up a panel using a device tree node
 * @np: device tree node of the panel
 *
 * Searches the set of registered panels for one that matches the given device
 * tree node. If a matching panel is found, return a reference to it.
 *
 * Return: A pointer to the panel registered for the specified device tree
 * node or NULL if no panel matching the device tree node can be found.
 */
struct drm_panel *of_drm_find_panel(struct device_node *np)
{
	struct registry_record *record;

	record = registry_find_by_of_node(&panels, np);
	if (record)
		return container_of(record, struct drm_panel, record);

	return NULL;
}
EXPORT_SYMBOL(of_drm_find_panel);
#endif

MODULE_AUTHOR("Thierry Reding <treding@nvidia.com>");
MODULE_DESCRIPTION("DRM panel infrastructure");
MODULE_LICENSE("GPL and additional rights");
