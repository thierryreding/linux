/*
 * Copyright (C) 2014, NVIDIA Corporation.  All rights reserved.
 *
 * This file is released under the GPL v2.
 */

#ifndef __LINUX_REGISTRY_H
#define __LINUX_REGISTRY_H

#include <linux/kref.h>
#include <linux/list.h>
#include <linux/mutex.h>

struct device;
struct device_node;
struct module;

struct registry;

/**
 * struct registry_record - registry record object
 * @list: entry in registry for this record
 * @owner: owner module
 * @kref: reference count
 * @dev: parent device
 * @release: callback to destroy a record when no reference are left
 */
struct registry_record {
	struct list_head list;
	struct module *owner;
	struct kref kref;
	struct device *dev;

	void (*release)(struct registry_record *record);
};

void registry_record_init(struct registry_record *record);
struct registry_record *registry_record_ref(struct registry_record *record);
void registry_record_unref(struct registry_record *record);

/**
 * struct registry - generic object registry
 * @list: list head of objects
 * @owner: owner module
 * @lock: lock for object list
 */
struct registry {
	struct list_head list;
	struct module *owner;
	struct mutex lock;
};

int registry_add(struct registry *registry, struct registry_record *record);
void registry_remove(struct registry *registry,
		     struct registry_record *record);

#ifdef CONFIG_OF
struct registry_record *registry_find_by_of_node(struct registry *registry,
						 struct device_node *np);
#endif

#endif
