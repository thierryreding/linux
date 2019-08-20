// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 NVIDIA Corporation.
 */

#include <linux/memory-controller.h>
#include <linux/module.h>
#include <linux/of.h>

static DEFINE_MUTEX(controllers_lock);
static LIST_HEAD(controllers);

static void memory_controller_release(struct kref *ref)
{
	struct memory_controller *mc = container_of(ref, struct memory_controller, ref);

	WARN_ON(!list_empty(&mc->list));
}

/**
 * memory_controller_register() - register a memory controller
 * @mc: memory controller
 */
int memory_controller_register(struct memory_controller *mc)
{
	kref_init(&mc->ref);

	mutex_lock(&controllers_lock);
	list_add_tail(&mc->list, &controllers);
	mutex_unlock(&controllers_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(memory_controller_register);

/**
 * memory_controller_unregister() - unregister a memory controller
 * @mc: memory controller
 */
void memory_controller_unregister(struct memory_controller *mc)
{
	mutex_lock(&controllers_lock);
	list_del_init(&mc->list);
	mutex_unlock(&controllers_lock);

	kref_put(&mc->ref, memory_controller_release);
}
EXPORT_SYMBOL_GPL(memory_controller_unregister);

static struct memory_controller *
of_memory_controller_get(struct device *dev, struct device_node *np,
			 const char *con_id)
{
	const char *cells = "#memory-controller-cells";
	const char *names = "memory-controller-names";
	const char *prop = "memory-controllers";
	struct memory_controller *mc;
	struct of_phandle_args args;
	int index = 0, err;

	if (con_id) {
		index = of_property_match_string(np, names, con_id);
		if (index < 0)
			return ERR_PTR(index);
	}

	err = of_parse_phandle_with_args(np, prop, cells, index, &args);
	if (err) {
		if (err == -ENOENT)
			err = -ENODEV;

		return ERR_PTR(err);
	}

	mutex_lock(&controllers_lock);

	list_for_each_entry(mc, &controllers, list) {
		if (mc->dev && mc->dev->of_node == args.np) {
			__module_get(mc->dev->driver->owner);
			kref_get(&mc->ref);
			goto unlock;
		}
	}

	mc = ERR_PTR(-EPROBE_DEFER);

unlock:
	mutex_unlock(&controllers_lock);
	of_node_put(args.np);
	return mc;
}

/**
 * memory_controller_get() - obtain a reference to a memory controller
 * @dev: consumer device
 * @con_id: consumer name
 *
 * Returns: A pointer to the requested memory controller or an ERR_PTR()-
 * encoded error code on failure.
 */
struct memory_controller *
memory_controller_get(struct device *dev, const char *con_id)
{
	if (IS_ENABLED(CONFIG_OF) && dev && dev->of_node)
		return of_memory_controller_get(dev, dev->of_node, con_id);

	return ERR_PTR(-ENODEV);
}
EXPORT_SYMBOL_GPL(memory_controller_get);

/**
 * memory_controller_get_optional() - obtain a reference to an optional
 *                                    memory controller
 * @dev: consumer device
 * @con_id: consumer name
 *
 * Returns: A pointer to the requested memory controller, NULL if no memory
 * controller for the consumer device/name pair exists, or an ERR_PTR()-
 * encoded error code on failure.
 */
struct memory_controller *
memory_controller_get_optional(struct device *dev, const char *con_id)
{
	struct memory_controller *mc;

	mc = memory_controller_get(dev, con_id);
	if (IS_ERR(mc)) {
		if (mc == ERR_PTR(-ENODEV))
			return NULL;
	}

	return mc;
}
EXPORT_SYMBOL_GPL(memory_controller_get_optional);

/**
 * memory_controller_put() - release a reference to a memory controller
 * @mc: memory controller
 */
void memory_controller_put(struct memory_controller *mc)
{
	if (mc) {
		kref_put(&mc->ref, memory_controller_release);
		module_put(mc->dev->driver->owner);
	}
}
EXPORT_SYMBOL_GPL(memory_controller_put);

static void devm_memory_controller_release(struct device *dev, void *res)
{
	memory_controller_put(*(struct memory_controller **)res);
}

/**
 * devm_memory_controller_get() - obtain a reference to a memory controller
 * @dev: consumer device
 * @con_id: consumer name
 *
 * This is a device-managed variant of memory_controller_get(). The memory
 * controller reference obtained with this function is automatically released
 * when the device is unbound from its driver.
 *
 * Returns: A pointer to the requested memory controller or an ERR_PTR()-
 * encoded error code on failure.
 */
struct memory_controller *devm_memory_controller_get(struct device *dev,
						     const char *con_id)
{
	struct memory_controller **ptr, *mc;

	ptr = devres_alloc(devm_memory_controller_release, sizeof(*ptr),
			   GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	mc = memory_controller_get(dev, con_id);
	if (!IS_ERR(mc)) {
		*ptr = mc;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return mc;
}
EXPORT_SYMBOL_GPL(devm_memory_controller_get);

/**
 * memory_controller_get_optional() - obtain a reference to an optional
 *                                    memory controller
 * @dev: consumer device
 * @con_id: consumer name
 *
 * This is a device-managed variant of memory_controller_get_optional(). The
 * memory controller reference obtained with this function is automatically
 * released when the device is unbound from its driver.
 *
 * Returns: A pointer to the requested memory controller, NULL if no memory
 * controller for the consumer device/name pair exists, or an ERR_PTR()-
 * encoded error code on failure.
 */
struct memory_controller *
devm_memory_controller_get_optional(struct device *dev, const char *con_id)
{
	struct memory_controller **ptr, *mc;

	ptr = devres_alloc(devm_memory_controller_release, sizeof(*ptr),
			   GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	mc = memory_controller_get_optional(dev, con_id);
	if (!IS_ERR(mc)) {
		*ptr = mc;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return mc;
}
EXPORT_SYMBOL_GPL(devm_memory_controller_get_optional);

static int devm_memory_controller_match(struct device *dev, void *res, void *data)
{
	struct memory_controller **mc = res;

	if (WARN_ON(!mc || !*mc))
		return 0;

	return *mc == data;
}

/**
 * devm_memory_controller_put() - release a reference to a memory controller
 * @mc: memory controller
 *
 * This is a device-managed variant of memory_controller_put(). Typically it
 * should never be necessary to call this function, since the device-managed
 * code should take care of releasing the reference at the right time.
 */
void devm_memory_controller_put(struct device *dev,
				struct memory_controller *mc)
{
	WARN_ON(devres_release(dev, devm_memory_controller_release,
			       devm_memory_controller_match, mc));
}
EXPORT_SYMBOL_GPL(devm_memory_controller_put);
