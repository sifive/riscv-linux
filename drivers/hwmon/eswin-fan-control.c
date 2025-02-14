// SPDX-License-Identifier: GPL-2.0
/*
 * Fan Control CORE driver
 *
 * Copyright 2022 ESWIN Inc.
 */
#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/wait.h>

#define FAN_PWM_DUTY           0x0
#define FAN_PWM_PERIOD         0x1
#define FAN_PWM_FREE	       0x2

/* register map */
#define REG_FAN_INT            0x0
#define REG_FAN_RPM            0x4

/* wait for 50 times pwm period to trigger read interrupt */
#define TIMEOUT(period)        nsecs_to_jiffies(50*(period))

struct eswin_fan_control_data {
	struct reset_control *fan_rst;
	struct clk *clk;
	void __iomem *base;
	struct device *hdev;
	unsigned long clk_rate;
	int pwm_id;
	struct pwm_device *pwm;
	wait_queue_head_t wq;
	bool wait_flag;
	int irq;
	/* pwm minimum period */
	u32 min_period;
	/* pulses per revolution */
	u32 ppr;
	/* revolutions per minute */
	u32 rpm;
};

static inline void fan_iowrite(const u32 val, const u32 reg,
			       const struct eswin_fan_control_data *ctl)
{
	iowrite32(val, ctl->base + reg);
}

static inline u32 fan_ioread(const u32 reg,
			     const struct eswin_fan_control_data *ctl)
{
	return ioread32(ctl->base + reg);
}

static ssize_t eswin_fan_pwm_ctl_show(struct device *dev, struct device_attribute *da, char *buf)
{
	struct eswin_fan_control_data *ctl = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	long temp = 0;

	if (FAN_PWM_DUTY == attr->index) {
		temp = pwm_get_duty_cycle(ctl->pwm);
	}
	else if (FAN_PWM_PERIOD == attr->index) {
		temp = pwm_get_period(ctl->pwm);
	}
	else {
		dev_err(dev, "get error attr index 0x%x\n", attr->index);
	}

	return sprintf(buf, "%lu\n", temp);
}

static ssize_t eswin_fan_pwm_ctl_store(struct device *dev, struct device_attribute *da,
				     const char *buf, size_t count)
{
	struct eswin_fan_control_data *ctl = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct pwm_state state;
	int ret;

	pwm_get_state(ctl->pwm, &state);

	if (FAN_PWM_DUTY == attr->index) {
		long val = 0;
		ret = kstrtoul(buf, 10, &val);
		if (ret)
			return ret;

		state.duty_cycle = val;
	}
	else if (FAN_PWM_PERIOD == attr->index) {
		long val = 0;
		ret = kstrtoul(buf, 10, &val);
		if (ret)
			return ret;
		if (val >= ctl->min_period)
			state.period = val;
		else
			dev_err(dev, "invalid pwm period!\n");
	}
	else {
		dev_err(dev, "get error attr index 0x%x\n", attr->index);
	}

	pwm_apply_might_sleep(ctl->pwm, &state);

	return count;
}

static ssize_t eswin_fan_pwm_free_store(struct device *dev, struct device_attribute *da,
				     const char *buf, size_t count)
{
	struct eswin_fan_control_data *ctl = dev_get_drvdata(dev);
	long val;
	int ret;
	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	if (val) {
		pwm_put(ctl->pwm);
	}

	return count;
}

static long eswin_fan_control_get_pwm_duty(const struct eswin_fan_control_data *ctl)
{
	struct pwm_state state;
	int duty;

	pwm_get_state(ctl->pwm, &state);
	duty = pwm_get_relative_duty_cycle(&state, 100);

	return duty;
}

static long eswin_fan_control_get_fan_rpm(struct eswin_fan_control_data *ctl)
{
	unsigned int val;
	long period, timeout;
	int ret;

	ctl->wait_flag = false;
	period = pwm_get_period(ctl->pwm);
	timeout = msecs_to_jiffies(1500);

	val = fan_ioread(REG_FAN_INT, ctl);
	val = val | 0x1;
	fan_iowrite(val, REG_FAN_INT, ctl);

	/* wait read interrupt */
	ret = wait_event_interruptible_timeout(ctl->wq,
                                        ctl->wait_flag,
                                        timeout);

	if (!ret){
		/* timeout, set rpm to 0 */
		ctl->rpm = 0;
	}

	if(ctl->rpm)
		ctl->rpm = DIV_ROUND_CLOSEST(60 * ctl->clk_rate, ctl->ppr * ctl->rpm);

	return ret;
}

static int eswin_fan_control_read_fan(struct device *dev, u32 attr, long *val)
{
	struct eswin_fan_control_data *ctl = dev_get_drvdata(dev);

	switch (attr) {
	case hwmon_fan_input:
		if(!eswin_fan_control_get_fan_rpm(ctl)){
			dev_err(dev, "wait read interrupt timeout!\n");
		}
		*val = ctl->rpm;
		return 0;
	default:
		return -ENOTSUPP;
	}
}

static int eswin_fan_control_read_pwm(struct device *dev, u32 attr, long *val)
{
	struct eswin_fan_control_data *ctl = dev_get_drvdata(dev);

	switch (attr) {
	case hwmon_pwm_input:
		*val = eswin_fan_control_get_pwm_duty(ctl);
		return 0;
	default:
		return -ENOTSUPP;
	}
}

static int eswin_fan_control_set_pwm_duty(const long val, struct eswin_fan_control_data *ctl)
{
	struct pwm_state state;

	pwm_get_state(ctl->pwm, &state);
	pwm_set_relative_duty_cycle(&state, val, 100);
	pwm_apply_might_sleep(ctl->pwm, &state);

	return 0;
}

static int eswin_fan_control_write_pwm(struct device *dev, u32 attr, long val)
{
	struct eswin_fan_control_data *ctl = dev_get_drvdata(dev);

	switch (attr) {
		case hwmon_pwm_input:
	if((val < 0)||(val > 100))
		return -EINVAL;
	else
		return eswin_fan_control_set_pwm_duty(val, ctl);
	default:
		return -ENOTSUPP;
	}

	return 0;
}

static int eswin_fan_control_read_labels(struct device *dev,
					enum hwmon_sensor_types type,
					u32 attr, int channel, const char **str)
{
	switch (type) {
	case hwmon_fan:
		*str = "FAN";
		return 0;
	default:
		return -ENOTSUPP;
	}
}

static int eswin_fan_control_read(struct device *dev,
				enum hwmon_sensor_types type,
				u32 attr, int channel, long *val)
{
	switch (type) {
	case hwmon_fan:
		return eswin_fan_control_read_fan(dev, attr, val);
	case hwmon_pwm:
		return eswin_fan_control_read_pwm(dev, attr, val);
	default:
		return -ENOTSUPP;
	}
}

static int eswin_fan_control_write(struct device *dev,
				 enum hwmon_sensor_types type,
				 u32 attr, int channel, long val)
{
	switch (type) {
	case hwmon_pwm:
		return eswin_fan_control_write_pwm(dev, attr, val);
	default:
		return -ENOTSUPP;
	}
}

static umode_t eswin_fan_control_fan_is_visible(const u32 attr)
{
	switch (attr) {
	case hwmon_fan_input:
	case hwmon_fan_label:
		return 0444;
	default:
		return 0;
	}
}

static umode_t eswin_fan_control_pwm_is_visible(const u32 attr)
{
	switch (attr) {
	case hwmon_pwm_input:
		return 0644;
	default:
		return 0;
	}
}

static umode_t eswin_fan_control_is_visible(const void *data,
					enum hwmon_sensor_types type,
					u32 attr, int channel)
{
	switch (type) {
	case hwmon_fan:
		return eswin_fan_control_fan_is_visible(attr);
	case hwmon_pwm:
		return eswin_fan_control_pwm_is_visible(attr);
	default:
		return 0;
	}
}

static irqreturn_t eswin_fan_control_irq_handler(int irq, void *data)
{
	struct eswin_fan_control_data *ctl = (struct eswin_fan_control_data *)data;
	u32 status = 0;

	status = fan_ioread(REG_FAN_INT, ctl);
	if (0x3 == (status & 0x3)){
		ctl->rpm = fan_ioread(REG_FAN_RPM, ctl);

		/* clear interrupt */
		fan_iowrite(0x5, REG_FAN_INT, ctl);

		/* wake up fan_rpm read */
		ctl->wait_flag = true;
		wake_up_interruptible(&ctl->wq);
	}

	return IRQ_HANDLED;
}

static int eswin_fan_control_init(struct eswin_fan_control_data *ctl,
				const struct device_node *np)
{
	int ret;
	/* get fan pulses per revolution */
	ret = of_property_read_u32(np, "pulses-per-revolution", &ctl->ppr);
	if (ret)
		return ret;

	/* 1, 2 and 4 are the typical and accepted values */
	if (ctl->ppr != 1 && ctl->ppr != 2 && ctl->ppr != 4)
		return -EINVAL;

	/* get pwm minimum period */
	ret = of_property_read_u32(np, "pwm-minimum-period", &ctl->min_period);
	if (ret)
		return ret;

	return ret;
}

static void eswin_fan_control_remove(void *data)
{
	int ret;
	struct eswin_fan_control_data *ctl = data;
	pwm_put(ctl->pwm);
	ret = reset_control_assert(ctl->fan_rst);
	WARN_ON(0 != ret);
	clk_disable_unprepare(ctl->clk);
}

static const struct hwmon_channel_info *eswin_fan_control_info[] = {
	HWMON_CHANNEL_INFO(pwm, HWMON_PWM_INPUT),
	HWMON_CHANNEL_INFO(fan, HWMON_F_INPUT | HWMON_F_LABEL),
	NULL
};

static const struct hwmon_ops eswin_fan_control_hwmon_ops = {
	.is_visible = eswin_fan_control_is_visible,
	.read = eswin_fan_control_read,
	.write = eswin_fan_control_write,
	.read_string = eswin_fan_control_read_labels,
};

static const struct hwmon_chip_info eswin_chip_info = {
	.ops = &eswin_fan_control_hwmon_ops,
	.info = eswin_fan_control_info,
};

static SENSOR_DEVICE_ATTR_RW(fan_pwm_duty,   eswin_fan_pwm_ctl,    FAN_PWM_DUTY);
static SENSOR_DEVICE_ATTR_RW(fan_pwm_period, eswin_fan_pwm_ctl,    FAN_PWM_PERIOD);
static SENSOR_DEVICE_ATTR_WO(fan_pwm_free,   eswin_fan_pwm_free,   FAN_PWM_FREE);

static struct attribute *eswin_fan_control_attrs[] = {
	&sensor_dev_attr_fan_pwm_duty.dev_attr.attr,
	&sensor_dev_attr_fan_pwm_period.dev_attr.attr,
	&sensor_dev_attr_fan_pwm_free.dev_attr.attr,
	NULL,
};
ATTRIBUTE_GROUPS(eswin_fan_control);

static const struct of_device_id eswin_fan_control_of_match[] = {
	{ .compatible = "eswin-fan-control"},
	{}
};
MODULE_DEVICE_TABLE(of, eswin_fan_control_of_match);

static int eswin_fan_control_probe(struct platform_device *pdev)
{
	struct eswin_fan_control_data *ctl;
	const struct of_device_id *id;
	const char *name = "eswin_fan_control";
	int ret;

	id = of_match_node(eswin_fan_control_of_match, pdev->dev.of_node);
	if (!id)
		return -EINVAL;

	ctl = devm_kzalloc(&pdev->dev, sizeof(*ctl), GFP_KERNEL);
	if (!ctl)
		return -ENOMEM;

	ctl->base = devm_platform_ioremap_resource(pdev, 0);

	if (IS_ERR(ctl->base))
		return PTR_ERR(ctl->base);

	ctl->clk = devm_clk_get(&pdev->dev, "pclk");
	if (IS_ERR(ctl->clk)) {
		dev_err(&pdev->dev, "Couldn't get the clock for fan-controller\n");
		return -ENODEV;
	}

	ret = clk_prepare_enable(ctl->clk);
	if (ret) {
		dev_err(&pdev->dev, "Failed to enable clock for fan-controller\n");
		return ret;
	}

	ctl->clk_rate = clk_get_rate(ctl->clk);
	if (!ctl->clk_rate)
		return -EINVAL;

	ctl->fan_rst = devm_reset_control_get_optional(&pdev->dev, "fan_rst");
	if (IS_ERR_OR_NULL(ctl->fan_rst)) {
		dev_err(&pdev->dev, "Failed to get fan_rst reset handle\n");
		return -EFAULT;
	}
	ret = reset_control_reset(ctl->fan_rst);
	WARN_ON(0 != ret);

	init_waitqueue_head(&ctl->wq);

	ctl->irq = platform_get_irq(pdev, 0);
	if (ctl->irq < 0)
		return ctl->irq;

	ret = devm_request_threaded_irq(&pdev->dev, ctl->irq,
					eswin_fan_control_irq_handler, NULL,
					IRQF_ONESHOT | IRQF_TRIGGER_HIGH,
					pdev->driver_override, ctl);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request an irq, %d", ret);
		return ret;
	}

	ret = eswin_fan_control_init(ctl, pdev->dev.of_node);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize device\n");
		return ret;
	}

	ctl->pwm = pwm_get(&pdev->dev, NULL);
	if (IS_ERR(ctl->pwm)) {
		ret = PTR_ERR(ctl->pwm);
		dev_err(&pdev->dev, "Failed to request pwm device: %d\n", ret);
		return ret;
	}
	pwm_enable(ctl->pwm);

	ret = devm_add_action_or_reset(&pdev->dev, eswin_fan_control_remove, ctl);
	if (ret)
		return ret;

	ctl->hdev = devm_hwmon_device_register_with_info(&pdev->dev,
							 name,
							 ctl,
							 &eswin_chip_info,
							 eswin_fan_control_groups);
	return PTR_ERR_OR_ZERO(ctl->hdev);
}

static struct platform_driver eswin_fan_control_driver = {
	.driver = {
		.name = "eswin_fan_control_driver",
		.of_match_table = eswin_fan_control_of_match,
	},
	.probe = eswin_fan_control_probe,
};
module_platform_driver(eswin_fan_control_driver);

MODULE_AUTHOR("Han Min <hanmin@eswincomputing.com>");
MODULE_DESCRIPTION("ESWIN Fan Control CORE driver");
MODULE_LICENSE("GPL");
