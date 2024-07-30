// SPDX-License-Identifier: GPL-2.0
/*
 * ESWIN PVT Device Driver
 *
 * Copyright 2024, Beijing ESWIN Computing Technology Co., Ltd.. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Authors: Bingzheng Yu <yubingzheng@eswincomputing.com>
 */

#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/seqlock.h>
#include <linux/sysfs.h>
#include <linux/types.h>
#include "eswin_pvt.h"

/*
 * For the sake of the code simplification we created the sensors info table
 * with the sensor names, activation modes, threshold registers base address
 * and the thresholds bit fields.
 */
static const struct pvt_sensor_info pvt_info[] = {
	PVT_SENSOR_INFO(0, "CPU Core Temperature", hwmon_temp, TEMP, TTHRES),
	PVT_SENSOR_INFO(0, "CPU Core Voltage", hwmon_in, VOLT, VTHRES),
	PVT_SENSOR_INFO(1, "CPU Core Low-Vt", hwmon_in, LVT, LTHRES),
	PVT_SENSOR_INFO(2, "CPU Core UltraLow-Vt", hwmon_in, ULVT, ULTHRES),
	PVT_SENSOR_INFO(3, "CPU Core Standard-Vt", hwmon_in, SVT, STHRES),
};

/*
 * The original translation formulae of the temperature (in degrees of Celsius)
 * to PVT data and vice-versa are following:
 * N = 6.0818e-8*(T^4) +1.2873e-5*(T^3) + 7.2244e-3*(T^2) + 3.6484*(T^1) +
 *     1.6198e2,
 * T = -1.8439e-11*(N^4) + 8.0705e-8*(N^3) + -1.8501e-4*(N^2) +
 *     3.2843e-1*(N^1) - 4.8690e1,
 * where T = [-40, 125]C and N = [27, 771].
 * They must be accordingly altered to be suitable for the integer arithmetics.
 * The technique is called 'factor redistribution', which just makes sure the
 * multiplications and divisions are made so to have a result of the operations
 * within the integer numbers limit. In addition we need to translate the
 * formulae to accept millidegrees of Celsius. Here what they look like after
 * the alterations:
 * N = (60818e-20*(T^4) + 12873e-14*(T^3) + 72244e-9*(T^2) + 36484e-3*T +
 *     16198e2) / 1e4,
 * T = -18439e-12*(N^4) + 80705e-9*(N^3) - 185010e-6*(N^2) + 328430e-3*N -
 *     48690,
 * where T = [-40000, 125000] mC and N = [27, 771].
 */
static const struct pvt_poly __maybe_unused poly_temp_to_N = {
	.total_divider = 10000,
	.terms = {
		{4, 60818, 10000, 10000},
		{3, 12873, 10000, 100},
		{2, 72244, 10000, 10},
		{1, 36484, 1000, 1},
		{0, 1619800, 1, 1}
	}
};

static const struct pvt_poly poly_N_to_temp = {
	.total_divider = 1,
	.terms = {
		{4, -18439, 1000, 1},
		{3, 80705, 1000, 1},
		{2, -185010, 1000, 1},
		{1, 328430, 1000, 1},
		{0, -48690, 1, 1}
	}
};

/*
 * Similar alterations are performed for the voltage conversion equations.
 * The original formulae are:
 * N = 1.3905e3*V - 5.7685e2,
 * V = (N + 5.7685e2) / 1.3905e3,
 * where V = [0.72, 0.88] V and N = [424, 646].
 * After the optimization they looks as follows:
 * N = (13905e-3*V - 5768.5) / 10,
 * V = (N * 10^5 / 13905 + 57685 * 10^3 / 13905) / 10.
 * where V = [720, 880] mV and N = [424, 646].
 */
static const struct pvt_poly __maybe_unused poly_volt_to_N = {
	.total_divider = 10,
	.terms = {
		{1, 13905, 1000, 1},
		{0, -57685, 1, 10}
	}
};

static const struct pvt_poly poly_N_to_volt = {
	.total_divider = 10,
	.terms = {
		{1, 100000, 13905, 1},
		{0, 57685000, 1, 13905}
	}
};

/*
 * Here is the polynomial calculation function, which performs the
 * redistributed terms calculations. It's pretty straightforward. We walk
 * over each degree term up to the free one, and perform the redistributed
 * multiplication of the term coefficient, its divider (as for the rationale
 * fraction representation), data power and the rational fraction divider
 * leftover. Then all of this is collected in a total sum variable, which
 * value is normalized by the total divider before being returned.
 */
static long eswin_pvt_calc_poly(const struct pvt_poly *poly, long data)
{
	const struct pvt_poly_term *term = poly->terms;
	long tmp, ret = 0;
	int deg;
	do {
		tmp = term->coef;
		for (deg = 0; deg < term->deg; ++deg)
			tmp = mult_frac(tmp, data, term->divider);
		ret += tmp / term->divider_leftover;
	} while ((term++)->deg);

	return ret / poly->total_divider;
}

static inline u32 eswin_pvt_update(void __iomem *reg, u32 mask, u32 data)
{
	u32 old;

	old = readl_relaxed(reg);
	writel((old & ~mask) | (data & mask), reg);

	return old & mask;
}

static inline void eswin_pvt_set_mode(struct pvt_hwmon *pvt, u32 mode)
{
	u32 old;

	mode = FIELD_PREP(PVT_MODE_MASK, mode);

	old = eswin_pvt_update(pvt->regs + PVT_ENA, PVT_ENA_EN, 0);
	eswin_pvt_update(pvt->regs + PVT_MODE, PVT_MODE_MASK, mode);
	eswin_pvt_update(pvt->regs + PVT_ENA, PVT_ENA_EN, old);
}

static inline u32 eswin_pvt_calc_trim(long temp)
{
	temp = clamp_val(temp, 0, PVT_TRIM_TEMP);

	return DIV_ROUND_UP(temp, PVT_TRIM_STEP);
}

static inline void eswin_pvt_set_trim(struct pvt_hwmon *pvt, u32 val)
{
	u32 old;

	old = eswin_pvt_update(pvt->regs + PVT_ENA, PVT_ENA_EN, 0);
	writel(val, pvt->regs + PVT_TRIM);
	eswin_pvt_update(pvt->regs + PVT_ENA, PVT_ENA_EN, old);
}

static irqreturn_t eswin_pvt_hard_isr(int irq, void *data)
{
	struct pvt_hwmon *pvt = data;
	struct pvt_cache *cache;
	u32 val;

	eswin_pvt_update(pvt->regs + PVT_INT, PVT_INT_CLR, PVT_INT_CLR);

	/*
	 * Nothing special for alarm-less driver. Just read the data, update
	 * the cache and notify a waiter of this event.
	 */

	val = readl(pvt->regs + PVT_DATA);

	cache = &pvt->cache[pvt->sensor];

	WRITE_ONCE(cache->data, FIELD_GET(PVT_DATA_OUT, val));

	complete(&cache->conversion);

	return IRQ_HANDLED;
}

static inline umode_t eswin_pvt_limit_is_visible(enum pvt_sensor_type type)
{
	return 0;
}

static inline umode_t eswin_pvt_pvt_alarm_is_visible(enum pvt_sensor_type type)
{
	return 0;
}

static int eswin_pvt_read_data(struct pvt_hwmon *pvt, enum pvt_sensor_type type,
			 long *val)
{
	struct pvt_cache *cache = &pvt->cache[type];
	unsigned long timeout;
	u32 data;
	int ret;

	/*
	 * Lock PVT conversion interface until data cache is updated. The
	 * data read procedure is following: set the requested PVT sensor
	 * mode, enable IRQ and conversion, wait until conversion is finished,
	 * then disable conversion and IRQ, and read the cached data.
	 */
	ret = mutex_lock_interruptible(&pvt->iface_mtx);
	if (ret)
		return ret;

	pvt->sensor = type;
	eswin_pvt_set_mode(pvt, pvt_info[type].mode);

	eswin_pvt_update(pvt->regs + PVT_ENA, PVT_ENA_EN, PVT_ENA_EN);

	/*
	 * Wait with timeout since in case if the sensor is suddenly powered
	 * down the request won't be completed and the caller will hang up on
	 * this procedure until the power is back up again. Multiply the
	 * timeout by the factor of two to prevent a false timeout.
	 */
	timeout = 2 * usecs_to_jiffies(ktime_to_us(pvt->timeout));
	if(type==PVT_TEMP){
		timeout = 20 * usecs_to_jiffies(ktime_to_us(pvt->timeout));
	}
	ret = wait_for_completion_timeout(&cache->conversion, timeout);

	eswin_pvt_update(pvt->regs + PVT_ENA, PVT_ENA_EN, 0);
	eswin_pvt_update(pvt->regs + PVT_INT, PVT_INT_CLR, PVT_INT_CLR);

	data = READ_ONCE(cache->data);

	mutex_unlock(&pvt->iface_mtx);

	if (!ret)
		return -ETIMEDOUT;

	if (type == PVT_TEMP)
		*val = eswin_pvt_calc_poly(&poly_N_to_temp, data);
	else if (type == PVT_VOLT)
		*val = eswin_pvt_calc_poly(&poly_N_to_volt, data);
	else
		*val = data;

	return 0;
}

static int eswin_pvt_read_limit(struct pvt_hwmon *pvt, enum pvt_sensor_type type,
			  bool is_low, long *val)
{
	return -EOPNOTSUPP;
}

static int eswin_pvt_write_limit(struct pvt_hwmon *pvt, enum pvt_sensor_type type,
			   bool is_low, long val)
{
	return -EOPNOTSUPP;
}

static int eswin_pvt_read_alarm(struct pvt_hwmon *pvt, enum pvt_sensor_type type,
			  bool is_low, long *val)
{
	return -EOPNOTSUPP;
}

static const struct hwmon_channel_info *pvt_channel_info[] = {
	HWMON_CHANNEL_INFO(chip,
			   HWMON_C_REGISTER_TZ | HWMON_C_UPDATE_INTERVAL),
	HWMON_CHANNEL_INFO(temp,
			   HWMON_T_INPUT | HWMON_T_TYPE | HWMON_T_LABEL |
			   HWMON_T_OFFSET),
	HWMON_CHANNEL_INFO(in,
			   HWMON_I_INPUT | HWMON_I_LABEL,
			   HWMON_I_INPUT | HWMON_I_LABEL,
			   HWMON_I_INPUT | HWMON_I_LABEL,
			   HWMON_I_INPUT | HWMON_I_LABEL),
	NULL
};

static inline bool eswin_pvt_hwmon_channel_is_valid(enum hwmon_sensor_types type,
					      int ch)
{
	switch (type) {
	case hwmon_temp:
		if (ch < 0 || ch >= PVT_TEMP_CHS)
			return false;
		break;
	case hwmon_in:
		if (ch < 0 || ch >= PVT_VOLT_CHS)
			return false;
		break;
	default:
		break;
	}

	/* The rest of the types are independent from the channel number. */
	return true;
}

static umode_t eswin_pvt_hwmon_is_visible(const void *data,
				    enum hwmon_sensor_types type,
				    u32 attr, int ch)
{
	if (!eswin_pvt_hwmon_channel_is_valid(type, ch))
		return 0;

	switch (type) {
	case hwmon_chip:
		switch (attr) {
		case hwmon_chip_update_interval:
			return 0644;
		}
		break;
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
		case hwmon_temp_type:
		case hwmon_temp_label:
			return 0444;
		case hwmon_temp_min:
		case hwmon_temp_max:
			return eswin_pvt_limit_is_visible(ch);
		case hwmon_temp_min_alarm:
		case hwmon_temp_max_alarm:
			return eswin_pvt_pvt_alarm_is_visible(ch);
		case hwmon_temp_offset:
			return 0644;
		}
		break;
	case hwmon_in:
		switch (attr) {
		case hwmon_in_input:
		case hwmon_in_label:
			return 0444;
		case hwmon_in_min:
		case hwmon_in_max:
			return eswin_pvt_limit_is_visible(PVT_VOLT + ch);
		case hwmon_in_min_alarm:
		case hwmon_in_max_alarm:
			return eswin_pvt_pvt_alarm_is_visible(PVT_VOLT + ch);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int eswin_pvt_read_trim(struct pvt_hwmon *pvt, long *val)
{
	u32 data;

	data = readl(pvt->regs + PVT_TRIM);
	*val = data;

	return 0;
}

static int eswin_pvt_write_trim(struct pvt_hwmon *pvt, long val)
{
	int ret;
	/*
	 * Serialize trim update, since a part of the register is changed and
	 * the controller is supposed to be disabled during this operation.
	 */
	ret = mutex_lock_interruptible(&pvt->iface_mtx);
	if (ret)
		return ret;

	eswin_pvt_set_trim(pvt, val);

	mutex_unlock(&pvt->iface_mtx);

	return 0;
}

static int eswin_pvt_read_timeout(struct pvt_hwmon *pvt, long *val)
{
	int ret;

	ret = mutex_lock_interruptible(&pvt->iface_mtx);
	if (ret)
		return ret;

	/* Return the result in msec as hwmon sysfs interface requires. */
	*val = ktime_to_ms(pvt->timeout);

	mutex_unlock(&pvt->iface_mtx);

	return 0;
}

static int eswin_pvt_write_timeout(struct pvt_hwmon *pvt, long val)
{
	unsigned long rate;
	ktime_t kt, cache;
	u32 data;
	int ret;

	rate = clk_get_rate(pvt->clk);
	if (!rate)
		return -ENODEV;

	/*
	 * If alarms are enabled, the requested timeout must be divided
	 * between all available sensors to have the requested delay
	 * applicable to each individual sensor.
	 */
	cache = kt = ms_to_ktime(val);

	/*
	 * Subtract a constant lag, which always persists due to the limited
	 * PVT sampling rate. Make sure the timeout is not negative.
	 */
	kt = ktime_sub_ns(kt, PVT_TOUT_MIN);
	if (ktime_to_ns(kt) < 0)
		kt = ktime_set(0, 0);

	/*
	 * Finally recalculate the timeout in terms of the reference clock
	 * period.
	 */
	data = ktime_divns(kt * rate, NSEC_PER_SEC);

	/*
	 * Update the measurements delay, but lock the interface first, since
	 * we have to disable PVT in order to have the new delay actually
	 * updated.
	 */
	ret = mutex_lock_interruptible(&pvt->iface_mtx);
	if (ret)
		return ret;

	pvt->timeout = cache;

	mutex_unlock(&pvt->iface_mtx);

	return 0;
}

static int eswin_pvt_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
			  u32 attr, int ch, long *val)
{
	struct pvt_hwmon *pvt = dev_get_drvdata(dev);

	if (!eswin_pvt_hwmon_channel_is_valid(type, ch))
		return -EINVAL;

	switch (type) {
	case hwmon_chip:
		switch (attr) {
		case hwmon_chip_update_interval:
			return eswin_pvt_read_timeout(pvt, val);
		}
		break;
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
			return eswin_pvt_read_data(pvt, ch, val);
		case hwmon_temp_type:
			*val = 1;
			return 0;
		case hwmon_temp_min:
			return eswin_pvt_read_limit(pvt, ch, true, val);
		case hwmon_temp_max:
			return eswin_pvt_read_limit(pvt, ch, false, val);
		case hwmon_temp_min_alarm:
			return eswin_pvt_read_alarm(pvt, ch, true, val);
		case hwmon_temp_max_alarm:
			return eswin_pvt_read_alarm(pvt, ch, false, val);
		case hwmon_temp_offset:
			return eswin_pvt_read_trim(pvt, val);
		}
		break;
	case hwmon_in:
		switch (attr) {
		case hwmon_in_input:
			return eswin_pvt_read_data(pvt, PVT_VOLT + ch, val);
		case hwmon_in_min:
			return eswin_pvt_read_limit(pvt, PVT_VOLT + ch, true, val);
		case hwmon_in_max:
			return eswin_pvt_read_limit(pvt, PVT_VOLT + ch, false, val);
		case hwmon_in_min_alarm:
			return eswin_pvt_read_alarm(pvt, PVT_VOLT + ch, true, val);
		case hwmon_in_max_alarm:
			return eswin_pvt_read_alarm(pvt, PVT_VOLT + ch, false, val);
		}
		break;
	default:
		break;
	}

	return -EOPNOTSUPP;
}

static int eswin_pvt_hwmon_read_string(struct device *dev,
				 enum hwmon_sensor_types type,
				 u32 attr, int ch, const char **str)
{
	if (!eswin_pvt_hwmon_channel_is_valid(type, ch))
		return -EINVAL;

	switch (type) {
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_label:
			*str = pvt_info[ch].label;
			return 0;
		}
		break;
	case hwmon_in:
		switch (attr) {
		case hwmon_in_label:
			*str = pvt_info[PVT_VOLT + ch].label;
			return 0;
		}
		break;
	default:
		break;
	}

	return -EOPNOTSUPP;
}

static int eswin_pvt_hwmon_write(struct device *dev, enum hwmon_sensor_types type,
			   u32 attr, int ch, long val)
{
	struct pvt_hwmon *pvt = dev_get_drvdata(dev);

	if (!eswin_pvt_hwmon_channel_is_valid(type, ch))
		return -EINVAL;

	switch (type) {
	case hwmon_chip:
		switch (attr) {
		case hwmon_chip_update_interval:
			return eswin_pvt_write_timeout(pvt, val);
		}
		break;
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_min:
			return eswin_pvt_write_limit(pvt, ch, true, val);
		case hwmon_temp_max:
			return eswin_pvt_write_limit(pvt, ch, false, val);
		case hwmon_temp_offset:
			return eswin_pvt_write_trim(pvt, val);
		}
		break;
	case hwmon_in:
		switch (attr) {
		case hwmon_in_min:
			return eswin_pvt_write_limit(pvt, PVT_VOLT + ch, true, val);
		case hwmon_in_max:
			return eswin_pvt_write_limit(pvt, PVT_VOLT + ch, false, val);
		}
		break;
	default:
		break;
	}

	return -EOPNOTSUPP;
}

static const struct hwmon_ops pvt_hwmon_ops = {
	.is_visible = eswin_pvt_hwmon_is_visible,
	.read = eswin_pvt_hwmon_read,
	.read_string = eswin_pvt_hwmon_read_string,
	.write = eswin_pvt_hwmon_write
};

static const struct hwmon_chip_info pvt_hwmon_info = {
	.ops = &pvt_hwmon_ops,
	.info = pvt_channel_info
};

static void pvt_clear_data(void *data)
{
	struct pvt_hwmon *pvt = data;
	int idx;

	for (idx = 0; idx < PVT_SENSORS_NUM; ++idx)
		complete_all(&pvt->cache[idx].conversion);

	mutex_destroy(&pvt->iface_mtx);
}

static struct pvt_hwmon *eswin_pvt_create_data(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct pvt_hwmon *pvt;
	int ret, idx;

	pvt = devm_kzalloc(dev, sizeof(*pvt), GFP_KERNEL);
	if (!pvt)
		return ERR_PTR(-ENOMEM);

	ret = devm_add_action(dev, pvt_clear_data, pvt);
	if (ret) {
		dev_err(dev, "Can't add PVT data clear action\n");
		return ERR_PTR(ret);
	}

	pvt->dev = dev;
	pvt->sensor = PVT_SENSOR_FIRST;
	mutex_init(&pvt->iface_mtx);

	for (idx = 0; idx < PVT_SENSORS_NUM; ++idx)
		init_completion(&pvt->cache[idx].conversion);

	return pvt;
}

static int eswin_pvt_request_regs(struct pvt_hwmon *pvt)
{
	struct platform_device *pdev = to_platform_device(pvt->dev);
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(pvt->dev, "Couldn't find PVT memresource\n");
		return -EINVAL;
	}

	pvt->regs = devm_ioremap_resource(pvt->dev, res);
	if (IS_ERR(pvt->regs))
		return PTR_ERR(pvt->regs);

	return 0;
}

static void eswin_pvt_remove(void *data)
{
	int ret;
	struct pvt_hwmon *pvt = data;
	ret = reset_control_assert(pvt->pvt_rst);
	WARN_ON(0 != ret);
	clk_disable_unprepare(pvt->clk);
}

static int eswin_pvt_request_clks(struct pvt_hwmon *pvt)
{
	int ret;

	pvt->clk = devm_clk_get(pvt->dev, "pvt_clk");
	if (IS_ERR(pvt->clk)) {
		dev_err(pvt->dev, "Couldn't get PVT clock\n");
		return -ENODEV;
	}

	ret = clk_prepare_enable(pvt->clk);
	if (ret) {
		dev_err(pvt->dev, "Couldn't enable the PVT clocks\n");
		return ret;
	}

	return 0;
}

static int eswin_pvt_request_rst(struct pvt_hwmon *pvt)
{
	int ret;
	pvt->pvt_rst = devm_reset_control_get_optional(pvt->dev, "pvt_rst");
	if(IS_ERR_OR_NULL(pvt->pvt_rst)){
		dev_err(pvt->dev, "Couldn't get PVT reset\n");
	}
	ret = reset_control_reset(pvt->pvt_rst);
	WARN_ON(0 != ret);
	return 0;
}

static int eswin_pvt_check_pwr(struct pvt_hwmon *pvt)
{
	unsigned long tout;
	int ret = 0;
	u32 data;

	/*
	 * Test out the sensor conversion functionality. If it is not done on
	 * time then the domain must have been unpowered and we won't be able
	 * to use the device later in this driver.
	 * Note If the power source is lost during the normal driver work the
	 * data read procedure will either return -ETIMEDOUT (for the
	 * alarm-less driver configuration) or just stop the repeated
	 * conversion. In the later case alas we won't be able to detect the
	 * problem.
	 */

	eswin_pvt_update(pvt->regs + PVT_ENA, PVT_ENA_EN, PVT_ENA_EN);
	readl(pvt->regs + PVT_DATA);

	tout = PVT_TOUT_MIN / NSEC_PER_USEC;
	usleep_range(tout, 2 * tout);

	data = readl(pvt->regs + PVT_DATA);

	eswin_pvt_update(pvt->regs + PVT_ENA, PVT_ENA_EN, 0);
	eswin_pvt_update(pvt->regs + PVT_INT, PVT_INT_CLR, PVT_INT_CLR);

	return ret;
}

static int eswin_pvt_init_iface(struct pvt_hwmon *pvt)
{
	unsigned long rate;

	rate = clk_get_rate(pvt->clk);
	if (!rate) {
		dev_err(pvt->dev, "Invalid reference clock rate\n");
		return -ENODEV;
	}

	/*
	 * Make sure all interrupts and controller are disabled so not to
	 * accidentally have ISR executed before the driver data is fully
	 * initialized. Clear the IRQ status as well.
	 */
	eswin_pvt_update(pvt->regs + PVT_ENA, PVT_ENA_EN, 0);
	eswin_pvt_update(pvt->regs + PVT_INT, PVT_INT_CLR, PVT_INT_CLR);

	readl(pvt->regs + PVT_DATA);

	/* Setup default sensor mode, timeout and temperature trim. */
	eswin_pvt_set_mode(pvt, pvt_info[pvt->sensor].mode);

	/*
	 * Preserve the current ref-clock based delay (Ttotal) between the
	 * sensors data samples in the driver data so not to recalculate it
	 * each time on the data requests and timeout reads. It consists of the
	 * delay introduced by the internal ref-clock timer (N / Fclk) and the
	 * constant timeout caused by each conversion latency (Tmin):
	 *   Ttotal = N / Fclk + Tmin
	 * If alarms are enabled the sensors are polled one after another and
	 * in order to get the next measurement of a particular sensor the
	 * caller will have to wait for at most until all the others are
	 * polled. In that case the formulae will look a bit different:
	 *   Ttotal = 5 * (N / Fclk + Tmin)
	 */

	pvt->timeout = ktime_set(PVT_TOUT_DEF, 0);
	pvt->timeout = ktime_divns(pvt->timeout, rate);
	pvt->timeout = ktime_add_ns(pvt->timeout, PVT_TOUT_MIN);

	return 0;
}

static int eswin_pvt_request_irq(struct pvt_hwmon *pvt)
{
	struct platform_device *pdev = to_platform_device(pvt->dev);
	int ret;

	pvt->irq = platform_get_irq(pdev, 0);
	if (pvt->irq < 0)
		return pvt->irq;

	ret = devm_request_threaded_irq(pvt->dev, pvt->irq,
					eswin_pvt_hard_isr, NULL,
					IRQF_SHARED | IRQF_TRIGGER_HIGH,
					"pvt", pvt);
	if (ret) {
		dev_err(pvt->dev, "Couldn't request PVT IRQ\n");
		return ret;
	}

	return 0;
}

static int eswin_pvt_create_hwmon(struct pvt_hwmon *pvt)
{
	pvt->hwmon = devm_hwmon_device_register_with_info(pvt->dev, "pvt", pvt,
		&pvt_hwmon_info, NULL);
	if (IS_ERR(pvt->hwmon)) {
		dev_err(pvt->dev, "Couldn't create hwmon device\n");
		return PTR_ERR(pvt->hwmon);
	}

	return 0;
}

static int eswin_pvt_probe(struct platform_device *pdev)
{
	struct pvt_hwmon *pvt;
	int ret;

	pvt = eswin_pvt_create_data(pdev);
	if (IS_ERR(pvt))
		return PTR_ERR(pvt);

	ret = eswin_pvt_request_regs(pvt);
	if (ret)
		return ret;

	ret = eswin_pvt_request_clks(pvt);
	if (ret)
		return ret;

	ret = eswin_pvt_request_rst(pvt);
	if (ret)
		return ret;

	ret = eswin_pvt_check_pwr(pvt);
	if (ret)
		return ret;

	ret = eswin_pvt_init_iface(pvt);
	if (ret)
		return ret;

	ret = eswin_pvt_request_irq(pvt);
	if (ret)
		return ret;

	ret = eswin_pvt_create_hwmon(pvt);
	if (ret)
		return ret;

	ret = devm_add_action_or_reset(pvt->dev, eswin_pvt_remove, pvt);
	if (ret) {
		dev_err(pvt->dev, "Can't add PVT clocks disable action\n");
		return ret;
	}

	return 0;
}

static const struct of_device_id pvt_of_match[] = {
	{ .compatible = "eswin,eswin-pvt" },
	{ }
};
MODULE_DEVICE_TABLE(of, pvt_of_match);

static struct platform_driver pvt_driver = {
	.probe = eswin_pvt_probe,
	.driver = {
		.name = "eswin-pvt",
		.of_match_table = pvt_of_match
	},
};
module_platform_driver(pvt_driver);

MODULE_DESCRIPTION("Eswin PVT driver");
MODULE_AUTHOR("Yu Bingzheng <yubingzheng@eswincomputing.com>");
MODULE_LICENSE("GPL v2");
