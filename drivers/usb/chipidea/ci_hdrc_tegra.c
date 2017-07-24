// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016, NVIDIA Corporation
 */

#define DEBUG

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/reset.h>

#include <linux/usb/chipidea.h>
#include <linux/usb/hcd.h>
#include <linux/usb/tegra_usb_phy.h>

#include "../host/ehci.h"

#include "ci.h"

#define PORT_WAKE_BITS (PORT_WKOC_E | PORT_WKDISC_E | PORT_WKCONN_E)

struct tegra_udc {
	struct ci_hdrc_platform_data data;
	struct platform_device *hdrc;
	struct device *dev;

	struct usb_phy *phy;
	struct clk *clk;
	struct reset_control *rst;

	bool port_resuming;
};

struct tegra_udc_soc_info {
	unsigned long flags;
};

static const struct tegra_udc_soc_info tegra20_udc_soc_info = {
	.flags = CI_HDRC_REQUIRES_ALIGNED_DMA,
};

static const struct tegra_udc_soc_info tegra30_udc_soc_info = {
	.flags = CI_HDRC_REQUIRES_ALIGNED_DMA,
};

static const struct tegra_udc_soc_info tegra114_udc_soc_info = {
	.flags = 0,
};

static const struct tegra_udc_soc_info tegra124_udc_soc_info = {
	.flags = 0,
};

static const struct of_device_id tegra_udc_of_match[] = {
	{
		.compatible = "nvidia,tegra20-udc",
		.data = &tegra20_udc_soc_info,
	}, {
		.compatible = "nvidia,tegra30-udc",
		.data = &tegra30_udc_soc_info,
	}, {
		.compatible = "nvidia,tegra114-udc",
		.data = &tegra114_udc_soc_info,
	}, {
		.compatible = "nvidia,tegra124-udc",
		.data = &tegra124_udc_soc_info,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, tegra_udc_of_match);

static int tegra_ehci_hub_control(struct usb_hcd *hcd, u16 typeReq, u16 wValue,
				  u16 wIndex, char *buf, u16 wLength)
{
	struct tegra_udc *udc = dev_get_drvdata(hcd->self.controller);
	struct device *dev = hcd->self.controller;
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	u32 __iomem *status_reg, value;
	unsigned long flags;
	int err = 0;

	dev_dbg(dev, "> %s(hcd=%p, typeReq=%x, wValue=%x, wIndex=%x, buf=%p, wLength=%u)\n", __func__, hcd, typeReq, wValue, wIndex, buf, wLength);

	status_reg = &ehci->regs->port_status[(wIndex & 0xff) - 1];
	tegra_usb_phy_preresume(hcd->usb_phy);

	spin_lock_irqsave(&ehci->lock, flags);

	if (typeReq == GetPortStatus) {
		dev_dbg(dev, "  GetPortStatus: %p\n", status_reg);

		value = ehci_readl(ehci, status_reg);
		dev_dbg(dev, "    %08x\n", value);
		if (udc->port_resuming && !(value & PORT_SUSPEND)) {
			/* Resume completed, re-enable disconnect detection */
			udc->port_resuming = 0;
			tegra_usb_phy_postresume(hcd->usb_phy);
		}
	}

	else if (typeReq == SetPortFeature && wValue == USB_PORT_FEAT_SUSPEND) {
		dev_dbg(dev, "SetPortFeature: SUSPEND\n");

		value = ehci_readl(ehci, status_reg);
		if ((value & PORT_PE) == 0 || (value & PORT_RESET) != 0) {
			err = -EPIPE;
			goto done;
		}

		value &= ~(PORT_RWC_BITS | PORT_WKCONN_E);
		value |= PORT_WKDISC_E | PORT_WKOC_E;
		ehci_writel(ehci, value | PORT_SUSPEND, status_reg);

		/*
		 * If a transaction is in progress, there may be a delay in
		 * suspending the port. Poll until the port is suspended.
		 */
		if (ehci_handshake(ehci, status_reg, PORT_SUSPEND,
						PORT_SUSPEND, 5000))
			pr_err("%s: timeout waiting for SUSPEND\n", __func__);

		set_bit((wIndex & 0xff) - 1, &ehci->suspended_ports);
		goto done;
	}

#if 0
	/* For USB1 port we need to issue Port Reset twice internally */
	if (udc->needs_double_reset &&
	   (typeReq == SetPortFeature && wValue == USB_PORT_FEAT_RESET)) {
		spin_unlock_irqrestore(&ehci->lock, flags);
		return tegra_ehci_internal_port_reset(ehci, status_reg);
	}
#endif

	/*
	 * Tegra host controller will time the resume operation to clear the bit
	 * when the port control state switches to HS or FS Idle. This behavior
	 * is different from EHCI where the host controller driver is required
	 * to set this bit to a zero after the resume duration is timed in the
	 * driver.
	 */
	else if (typeReq == ClearPortFeature &&
					wValue == USB_PORT_FEAT_SUSPEND) {
		dev_dbg(dev, "  ClearPortFeature: SUSPEND\n");

		value = ehci_readl(ehci, status_reg);
		if ((value & PORT_RESET) || !(value & PORT_PE)) {
			err = -EPIPE;
			goto done;
		}

		if (!(value & PORT_SUSPEND))
			goto done;

		/* Disable disconnect detection during port resume */
		tegra_usb_phy_preresume(hcd->usb_phy);

		ehci->reset_done[wIndex-1] = jiffies + msecs_to_jiffies(25);

		value &= ~(PORT_RWC_BITS | PORT_WAKE_BITS);
		/* start resume signalling */
		ehci_writel(ehci, value | PORT_RESUME, status_reg);
		set_bit(wIndex-1, &ehci->resuming_ports);

		spin_unlock_irqrestore(&ehci->lock, flags);
		msleep(20);
		spin_lock_irqsave(&ehci->lock, flags);

		/* Poll until the controller clears RESUME and SUSPEND */
		if (ehci_handshake(ehci, status_reg, PORT_RESUME, 0, 2000))
			pr_err("%s: timeout waiting for RESUME\n", __func__);
		if (ehci_handshake(ehci, status_reg, PORT_SUSPEND, 0, 2000))
			pr_err("%s: timeout waiting for SUSPEND\n", __func__);

		ehci->reset_done[wIndex-1] = 0;
		clear_bit(wIndex-1, &ehci->resuming_ports);

		udc->port_resuming = 1;
		goto done;
	}

	spin_unlock_irqrestore(&ehci->lock, flags);

	/* Handle the hub control events here */
	err = ehci_hub_control(hcd, typeReq, wValue, wIndex, buf, wLength);
	goto out;

done:
	spin_unlock_irqrestore(&ehci->lock, flags);
out:
	dev_dbg(dev, "< %s() = %d\n", __func__, err);
	return err;
}

/*
 * The 1st USB controller contains some UTMI pad registers that are global for
 * all the controllers on the chip. Those registers are also cleared when
 * reset is asserted to the 1st controller. This means that the 1st controller
 * can only be reset when no other controlled has finished probing. So we'll
 * reset the 1st controller before doing any other setup on any of the
 * controllers, and then never again.
 *
 * Since this is a PHY issue, the Tegra PHY driver should probably be doing
 * the resetting of the USB controllers. But to keep compatibility with old
 * device trees that don't have reset phandles in the PHYs, do it here.
 * Those old DTs will be vulnerable to total USB breakage if the 1st EHCI
 * device isn't the first one to finish probing, so warn them.
 */
static int tegra_reset_usb_controller(struct tegra_udc *udc)
{
	static bool usb1_reset_attempted = false;
	bool has_utmi_pad_registers = false;
	struct device_node *phy_np;

	phy_np = of_parse_phandle(udc->dev->of_node, "nvidia,phy", 0);
	if (!phy_np)
		return -ENOENT;

	if (of_property_read_bool(phy_np, "nvidia,has-utmi-pad-registers"))
		has_utmi_pad_registers = true;

	if (!usb1_reset_attempted) {
		struct reset_control *usb1_reset;

		if (!has_utmi_pad_registers)
			usb1_reset = of_reset_control_get(phy_np, "utmi-pads");
		else
			usb1_reset = udc->rst;

		if (IS_ERR(usb1_reset)) {
			dev_warn(udc->dev,
				 "can't get utmi-pads reset from the PHY\n");
			dev_warn(udc->dev,
				 "continuing, but please update your DT\n");
		} else {
			reset_control_assert(usb1_reset);
			udelay(1);
			reset_control_deassert(usb1_reset);

			if (!has_utmi_pad_registers)
				reset_control_put(usb1_reset);
		}

		usb1_reset_attempted = true;
	}

	if (!has_utmi_pad_registers) {
		reset_control_assert(udc->rst);
		udelay(1);
		reset_control_deassert(udc->rst);
	}

	of_node_put(phy_np);

	return 0;
}

static int tegra_udc_probe(struct platform_device *pdev)
{
	const struct tegra_udc_soc_info *soc;
	struct tegra_udc *udc;
	int err;

	udc = devm_kzalloc(&pdev->dev, sizeof(*udc), GFP_KERNEL);
	if (!udc)
		return -ENOMEM;

	udc->dev = &pdev->dev;

	soc = of_device_get_match_data(&pdev->dev);
	if (!soc) {
		dev_err(&pdev->dev, "failed to match OF data\n");
		return -EINVAL;
	}

	udc->phy = devm_usb_get_phy_by_phandle(&pdev->dev, "nvidia,phy", 0);
	if (IS_ERR(udc->phy)) {
		err = PTR_ERR(udc->phy);
		dev_err(&pdev->dev, "failed to get PHY: %d\n", err);
		return err;
	}

	udc->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(udc->clk)) {
		err = PTR_ERR(udc->clk);
		dev_err(&pdev->dev, "failed to get clock: %d\n", err);
		return err;
	}

	udc->rst = devm_reset_control_get(&pdev->dev, "usb");
	if (IS_ERR(udc->rst)) {
		err = PTR_ERR(udc->rst);
		dev_err(&pdev->dev, "failed to get reset: %d\n", err);
		return err;
	}

	err = clk_prepare_enable(udc->clk);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to enable clock: %d\n", err);
		return err;
	}

	err = tegra_reset_usb_controller(udc);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to reset controller: %d\n", err);
		goto disable_clock;
	}

	/*
	 * Tegra's USB PHY driver doesn't implement optional phy_init()
	 * hook, so we have to power on UDC controller before ChipIdea
	 * driver initialization kicks in.
	 */
	usb_phy_set_suspend(udc->phy, 0);

	/* setup and register ChipIdea HDRC device */
	udc->data.name = "tegra-udc";
	udc->data.flags = soc->flags;
	udc->data.usb_phy = udc->phy;
	udc->data.capoffset = DEF_CAPOFFSET;
	udc->data.hub_control = tegra_ehci_hub_control;

	udc->hdrc = ci_hdrc_add_device(&pdev->dev, pdev->resource,
				       pdev->num_resources, &udc->data);
	if (IS_ERR(udc->hdrc)) {
		err = PTR_ERR(udc->hdrc);
		dev_err(&pdev->dev, "failed to add HDRC device: %d\n", err);
		goto fail_power_off;
	}

	platform_set_drvdata(pdev, udc);

	return 0;

fail_power_off:
	usb_phy_set_suspend(udc->phy, 1);
disable_clock:
	clk_disable_unprepare(udc->clk);
	return err;
}

static int tegra_udc_remove(struct platform_device *pdev)
{
	struct tegra_udc *udc = platform_get_drvdata(pdev);

	usb_phy_set_suspend(udc->phy, 1);
	clk_disable_unprepare(udc->clk);

	return 0;
}

static struct platform_driver tegra_udc_driver = {
	.driver = {
		.name = "tegra-udc",
		.of_match_table = tegra_udc_of_match,
	},
	.probe = tegra_udc_probe,
	.remove = tegra_udc_remove,
};
module_platform_driver(tegra_udc_driver);

MODULE_DESCRIPTION("NVIDIA Tegra USB device mode driver");
MODULE_AUTHOR("Thierry Reding <treding@nvidia.com>");
MODULE_ALIAS("platform:tegra-udc");
MODULE_LICENSE("GPL v2");
