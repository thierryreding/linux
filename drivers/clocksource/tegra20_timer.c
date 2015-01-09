/*
 * Copyright (C) 2010 Google, Inc.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/sched_clock.h>
#include <linux/delay.h>
#include <linux/watchdog.h>

#ifndef CONFIG_ARM64
#include <asm/mach/time.h>
#include <asm/smp_twd.h>
#endif

#define RTC_SECONDS            0x08
#define RTC_SHADOW_SECONDS     0x0c
#define RTC_MILLISECONDS       0x10

#define TIMERUS_CNTR_1US 0x10
#define TIMERUS_USEC_CFG 0x14
#define TIMERUS_CNTR_FREEZE 0x4c

#define TIMER1_BASE 0x000
#define TIMER2_BASE 0x008
#define TIMER3_BASE 0x050
#define TIMER4_BASE 0x058
#define TIMER5_BASE 0x060

#define TIMER_PTV 0x0
#define  TIMER_PTV_ENABLE   (1 << 31)
#define  TIMER_PTV_PERIODIC (1 << 30)

#define TIMER_PCR 0x4
#define  TIMER_PCR_INTR_CLR (1 << 30)

/*
 * Register base of the timer that's selected for pairing with the watchdog.
 * This driver arbitrarily uses timer 5, which is currently unused by
 * other drivers (in particular, the Tegra clocksource driver).  If this
 * needs to change, take care that the new timer is not used by the
 * clocksource driver.
 */
#define WDT_TIMER_BASE 0x60
#define WDT_TIMER_ID 5

/* WDT registers */
#define WDT_BASE 0x100

#define WDT_CFG				0x0
#define  WDT_CFG_PERIOD_SHIFT		4
#define  WDT_CFG_PERIOD_MASK		0xff
#define  WDT_CFG_INT_EN			(1 << 12)
#define  WDT_CFG_PMC2CAR_RST_EN		(1 << 15)

#define WDT_STS				0x4
#define  WDT_STS_COUNT_SHIFT		4
#define  WDT_STS_COUNT_MASK		0xff
#define  WDT_STS_EXP_SHIFT		12
#define  WDT_STS_EXP_MASK		0x3

#define WDT_CMD				0x8
#define  WDT_CMD_START_COUNTER		(1 << 0)
#define  WDT_CMD_DISABLE_COUNTER	(1 << 1)

#define WDT_UNLOCK			0xc
#define WDT_UNLOCK_PATTERN		(0xc45a << 0)

struct tegra_wdt {
	struct watchdog_device wdd;
	void __iomem *timer;
	void __iomem *base;
};

static inline struct tegra_wdt *to_tegra_wdt(struct watchdog_device *wdd)
{
	return container_of(wdd, struct tegra_wdt, wdd);
}

struct tegra_timer {
	void __iomem *base;
	struct clk *clk;
	int irq;

	struct clock_event_device clockevent;
#ifndef CONFIG_ARM64
	struct delay_timer delay;
#endif

	struct tegra_wdt *wdt;

	u32 usec_cfg;
};

static inline void timer_writel(struct tegra_timer *timer, u32 value,
				unsigned long offset)
{
	writel(value, timer->base + offset);
}

static inline u32 timer_readl(struct tegra_timer *timer, unsigned long offset)
{
	return readl(timer->base + offset);
}

static struct tegra_timer *timer = &(struct tegra_timer) {
	.base = NULL,
};

static void __iomem *rtc_base;

#ifndef CONFIG_ARM64
static struct timespec64 persistent_ts;
static u64 persistent_ms, last_persistent_ms;
#endif

static int tegra_timer_set_next_event(unsigned long cycles,
					 struct clock_event_device *evt)
{
	u32 value;

	value = TIMER_PTV_ENABLE | ((cycles > 1) ? (cycles - 1) : 0);
	timer_writel(timer, value, TIMER3_BASE + TIMER_PTV);

	return 0;
}

static inline void timer_shutdown(struct clock_event_device *evt)
{
	timer_writel(timer, 0, TIMER3_BASE + TIMER_PTV);
}

static int tegra_timer_shutdown(struct clock_event_device *evt)
{
	timer_shutdown(evt);
	return 0;
}

static int tegra_timer_set_periodic(struct clock_event_device *evt)
{
	u32 reg = 0xC0000000 | ((1000000 / HZ) - 1);

	timer_shutdown(evt);
	timer_writel(timer, reg, TIMER3_BASE + TIMER_PTV);
	return 0;
}

static u64 notrace tegra_read_sched_clock(void)
{
	return timer_readl(timer, TIMERUS_CNTR_1US);
}

#ifndef CONFIG_ARM64
/*
 * tegra_rtc_read - Reads the Tegra RTC registers
 * Care must be taken that this funciton is not called while the
 * tegra_rtc driver could be executing to avoid race conditions
 * on the RTC shadow register
 */
static u64 tegra_rtc_read_ms(void)
{
	u32 ms = readl(rtc_base + RTC_MILLISECONDS);
	u32 s = readl(rtc_base + RTC_SHADOW_SECONDS);
	return (u64)s * MSEC_PER_SEC + ms;
}

/*
 * tegra_read_persistent_clock64 -  Return time from a persistent clock.
 *
 * Reads the time from a source which isn't disabled during PM, the
 * 32k sync timer.  Convert the cycles elapsed since last read into
 * nsecs and adds to a monotonically increasing timespec64.
 * Care must be taken that this funciton is not called while the
 * tegra_rtc driver could be executing to avoid race conditions
 * on the RTC shadow register
 */
static void tegra_read_persistent_clock64(struct timespec64 *ts)
{
	u64 delta;

	last_persistent_ms = persistent_ms;
	persistent_ms = tegra_rtc_read_ms();
	delta = persistent_ms - last_persistent_ms;

	timespec64_add_ns(&persistent_ts, delta * NSEC_PER_MSEC);
	*ts = persistent_ts;
}

static unsigned long tegra_delay_timer_read_counter_long(void)
{
	return timer_readl(timer, TIMERUS_CNTR_1US);
}
#endif

static irqreturn_t tegra_timer_interrupt(int irq, void *dev_id)
{
	struct tegra_timer *timer = dev_id;

	timer_writel(timer, TIMER_PCR_INTR_CLR, TIMER3_BASE + TIMER_PCR);
	timer->clockevent.event_handler(&timer->clockevent);

	return IRQ_HANDLED;
}

static const struct tegra_usec_config{
	unsigned long input;
	unsigned int mul;
	unsigned int div;
} tegra_usec_configs[] = {
	{ 12000000, 1,  12 },
	{ 13000000, 1,  13 },
	{ 16800000, 5,  84 },
	{ 19200000, 5,  96 },
	{ 26000000, 1,  26 },
	{ 38400000, 5, 192 },
	{ 48000000, 1,  48 },
};

static const struct tegra_usec_config *
tegra_timer_get_usec_config(unsigned long rate)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(tegra_usec_configs); i++)
		if (tegra_usec_configs[i].input == rate)
			return &tegra_usec_configs[i];

	return NULL;
}

static void __init tegra20_init_timer(struct device_node *np)
{
	const struct tegra_usec_config *cfg;
	unsigned long rate;
	int ret;

	timer->base = of_iomap(np, 0);
	if (!timer->base) {
		pr_err("Can't map timer registers\n");
		BUG();
	}

	timer->irq = irq_of_parse_and_map(np, 2);
	if (timer->irq == 0) {
		pr_err("Failed to map timer IRQ\n");
		BUG();
	}

	timer->clk = of_clk_get(np, 0);
	if (IS_ERR(timer->clk)) {
		pr_warn("Unable to get timer clock. Assuming 12MHz input clock.\n");
		rate = 12000000;
	} else {
		ret = clk_prepare_enable(timer->clk);
		if (ret < 0) {
			pr_err("Failed to enable clock: %d\n", ret);
			BUG();
		}

		rate = clk_get_rate(timer->clk);
	}

	cfg = tegra_timer_get_usec_config(rate);
	if (!WARN(cfg == NULL, "Unknown clock rate: %lu Hz", rate)) {
		u32 value = ((cfg->mul - 1) << 8) | (cfg->div - 1);
		timer_writel(timer, value, TIMERUS_USEC_CFG);
	}

	sched_clock_register(tegra_read_sched_clock, 32, USEC_PER_SEC);

	if (clocksource_mmio_init(timer->base + TIMERUS_CNTR_1US, "timer_us",
				  USEC_PER_SEC, 300, 32,
				  clocksource_mmio_readl_up)) {
		pr_err("Failed to register clocksource\n");
		BUG();
	}

#ifndef CONFIG_ARM64
	timer->delay.read_current_timer = tegra_delay_timer_read_counter_long;
	timer->delay.freq = USEC_PER_SEC;
	register_current_timer_delay(&timer->delay);
#endif

	ret = request_irq(timer->irq, tegra_timer_interrupt,
			  IRQF_TIMER | IRQF_TRIGGER_HIGH, "timer0", timer);
	if (ret < 0) {
		pr_err("Failed to register timer IRQ: %d\n", ret);
		BUG();
	}

	timer->clockevent.set_next_event = tegra_timer_set_next_event;
	timer->clockevent.set_state_shutdown = tegra_timer_shutdown,
	timer->clockevent.set_state_periodic = tegra_timer_set_periodic,
	timer->clockevent.set_state_oneshot = tegra_timer_shutdown,
	timer->clockevent.tick_resume = tegra_timer_shutdown,

	timer->clockevent.features = CLOCK_EVT_FEAT_ONESHOT |
				     CLOCK_EVT_FEAT_PERIODIC |
				     CLOCK_EVT_FEAT_DYNIRQ;
	timer->clockevent.name = "timer0";
	timer->clockevent.rating = 300;
	timer->clockevent.irq = timer->irq;
	timer->clockevent.cpumask = cpu_all_mask;

	clockevents_config_and_register(&timer->clockevent, USEC_PER_SEC,
					0x1, 0x1fffffff);
}
CLOCKSOURCE_OF_DECLARE(tegra20_timer, "nvidia,tegra20-timer", tegra20_init_timer);

static void __init tegra20_init_rtc(struct device_node *np)
{
	struct clk *clk;

	rtc_base = of_iomap(np, 0);
	if (!rtc_base) {
		pr_err("Can't map RTC registers");
		BUG();
	}

	/*
	 * rtc registers are used by read_persistent_clock, keep the rtc clock
	 * enabled
	 */
	clk = of_clk_get(np, 0);
	if (IS_ERR(clk))
		pr_warn("Unable to get rtc-tegra clock\n");
	else
		clk_prepare_enable(clk);

#ifndef CONFIG_ARM64
	register_persistent_clock(NULL, tegra_read_persistent_clock64);
#endif
}
CLOCKSOURCE_OF_DECLARE(tegra20_rtc, "nvidia,tegra20-rtc", tegra20_init_rtc);

static const struct watchdog_info tegra_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_MAGICCLOSE | WDIOF_KEEPALIVEPING,
	.identity = "Tegra Watchdog",
};

static int tegra_wdt_start(struct watchdog_device *wdd)
{
	struct tegra_wdt *wdt = to_tegra_wdt(wdd);
	u32 value;

	/*
	 * This thing has a fixed 1MHz clock.  Normally, we would set the
	 * period to 1 second by writing 1000000ul, but the watchdog system
	 * reset actually occurs on the 4th expiration of this counter,
	 * so we set the period to 1/4 of this amount.
	 */
	value = TIMER_PTV_ENABLE | TIMER_PTV_PERIODIC | (USEC_PER_SEC / 4);
	writel(value, wdt->timer + TIMER_PTV);

	/*
	 * Set number of periods and start counter.
	 *
	 * Interrupt handler is not required for user space
	 * WDT accesses, since the caller is responsible to ping the
	 * WDT to reset the counter before expiration, through ioctls.
	 */
	value = WDT_TIMER_ID | (wdd->timeout << WDT_CFG_PERIOD_SHIFT) |
		WDT_CFG_PMC2CAR_RST_EN;
	writel(value, wdt->base + WDT_CFG);

	writel(WDT_CMD_START_COUNTER, wdt->base + WDT_CMD);

	return 0;
}

static int tegra_wdt_stop(struct watchdog_device *wdd)
{
	struct tegra_wdt *wdt = to_tegra_wdt(wdd);

	writel(WDT_UNLOCK_PATTERN, wdt->base + WDT_UNLOCK);
	writel(WDT_CMD_DISABLE_COUNTER, wdt->base + WDT_CMD);
	writel(0, wdt->timer + TIMER_PTV);

	return 0;
}

static int tegra_wdt_ping(struct watchdog_device *wdd)
{
	struct tegra_wdt *wdt = to_tegra_wdt(wdd);

	writel(WDT_CMD_START_COUNTER, wdt->base + WDT_CMD);

	return 0;
}

static int tegra_wdt_set_timeout(struct watchdog_device *wdd,
				 unsigned int timeout)
{
	wdd->timeout = timeout;

	if (watchdog_active(wdd)) {
		tegra_wdt_stop(wdd);
		return tegra_wdt_start(wdd);
	}

	return 0;
}

static unsigned int tegra_wdt_get_timeleft(struct watchdog_device *wdd)
{
	struct tegra_wdt *wdt = to_tegra_wdt(wdd);
	unsigned int count, exp;
	u32 value;

	value = readl(wdt->base + WDT_STS);

	/* Current countdown (from timeout) */
	count = (value >> WDT_STS_COUNT_SHIFT) & WDT_STS_COUNT_MASK;

	/* Number of expirations (we are waiting for the 4th expiration) */
	exp = (value >> WDT_STS_EXP_SHIFT) & WDT_STS_EXP_MASK;

	/*
	 * The entire thing is divided by 4 because we are ticking down 4 times
	 * faster due to needing to wait for the 4th expiration.
	 */
	return (((3 - exp) * wdd->timeout) + count) / 4;
}

static const struct watchdog_ops tegra_wdt_ops = {
	.owner = THIS_MODULE,
	.start = tegra_wdt_start,
	.stop = tegra_wdt_stop,
	.ping = tegra_wdt_ping,
	.set_timeout = tegra_wdt_set_timeout,
	.get_timeleft = tegra_wdt_get_timeleft,
};

static struct tegra_wdt *tegra_wdt_probe(struct device *dev, void __iomem *base)
{
	struct tegra_wdt *wdt;
	int err;

	wdt = devm_kzalloc(dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return ERR_PTR(-ENOMEM);

	wdt->timer = base + TIMER5_BASE;
	wdt->base = base + WDT_BASE;

	wdt->wdd.info = &tegra_wdt_info;
	wdt->wdd.ops = &tegra_wdt_ops;
	wdt->wdd.min_timeout = 1;
	wdt->wdd.max_timeout = 255;
	wdt->wdd.timeout = 120;
	wdt->wdd.parent = dev;

	watchdog_set_nowayout(&wdt->wdd, WATCHDOG_NOWAYOUT);
	watchdog_set_drvdata(&wdt->wdd, timer);

	err = watchdog_register_device(&wdt->wdd);
	if (err < 0)
		return ERR_PTR(err);

	return wdt;
}

struct tegra_timer_soc {
	bool has_watchdog;
};

/*
 * Tegra20 has a watchdog but it is different from the one found on Tegra30
 * and later. Most of it is implemented in the clock and reset controller, but
 * it relies on either timer 1 or timer 2 as source.
 *
 * For now don't register a watchdog device on Tegra20 until it has been. Even
 * if it was implemented at some point in the future it would most likely be
 * registered from the clock and reset controller driver.
 */
static const struct tegra_timer_soc tegra20_timer_soc = {
	.has_watchdog = false,
};

static const struct tegra_timer_soc tegra30_timer_soc = {
	.has_watchdog = true,
};

static const struct of_device_id tegra_timer_of_match[] = {
	{ .compatible = "nvidia,tegra124-timer", .data = &tegra30_timer_soc },
	{ .compatible = "nvidia,tegra114-timer", .data = &tegra30_timer_soc },
	{ .compatible = "nvidia,tegra30-timer", .data = &tegra30_timer_soc },
	{ .compatible = "nvidia,tegra20-timer", .data = &tegra20_timer_soc },
	{ }
};

static int tegra_timer_probe(struct platform_device *pdev)
{
	const struct tegra_timer_soc *soc;
	const struct of_device_id *match;
	struct resource *regs;
	void __iomem *base;

	match = of_match_device(tegra_timer_of_match, &pdev->dev);
	if (!match)
		return -ENODEV;

	soc = match->data;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(base))
		return PTR_ERR(base);

	if (IS_ENABLED(CONFIG_TEGRA_WATCHDOG) && soc->has_watchdog) {
		timer->wdt = tegra_wdt_probe(&pdev->dev, base);
		if (IS_ERR(timer->wdt))
			return PTR_ERR(timer->wdt);
	}

	platform_set_drvdata(pdev, timer);
	timer->base = base;

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra_timer_suspend(struct device *dev)
{
	struct tegra_timer *timer = dev_get_drvdata(dev);

	if (IS_ENABLED(CONFIG_TEGRA_WATCHDOG) && timer->wdt) {
		struct watchdog_device *wdd = &timer->wdt->wdd;

		if (watchdog_active(wdd))
			tegra_wdt_stop(wdd);
	}

	timer->usec_cfg = timer_readl(timer, TIMERUS_USEC_CFG);

	return 0;
}

static int tegra_timer_resume(struct device *dev)
{
	struct tegra_timer *timer = dev_get_drvdata(dev);

	timer_writel(timer, timer->usec_cfg, TIMERUS_USEC_CFG);

	if (IS_ENABLED(CONFIG_TEGRA_WATCHDOG) && timer->wdt) {
		struct watchdog_device *wdd = &timer->wdt->wdd;

		if (watchdog_active(wdd))
			tegra_wdt_start(wdd);
	}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(tegra_timer_pm_ops, tegra_timer_suspend,
			 tegra_timer_resume);

static struct platform_driver tegra_timer_driver = {
	.driver = {
		.name = "tegra-timer",
		.pm = &tegra_timer_pm_ops,
		.of_match_table = tegra_timer_of_match,
		.suppress_bind_attrs = true,
	},
	.probe = tegra_timer_probe,
};
module_platform_driver(tegra_timer_driver);
