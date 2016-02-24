/*
 * Tegra host1x Debug
 *
 * Copyright (c) 2011-2013 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __HOST1X_DEBUG_H
#define __HOST1X_DEBUG_H

#include <linux/debugfs.h>
#include <linux/seq_file.h>

struct host1x;

#define HOST1X_OUTPUT_CONT (1 << 0)

struct output {
	void (*fn)(struct output *output, const char *str, size_t len);
	unsigned long flags;
	void *ctx;
	char buf[256];
};

static inline void write_to_seqfile(struct output *output, const char *str, size_t len)
{
	struct seq_file *s = output->ctx;

	seq_write(s, str, len);
}

static inline void write_to_printk(struct output *output, const char *str, size_t len)
{
	if (output->flags & HOST1X_OUTPUT_CONT)
		pr_cont("%s", str);
	else
		pr_info("%s", str);
}

void __printf(2, 3) host1x_debug_output(struct output *o, const char *fmt, ...);

extern unsigned int host1x_debug_trace_cmdbuf;

void host1x_debug_init(struct host1x *host1x);
void host1x_debug_deinit(struct host1x *host1x);
void host1x_debug_dump(struct host1x *host1x);
void host1x_debug_dump_syncpts(struct host1x *host1x);

#endif
