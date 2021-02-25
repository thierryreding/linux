// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
 */

#include <linux/console.h>
#include <linux/io.h>
#include <linux/serial_core.h>

#define NUM_BYTES_FIELD_BIT	24
#define FLUSH_BIT		26
#define INTR_TRIGGER_BIT	31

static u32 update_and_send_mbox(u8 __iomem *addr, u32 mbox_val, char c)
{
	int bytes = bytes = (mbox_val >> NUM_BYTES_FIELD_BIT) & 0x3;

	mbox_val |= BIT(INTR_TRIGGER_BIT);
	mbox_val |= c << (bytes * 8);
	bytes++;
	mbox_val = (mbox_val & ~(3 << NUM_BYTES_FIELD_BIT)) |
		(bytes << NUM_BYTES_FIELD_BIT);

	if (bytes == 3) {
		/* Send current packet to SPE */
		while (readl(addr) & BIT(INTR_TRIGGER_BIT))
			cpu_relax();
		writel(mbox_val, addr);
		mbox_val = BIT(INTR_TRIGGER_BIT);
	}

	return mbox_val;
}

/*
 * This function splits the string to be printed (const char *s) into multiple
 * packets. Each packet contains a max of 3 characters. Packets are sent to the
 * SPE-based combined UART server for printing. Communication with SPE is done
 * through mailbox registers which can generate interrupts for SPE.
 */
static void early_tcu_write(struct console *console, const char *s, unsigned int count)
{
	struct earlycon_device *device = console->data;
	u8 __iomem *addr = device->port.membase;
	u32 mbox_val = BIT(INTR_TRIGGER_BIT);
	unsigned int i;

	/* Loop for processing each 3 char packet */
	for (i = 0; i < count; i++) {
		if (s[i] == '\n')
			mbox_val = update_and_send_mbox(addr, mbox_val, '\r');
		mbox_val = update_and_send_mbox(addr, mbox_val, s[i]);
	}

	if ((mbox_val >> NUM_BYTES_FIELD_BIT) & 0x3) {
		while (readl(addr) & BIT(INTR_TRIGGER_BIT))
			cpu_relax();
		writel(mbox_val, addr);
	}
}

int __init early_tegra_combined_uart_setup(struct earlycon_device *device, const char *options)
{
	if (!(device->port.membase))
		return -ENODEV;

	device->con->write = early_tcu_write;

	return 0;
}

EARLYCON_DECLARE(tegra_comb_uart, early_tegra_combined_uart_setup);
