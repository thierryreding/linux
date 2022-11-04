#define DEBUG
#include <linux/of_platform.h>
#include <linux/platform_device.h>

static int tegra_dce_probe(struct platform_device *pdev)
{
	int err;

	dev_dbg(&pdev->dev, "> %s(pdev=%px)\n", __func__, pdev);

	err = devm_of_platform_populate(&pdev->dev);
	if (err < 0)
		dev_err(&pdev->dev, "failed to populate child devices\n");

	dev_dbg(&pdev->dev, "< %s() = %d\n", __func__, err);
	return err;
}

static int tegra_dce_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "> %s(pdev=%px)\n", __func__, pdev);
	dev_dbg(&pdev->dev, "< %s()\n", __func__);
	return 0;
}

static int __maybe_unused tegra_dce_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "> %s(dev=%px)\n", __func__, dev);
	dev_dbg(dev, "< %s()\n", __func__);
	return 0;
}

static int __maybe_unused tegra_dce_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "> %s(dev=%px)\n", __func__, dev);
	dev_dbg(dev, "< %s()\n", __func__);
	return 0;
}

static int __maybe_unused tegra_dce_suspend(struct device *dev)
{
	dev_dbg(dev, "> %s(dev=%px)\n", __func__, dev);
	dev_dbg(dev, "< %s()\n", __func__);
	return 0;
}

static int __maybe_unused tegra_dce_resume(struct device *dev)
{
	dev_dbg(dev, "> %s(dev=%px)\n", __func__, dev);
	dev_dbg(dev, "< %s()\n", __func__);
	return 0;
}

static const struct dev_pm_ops tegra_dce_pm = {
	SET_RUNTIME_PM_OPS(tegra_dce_runtime_suspend, tegra_dce_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra_dce_suspend, tegra_dce_resume)
};

static const struct of_device_id tegra_dce_match[] = {
	{ .compatible = "nvidia,tegra234-dce" },
	{ }
};

static struct platform_driver tegra_dce_driver = {
	.driver = {
		.name = "tegra-dce",
		.of_match_table = tegra_dce_match,
		.pm = &tegra_dce_pm,
	},
	.probe = tegra_dce_probe,
	.remove = tegra_dce_remove,
};
module_platform_driver(tegra_dce_driver);

MODULE_AUTHOR("Thierry Reding <treding@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra234 DCE driver");
MODULE_LICENSE("GPL v2");
