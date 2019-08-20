// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 NVIDIA Corporation.
 */

#ifndef _LINUX_MEMORY_CONTROLLER_H
#define _LINUX_MEMORY_CONTROLLER_H

#include <linux/device.h>
#include <linux/list.h>

struct memory_controller {
	struct device *dev;
	struct kref ref;
	struct list_head list;
};

int memory_controller_register(struct memory_controller *mc);
void memory_controller_unregister(struct memory_controller *mc);

struct memory_controller *memory_controller_get(struct device *dev,
						const char *con_id);
struct memory_controller *memory_controller_get_optional(struct device *dev,
							 const char *con_id);
void memory_controller_put(struct memory_controller *mc);

struct memory_controller *devm_memory_controller_get(struct device *dev,
						     const char *con_id);
struct memory_controller *
devm_memory_controller_get_optional(struct device *dev, const char *con_id);
void devm_memory_controller_put(struct device *dev,
				struct memory_controller *mc);

#endif
