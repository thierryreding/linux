// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018, NVIDIA CORPORATION.  All rights reserved.
 */

#include <linux/console.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>

#define TCU_MBOX_BYTE(i, x)			((x) << (i*8))
#define TCU_MBOX_BYTE_V(x, i)			(((x) >> (i*8)) & 0xff)
#define TCU_MBOX_NUM_BYTES(x)			((x) << 24)
#define TCU_MBOX_NUM_BYTES_V(x)			(((x) >> 24) & 0x3)

static struct uart_driver tegra_tcu_uart_driver;
static struct uart_port tegra_tcu_uart_port;

struct tegra_tcu {
	struct mbox_client tx_client, rx_client;
	struct mbox_chan *tx, *rx;
};

static unsigned int tegra_tcu_uart_tx_empty(struct uart_port *port)
{
	return TIOCSER_TEMT;
}

static void tegra_tcu_uart_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static unsigned int tegra_tcu_uart_get_mctrl(struct uart_port *port)
{
	return 0;
}

static void tegra_tcu_uart_stop_tx(struct uart_port *port)
{
}

static void tegra_tcu_write(const char *s, unsigned int count)
{
	struct tegra_tcu *tcu = tegra_tcu_uart_port.private_data;
	unsigned int written = 0, i = 0;
	bool insert_nl = false;
	uint32_t value = 0;

	while (i < count) {
		if (insert_nl) {
			value |= TCU_MBOX_BYTE(written++, '\n');
			insert_nl = false;
			i++;
		} else if (s[i] == '\n') {
			value |= TCU_MBOX_BYTE(written++, '\r');
			insert_nl = true;
		} else {
			value |= TCU_MBOX_BYTE(written++, s[i++]);
		}

		if (written == 3) {
			value |= TCU_MBOX_NUM_BYTES(3);
			mbox_send_message(tcu->tx, &value);
			value = 0;
			written = 0;
		}
	}

	if (written) {
		value |= TCU_MBOX_NUM_BYTES(written);
		mbox_send_message(tcu->tx, &value);
	}
}

static void tegra_tcu_uart_start_tx(struct uart_port *port)
{
	struct circ_buf *xmit = &port->state->xmit;
	unsigned long count;

	for (;;) {
		count = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
		if (!count)
			break;

		tegra_tcu_write(&xmit->buf[xmit->tail], count);
		xmit->tail = (xmit->tail + count) & (UART_XMIT_SIZE - 1);
	}

	uart_write_wakeup(port);
}

static void tegra_tcu_uart_stop_rx(struct uart_port *port)
{
}

static void tegra_tcu_uart_break_ctl(struct uart_port *port, int ctl)
{
}

static int tegra_tcu_uart_startup(struct uart_port *port)
{
	return 0;
}

static void tegra_tcu_uart_shutdown(struct uart_port *port)
{
}

static void tegra_tcu_uart_set_termios(struct uart_port *port,
				       struct ktermios *new,
				       struct ktermios *old)
{
}

static const struct uart_ops tegra_tcu_uart_ops = {
	.tx_empty = tegra_tcu_uart_tx_empty,
	.set_mctrl = tegra_tcu_uart_set_mctrl,
	.get_mctrl = tegra_tcu_uart_get_mctrl,
	.stop_tx = tegra_tcu_uart_stop_tx,
	.start_tx = tegra_tcu_uart_start_tx,
	.stop_rx = tegra_tcu_uart_stop_rx,
	.break_ctl = tegra_tcu_uart_break_ctl,
	.startup = tegra_tcu_uart_startup,
	.shutdown = tegra_tcu_uart_shutdown,
	.set_termios = tegra_tcu_uart_set_termios,
};

static void tegra_tcu_console_write(struct console *cons, const char *s,
				    unsigned int count)
{
	tegra_tcu_write(s, count);
}

static int tegra_tcu_console_setup(struct console *cons, char *options)
{
	return 0;
}

static struct console tegra_tcu_console = {
	.name = "ttyTCU",
	.device = uart_console_device,
	.flags = CON_PRINTBUFFER | CON_ANYTIME,
	.index = -1,
	.write = tegra_tcu_console_write,
	.setup = tegra_tcu_console_setup,
	.data = &tegra_tcu_uart_driver,
};

static struct uart_driver tegra_tcu_uart_driver = {
	.owner = THIS_MODULE,
	.driver_name = "tegra-tcu",
	.dev_name = "ttyTCU",
	.cons = &tegra_tcu_console,
	.nr = 1,
};

static void tegra_tcu_receive(struct mbox_client *client, void *msg_p)
{
	struct tty_port *port = &tegra_tcu_uart_port.state->port;
	uint32_t msg = *(uint32_t *)msg_p;
	unsigned int num_bytes;
	int i;

	num_bytes = TCU_MBOX_NUM_BYTES_V(msg);
	for (i = 0; i < num_bytes; i++)
		tty_insert_flip_char(port, TCU_MBOX_BYTE_V(msg, i), TTY_NORMAL);

	tty_flip_buffer_push(port);
}

static int tegra_tcu_probe(struct platform_device *pdev)
{
	struct uart_port *port = &tegra_tcu_uart_port;
	struct tegra_tcu *tcu;
	int err;

	tcu = devm_kzalloc(&pdev->dev, sizeof(*tcu), GFP_KERNEL);
	if (!tcu)
		return -ENOMEM;

	tcu->tx_client.dev = &pdev->dev;
	tcu->rx_client.dev = &pdev->dev;
	tcu->rx_client.rx_callback = tegra_tcu_receive;

	tcu->tx = mbox_request_channel_byname(&tcu->tx_client, "tx");
	if (IS_ERR(tcu->tx)) {
		err = PTR_ERR(tcu->tx);
		dev_err(&pdev->dev, "failed to get tx mailbox: %d\n", err);
		return err;
	}

	tcu->rx = mbox_request_channel_byname(&tcu->rx_client, "rx");
	if (IS_ERR(tcu->rx)) {
		err = PTR_ERR(tcu->rx);
		dev_err(&pdev->dev, "failed to get rx mailbox: %d\n", err);
		goto free_tx;
	}

	err = uart_register_driver(&tegra_tcu_uart_driver);
	if (err) {
		dev_err(&pdev->dev, "failed to register UART driver: %d\n",
			err);
		goto free_rx;
	}

	spin_lock_init(&port->lock);
	port->dev = &pdev->dev;
	port->type = PORT_TEGRA_TCU;
	port->ops = &tegra_tcu_uart_ops;
	port->fifosize = 1;
	port->iotype = UPIO_MEM;
	port->flags = UPF_BOOT_AUTOCONF;
	port->private_data = tcu;

	err = uart_add_one_port(&tegra_tcu_uart_driver, port);
	if (err) {
		dev_err(&pdev->dev, "failed to add UART port: %d\n", err);
		goto unregister_uart;
	}

	platform_set_drvdata(pdev, tcu);

	return 0;

unregister_uart:
	uart_unregister_driver(&tegra_tcu_uart_driver);
free_rx:
	mbox_free_channel(tcu->rx);
free_tx:
	mbox_free_channel(tcu->tx);

	return err;
}

static int tegra_tcu_remove(struct platform_device *pdev)
{
	struct tegra_tcu *tcu = platform_get_drvdata(pdev);

	uart_remove_one_port(&tegra_tcu_uart_driver, &tegra_tcu_uart_port);
	uart_unregister_driver(&tegra_tcu_uart_driver);
	mbox_free_channel(tcu->rx);
	mbox_free_channel(tcu->tx);

	return 0;
}

static const struct of_device_id tegra_tcu_match[] = {
	{ .compatible = "nvidia,tegra194-tcu" },
	{ }
};

static struct platform_driver tegra_tcu_driver = {
	.driver = {
		.name = "tegra-tcu",
		.of_match_table = tegra_tcu_match,
	},
	.probe = tegra_tcu_probe,
	.remove = tegra_tcu_remove,
};

static int __init tegra_tcu_init(void)
{
	int err;

	err = platform_driver_register(&tegra_tcu_driver);
	if (err)
		return err;

	register_console(&tegra_tcu_console);

	return 0;
}
module_init(tegra_tcu_init);

static void __exit tegra_tcu_exit(void)
{
	unregister_console(&tegra_tcu_console);
	platform_driver_unregister(&tegra_tcu_driver);
}
module_exit(tegra_tcu_exit);

MODULE_AUTHOR("Mikko Perttunen <mperttunen@nvidia.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("NVIDIA Tegra Combined UART driver");
