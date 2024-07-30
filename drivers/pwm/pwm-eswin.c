/*
 * Copyright (c) 2022, ESWIN, Inc.
 * Author: zhangchunyun@eswincomputing.com
 */

#include <linux/bitops.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/time.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>

#define	NUM_PWM_CHANNEL	3
#define ESWIN_TIM_LD_CNT(n)   ((n) * 0x14)
#define ESWIN_TIM_LD_CNT2(n)  (((n) * 4) + 0xb0)
#define ESWIN_TIM_CUR_VAL(n)  (((n) * 0x14) + 0x04)
#define ESWIN_TIM_CTRL(n)     (((n) * 0x14) + 0x08)
#define ESWIN_TIM_EOI(n)      (((n) * 0x14) + 0x0c)
#define ESWIN_TIM_INT_STS(n)  (((n) * 0x14) + 0x10)

#define ESWIN_TIMERS_INT_STS  0xa0
#define ESWIN_TIMERS_EOI      0xa4
#define ESWIN_TIMERS_RAW_INT_STS  0xa8
#define ESWIN_TIMERS_COMP_VERSION 0xac

#define ESWIN_TIMERS_TOTAL    8
#define NSEC_TO_SEC	1000000000

/* Timer Control Register */
#define ESWIN_TIM_CTRL_EN     BIT(0)
#define ESWIN_TIM_CTRL_MODE   BIT(1)
#define ESWIN_TIM_CTRL_MODE_FREE  (0 << 1)
#define ESWIN_TIM_CTRL_MODE_USER  (1 << 1)
#define ESWIN_TIM_CTRL_INT_MASK   BIT(2)
#define ESWIN_TIM_CTRL_PWM    BIT(3)

struct eswin_pwm_ctx {
	u32 cnt;
	u32 cnt2;
	u32 ctrl;
};

struct eswin_pwm {
	void __iomem *base;
	struct clk *clk;
	struct clk *pclk;
	struct eswin_pwm_ctx ctx[ESWIN_TIMERS_TOTAL];
	struct reset_control * pwm_rst;
	u32 clk_period_ns;
};

#define to_eswin_pwm(p)   pwmchip_get_drvdata(p)

static inline u32 eswin_pwm_readl(struct eswin_pwm *eswin, u32 offset)
{
	return readl(eswin->base + offset);
}

static inline void eswin_pwm_writel(struct eswin_pwm *eswin, u32 value, u32 offset)
{
	writel(value, eswin->base + offset);
}

static void __eswin_pwm_set_enable(struct eswin_pwm *eswin, int pwm, int enabled)
{
	u32 reg;

	reg = eswin_pwm_readl(eswin, ESWIN_TIM_CTRL(pwm));

	if (enabled)
		reg |= ESWIN_TIM_CTRL_EN;
	else
		reg &= ~ESWIN_TIM_CTRL_EN;

	eswin_pwm_writel(eswin, reg, ESWIN_TIM_CTRL(pwm));
	reg = eswin_pwm_readl(eswin, ESWIN_TIM_CTRL(pwm));
}

static int __eswin_pwm_configure_timer(struct eswin_pwm *eswin,
       struct pwm_device *pwm,
       const struct pwm_state *state)
{
	u64 tmp;
	u32 ctrl;
	u32 high;
	u32 low;

	/*
	¦* Calculate width of low and high period in terms of input clock
	¦* periods and check are the result within HW limits between 1 and
	¦* 2^32 periods.
	¦*/

	tmp = DIV_ROUND_CLOSEST_ULL(state->duty_cycle, eswin->clk_period_ns);
	if (tmp < 1 || tmp > (1ULL << 32))
              return -ERANGE;
	high = tmp - 1;

	tmp = DIV_ROUND_CLOSEST_ULL(state->period - state->duty_cycle,
         eswin->clk_period_ns);
	if (tmp < 1 || tmp > (1ULL << 32))
        return -ERANGE;
	low = tmp - 1;
	/*
	¦* Specification says timer usage flow is to disable timer, then
	¦* program it followed by enable. It also says Load Count is loaded
	¦* into timer after it is enabled - either after a disable or
	¦* a reset. Based on measurements it happens also without disable
	¦* whenever Load Count is updated. But follow the specification.
	¦*/
	__eswin_pwm_set_enable(eswin, pwm->hwpwm, false);

	/*
	¦* Write Load Count and Load Count 2 registers. Former defines the
	¦* width of low period and latter the width of high period in terms
	¦* multiple of input clock periods:
	¦* Width = ((Count + 1) * input clock period).
	¦*/
	eswin_pwm_writel(eswin, low, ESWIN_TIM_LD_CNT(pwm->hwpwm));
	eswin_pwm_writel(eswin, high, ESWIN_TIM_LD_CNT2(pwm->hwpwm));

	/*
	¦* Set user-defined mode, timer reloads from Load Count registers
	¦* when it counts down to 0.
	¦* Set PWM mode, it makes output to toggle and width of low and high
	¦* periods are set by Load Count registers.
	¦*/
	ctrl = ESWIN_TIM_CTRL_MODE_USER | ESWIN_TIM_CTRL_PWM;
	eswin_pwm_writel(eswin, ctrl, ESWIN_TIM_CTRL(pwm->hwpwm));

	/*
	¦* Enable timer. Output starts from low period.
	¦*/
	__eswin_pwm_set_enable(eswin, pwm->hwpwm, state->enabled);

	return 0;
}

static int eswin_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
	const struct pwm_state *state)
{
	struct eswin_pwm *eswin = to_eswin_pwm(chip);
	struct pwm_state curstate;
	int ret = 0;

	ret = clk_enable(eswin->pclk);

	ret = clk_enable(eswin->clk);

	pwm_get_state(pwm, &curstate);

	__eswin_pwm_configure_timer(eswin, pwm, state);

	return 0;
}

static int eswin_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
	struct pwm_state *state)
{
	struct eswin_pwm *eswin = to_eswin_pwm(chip);
	u64 duty, period;

	pm_runtime_get_sync(pwmchip_parent(chip));

	state->enabled = !!(eswin_pwm_readl(eswin,
		ESWIN_TIM_CTRL(pwm->hwpwm)) & ESWIN_TIM_CTRL_EN);

	duty = eswin_pwm_readl(eswin, ESWIN_TIM_LD_CNT(pwm->hwpwm));
	duty += 1;
	duty *= eswin->clk_period_ns;
	state->duty_cycle = duty;

	period = eswin_pwm_readl(eswin, ESWIN_TIM_LD_CNT2(pwm->hwpwm));
	period += 1;
	period *= eswin->clk_period_ns;
	period += duty;
	state->period = period;

	state->polarity = PWM_POLARITY_INVERSED;

	pm_runtime_put_sync(pwmchip_parent(chip));

	return 0;
}


static const struct pwm_ops eswin_pwm_ops = {
	.get_state = eswin_pwm_get_state,
	.apply = eswin_pwm_apply,
};

static const struct of_device_id eswin_pwm_dt_ids[] = {
	{ .compatible = "eswin,pwm-eswin", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, eswin_pwm_dt_ids);

static int eswin_pwm_probe(struct platform_device *pdev)
{
	struct eswin_pwm *pc;
	struct pwm_chip *chip;
	int ret, count;
	struct resource *res;
	int clk_rate;

	chip = devm_pwmchip_alloc(&pdev->dev, NUM_PWM_CHANNEL, sizeof(*pc));
	if(IS_ERR(chip))
		return PTR_ERR(chip);

	pc = pwmchip_get_drvdata(chip);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	pc->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pc->base))
		return PTR_ERR(pc->base);

	pc->clk = devm_clk_get(&pdev->dev, "pwm");
	if (IS_ERR(pc->clk)) {
		pc->clk = devm_clk_get(&pdev->dev, "pclk");
        if (IS_ERR(pc->clk))
			return dev_err_probe(&pdev->dev, PTR_ERR(pc->clk),
                    "Can't get PWM clk\n");
	}

	count = of_count_phandle_with_args(pdev->dev.of_node,
                      "clocks", "#clock-cells");
	if (count == 2)
		pc->pclk = devm_clk_get(&pdev->dev, "pclk");
	else
		pc->pclk = pc->clk;

	if (IS_ERR(pc->pclk)) {
		ret = PTR_ERR(pc->pclk);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Can't get APB clk: %d\n", ret);
		return ret;
	}

	clk_rate = clk_get_rate(pc->pclk);
	pc->clk_period_ns = DIV_ROUND_CLOSEST_ULL(NSEC_TO_SEC, clk_rate);

	/* pwm reset init */
	pc->pwm_rst = devm_reset_control_get_optional(&pdev->dev, "pwmrst");
	if(IS_ERR_OR_NULL(pc->pwm_rst)) {
		dev_err(&pdev->dev, "Failed to get pwmrst reset handle\n");
		return -EFAULT;
	}

	ret = clk_prepare_enable(pc->clk);
	if (ret) {
		dev_err(&pdev->dev, "Can't prepare enable PWM clk: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(pc->pclk);
	if (ret) {
		dev_err(&pdev->dev, "Can't prepare enable APB clk: %d\n", ret);
		goto err_clk;
	}

	/* reset pwm */
	ret = reset_control_assert(pc->pwm_rst);
	WARN_ON(0 != ret);
	ret = reset_control_deassert(pc->pwm_rst);
	WARN_ON(0 != ret);

	platform_set_drvdata(pdev, pc);

	chip->ops = &eswin_pwm_ops;

	ret = pwmchip_add(chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "pwmchip_add() failed: %d\n", ret);
		goto err_pclk;
	}

	return 0;

err_pclk:
    clk_disable_unprepare(pc->pclk);
err_clk:
    clk_disable_unprepare(pc->clk);

	return ret;
}

static void eswin_pwm_remove(struct platform_device *pdev)
{
	struct pwm_chip *chip = platform_get_drvdata(pdev);
	struct eswin_pwm *pc = to_eswin_pwm(chip);

	pwmchip_remove(chip);

	clk_disable_unprepare(pc->pclk);
	clk_disable_unprepare(pc->clk);
}

static struct platform_driver eswin_pwm_driver = {
	.driver = {
		.name = "eswin-pwm",
		.of_match_table = eswin_pwm_dt_ids,
	},
	.probe = eswin_pwm_probe,
	.remove = eswin_pwm_remove,
};
module_platform_driver(eswin_pwm_driver);

MODULE_DESCRIPTION("eswin SoC PWM driver");
MODULE_AUTHOR("zhangchunyun@eswincomputing.com");
MODULE_LICENSE("GPL");

