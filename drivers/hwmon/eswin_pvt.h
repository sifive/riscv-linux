// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) ESWIN Electronics Co.Ltd
 *
 * Eswin Process, Voltage, Temperature sensor driver
 */
#ifndef __HWMON_ESWIN_PVT_H__
#define __HWMON_ESWIN_PVT_H__

#include <linux/completion.h>
#include <linux/hwmon.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/seqlock.h>

/* Eswin PVT registers and their bitfields */
#define PVT_DIV               0x00
#define PVT_TRIM              0x04
#define PVT_TRIM_G            GENMASK(4,0)
#define PVT_TRIM_O            GENMASK(13,8)
#define PVT_MODE              0x08
#define PVT_MODE_MASK         GENMASK(2, 0)
#define PVT_CTRL_MODE_TEMP    0x0
#define PVT_CTRL_MODE_VOLT    0x4
#define PVT_CTRL_MODE_LVT     0x1
#define PVT_CTRL_MODE_ULVT    0x2
#define PVT_CTRL_MODE_SVT     0x3
#define PVT_MODE_PSAMPLE_0    BIT(0)
#define PVT_MODE_PSAMPLE_1    BIT(1)
#define PVT_MODE_VSAMPLE      BIT(2)
#define PVT_ENA               0x0c
#define PVT_ENA_EN            BIT(0)
#define PVT_INT               0x10
#define PVT_INT_CLR           BIT(1)
#define PVT_DATA              0x14
#define PVT_DATA_OUT          GENMASK(9,0)

/* alarm related */
#define PVT_TTHRES			0x08
#define PVT_VTHRES			0x0C
#define PVT_LTHRES			0x10
#define PVT_ULTHRES			0x14
#define PVT_STHRES			0x18
#define PVT_INTR_DVALID			BIT(0)
#define PVT_INTR_TTHRES_LO		BIT(1)
#define PVT_INTR_TTHRES_HI		BIT(2)
#define PVT_INTR_VTHRES_LO		BIT(3)
#define PVT_INTR_VTHRES_HI		BIT(4)
#define PVT_INTR_LTHRES_LO		BIT(5)
#define PVT_INTR_LTHRES_HI		BIT(6)
#define PVT_INTR_ULTHRES_LO		BIT(7)
#define PVT_INTR_ULTHRES_HI		BIT(8)
#define PVT_INTR_STHRES_LO		BIT(9)
#define PVT_INTR_STHRES_HI		BIT(10)

/*
 * PVT sensors-related limits and default values
 * @PVT_TEMP_MIN: Minimal temperature in millidegrees of Celsius.
 * @PVT_TEMP_MAX: Maximal temperature in millidegrees of Celsius.
 * @PVT_TEMP_CHS: Number of temperature hwmon channels.
 * @PVT_VOLT_MIN: Minimal voltage in mV.
 * @PVT_VOLT_MAX: Maximal voltage in mV.
 * @PVT_VOLT_CHS: Number of voltage hwmon channels.
 * @PVT_DATA_MIN: Minimal PVT raw data value.
 * @PVT_DATA_MAX: Maximal PVT raw data value.
 * @PVT_TRIM_MIN: Minimal temperature sensor trim value.
 * @PVT_TRIM_MAX: Maximal temperature sensor trim value.
 * @PVT_TRIM_DEF: Default temperature sensor trim value (set a proper value
 *		  when one is determined for ESWIN SoC).
 * @PVT_TRIM_TEMP: Maximum temperature encoded by the trim factor.
 * @PVT_TRIM_STEP: Temperature stride corresponding to the trim value.
 * @PVT_TOUT_MIN: Minimal timeout between samples in nanoseconds.
 * @PVT_TOUT_DEF: Default data measurements timeout. In case if alarms are
 *		  activated the PVT IRQ is enabled to be raised after each
 *		  conversion in order to have the thresholds checked and the
 *		  converted value cached. Too frequent conversions may cause
 *		  the system CPU overload. Lets set the 50ms delay between
 *		  them by default to prevent this.
 */
#define PVT_TEMP_MIN		-40000L
#define PVT_TEMP_MAX		125000L
#define PVT_TEMP_CHS		1
#define PVT_VOLT_MIN		720L
#define PVT_VOLT_MAX		880L
#define PVT_VOLT_CHS		4
#define PVT_DATA_MIN		0
#define PVT_DATA_DATA_FLD       0
#define PVT_CTRL_TRIM_FLD       4
#define PVT_CTRL_TRIM_MASK      GENMASK(8,4)
#define PVT_DATA_MAX		(PVT_DATA_DATA_MASK >> PVT_DATA_DATA_FLD)
#define PVT_TRIM_MIN		0
#define PVT_TRIM_MAX		(PVT_CTRL_TRIM_MASK >> PVT_CTRL_TRIM_FLD)
#define PVT_TRIM_TEMP		7130
#define PVT_TRIM_STEP		(PVT_TRIM_TEMP / PVT_TRIM_MAX)
#define PVT_TRIM_DEF		0
#define PVT_TOUT_MIN		(NSEC_PER_SEC / 3000)
# define PVT_TOUT_DEF		0

/*
 * enum pvt_sensor_type - ESWIN PVT sensor types (correspond to each PVT
 *			  sampling mode)
 * @PVT_SENSOR*: helpers to traverse the sensors in loops.
 * @PVT_TEMP: PVT Temperature sensor.
 * @PVT_VOLT: PVT Voltage sensor.
 * @PVT_LVT: PVT Low-Voltage threshold sensor.
 * @PVT_HVT: PVT High-Voltage threshold sensor.
 * @PVT_SVT: PVT Standard-Voltage threshold sensor.
 */
enum pvt_sensor_type {
	PVT_SENSOR_FIRST,
	PVT_TEMP = PVT_SENSOR_FIRST,
	PVT_VOLT,
	PVT_LVT,
	PVT_ULVT,
	PVT_SVT,
	PVT_SENSOR_LAST = PVT_SVT,
	PVT_SENSORS_NUM
};

/*
 * struct pvt_sensor_info - ESWIN PVT sensor informational structure
 * @channel: Sensor channel ID.
 * @label: hwmon sensor label.
 * @mode: PVT mode corresponding to the channel.
 * @thres_base: upper and lower threshold values of the sensor.
 * @thres_sts_lo: low threshold status bitfield.
 * @thres_sts_hi: high threshold status bitfield.
 * @type: Sensor type.
 * @attr_min_alarm: Min alarm attribute ID.
 * @attr_min_alarm: Max alarm attribute ID.
 */
struct pvt_sensor_info {
	int channel;
	const char *label;
	u32 mode;
	unsigned long thres_base;
	u32 thres_sts_lo;
	u32 thres_sts_hi;
	enum hwmon_sensor_types type;
	u32 attr_min_alarm;
	u32 attr_max_alarm;
};

#define PVT_SENSOR_INFO(_ch, _label, _type, _mode, _thres)	\
	{							\
		.channel = _ch,					\
		.label = _label,				\
		.mode = PVT_CTRL_MODE_ ##_mode,			\
		.thres_base = PVT_ ##_thres,			\
		.thres_sts_lo = PVT_INTR_ ##_thres## _LO,	\
		.thres_sts_hi = PVT_INTR_ ##_thres## _HI,	\
		.type = _type,					\
		.attr_min_alarm = _type## _min,			\
		.attr_max_alarm = _type## _max,			\
	}

/*
 * struct pvt_cache - PVT sensors data cache
 * @data: data cache in raw format.
 * @thres_sts_lo: low threshold status saved on the previous data conversion.
 * @thres_sts_hi: high threshold status saved on the previous data conversion.
 * @data_seqlock: cached data seq-lock.
 * @conversion: data conversion completion.
 */
struct pvt_cache {
	u32 data;
	struct completion conversion;
};

/*
 * struct pvt_hwmon - Eswin PVT private data
 * @dev: device structure of the PVT platform device.
 * @hwmon: hwmon device structure.
 * @regs: pointer to the Eswin PVT registers region.
 * @irq: PVT events IRQ number.
 * @clk: PVT core clock (1.2MHz).
 * @pvt_rst: pointer to the reset descriptor.
 * @iface_mtx: Generic interface mutex (used to lock the alarm registers
 *	       when the alarms enabled, or the data conversion interface
 *	       if alarms are disabled).
 * @sensor: current PVT sensor the data conversion is being performed for.
 * @cache: data cache descriptor.
 * @timeout: conversion timeout cache.
 */
struct pvt_hwmon {
	struct device *dev;
	struct device *hwmon;

	void __iomem *regs;
	int irq;

	struct clk *clk;
	struct reset_control *pvt_rst;
	struct mutex iface_mtx;
	enum pvt_sensor_type sensor;
	struct pvt_cache cache[PVT_SENSORS_NUM];
	ktime_t timeout;
};

/*
 * struct pvt_poly_term - a term descriptor of the PVT data translation
 *			  polynomial
 * @deg: degree of the term.
 * @coef: multiplication factor of the term.
 * @divider: distributed divider per each degree.
 * @divider_leftover: divider leftover, which couldn't be redistributed.
 */
struct pvt_poly_term {
	unsigned int deg;
	long coef;
	long divider;
	long divider_leftover;
};

/*
 * struct pvt_poly - PVT data translation polynomial descriptor
 * @total_divider: total data divider.
 * @terms: polynomial terms up to a free one.
 */
struct pvt_poly {
	long total_divider;
	struct pvt_poly_term terms[];
};

#endif /* __HWMON_ESWIN_PVT_H__ */

