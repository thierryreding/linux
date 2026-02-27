// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (c) 2022-2025, NVIDIA CORPORATION. All rights reserved.
/*
 * PCIe host controller driver for Tegra264 SoC
 *
 * Author: Manikanta Maddireddy <mmaddireddy@nvidia.com>
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/interconnect.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_pci.h>
#include <linux/of_platform.h>
#include <linux/pci-ecam.h>
#include <linux/pci.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include <soc/tegra/bpmp.h>
#include <soc/tegra/bpmp-abi.h>
#include <soc/tegra/fuse.h>

#include "../pci.h"

#define PCIE_LINK_UP_DELAY	10000	/* 10 msec */
#define PCIE_LINK_UP_TIMEOUT	1000000	/* 1 s */

/* XAL registers */
#define XAL_RC_ECAM_BASE_HI			0x00
#define XAL_RC_ECAM_BASE_LO			0x04
#define XAL_RC_ECAM_BUSMASK			0x08
#define XAL_RC_IO_BASE_HI			0x0c
#define XAL_RC_IO_BASE_LO			0x10
#define XAL_RC_IO_LIMIT_HI			0x14
#define XAL_RC_IO_LIMIT_LO			0x18
#define XAL_RC_MEM_32BIT_BASE_HI		0x1c
#define XAL_RC_MEM_32BIT_BASE_LO		0x20
#define XAL_RC_MEM_32BIT_LIMIT_HI		0x24
#define XAL_RC_MEM_32BIT_LIMIT_LO		0x28
#define XAL_RC_MEM_64BIT_BASE_HI		0x2c
#define XAL_RC_MEM_64BIT_BASE_LO		0x30
#define XAL_RC_MEM_64BIT_LIMIT_HI		0x34
#define XAL_RC_MEM_64BIT_LIMIT_LO		0x38
#define XAL_RC_BAR_CNTL_STANDARD		0x40
#define XAL_RC_BAR_CNTL_STANDARD_IOBAR_EN	BIT(0)
#define XAL_RC_BAR_CNTL_STANDARD_32B_BAR_EN	BIT(1)
#define XAL_RC_BAR_CNTL_STANDARD_64B_BAR_EN	BIT(2)

/* XTL registers */
#define XTL_RC_PCIE_CFG_LINK_CONTROL_STATUS		0x58
#define XTL_RC_PCIE_CFG_LINK_CONTROL_STATUS_DLL_ACTIVE	BIT(29)

#define XTL_RC_PCIE_CFG_LINK_STATUS		0x5a

#define XTL_RC_MGMT_PERST_CONTROL		0x218
#define XTL_RC_MGMT_PERST_CONTROL_PERST_O_N	BIT(0)

#define XTL_RC_MGMT_CLOCK_CONTROL		0x47C
#define XTL_RC_MGMT_CLOCK_CONTROL_PEX_CLKREQ_I_N_PIN_USE_CONV_TO_PRSNT	BIT(9)

struct tegra264_pcie {
	struct device *dev;
	bool link_state;

	/* I/O memory */
	void __iomem *xal;
	void __iomem *xtl;
	void __iomem *ecam;

	/* bridge configuration */
	struct pci_config_window *cfg;
	struct pci_host_bridge *bridge;

	/* wake IRQ */
	struct gpio_desc *wake_gpio;
	unsigned int wake_irq;

	/* BPMP and bandwidth management */
	struct icc_path *icc_path;
	struct tegra_bpmp *bpmp;
	u32 ctl_id;
};

static int tegra264_pcie_parse_dt(struct tegra264_pcie *pcie)
{
	int ret;

	pcie->wake_gpio = devm_gpiod_get_optional(pcie->dev, "nvidia,pex-wake",
						  GPIOD_IN);
	if (IS_ERR(pcie->wake_gpio))
		return PTR_ERR(pcie->wake_gpio);

	if (pcie->wake_gpio) {
		device_init_wakeup(pcie->dev, true);

		ret = gpiod_to_irq(pcie->wake_gpio);
		if (ret < 0) {
			dev_err(pcie->dev, "failed to get wake IRQ: %d\n", ret);
			return ret;
		}

		pcie->wake_irq = (unsigned int)ret;
	}

	return 0;
}

static void tegra264_pcie_bpmp_set_rp_state(struct tegra264_pcie *pcie)
{
	struct tegra_bpmp_message msg;
	struct mrq_pcie_request req;
	int ret;

	memset(&req, 0, sizeof(req));

	req.cmd = CMD_PCIE_RP_CONTROLLER_OFF;
	req.rp_ctrlr_off.rp_controller = pcie->ctl_id;

	memset(&msg, 0, sizeof(msg));
	msg.mrq = MRQ_PCIE;
	msg.tx.data = &req;
	msg.tx.size = sizeof(req);

	ret = tegra_bpmp_transfer(pcie->bpmp, &msg);
	if (ret)
		dev_info(pcie->dev, "failed to turn off PCIe #%u: %d\n",
			 pcie->ctl_id, ret);

	if (msg.rx.ret)
		dev_info(pcie->dev, "failed to turn off PCIe #%u: %d\n",
			 pcie->ctl_id, msg.rx.ret);
}

static void tegra264_pcie_icc_set(struct tegra264_pcie *pcie)
{
	u32 value, speed, width, bw;
	int ret;

	value = readw(pcie->ecam + XTL_RC_PCIE_CFG_LINK_STATUS);
	speed = FIELD_GET(PCI_EXP_LNKSTA_CLS, value);
	width = FIELD_GET(PCI_EXP_LNKSTA_NLW, value);

	bw = width * (PCIE_SPEED2MBS_ENC(speed) / BITS_PER_BYTE);
	value = MBps_to_icc(bw);

	ret = icc_set_bw(pcie->icc_path, bw, bw);
	if (ret < 0)
		dev_err(pcie->dev,
			"failed to request bandwidth (%u MBps): %d\n",
			bw, ret);
}

static void tegra264_pcie_init(struct tegra264_pcie *pcie)
{
	u32 value, hi, lo;
	phys_addr_t phys;

	dev_info(pcie->dev, "> %s()\n", __func__);

	hi = readl(pcie->xal + XAL_RC_ECAM_BASE_HI);
	lo = readl(pcie->xal + XAL_RC_ECAM_BASE_LO);
	phys = (phys_addr_t)hi << 32 | lo;
	dev_info(pcie->dev, "  ECAM:      %pap\n", &phys);
	msleep(100);

	hi = readl(pcie->xal + XAL_RC_IO_BASE_HI);
	lo = readl(pcie->xal + XAL_RC_IO_BASE_LO);
	phys = (phys_addr_t)hi << 32 | lo;
	dev_info(pcie->dev, "  IO:        %pap\n", &phys);
	msleep(100);

	hi = readl(pcie->xal + XAL_RC_IO_LIMIT_HI);
	lo = readl(pcie->xal + XAL_RC_IO_LIMIT_LO);
	phys = (phys_addr_t)hi << 32 | lo;
	dev_info(pcie->dev, "    limit:   %pap\n", &phys);
	msleep(100);

	hi = readl(pcie->xal + XAL_RC_MEM_32BIT_BASE_HI);
	lo = readl(pcie->xal + XAL_RC_MEM_32BIT_BASE_LO);
	phys = (phys_addr_t)hi << 32 | lo;
	dev_info(pcie->dev, "  MEM32:     %pap\n", &phys);
	msleep(100);

	hi = readl(pcie->xal + XAL_RC_MEM_32BIT_LIMIT_HI);
	lo = readl(pcie->xal + XAL_RC_MEM_32BIT_LIMIT_LO);
	phys = (phys_addr_t)hi << 32 | lo;
	dev_info(pcie->dev, "    limit:   %pap\n", &phys);
	msleep(100);

	hi = readl(pcie->xal + XAL_RC_MEM_64BIT_BASE_HI);
	lo = readl(pcie->xal + XAL_RC_MEM_64BIT_BASE_LO);
	phys = (phys_addr_t)hi << 32 | lo;
	dev_info(pcie->dev, "  MEM64:     %pap\n", &phys);
	msleep(100);

	hi = readl(pcie->xal + XAL_RC_MEM_64BIT_LIMIT_HI);
	lo = readl(pcie->xal + XAL_RC_MEM_64BIT_LIMIT_LO);
	phys = (phys_addr_t)hi << 32 | lo;
	dev_info(pcie->dev, "    limit:   %pap\n", &phys);
	msleep(100);

	if (!tegra_is_silicon()) {
		dev_info(pcie->dev,
			 "skipping link state for PCIe #%u in simulation\n",
			 pcie->ctl_id);
		pcie->link_state = true;
		return;
	}

	/* Poll every 10 msec for 1 sec to link up */
	readl_poll_timeout(pcie->ecam + XTL_RC_PCIE_CFG_LINK_CONTROL_STATUS,
		value, value & XTL_RC_PCIE_CFG_LINK_CONTROL_STATUS_DLL_ACTIVE,
		PCIE_LINK_UP_DELAY, PCIE_LINK_UP_TIMEOUT);

	if (value & XTL_RC_PCIE_CFG_LINK_CONTROL_STATUS_DLL_ACTIVE) {
		/* Per PCIe r5.0, 6.6.1 wait for 100ms after DLL up */
		msleep(100);
		dev_info(pcie->dev, "PCIe #%u link is up (speed: %d)\n",
			 pcie->ctl_id, (value & 0xf0000) >> 16);
		pcie->link_state = true;
		tegra264_pcie_icc_set(pcie);
	} else {
		dev_info(pcie->dev, "PCIe #%u link is down\n", pcie->ctl_id);

		value = readl(pcie->xtl + XTL_RC_MGMT_CLOCK_CONTROL);

		/** Set link state only when link fails and no hot-plug feature is present */
		if ((value & XTL_RC_MGMT_CLOCK_CONTROL_PEX_CLKREQ_I_N_PIN_USE_CONV_TO_PRSNT) == 0) {
			dev_info(pcie->dev,
				 "PCIe #%u link is down and not hotplug-capable, turning off\n",
				 pcie->ctl_id);
			tegra264_pcie_bpmp_set_rp_state(pcie);
			pcie->link_state = false;
		} else {
			pcie->link_state = true;
		}
	}

	dev_info(pcie->dev, "< %s()\n", __func__);
}

static int tegra264_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pci_host_bridge *bridge;
	struct tegra264_pcie *pcie;
	struct resource_entry *bus;
	struct resource *res;
	int ret;

	bridge = devm_pci_alloc_host_bridge(dev, sizeof(struct tegra264_pcie));
	if (!bridge) {
		dev_err(dev, "failed to allocate host bridge\n");
		return -ENOMEM;
	}

	pcie = pci_host_bridge_priv(bridge);
	platform_set_drvdata(pdev, pcie);
	pcie->bridge = bridge;
	pcie->dev = dev;

	ret = pinctrl_pm_select_default_state(dev);
	if (ret < 0) {
		dev_err(dev, "failed to configure sideband pins: %d\n", ret);
		return ret;
	}

	ret = tegra264_pcie_parse_dt(pcie);
	if (ret < 0)
		return dev_err_probe(dev, ret, "failed to parse device tree");

	pcie->xal = devm_platform_ioremap_resource_byname(pdev, "xal");
	if (IS_ERR(pcie->xal)) {
		ret = PTR_ERR(pcie->xal);
		dev_err(dev, "failed to map xal memory: %d\n", ret);
		return ret;
	}

	pcie->xtl = devm_platform_ioremap_resource_byname(pdev, "xtl-pri");
	if (IS_ERR(pcie->xtl)) {
		ret = PTR_ERR(pcie->xtl);
		dev_err(dev, "failed to map xtl-pri memory: %d\n", ret);
		return ret;
	}

	bus = resource_list_first_type(&bridge->windows, IORESOURCE_BUS);
	if (!bus) {
		dev_err(dev, "failed to get bus resource\n");
		return -ENODEV;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ecam");
	if (!res) {
		dev_err(dev, "failed to get ECAM resource\n");
		return -ENXIO;
	}

	pcie->icc_path = devm_of_icc_get(&pdev->dev, "write");
	if (IS_ERR(pcie->icc_path))
		return dev_err_probe(&pdev->dev, PTR_ERR(pcie->icc_path),
				     "failed to get ICC");

	pcie->cfg = pci_ecam_create(dev, res, bus->res, &pci_generic_ecam_ops);
	if (IS_ERR(pcie->cfg)) {
		ret = PTR_ERR(pcie->cfg);
		dev_err(dev, "failed to create ECAM: %d\n", ret);
		goto put;
	}

	bridge->ops = (struct pci_ops *)&pci_generic_ecam_ops.pci_ops;
	bridge->sysdata = pcie->cfg;
	pcie->ecam = pcie->cfg->win;

	/*
	 * Parse BPMP property only for silicon, as interaction with BPMP is
	 * not needed for other platforms.
	 */
	if (tegra_is_silicon()) {
		pcie->bpmp = tegra_bpmp_get_with_id(dev, &pcie->ctl_id);
		if (IS_ERR(pcie->bpmp)) {
			ret = PTR_ERR(pcie->bpmp);
			dev_err(dev, "failed to get BPMP: %d\n", ret);
			goto free;
		}
	}

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	tegra264_pcie_init(pcie);

	if (pcie->link_state == false)
		goto put;

	ret = pci_host_probe(bridge);
	if (ret < 0) {
		dev_err(dev, "failed to register host: %d\n", ret);
		goto put_pm;
	}

	return ret;

put_pm:
	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);
put:
	if (tegra_is_silicon())
		tegra_bpmp_put(pcie->bpmp);
free:
	pci_ecam_free(pcie->cfg);
	return ret;
}

static void tegra264_pcie_remove(struct platform_device *pdev)
{
	struct tegra264_pcie *pcie = platform_get_drvdata(pdev);

	/*
	 * If we undo tegra264_pcie_init() then link goes down and need
	 * controller reset to bring up the link again. Remove intention is
	 * to clean up the root bridge and re-enumerate during bind.
	 */
	pci_lock_rescan_remove();
	pci_stop_root_bus(pcie->bridge->bus);
	pci_remove_root_bus(pcie->bridge->bus);
	pci_unlock_rescan_remove();

	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	if (tegra_is_silicon())
		tegra_bpmp_put(pcie->bpmp);

	pci_ecam_free(pcie->cfg);
}

static int tegra264_pcie_suspend_noirq(struct device *dev)
{
	struct tegra264_pcie *pcie = dev_get_drvdata(dev);
	int ret;

	if (pcie->wake_gpio && device_may_wakeup(dev)) {
		ret = enable_irq_wake(pcie->wake_irq);
		if (ret < 0)
			dev_err(dev, "failed to enable wake IRQ: %d\n", ret);
	}

	return 0;
}

static int tegra264_pcie_resume_noirq(struct device *dev)
{
	struct tegra264_pcie *pcie = dev_get_drvdata(dev);
	int ret;

	if (pcie->wake_gpio && device_may_wakeup(dev)) {
		ret = disable_irq_wake(pcie->wake_irq);
		if (ret < 0)
			dev_err(dev, "failed to disable wake IRQ: %d\n", ret);
	}

	if (pcie->link_state == false)
		return 0;

	tegra264_pcie_init(pcie);

	return 0;
}

static const struct dev_pm_ops tegra264_pcie_pm_ops = {
	.resume_noirq = tegra264_pcie_resume_noirq,
	.suspend_noirq = tegra264_pcie_suspend_noirq,
};

static const struct of_device_id tegra264_pcie_of_match[] = {
	{
		.compatible = "nvidia,tegra264-pcie",
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, tegra264_pcie_of_match);

static struct platform_driver tegra264_pcie_driver = {
	.probe = tegra264_pcie_probe,
	.remove = tegra264_pcie_remove,
	.driver = {
		.name = "tegra264-pcie",
		.pm = &tegra264_pcie_pm_ops,
		.of_match_table = tegra264_pcie_of_match,
	},
};
module_platform_driver(tegra264_pcie_driver);

MODULE_AUTHOR("Manikanta Maddireddy <mmaddireddy@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra264 PCIe host controller driver");
MODULE_LICENSE("GPL");
