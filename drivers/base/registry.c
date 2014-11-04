/*
 * Copyright (C) 2014, NVIDIA Corporation.  All rights reserved.
 *
 * This file is released under the GPL v2.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/registry.h>

static inline struct registry_record *to_registry_record(struct kref *kref)
{
	return container_of(kref, struct registry_record, kref);
}

static void registry_record_release(struct kref *kref)
{
	struct registry_record *record = to_registry_record(kref);

	/*
	 * Drivers must remove the device from the registry before dropping
	 * the last reference. Try to detect this by warning if a record's
	 * last reference goes away but it is still registered.
	 */
	if (WARN_ON(!list_empty(&record->list)))
		list_del_init(&record->list);

	record->release(record);
}

/**
 * registry_record_init - initialize a registry record
 * @record: record to initialize
 *
 * Sets up internal fields of the registry record so that it can subsequently
 * be added to a registry.
 */
void registry_record_init(struct registry_record *record)
{
	INIT_LIST_HEAD(&record->list);
	kref_init(&record->kref);
}
EXPORT_SYMBOL_GPL(registry_record_init);

/**
 * registry_record_ref - reference on the registry record
 * @record: record to reference
 *
 * Increases the reference count on the record and returns a pointer to it.
 *
 * Return: A pointer to the record on success or NULL on failure.
 */
struct registry_record *registry_record_ref(struct registry_record *record)
{
	if (record)
		kref_get(&record->kref);

	return record;
}
EXPORT_SYMBOL_GPL(registry_record_ref);

/**
 * registry_record_unref - drop a reference to a registry record
 * @record: record to unreference
 *
 * Decreases the reference count on the record. When the reference count
 * reaches zero the record will be destroyed.
 */
void registry_record_unref(struct registry_record *record)
{
	if (record)
		kref_put(&record->kref, registry_record_release);
}
EXPORT_SYMBOL_GPL(registry_record_unref);

/**
 * registry_add - add a record to a registry
 * @registry: registry to add the record to
 * @record: record to add
 *
 * Tries to increase the reference count of both the module owning the record
 * and the module owning the registry. If successful adds a reference to the
 * new record to the registry.
 *
 * Return: 0 on success or a negative error code on failure.
 */
int registry_add(struct registry *registry, struct registry_record *record)
{
	/*
	 * The registry keeps a reference to the record to make sure it does
	 * not get freed before it is properly removed.
	 */
	if (!registry_record_ref(record))
		return -ENODEV;

	/*
	 * As long as the registry keeps a reference to a record it must keep
	 * a reference to the module owning that record to ensure it doesn't
	 * disappear unexpectedly.
	 */
	if (!try_module_get(record->owner))
		goto unref;

	if (!try_module_get(registry->owner)) {
		module_put(record->owner);
		goto unref;
	}

	mutex_lock(&registry->lock);
	list_add_tail(&record->list, &registry->list);
	mutex_unlock(&registry->lock);

	return 0;

unref:
	registry_record_unref(record);
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(registry_add);

/**
 * registry_remove - remove a record from a registry
 * @registry: registry to remove the record from
 * @record: record to remove
 *
 * Decreases the reference count on the module owning the registry.
 */
void registry_remove(struct registry *registry,
		     struct registry_record *record)
{
	mutex_lock(&registry->lock);
	list_del_init(&record->list);
	mutex_unlock(&registry->lock);

	module_put(registry->owner);
	module_put(record->owner);
	registry_record_unref(record);
}
EXPORT_SYMBOL_GPL(registry_remove);

#ifdef CONFIG_OF
/**
 * registry_find_by_of_node - look up an object by device node in a registry
 * @registry: registry to search
 * @np: device node to match on
 *
 * Return: A pointer to the record matching @np or NULL if no such record was
 * found.
 */
struct registry_record *registry_find_by_of_node(struct registry *registry,
						 struct device_node *np)
{
	struct registry_record *record;

	mutex_lock(&registry->lock);

	list_for_each_entry(record, &registry->list, list)
		if (record->dev->of_node == np) {
			registry_record_ref(record);
			goto out;
		}

	record = NULL;

out:
	mutex_unlock(&registry->lock);
	return record;
}
EXPORT_SYMBOL_GPL(registry_find_by_of_node);
#endif
