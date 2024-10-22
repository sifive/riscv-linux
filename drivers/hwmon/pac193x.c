// SPDX-License-Identifier: GPL-2.0
/*
 *  Driver for microchip  pac1931,pac1932,pac1933,pac1934 power monitor chips
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
 * Authors: Yang Wei <yangwei1@eswincomputing.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/jiffies.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/util_macros.h>

#define PAC193X_MAX_CHAN_CNT 4

#define PAC193X_REGISTERS 0x27
#define PAC193X_VPOWERN_ACC_LEN 6
#define PAC193X_VOLT_VALUE_LEN 2
#define PAC193X_VPOWERN_VALUE_LEN 4
#define PAC193X_ACC_COUNT_LEN 3

#define PAC193X_CMD_REFRESH 0x0
#define PAC193X_CMD_CTRL 0x1
#define PAC193X_CMD_ACC_COUNT 0x2
#define PAC193X_CMD_VPOWER1_ACC 0x3
#define PAC193X_CMD_VPOWER2_ACC 0x4
#define PAC193X_CMD_VPOWER3_ACC 0x5
#define PAC193X_CMD_VPOWER4_ACC 0x6
#define PAC193X_CMD_VBUS1 0x7
#define PAC193X_CMD_VBUS2 0x8
#define PAC193X_CMD_VBUS3 0x9
#define PAC193X_CMD_VBUS4 0xa
#define PAC193X_CMD_VSENSE1 0xb
#define PAC193X_CMD_VSENSE2 0xc
#define PAC193X_CMD_VSENSE3 0xd
#define PAC193X_CMD_VSENSE4 0xe
#define PAC193X_CMD_VBUS1_AVG 0xf
#define PAC193X_CMD_VBUS2_AVG 0x10
#define PAC193X_CMD_VBUS3_AVG 0x11
#define PAC193X_CMD_VBUS4_AVG 0x12
#define PAC193X_CMD_VSENSE1_AVG 0x13
#define PAC193X_CMD_VSENSE2_AVG 0x14
#define PAC193X_CMD_VSENSE3_AVG 0x15
#define PAC193X_CMD_VSENSE4_AVG 0x16
#define PAC193X_CMD_VPOWER1 0x17
#define PAC193X_CMD_VPOWER2 0x18
#define PAC193X_CMD_VPOWER3 0x19
#define PAC193X_CMD_VPOWER4 0x1a
#define PAC193X_CMD_CHANNEL_SMBUS 0x1c
#define PAC193X_CMD_NEG_PWR 0x1D
#define PAC193X_CMD_REFRESH_G 0x1E
#define PAC193X_CMD_REFRESH_V 0x1F
#define PAC193X_CMD_SLOW 0x1F
#define PAC193X_CMD_CTRL_ACT 0x21
#define PAC193X_CMD_DIS_ACT 0x22
#define PAC193X_CMD_NEG_PWR_ACT 0x23
#define PAC193X_CMD_CTRL_LAT 0x24
#define PAC193X_CMD_DIS_LAT 0x25
#define PAC193X_CMD_NEG_PWR_LAT 0x26
#define PAC193X_CMD_PID 0xFD
#define PAC193X_CMD_MID 0xFE
#define PAC193X_CMD_REVERSION_ID 0xFF

#define PAC1932X_COSTANT_PWR_M 3200000000ull /* 3.2V^2*1000mO*/
#define PAC1932X_COSTANT_CURRENT_M 100000 /* 100mv*1000mO*/

struct pac193x_data {
	struct mutex config_lock;
	struct i2c_client *client;
	u32 update_time_ms;
	u32 energy_acc_count;
	struct workqueue_struct *update_workqueue;
	struct delayed_work update_work;
	u32 vbus_denominator[PAC193X_MAX_CHAN_CNT];
	u32 vsense_denominator[PAC193X_MAX_CHAN_CNT];
	u32 vpower_denominator[PAC193X_MAX_CHAN_CNT];
	u32 sense_resistances[PAC193X_MAX_CHAN_CNT];
	u32 sample_rate;
};

enum PAC193X_CHAN_INDEX {
	pac1931_index = 0,
	pac1932_index,
	pac1933_index,
	pac1934_index
};

static int pac193x_get_energy(struct device *dev, u8 commmd, u8 chan, long *val)
{
	struct pac193x_data *data = dev_get_drvdata(dev);
	int ret;
	u64 cache = 0;
	u8 *pcache = (u8 *)&cache;
	u64 energy_value = 0;

	commmd = commmd + chan;
	dev_dbg(dev, "%s.%d,chan:%d,commad:%d,LEN:%ld\n", __FUNCTION__,
		__LINE__, chan, commmd, sizeof(cache));
	mutex_lock(&data->config_lock);
	ret = i2c_smbus_read_i2c_block_data(data->client, commmd,
					    PAC193X_VPOWERN_ACC_LEN, pcache);
	mutex_unlock(&data->config_lock);

	energy_value = ((cache & 0xff) << 40) | (((cache >> 8) & 0xff) << 32) |
		       (((cache >> 16) & 0xff) << 24) |
		       (((cache >> 24) & 0xff) << 16) |
		       (((cache >> 32) & 0xff) << 8) | (((cache >> 40) & 0xff));
	energy_value = energy_value & 0xffffffffffffULL;
	dev_dbg(dev,
		"%s.%d,commd:0x%x,value:%lld,ret:%d,resistances:%u,denominator:%u,sample_rate:%u\n",
		__FUNCTION__, __LINE__, commmd, energy_value, ret,
		data->sense_resistances[chan], data->vpower_denominator[chan],
		data->sample_rate);

	/* energy=3200000*Vpower/(Rsense*denominator*sample_rate) */
	energy_value = ((energy_value / (u64)data->vpower_denominator[chan]) *
			(PAC1932X_COSTANT_PWR_M /
			 (data->sense_resistances[chan] * data->sample_rate)));

	*val = energy_value;

	return 0;
}

static u16 pac193x_get_word(struct pac193x_data *data, u8 commmd)
{
	int ret;
	u16 cache = 0;

	mutex_lock(&data->config_lock);
	ret = i2c_smbus_read_word_data(data->client, commmd);
	mutex_unlock(&data->config_lock);
	cache = ((ret & 0xff) << 8) | ((ret >> 8) & 0xff);
	dev_dbg(&data->client->dev, "%s.%d,commd:0x%x,value:0x%x,ret:0x%x\n",
		__FUNCTION__, __LINE__, commmd, cache, ret);

	return cache;
}

static int pac193x_get_volt(struct device *dev, u8 commmd, u8 chan, long *val)
{
	struct pac193x_data *data = dev_get_drvdata(dev);
	u16 cache = 0;

	commmd = commmd + chan;
	dev_dbg(dev, "%s.%d,commd:0x%x,chan:%d,vbus_denominator:%d\n",
		__FUNCTION__, __LINE__, commmd, chan,
		data->vbus_denominator[chan]);
	cache = pac193x_get_word(data, commmd);
	/*to mV*/
	cache = cache * 1000 / data->vbus_denominator[chan];
	*val = cache;
	return 0;
}

static int pac193x_get_current(struct device *dev, u8 commmd, u8 chan,
			       long *val)
{
	struct pac193x_data *data = dev_get_drvdata(dev);
	u16 cache = 0;

	commmd = commmd + chan;
	dev_dbg(dev,
		"%s.%d,commd:0x%x,chan:%d,vbus_denominator:%d,resistances:%d\n",
		__FUNCTION__, __LINE__, commmd, chan,
		data->vsense_denominator[chan], data->sense_resistances[chan]);
	cache = pac193x_get_word(data, commmd);
	/* I=Vsense*100/(Rsense*denominator)	*/
	cache = cache * PAC1932X_COSTANT_CURRENT_M /
		(data->vsense_denominator[chan] *
		 data->sense_resistances[chan]);
	*val = cache;
	return 0;
}

static int pac193x_get_power(struct device *dev, u8 commmd, u8 chan, long *val)
{
	struct pac193x_data *data = dev_get_drvdata(dev);
	int ret;
	u32 cache = 0;
	u8 *pcache = (u8 *)&cache;
	u64 pwr_value = 0;

	commmd = commmd + chan;
	mutex_lock(&data->config_lock);
	ret = i2c_smbus_read_i2c_block_data(data->client, commmd,
					    PAC193X_VPOWERN_VALUE_LEN, pcache);
	mutex_unlock(&data->config_lock);
	pwr_value = ((cache & 0xff) << 24) | (((cache >> 8) & 0xff) << 16) |
		    (((cache >> 16) & 0xff) << 8) | ((cache >> 24) & 0xff);
	pwr_value = pwr_value >> 4;
	dev_dbg(dev,
		"%s.%d,commd:0x%x,chan:%d,value:0x%x,pwr_value:0x%llx,%llu,ret:%d,resistances:%u,denominator:%u\n",
		__FUNCTION__, __LINE__, commmd, chan, cache, pwr_value,
		pwr_value, ret, data->sense_resistances[chan],
		data->vpower_denominator[chan]);
	/* pwr=3200000*Vpower/Rsense*denominator */
	pwr_value = pwr_value * PAC1932X_COSTANT_PWR_M /
		    ((u64)data->sense_resistances[chan] *
		     (u64)data->vpower_denominator[chan]);
	*val = pwr_value;
	return 0;
}

static int pac193x_send_refresh_cmd(struct pac193x_data *data, u8 command)
{
	int ret = 0;

	mutex_lock(&data->config_lock);
	ret = i2c_smbus_write_byte(data->client, command);
	mutex_unlock(&data->config_lock);
	dev_dbg(&data->client->dev, "%s.%d,commd:0x%x,ret:%d\n", __FUNCTION__,
		__LINE__, command, ret);
	return ret;
}

static ssize_t pac193x_refresh_store(struct device *dev,
				     struct device_attribute *da,
				     const char *buf, size_t count)
{
	struct sensor_device_attribute *sensor_attr = to_sensor_dev_attr(da);
	struct pac193x_data *data = dev_get_drvdata(dev);

	pac193x_send_refresh_cmd(data, (u8)sensor_attr->index);
	return count;
}

static struct sensor_device_attribute pac1934_refreshs[] = {
	SENSOR_ATTR_WO(refresh_clear_acc, pac193x_refresh, PAC193X_CMD_REFRESH),
	SENSOR_ATTR_WO(refresh_all_193x, pac193x_refresh,
		       PAC193X_CMD_REFRESH_G),
	SENSOR_ATTR_WO(refresh_updata_value, pac193x_refresh,
		       PAC193X_CMD_REFRESH_V),
};

static u8 pac193x_read_byte_data(struct pac193x_data *data, u8 command)
{
	int cache = 0;
	int cnt = 0;
	while (1) {
		mutex_lock(&data->config_lock);
		cache = i2c_smbus_read_byte_data(data->client, command);
		mutex_unlock(&data->config_lock);
		if (0xff != cache) {
			break;
		}
		cnt++;
		if (cnt > 100) {
			dev_err(&data->client->dev,
				"get command:%d value error\n", command);
			return 0xff;
		}
	}
	return (u8)cache;
}

static ssize_t pac193x_ctrl_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct pac193x_data *data = dev_get_drvdata(dev);
	u8 set_val = 0, act_val = 0, lat_val = 0;

	set_val = pac193x_read_byte_data(data, PAC193X_CMD_CTRL);
	act_val = pac193x_read_byte_data(data, PAC193X_CMD_CTRL_ACT);
	lat_val = pac193x_read_byte_data(data, PAC193X_CMD_CTRL_LAT);

	return sprintf(buf,
		       "%16s:%6s,%6s,%6s\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n",
		       "", "set", "act", "latch",
			   "sample_rate",(set_val >> 6) & 0x3, (act_val >> 6) & 0x3, (lat_val >> 6) & 0x3,
			   "SLEEP", (set_val >> 5) & 0x1,(act_val >> 5) & 0x1, (lat_val >> 5) & 0x1,
			   "SING", (set_val >> 4) & 0x1, (act_val >> 4) & 0x1, (lat_val >> 4) & 0x1,
			   "ALERT_PIN", (set_val >> 3) & 0x1, (act_val >> 3) & 0x1, (lat_val >> 3) & 0x1,
			   "ALERT_CC", (set_val >> 2) & 0x1, (act_val >> 2) & 0x1, (lat_val >> 2) & 0x1,
			   "OVF ALERT", (set_val >> 1) & 0x1, (act_val >> 1) & 0x1, (lat_val >> 1) & 0x1,
			   "OVF", (set_val >> 0) & 0x1, (act_val >> 0) & 0x1, (lat_val >> 0) & 0x1);
}

int pac193x_common_reg_set(struct device *dev, const char *buf, u8 commad)
{
	struct pac193x_data *data = dev_get_drvdata(dev);
	unsigned long val;
	int status;
	int ret = 0;

	status = kstrtoul(buf, 10, &val);
	if (status < 0)
		return status;
	mutex_lock(&data->config_lock);
	ret = i2c_smbus_write_byte_data(data->client, commad, (u8)val);
	mutex_unlock(&data->config_lock);
	return ret;
}

static ssize_t pac193x_ctrl_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	return pac193x_common_reg_set(dev, buf, PAC193X_CMD_CTRL);
}

DEVICE_ATTR(control, S_IWUSR | S_IRUGO, pac193x_ctrl_show, pac193x_ctrl_store);

static ssize_t pac193x_acc_count_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct pac193x_data *data = dev_get_drvdata(dev);
	int ret;
	u32 cache = 0;
	u8 *pcache = (u8 *)&cache;
	u32 acc_cnt = 0;
	mutex_lock(&data->config_lock);
	ret = i2c_smbus_read_i2c_block_data(data->client, PAC193X_CMD_ACC_COUNT,
					    PAC193X_ACC_COUNT_LEN, pcache);
	mutex_unlock(&data->config_lock);

	acc_cnt = ((cache & 0xff) << 16) | (((cache >> 8) & 0xff) << 8) |
		  ((cache >> 16) & 0xff);
	acc_cnt = acc_cnt & 0xffffff;

	ret = sysfs_emit(buf, "%u\n", acc_cnt);
	return ret;
}

DEVICE_ATTR(acc_count, 0400, pac193x_acc_count_show, NULL);

static ssize_t pac193x_dis_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct pac193x_data *data = dev_get_drvdata(dev);
	u8 set_val = 0, act_val = 0, lat_val = 0;

	set_val = pac193x_read_byte_data(data, PAC193X_CMD_CHANNEL_SMBUS);
	act_val = pac193x_read_byte_data(data, PAC193X_CMD_DIS_ACT);
	lat_val = pac193x_read_byte_data(data, PAC193X_CMD_DIS_LAT);

	return sprintf(buf,
		       "%16s:%6s,%6s,%6s\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n",
		       "", "set", "act", "latch",
			   "CH1_OFF", (set_val >> 7) & 0x1, (act_val >> 7) & 0x1, (lat_val >> 7) & 0x1,
			   "CH2_OFF", (set_val >> 6) & 0x1, (act_val >> 6) & 0x1, (lat_val >> 6) & 0x1,
			   "CH3_OFF", (set_val >> 5) & 0x1, (act_val >> 5) & 0x1, (lat_val >> 5) & 0x1,
			   "CH4_OFF", (set_val >> 4) & 0x1, (act_val >> 4) & 0x1, (lat_val >> 4) & 0x1,
			   "TIMEOUT", (set_val >> 3) & 0x1, (act_val >> 3) & 0x1, (lat_val >> 3) & 0x1,
			   "BYTE COUNT", (set_val >> 2) & 0x1, (act_val >> 2) & 0x1, (lat_val >> 2) & 0x1,
			   "NO SKIP", (set_val >> 1) & 0x1, (act_val >> 1) & 0x1, (lat_val >> 1) & 0x1);
}

static ssize_t pac193x_dis_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	return pac193x_common_reg_set(dev, buf, PAC193X_CMD_CHANNEL_SMBUS);
}

DEVICE_ATTR(disable_chan_pmbus, S_IWUSR | S_IRUGO, pac193x_dis_show,
	    pac193x_dis_store);

static ssize_t pac193x_neg_pwr_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct pac193x_data *data = dev_get_drvdata(dev);
	u8 set_val = 0, act_val = 0, lat_val = 0;

	set_val = pac193x_read_byte_data(data, PAC193X_CMD_NEG_PWR);
	act_val = pac193x_read_byte_data(data, PAC193X_CMD_NEG_PWR_ACT);
	lat_val = pac193x_read_byte_data(data, PAC193X_CMD_NEG_PWR_LAT);

	return sprintf(buf,
		       "%16s:%6s,%6s,%6s\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n"
		       "%16s:%6d,%6d,%6d\n",
		       "", "set", "act", "latch",
			   "CH1_BIDI", (set_val >> 7) & 0x1, (act_val >> 7) & 0x1, (lat_val >> 7) & 0x1,
			   "CH2_BIDI", (set_val >> 6) & 0x1, (act_val >> 6) & 0x1, (lat_val >> 6) & 0x1,
			   "CH3_BIDI", (set_val >> 5) & 0x1, (act_val >> 5) & 0x1, (lat_val >> 5) & 0x1,
			   "CH4_BIDI", (set_val >> 4) & 0x1, (act_val >> 4) & 0x1, (lat_val >> 5) & 0x1,
			   "CH1_BIDV", (set_val >> 3) & 0x1, (act_val >> 3) & 0x1, (lat_val >> 3) & 0x1,
			   "CH2_BIDV", (set_val >> 2) & 0x1, (act_val >> 2) & 0x1, (lat_val >> 2) & 0x1,
			   "CH3_BIDV", (set_val >> 1) & 0x1, (act_val >> 1) & 0x1, (lat_val >> 1) & 0x1,
			   "CH4_BIDV", (set_val >> 0) & 0x1, (act_val >> 0) & 0x1, (lat_val >> 0) & 0x1);
}

static ssize_t pac193x_neg_pwr_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	return pac193x_common_reg_set(dev, buf, PAC193X_CMD_NEG_PWR);
}

DEVICE_ATTR(neg_pwr, S_IWUSR | S_IRUGO, pac193x_neg_pwr_show,
	    pac193x_neg_pwr_store);

static ssize_t pac193x_slow_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct pac193x_data *data = dev_get_drvdata(dev);
	u8 set_val = 0;

	set_val = pac193x_read_byte_data(data, PAC193X_CMD_SLOW);

	return sprintf(buf,
		       "SLOW:%d,SLOW-LH:%d,SLOW-HL:%d, R_RISE:%d,"
		       "R_V_RISE:%d,R_FALL:%d,R_V_FALL:%d,POR:%d\n",
		       (set_val >> 7) & 0x1, (set_val >> 6) & 0x1,
		       (set_val >> 5) & 0x1, (set_val >> 4) & 0x1,
		       (set_val >> 3) & 0x1, (set_val >> 2) & 0x1,
		       (set_val >> 1) & 0x1, (set_val >> 0) & 0x1);
}

static ssize_t pac193x_slow_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	return pac193x_common_reg_set(dev, buf, PAC193X_CMD_SLOW);
}

DEVICE_ATTR(slow_ctrl, S_IWUSR | S_IRUGO, pac193x_slow_show,
	    pac193x_slow_store);

static ssize_t pac193x_version_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct pac193x_data *data = dev_get_drvdata(dev);
	int ret;
	u8 pid = 0, mid = 0, rid = 0;

	pid = pac193x_read_byte_data(data, PAC193X_CMD_PID);
	mid = pac193x_read_byte_data(data, PAC193X_CMD_MID);
	rid = pac193x_read_byte_data(data, PAC193X_CMD_REVERSION_ID);

	ret = sysfs_emit(buf, "PID:0x%x,MID:0x%x,RID:0x%x\n", pid, mid, rid);
	return ret;
}
DEVICE_ATTR(pac193x_version, 0400, pac193x_version_show, NULL);

static struct attribute *pac193x_attrs[] = {
	&dev_attr_control.attr,
	&dev_attr_acc_count.attr,
	&dev_attr_neg_pwr.attr,
	&dev_attr_disable_chan_pmbus.attr,
	&dev_attr_slow_ctrl.attr,
	&dev_attr_pac193x_version.attr,
	&pac1934_refreshs[0].dev_attr.attr,
	&pac1934_refreshs[1].dev_attr.attr,
	&pac1934_refreshs[2].dev_attr.attr,
	NULL,
};

ATTRIBUTE_GROUPS(pac193x);

static const struct hwmon_channel_info *pac1931_info[] = {
	HWMON_CHANNEL_INFO(in, HWMON_I_INPUT | HWMON_I_AVERAGE,
			    HWMON_I_INPUT | HWMON_I_AVERAGE),
	HWMON_CHANNEL_INFO(curr, HWMON_C_INPUT | HWMON_C_AVERAGE),
	HWMON_CHANNEL_INFO(power, HWMON_P_INPUT),
	HWMON_CHANNEL_INFO(energy, HWMON_E_INPUT), NULL
};
static const struct hwmon_channel_info *pac1932_info[] = {
	HWMON_CHANNEL_INFO(in, HWMON_I_INPUT | HWMON_I_AVERAGE,
			   HWMON_I_INPUT | HWMON_I_AVERAGE,
			   HWMON_I_INPUT | HWMON_I_AVERAGE),
	HWMON_CHANNEL_INFO(curr, HWMON_C_INPUT | HWMON_C_AVERAGE,
			   HWMON_C_INPUT | HWMON_C_AVERAGE),
	HWMON_CHANNEL_INFO(power, HWMON_P_INPUT, HWMON_P_INPUT),
	HWMON_CHANNEL_INFO(energy, HWMON_E_INPUT, HWMON_E_INPUT), NULL
};
static const struct hwmon_channel_info *pac1933_info[] = {
	HWMON_CHANNEL_INFO(in, HWMON_I_INPUT | HWMON_I_AVERAGE,
			   HWMON_I_INPUT | HWMON_I_AVERAGE,
			   HWMON_I_INPUT | HWMON_I_AVERAGE,
			   HWMON_I_INPUT | HWMON_I_AVERAGE),
	HWMON_CHANNEL_INFO(curr, HWMON_C_INPUT | HWMON_C_AVERAGE,
			   HWMON_C_INPUT | HWMON_C_AVERAGE,
			   HWMON_C_INPUT | HWMON_C_AVERAGE),
	HWMON_CHANNEL_INFO(power, HWMON_P_INPUT, HWMON_P_INPUT, HWMON_P_INPUT),
	HWMON_CHANNEL_INFO(energy, HWMON_E_INPUT, HWMON_E_INPUT, HWMON_E_INPUT),
	NULL
};
static const struct hwmon_channel_info *pac1934_info[] = {
	HWMON_CHANNEL_INFO(in,
			   HWMON_I_INPUT | HWMON_I_AVERAGE,
			   HWMON_I_INPUT | HWMON_I_AVERAGE,
			   HWMON_I_INPUT | HWMON_I_AVERAGE,
			   HWMON_I_INPUT | HWMON_I_AVERAGE,
			   HWMON_I_INPUT | HWMON_I_AVERAGE),
	HWMON_CHANNEL_INFO(curr,
			   HWMON_C_INPUT | HWMON_C_AVERAGE,
			   HWMON_C_INPUT | HWMON_C_AVERAGE,
			   HWMON_C_INPUT | HWMON_C_AVERAGE,
			   HWMON_C_INPUT | HWMON_C_AVERAGE),
	HWMON_CHANNEL_INFO(power, HWMON_P_INPUT, HWMON_P_INPUT, HWMON_P_INPUT,
			   HWMON_P_INPUT),
	HWMON_CHANNEL_INFO(energy, HWMON_E_INPUT, HWMON_E_INPUT, HWMON_E_INPUT,
			   HWMON_E_INPUT),
	NULL
};

static umode_t pac1934x_is_visible(const void *_data,
				   enum hwmon_sensor_types type, u32 attr,
				   int channel)
{
	switch (type) {
	case hwmon_in:
		switch (attr) {
		case hwmon_in_input:
		case hwmon_in_average:
			if(channel == 0)
			{
				return 0;
			}
			else
			{
				return S_IRUGO;
			}
		}
		break;
	case hwmon_curr:
		switch (attr) {
		case hwmon_curr_input:
		case hwmon_curr_average:
			return S_IRUGO;
		}
		break;
	case hwmon_power:
		switch (attr) {
		case hwmon_power_input:
			return S_IRUGO;
		}
		break;
	case hwmon_energy:
		switch (attr) {
		case hwmon_energy_input:
			return S_IRUGO;
		}
		break;
	default:
		break;
	}
	return 0;
}

static int pac193x_read(struct device *dev, enum hwmon_sensor_types type,
			u32 attr, int channel, long *val)
{
	switch (type) {
	case hwmon_in:
		switch (attr) {
		case hwmon_in_input:
			return pac193x_get_volt(dev, PAC193X_CMD_VBUS1, channel - 1,
						val);
		case hwmon_in_average:
			return pac193x_get_volt(dev, PAC193X_CMD_VBUS1_AVG,
						channel - 1, val);
		}
		break;
	case hwmon_curr:
		switch (attr) {
		case hwmon_curr_input:
			return pac193x_get_current(dev, PAC193X_CMD_VSENSE1,
						   channel, val);
		case hwmon_curr_average:
			return pac193x_get_current(dev, PAC193X_CMD_VSENSE1_AVG,
						   channel, val);
		}
		break;
	case hwmon_power:
		switch (attr) {
		case hwmon_power_input:
			return pac193x_get_power(dev, PAC193X_CMD_VPOWER1,
						 channel, val);
		}
		break;
	case hwmon_energy:
		switch (attr) {
		case hwmon_energy_input:
			return pac193x_get_energy(dev, PAC193X_CMD_VPOWER1_ACC,
						  channel, val);
		}
		break;
	default:
		break;
	}
	return 0;
}

static const struct hwmon_ops pac193x_hwmon_ops = {
	.is_visible = pac1934x_is_visible,
	.read = pac193x_read,
};

static struct hwmon_chip_info pac193x_chip_info = {
	.ops = &pac193x_hwmon_ops,
	.info = NULL,
};

static void update_reg_data(struct work_struct *work)
{
	static u32 updata_cnt = 0;
	struct pac193x_data *data;
	int act_val = 0, ctrl_val = 0, slow_val = 0;
	int num = 0;
	bool is_neg = false;

	data = container_of(work, struct pac193x_data, update_work.work);
	if ((data->energy_acc_count == 0)||(updata_cnt < data->energy_acc_count)) {
		pac193x_send_refresh_cmd(data, PAC193X_CMD_REFRESH_V);
		updata_cnt++;
	} else {
		pac193x_send_refresh_cmd(data, PAC193X_CMD_REFRESH);
		updata_cnt = 0;
	}

	act_val = pac193x_read_byte_data(data, PAC193X_CMD_NEG_PWR_ACT);

	for (num = 0; num < PAC193X_MAX_CHAN_CNT; num++) {
		is_neg = false;
		if (0x1 == ((act_val >> num) & 0x1)) {
			/* Vsource=32*Vbus/2^15 = Vbus/2^10=Vbus/1024  */
			data->vbus_denominator[3 - num] = 1024;
			is_neg = true;
		} else {
			/* Vsource=32*Vbus/2^16 = Vbus/2^10=Vbus/1024  */
			data->vbus_denominator[3 - num] = 2048;
		}

		if (0x1 == ((act_val >> (num + 4)) & 0x1)) {
			/* 2^15  */
			data->vsense_denominator[3 - num] = 32768;
			is_neg = true;
		} else {
			/*2^16 */
			data->vsense_denominator[3 - num] = 65536;
		}
		if (true == is_neg) {
			/* 2^28  */
			data->vpower_denominator[3 - num] = 134217728;
		} else {
			/* 2^29  */
			data->vpower_denominator[3 - num] = 268435456;
		}
	}

	slow_val = pac193x_read_byte_data(data, PAC193X_CMD_SLOW);
	ctrl_val = pac193x_read_byte_data(data, PAC193X_CMD_CTRL_ACT);
	if ((0x1 == ((slow_val >> 7) & 0x1)) &&
	    (0x0 == ((ctrl_val >> 3) & 0x1))) {
		data->sample_rate = 8;
	} else {
		switch ((ctrl_val >> 6) & 0x3) {
		case 0:
			data->sample_rate = 1024;
			break;
		case 1:
			data->sample_rate = 256;
			break;
		case 2:
			data->sample_rate = 64;
			break;
		case 3:
			data->sample_rate = 8;
			break;
		default:
			break;
		}
	}
	queue_delayed_work(data->update_workqueue, &data->update_work,
			   msecs_to_jiffies(data->update_time_ms));
}

static int pac193x_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct pac193x_data *data;
	struct device *hwmon_dev;
	int ret = 0;
	int num = 0;
	enum PAC193X_CHAN_INDEX chan_index =
		(enum PAC193X_CHAN_INDEX)of_device_get_match_data(&client->dev);

	switch (chan_index) {
	case pac1931_index:
		pac193x_chip_info.info = pac1931_info;
		break;
	case pac1932_index:
		pac193x_chip_info.info = pac1932_info;
		break;
	case pac1933_index:
		pac193x_chip_info.info = pac1933_info;
		break;
	case pac1934_index:
		pac193x_chip_info.info = pac1934_info;
		break;
	default:
		break;
	}

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	for (num = 0; num < PAC193X_MAX_CHAN_CNT; num++) {
		data->vbus_denominator[num] = 1;
		data->vsense_denominator[num] = 1;
		data->sense_resistances[num] = 1;
	}

	ret = of_property_read_u32(dev->of_node, "update_time_ms",
				   &data->update_time_ms);
	if (0 != ret) {
		dev_err(dev, "can not get update_time_ms:%d\n", ret);
		data->update_time_ms = 100;
	}

	ret = of_property_read_u32(dev->of_node, "energy_acc_count",
				   &data->energy_acc_count);
	if (0 != ret) {
		dev_err(dev, "can not get energy_acc_count:%d\n", ret);
		data->energy_acc_count = 0;
	}
	ret = of_property_read_u32_array(dev->of_node, "shunt_resistors",
					 data->sense_resistances,
					 PAC193X_MAX_CHAN_CNT);
	if (0 != ret) {
		dev_err(dev, "can not get sense_resistances:%d\n", ret);
	}

	dev_info(dev,
		"update_time:%d,energy_acc_count:%d,resistances:%d,%d,%d,%dmOhm\n",
		data->update_time_ms, data->energy_acc_count,
		data->sense_resistances[0], data->sense_resistances[1],
		data->sense_resistances[2], data->sense_resistances[3]);
	mutex_init(&data->config_lock);
	data->client = client;
	mutex_lock(&data->config_lock);
	i2c_smbus_write_byte_data(data->client, PAC193X_CMD_CTRL, 0x8);
	mutex_unlock(&data->config_lock);
	pac193x_send_refresh_cmd(data, PAC193X_CMD_REFRESH);

	hwmon_dev = devm_hwmon_device_register_with_info(
		dev, client->name, data, &pac193x_chip_info, pac193x_groups);
	if (IS_ERR(hwmon_dev))
		return PTR_ERR(hwmon_dev);

	data->update_workqueue = create_workqueue("update_workqueue");

	INIT_DELAYED_WORK(&data->update_work, update_reg_data);
	queue_delayed_work(data->update_workqueue, &data->update_work,
			   msecs_to_jiffies(0));

	return 0;
}

static const struct of_device_id __maybe_unused pac193x_of_match[] = {
	{ .compatible = "microchip,pac1931", .data = (void *)pac1931_index },
	{ .compatible = "microchip,pac1932", .data = (void *)pac1932_index },
	{ .compatible = "microchip,pac1933", .data = (void *)pac1933_index },
	{ .compatible = "microchip,pac1934", .data = (void *)pac1934_index },
	{},
};
MODULE_DEVICE_TABLE(of, pac193x_of_match);

static struct i2c_driver pac193x_driver = {
	.driver = {
		.name	= "pac193x",
		.of_match_table = of_match_ptr(pac193x_of_match),
	},
	.probe = pac193x_probe,
};

module_i2c_driver(pac193x_driver);

MODULE_AUTHOR("Yang Wei <yangwei1@eswincomputing.com");
MODULE_DESCRIPTION("pac193x driver");
MODULE_LICENSE("GPL");
