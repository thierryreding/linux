#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/stmmac.h>

#include "stmmac_platform.h"

#include "../../../pcs/pcs-xpcs.h"

struct tegra_mgbe {
	struct device *dev;

	struct clk *clk_rx_input_m;
	struct clk *clk_rx_input;
	struct clk *clk_rx_pcs_m;
	struct clk *clk_rx_pcs_input;
	struct clk *clk_rx_pcs;
	struct clk *clk_tx;
	struct clk *clk_tx_pcs;
	struct clk *clk_mac_div;
	struct clk *clk_mac;
	struct clk *clk_eee_pcs;
	struct clk *clk;
	struct clk *clk_ptp_ref;

	struct reset_control *rst_mac;
	struct reset_control *rst_pcs;

	void __iomem *hv;
	void __iomem *regs;
	void __iomem *xpcs;

	struct mii_bus *mii;
};

static void tegra_mgbe_fix_mac_speed(void *priv, unsigned int speed)
{
	struct tegra_mgbe *mgbe = priv;

	dev_info(mgbe->dev, "> %s(priv=%px, speed=%u)\n", __func__, priv, speed);
	dev_info(mgbe->dev, "< %s()\n", __func__);
}

static int tegra_mgbe_init(struct platform_device *pdev, void *priv)
{
	struct tegra_mgbe *mgbe = priv;

	dev_info(mgbe->dev, "> %s(pdev=%px, priv=%px)\n", __func__, pdev, priv);
	dev_info(mgbe->dev, "< %s()\n", __func__);

	return 0;
}

#define XPCS_REG_ADDR_SHIFT 10
#define XPCS_REG_ADDR_MASK 0x1fff
#define XPCS_ADDR 0x3fc

static int tegra_mgbe_xpcs_read(struct mii_bus *bus, int phyaddr, int phyreg)
{
	struct net_device *ndev = bus->priv;
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct tegra_mgbe *mgbe = priv->plat->bsp_priv;
	unsigned int base, offset;
	u32 value;

	//dev_info(mgbe->dev, "XPCS RD %02x:%08x\n", phyaddr, phyreg);

	base = (phyreg >> (MII_DEVADDR_C45_SHIFT - 8)) & 0x1fff;
	writel(base, mgbe->xpcs + XPCS_ADDR);

	offset = (phyreg & (MII_REGADDR_C45_MASK >> 8)) << 2;
	value = readl(mgbe->xpcs + offset);

	dev_info(mgbe->dev, "XPCS %04x:%02x > %04x\n", base, offset, value);
	return value;
}

static int tegra_mgbe_xpcs_write(struct mii_bus *bus, int phyaddr, int phyreg, u16 value)
{
	struct net_device *ndev = bus->priv;
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct tegra_mgbe *mgbe = priv->plat->bsp_priv;
	unsigned int base, offset;

	//dev_info(mgbe->dev, "XPCS WR %02x:%08x\n", phyaddr, phyreg);

	base = (phyreg >> (MII_DEVADDR_C45_SHIFT - 8)) & 0x1fff;
	writel(base, mgbe->xpcs + XPCS_ADDR);

	offset = (phyreg & (MII_REGADDR_C45_MASK >> 8)) << 2;
	writel(value, mgbe->xpcs + offset);

	dev_info(mgbe->dev, "XPCS %04x:%02x < %04x\n", base, offset, value);
	return 0;
}

static int xpcs_read_vendor(struct dw_xpcs *xpcs, int dev, u32 offset)
{
	return xpcs_read(xpcs, dev, DW_VENDOR | offset);
}

static int xpcs_write_vendor(struct dw_xpcs *xpcs, int dev, int offset, u16 value)
{
	return xpcs_write(xpcs, dev, DW_VENDOR | offset, value);
}

static int xpcs_read_vpcs(struct dw_xpcs *xpcs, int offset)
{
	return xpcs_read_vendor(xpcs, MDIO_MMD_PCS, offset);
}

static int xpcs_write_vpcs(struct dw_xpcs *xpcs, int offset, u16 value)
{
	return xpcs_write_vendor(xpcs, MDIO_MMD_PCS, offset, value);
}

static int tegra_mgbe_probe(struct platform_device *pdev)
{
	struct plat_stmmacenet_data *plat;
	struct stmmac_resources res;
	struct tegra_mgbe *mgbe;
	int irq, err;

	dev_info(&pdev->dev, "> %s(pdev=%px)\n", __func__, pdev);

	mgbe = devm_kzalloc(&pdev->dev, sizeof(*mgbe), GFP_KERNEL);
	if (!mgbe)
		return -ENOMEM;

	mgbe->dev = &pdev->dev;

	memset(&res, 0, sizeof(res));

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	mgbe->hv = devm_platform_ioremap_resource_byname(pdev, "hypervisor");
	if (IS_ERR(mgbe->hv))
		return PTR_ERR(mgbe->hv);

	mgbe->regs = devm_platform_ioremap_resource_byname(pdev, "mac");
	if (IS_ERR(mgbe->regs))
		return PTR_ERR(mgbe->regs);

	mgbe->xpcs = devm_platform_ioremap_resource_byname(pdev, "xpcs");
	if (IS_ERR(mgbe->xpcs))
		return PTR_ERR(mgbe->xpcs);

	res.addr = mgbe->regs;
	res.irq = irq;

	mgbe->clk_rx_input = devm_clk_get(&pdev->dev, "rx-input");
	if (IS_ERR(mgbe->clk_rx_input))
		return PTR_ERR(mgbe->clk_rx_input);

	err = clk_set_rate(mgbe->clk_rx_input, 644531250);
	if (err < 0)
		return err;

	err = clk_prepare_enable(mgbe->clk_rx_input);
	if (err < 0)
		return err;

	mgbe->clk_rx_input_m = devm_clk_get(&pdev->dev, "rx-input-m");
	if (IS_ERR(mgbe->clk_rx_input_m))
		return PTR_ERR(mgbe->clk_rx_input_m);

	err = clk_prepare_enable(mgbe->clk_rx_input_m);
	if (err < 0)
		return err;

	mgbe->clk_rx_pcs_m = devm_clk_get(&pdev->dev, "rx-pcs-m");
	if (IS_ERR(mgbe->clk_rx_pcs_m))
		return PTR_ERR(mgbe->clk_rx_pcs_m);

	err = clk_prepare_enable(mgbe->clk_rx_pcs_m);
	if (err < 0)
		return err;

	mgbe->clk_rx_pcs_input = devm_clk_get(&pdev->dev, "rx-pcs-input");
	if (IS_ERR(mgbe->clk_rx_pcs_input))
		return PTR_ERR(mgbe->clk_rx_pcs_input);

	err = clk_set_rate(mgbe->clk_rx_pcs_input, 156250000);
	if (err < 0)
		return err;

	err = clk_prepare_enable(mgbe->clk_rx_pcs_input);
	if (err < 0)
		return err;

	mgbe->clk_rx_pcs = devm_clk_get(&pdev->dev, "rx-pcs");
	if (IS_ERR(mgbe->clk_rx_pcs))
		return PTR_ERR(mgbe->clk_rx_pcs);

	err = clk_prepare_enable(mgbe->clk_rx_pcs);
	if (err < 0)
		return err;

	mgbe->clk_tx = devm_clk_get(&pdev->dev, "tx");
	if (IS_ERR(mgbe->clk_tx))
		return PTR_ERR(mgbe->clk_tx);

	err = clk_prepare_enable(mgbe->clk_tx);
	if (err < 0)
		return err;

	mgbe->clk_tx_pcs = devm_clk_get(&pdev->dev, "tx-pcs");
	if (IS_ERR(mgbe->clk_tx_pcs))
		return PTR_ERR(mgbe->clk_tx_pcs);

	err = clk_prepare_enable(mgbe->clk_tx_pcs);
	if (err < 0)
		return err;

	mgbe->clk_mac_div = devm_clk_get(&pdev->dev, "mac-divider");
	if (IS_ERR(mgbe->clk_mac_div))
		return PTR_ERR(mgbe->clk_mac_div);

	err = clk_prepare_enable(mgbe->clk_mac_div);
	if (err < 0)
		return err;

	mgbe->clk_mac = devm_clk_get(&pdev->dev, "mac");
	if (IS_ERR(mgbe->clk_mac))
		return PTR_ERR(mgbe->clk_mac);

	err = clk_prepare_enable(mgbe->clk_mac);
	if (err < 0)
		return err;

	mgbe->clk_eee_pcs = devm_clk_get(&pdev->dev, "eee-pcs");
	if (IS_ERR(mgbe->clk_eee_pcs))
		return PTR_ERR(mgbe->clk_eee_pcs);

	err = clk_prepare_enable(mgbe->clk_eee_pcs);
	if (err < 0)
		return err;

	mgbe->clk = devm_clk_get(&pdev->dev, "mgbe");
	if (IS_ERR(mgbe->clk))
		return PTR_ERR(mgbe->clk);

	err = clk_prepare_enable(mgbe->clk);
	if (err < 0)
		return err;

	mgbe->clk_ptp_ref = devm_clk_get(&pdev->dev, "ptp-ref");
	if (IS_ERR(mgbe->clk_ptp_ref))
		return PTR_ERR(mgbe->clk_ptp_ref);

	err = clk_prepare_enable(mgbe->clk_ptp_ref);
	if (err < 0)
		return err;

	mgbe->rst_mac = devm_reset_control_get(&pdev->dev, "mac");
	if (IS_ERR(mgbe->rst_mac))
		return PTR_ERR(mgbe->rst_mac);

	err = reset_control_assert(mgbe->rst_mac);
	if (err < 0)
		return err;

	usleep_range(2000, 4000);

	err = reset_control_deassert(mgbe->rst_mac);
	if (err < 0)
		return err;

	mgbe->rst_pcs = devm_reset_control_get(&pdev->dev, "pcs");
	if (IS_ERR(mgbe->rst_pcs))
		return PTR_ERR(mgbe->rst_pcs);

	err = reset_control_assert(mgbe->rst_pcs);
	if (err < 0)
		return err;

	usleep_range(2000, 4000);

	err = reset_control_deassert(mgbe->rst_pcs);
	if (err < 0)
		return err;

	plat = stmmac_probe_config_dt(pdev, res.mac);
	if (IS_ERR(plat))
		return PTR_ERR(plat);

	plat->clk_ptp_rate = clk_get_rate(mgbe->clk_ptp_ref);
	plat->clk_ptp_ref = mgbe->clk_ptp_ref;

	plat->has_xgmac = 1;
	plat->tso_en = 1;
	plat->pmt = 1;

	plat->fix_mac_speed = tegra_mgbe_fix_mac_speed;
	plat->init = tegra_mgbe_init;
	plat->bsp_priv = mgbe;

	/*
	err = dwc_eth_dwmac_config_dt(pdev, plat);
	if (err < 0)
		goto remove;
	*/

	if (!plat->mdio_node) {
		plat->mdio_node = of_get_child_by_name(pdev->dev.of_node, "mdio");
		dev_info(&pdev->dev, "MDIO node: %pOF\n", plat->mdio_node);
	}

	if (!plat->mdio_bus_data) {
		dev_info(&pdev->dev, "explicitly creating MDIO bus...\n");

		plat->mdio_bus_data = devm_kzalloc(&pdev->dev, sizeof(*plat->mdio_bus_data),
						   GFP_KERNEL);
		if (!plat->mdio_bus_data) {
			err = -ENOMEM;
			goto remove;
		}
	}

	plat->mdio_bus_data->needs_reset = true;
	plat->mdio_bus_data->xpcs_an_inband = true;
	plat->mdio_bus_data->has_xpcs = true;

	plat->mdio_write = tegra_mgbe_xpcs_write;
	plat->mdio_read = tegra_mgbe_xpcs_read;

	if (1) {
		unsigned int retry = 300;
		u32 value;

#define XPCS_WRAP_UPHY_RX_CONTROL 0x801c
#define XPCS_WRAP_UPHY_RX_CONTROL_RX_SW_OVRD BIT(31)
#define XPCS_WRAP_UPHY_RX_CONTROL_RX_PCS_PHY_RDY BIT(10)
#define XPCS_WRAP_UPHY_RX_CONTROL_RX_CDR_RESET BIT(9)
#define XPCS_WRAP_UPHY_RX_CONTROL_RX_CAL_EN BIT(8)
#define XPCS_WRAP_UPHY_RX_CONTROL_RX_SLEEP (BIT(7) | BIT(6))
#define XPCS_WRAP_UPHY_RX_CONTROL_AUX_RX_IDDQ BIT(5)
#define XPCS_WRAP_UPHY_RX_CONTROL_RX_IDDQ BIT(4)
#define XPCS_WRAP_UPHY_RX_CONTROL_RX_DATA_EN BIT(0)
#define XPCS_WRAP_UPHY_HW_INIT_CTRL 0x8020
#define XPCS_WRAP_UPHY_HW_INIT_CTRL_TX_EN BIT(0)
#define XPCS_WRAP_UPHY_HW_INIT_CTRL_RX_EN BIT(2)
#define XPCS_WRAP_UPHY_STATUS 0x8044
#define XPCS_WRAP_UPHY_STATUS_TX_P_UP BIT(0)
#define XPCS_WRAP_IRQ_STATUS 0x8050
#define XPCS_WRAP_IRQ_STATUS_PCS_LINK_STS BIT(6)

		value = readl(mgbe->xpcs + XPCS_WRAP_UPHY_HW_INIT_CTRL);
		dev_info(&pdev->dev, "XPCS_WRAP_UPHY_HW_INIT_CTRL: %08x\n", value);

		value = readl(mgbe->xpcs + XPCS_WRAP_UPHY_STATUS);
		dev_info(&pdev->dev, "XPCS_WRAP_UPHY_STATUS: %08x\n", value);

		if ((value & XPCS_WRAP_UPHY_STATUS_TX_P_UP) == 0) {
			value = readl(mgbe->xpcs + XPCS_WRAP_UPHY_HW_INIT_CTRL);
			value |= XPCS_WRAP_UPHY_HW_INIT_CTRL_TX_EN;
			writel(value, mgbe->xpcs + XPCS_WRAP_UPHY_HW_INIT_CTRL);
		}

		dev_info(&pdev->dev, "bringing up TX lane...\n");

		err = readl_poll_timeout(mgbe->xpcs + XPCS_WRAP_UPHY_HW_INIT_CTRL, value,
					 (value & XPCS_WRAP_UPHY_HW_INIT_CTRL_TX_EN) == 0,
					 500, 500 * 2000);
		if (err < 0) {
			dev_err(&pdev->dev, "timeout waiting for TX lane to become enabled\n");
		}

		usleep_range(10000, 20000);

		value = readl(mgbe->xpcs + XPCS_WRAP_UPHY_STATUS);
		dev_info(&pdev->dev, "XPCS_WRAP_UPHY_STATUS: %08x\n", value);

		value = readl(mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);
		value |= XPCS_WRAP_UPHY_RX_CONTROL_RX_SW_OVRD;
		writel(value, mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);

		value = readl(mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);
		value &= ~XPCS_WRAP_UPHY_RX_CONTROL_RX_IDDQ;
		writel(value, mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);

		value = readl(mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);
		value &= ~XPCS_WRAP_UPHY_RX_CONTROL_AUX_RX_IDDQ;
		writel(value, mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);

		value = readl(mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);
		value &= ~XPCS_WRAP_UPHY_RX_CONTROL_RX_SLEEP;
		writel(value, mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);

		value = readl(mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);
		value |= XPCS_WRAP_UPHY_RX_CONTROL_RX_CAL_EN;
		writel(value, mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);

		err = readl_poll_timeout(mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL, value,
					 (value & XPCS_WRAP_UPHY_RX_CONTROL_RX_CAL_EN) == 0,
					 1000, 1000 * 2000);
		if (err < 0) {
			dev_err(&pdev->dev, "timeout waiting for RX calibration to become enabled\n");
		}

		value = readl(mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);
		value |= XPCS_WRAP_UPHY_RX_CONTROL_RX_DATA_EN;
		writel(value, mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);

		value = readl(mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);
		value |= XPCS_WRAP_UPHY_RX_CONTROL_RX_CDR_RESET;
		writel(value, mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);

		value = readl(mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);
		value &= ~XPCS_WRAP_UPHY_RX_CONTROL_RX_CDR_RESET;
		writel(value, mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);

		value = readl(mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);
		value |= XPCS_WRAP_UPHY_RX_CONTROL_RX_PCS_PHY_RDY;
		writel(value, mgbe->xpcs + XPCS_WRAP_UPHY_RX_CONTROL);

		while (--retry) {
			err = readl_poll_timeout(mgbe->xpcs + XPCS_WRAP_IRQ_STATUS, value,
						 value & XPCS_WRAP_IRQ_STATUS_PCS_LINK_STS,
						 500, 500 * 2000);
			if (err < 0) {
				dev_err(&pdev->dev, "timeout waiting for link to become ready\n");
				usleep_range(10000, 20000);
				continue;
			}

			dev_info(&pdev->dev, "link ready\n");
			break;
		}

		/* clear status */
		writel(value, mgbe->xpcs + XPCS_WRAP_IRQ_STATUS);
	}

	err = stmmac_dvr_probe(&pdev->dev, plat, &res);
	if (err < 0)
		goto remove;

	/* xpcs_init() */
	if (1) {
		struct net_device *ndev = dev_get_drvdata(&pdev->dev);
		struct stmmac_priv *priv = netdev_priv(ndev);
		struct dw_xpcs *xpcs = priv->hw->xpcs;
		unsigned int retry = 10;
		int value;

		value = xpcs_read(xpcs, MDIO_MMD_PCS, MDIO_CTRL1);
		if (value < 0)
			return value;

		value |= 0x00;

		xpcs_write(xpcs, MDIO_MMD_PCS, MDIO_CTRL1, value);

		value = xpcs_read_vpcs(xpcs, MDIO_CTRL2);
		if (value < 0)
			return value;

		value &= ~(BIT(12) | BIT(11) | BIT(10));
		xpcs_write_vpcs(xpcs, MDIO_CTRL2, value);

		dev_info(&xpcs->mdiodev->dev, "initiating software reset...\n");

		value = xpcs_read_vpcs(xpcs, MDIO_CTRL1);
		if (value < 0)
			return value;

		value |= BIT(15);
		value |= BIT(9);

		xpcs_write_vpcs(xpcs, MDIO_CTRL1, value);

		while (retry--) {
			value = xpcs_read_vpcs(xpcs, MDIO_CTRL1);
			if (value < 0)
				return value;

			if ((value & BIT(15)) == 0) {
				dev_info(&xpcs->mdiodev->dev, "soft-reset complete\n");
				break;
			}

			usleep_range(100000, 200000);
		}

		value = xpcs_read(xpcs, MDIO_MMD_AN, 0x00);
		if (value < 0)
			return value;

		value &= ~BIT(12);
		xpcs_write(xpcs, MDIO_MMD_AN, 0x00, value);

		value = xpcs_read_vpcs(xpcs, MDIO_CTRL1);
		if (value < 0)
			return value;

		value |= BIT(12);

		xpcs_write_vpcs(xpcs, MDIO_CTRL1, value);
	}

	/* xpcs_start() */
	if (1) {
		struct net_device *ndev = dev_get_drvdata(&pdev->dev);
		struct stmmac_priv *priv = netdev_priv(ndev);
		struct dw_xpcs *xpcs = priv->hw->xpcs;
		unsigned int retry = 100;
		int value;

		value = xpcs_read(xpcs, MDIO_MMD_VEND2, MDIO_CTRL1);
		if (value < 0)
			return value;

		value |= BMCR_ANENABLE;

		xpcs_write(xpcs, MDIO_MMD_VEND2, MDIO_CTRL1, value);

		dev_info(&xpcs->mdiodev->dev, "waiting for auto-negotiation to complete...\n");

		while (retry--) {
			value = xpcs_read_vendor(xpcs, MDIO_MMD_VEND2, 0x02);
			if (value < 0)
				return value;

			if (value & BIT(0)) {
				dev_info(&xpcs->mdiodev->dev, "auto-negotiation complete\n");
				break;
			}

			usleep_range(100000, 200000);
		}

		/* clear interrupt */
		value &= ~BIT(0);
		xpcs_write_vendor(xpcs, MDIO_MMD_VEND2, 0x02, value);
	}

	dev_info(&pdev->dev, "< %s()\n", __func__);
	return 0;

remove:
	stmmac_remove_config_dt(pdev, plat);
	dev_info(&pdev->dev, "< %s() = %d\n", __func__, err);
	return err;
}

static int tegra_mgbe_remove(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "> %s(pdev=%px)\n", __func__, pdev);

	dev_info(&pdev->dev, "< %s()\n", __func__);
	return 0;
}

static int __maybe_unused tegra_mgbe_suspend(struct device *dev)
{
	dev_info(dev, "> %s(dev=%px)\n", __func__, dev);
	dev_info(dev, "< %s()\n", __func__);
	return 0;
}

static int __maybe_unused tegra_mgbe_resume(struct device *dev)
{
	dev_info(dev, "> %s(dev=%px)\n", __func__, dev);
	dev_info(dev, "< %s()\n", __func__);
	return 0;
}

static int __maybe_unused tegra_mgbe_runtime_suspend(struct device *dev)
{
	dev_info(dev, "> %s(dev=%px)\n", __func__, dev);
	dev_info(dev, "< %s()\n", __func__);
	return 0;
}

static int __maybe_unused tegra_mgbe_runtime_resume(struct device *dev)
{
	dev_info(dev, "> %s(dev=%px)\n", __func__, dev);
	dev_info(dev, "< %s()\n", __func__);
	return 0;
}

static const struct dev_pm_ops tegra_mgbe_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tegra_mgbe_suspend, tegra_mgbe_resume)
	SET_RUNTIME_PM_OPS(tegra_mgbe_runtime_suspend, tegra_mgbe_runtime_resume, NULL)
};

static const struct of_device_id tegra_mgbe_match[] = {
	{ .compatible = "nvidia,tegra234-mgbe", },
	{ }
};
MODULE_DEVICE_TABLE(of, tegra_mgbe_match);

static struct platform_driver tegra_mgbe_driver = {
	.probe = tegra_mgbe_probe,
	.remove = tegra_mgbe_remove,
	.driver = {
		.name = "tegra-mgbe",
		.pm = &tegra_mgbe_pm_ops,
		.of_match_table = tegra_mgbe_match,
	},
};
module_platform_driver(tegra_mgbe_driver);

MODULE_AUTHOR("Thierry Reding <treding@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra MGBE driver");
MODULE_LICENSE("GPL");
