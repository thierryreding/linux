// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 NVIDIA Corporation.
 */

#include <linux/memory-controller.h>
#include <linux/of.h>

static DEFINE_MUTEX(controllers_lock);
static LIST_HEAD(controllers);

static void memory_controller_release(struct kref *ref)
{
	struct memory_controller *mc = container_of(ref, struct memory_controller, ref);

	WARN_ON(!list_empty(&mc->list));
}

int memory_controller_register(struct memory_controller *mc)
{
	kref_init(&mc->ref);

	mutex_lock(&controllers_lock);
	list_add_tail(&mc->list, &controllers);
	mutex_unlock(&controllers_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(memory_controller_register);

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
			kref_get(&mc->ref);
			mutex_unlock(&controllers_lock);
			goto unlock;
		}
	}

	mc = ERR_PTR(-EPROBE_DEFER);

unlock:
	mutex_unlock(&controllers_lock);
	of_node_put(args.np);
	return mc;
}

struct memory_controller *
memory_controller_get(struct device *dev, const char *con_id)
{
	if (IS_ENABLED(CONFIG_OF) && dev && dev->of_node)
		return of_memory_controller_get(dev, dev->of_node, con_id);

	return ERR_PTR(-ENODEV);
}
EXPORT_SYMBOL_GPL(memory_controller_get);

void memory_controller_put(struct memory_controller *mc)
{
	if (mc)
		kref_put(&mc->ref, memory_controller_release);
}
EXPORT_SYMBOL_GPL(memory_controller_put);
