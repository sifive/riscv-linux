// SPDX-License-Identifier: GPL-2.0
/*
 * Onsemi FUSB303B Type-C Chip Driver
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
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/usb.h>
#include <linux/usb/typec.h>
#include <linux/usb/tcpm.h>
#include <linux/usb/pd.h>
#include <linux/workqueue.h>

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/usb/role.h>

#define FUSB303B_REG_DEVICE_ID 0X1
#define FUSB303B_REG_DEVICE_TYPE 0X2
#define FUSB303B_REG_PORTROLE 0X3
#define FUSB303B_REG_CONTROL 0X4
#define FUSB303B_REG_CONTROL1 0X5
#define FUSB303B_REG_MANUAL 0X9
#define FUSB303B_REG_RESET 0XA
#define FUSB303B_REG_MASK 0XE
#define FUSB303B_REG_MASK1 0XF
#define FUSB303B_REG_STATUS 0X11
#define FUSB303B_REG_STATUS1 0X12
#define FUSB303B_REG_TYPE 0X13
#define FUSB303B_REG_INTERRUPT 0X14
#define FUSB303B_REG_INTERRUPT1 0X15

#define FUSB303B_PORTROLE_DRP BIT(2)
#define FUSB303B_PORTROLE_SINK BIT(1)
#define FUSB303B_PORTROLE_SOURCE BIT(0)

#define FUSB303B_CONTROL_T_DRP BIT(6)
#define FUSB303B_CONTROL_DRPTOGGLE BIT(4)
#define FUSB303B_CONTROL_DCABLE_EN BIT(3)
#define FUSB303B_CONTROL_HOST_CUR BIT(1)
#define FUSB303B_CONTROL_INT_MASK BIT(0)

#define FUSB303B_CONTROL1_REMEDY_EN BIT(7)
#define FUSB303B_CONTROL1_AUTO_SNK_TH BIT(5)
#define FUSB303B_CONTROL1_AUTO_SNK_EN BIT(4)
#define FUSB303B_CONTROL1_ENABLE BIT(3)
#define FUSB303B_CONTROL1_TCCDEB BIT(0)

#define FUSB303B_STATUS_AUTOSNK BIT(7)
#define FUSB303B_STATUS_VSAFE0V BIT(6)
#define FUSB303B_STATUS_ORIENT BIT(4)
#define FUSB303B_STATUS_VBUSOK BIT(3)
#define FUSB303B_STATUS_BC_LVL BIT(1)
#define FUSB303B_STATUS_BC_LVL_MASK 0X6
#define FUSB303B_STATUS_ATTACH BIT(0)

#define FUSB303B_STATUS_MASK 0X30

#define FUSB303B_BC_LVL_SINK_OR_RA 0
#define FUSB303B_BC_LVL_SINK_DEFAULT 1
#define FUSB303B_BC_LVL_SINK_1_5A 2
#define FUSB303B_BC_LVL_SINK_3A 3

#define FUSB303B_INT_I_ORIENT BIT(6)
#define FUSB303B_INT_I_FAULT BIT(5)
#define FUSB303B_INT_I_VBUS_CHG BIT(4)
#define FUSB303B_INT_I_AUTOSNK BIT(3)
#define FUSB303B_INT_I_BC_LVL BIT(2)
#define FUSB303B_INT_I_DETACH BIT(1)
#define FUSB303B_INT_I_ATTACH BIT(0)

#define FUSB303B_INT1_I_REM_VBOFF BIT(6)
#define FUSB303B_INT1_I_REM_VBON BIT(5)
#define FUSB303B_INT1_I_REM_FAIL BIT(3)
#define FUSB303B_INT1_I_FRC_FAIL BIT(2)
#define FUSB303B_INT1_I_FRC_SUCC BIT(1)
#define FUSB303B_INT1_I_REMEDY BIT(0)

#define FUSB303B_TYPE_SINK BIT(4)
#define FUSB303B_TYPE_SOURCE BIT(3)

#define FUSB_REG_MASK_M_VBUS_CHG BIT(4)

#define LOG_BUFFER_ENTRIES 1024
#define LOG_BUFFER_ENTRY_SIZE 128

struct fusb303b_chip {
	struct device *dev;
	struct i2c_client *i2c_client;
	struct tcpm_port *tcpm_port;
	struct tcpc_dev tcpc_dev;

	spinlock_t irq_lock;
	struct work_struct irq_work;
	bool irq_suspended;
	bool irq_while_suspended;
	struct gpio_desc *gpio_int_n;
	int gpio_int_n_irq;

	/* lock for sharing chip states */
	struct mutex lock;

	/* port status */
	bool vconn_on;
	bool vbus_on;
	bool charge_on;
	bool vbus_present;
	enum typec_cc_polarity cc_polarity;
	enum typec_cc_status cc1;
	enum typec_cc_status cc2;

	struct task_struct *stat_task;
#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
	/* lock for log buffer access */
	struct mutex logbuffer_lock;
	int logbuffer_head;
	int logbuffer_tail;
	u8 *logbuffer[LOG_BUFFER_ENTRIES];
#endif
};

#ifdef CONFIG_DEBUG_FS
static bool fusb303b_log_full(struct fusb303b_chip *chip)
{
	return chip->logbuffer_tail ==
	       (chip->logbuffer_head + 1) % LOG_BUFFER_ENTRIES;
}

__printf(2, 0) static void _fusb303b_log(struct fusb303b_chip *chip,
					 const char *fmt, va_list args)
{
	char tmpbuffer[LOG_BUFFER_ENTRY_SIZE];
	u64 ts_nsec = local_clock();
	unsigned long rem_nsec;

	if (!chip->logbuffer[chip->logbuffer_head]) {
		chip->logbuffer[chip->logbuffer_head] =
			kzalloc(LOG_BUFFER_ENTRY_SIZE, GFP_KERNEL);
		if (!chip->logbuffer[chip->logbuffer_head])
			return;
	}

	vsnprintf(tmpbuffer, sizeof(tmpbuffer), fmt, args);

	mutex_lock(&chip->logbuffer_lock);

	if (fusb303b_log_full(chip)) {
		chip->logbuffer_head = max(chip->logbuffer_head - 1, 0);
		strlcpy(tmpbuffer, "overflow", sizeof(tmpbuffer));
	}

	if (chip->logbuffer_head < 0 ||
	    chip->logbuffer_head >= LOG_BUFFER_ENTRIES) {
		dev_warn(chip->dev, "Bad log buffer index %d\n",
			 chip->logbuffer_head);
		goto abort;
	}

	if (!chip->logbuffer[chip->logbuffer_head]) {
		dev_warn(chip->dev, "Log buffer index %d is NULL\n",
			 chip->logbuffer_head);
		goto abort;
	}

	rem_nsec = do_div(ts_nsec, 1000000000);
	scnprintf(chip->logbuffer[chip->logbuffer_head], LOG_BUFFER_ENTRY_SIZE,
		  "[%5lu.%06lu] %s", (unsigned long)ts_nsec, rem_nsec / 1000,
		  tmpbuffer);
	chip->logbuffer_head = (chip->logbuffer_head + 1) % LOG_BUFFER_ENTRIES;

abort:
	mutex_unlock(&chip->logbuffer_lock);
}

__printf(2, 3) static void fusb303b_log(struct fusb303b_chip *chip,
					const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	_fusb303b_log(chip, fmt, args);
	va_end(args);
}

static int fusb303b_debug_show(struct seq_file *s, void *v)
{
	struct fusb303b_chip *chip = (struct fusb303b_chip *)s->private;
	int tail;

	mutex_lock(&chip->logbuffer_lock);
	tail = chip->logbuffer_tail;
	while (tail != chip->logbuffer_head) {
		seq_printf(s, "%s\n", chip->logbuffer[tail]);
		tail = (tail + 1) % LOG_BUFFER_ENTRIES;
	}
	if (!seq_has_overflowed(s))
		chip->logbuffer_tail = tail;
	mutex_unlock(&chip->logbuffer_lock);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(fusb303b_debug);

static void fusb303b_debugfs_init(struct fusb303b_chip *chip)
{
	char name[NAME_MAX];

	mutex_init(&chip->logbuffer_lock);
	snprintf(name, NAME_MAX, "fusb303b-%s", dev_name(chip->dev));
	chip->dentry = debugfs_create_dir(name, usb_debug_root);
	debugfs_create_file("log", S_IFREG | 0444, chip->dentry, chip,
			    &fusb303b_debug_fops);
}

static void fusb303b_debugfs_exit(struct fusb303b_chip *chip)
{
	debugfs_remove(chip->dentry);
}

#else

static void fusb303b_log(const struct fusb303b_chip *chip, const char *fmt, ...)
{
}
static void fusb303b_debugfs_init(const struct fusb303b_chip *chip)
{
}
static void fusb303b_debugfs_exit(const struct fusb303b_chip *chip)
{
}

#endif

static int fusb303b_i2c_write(struct fusb303b_chip *chip, u8 address, u8 data)
{
	int ret = 0;

	ret = i2c_smbus_write_byte_data(chip->i2c_client, address, data);
	if (ret < 0)
		fusb303b_log(chip, "cannot write 0x%02x to 0x%02x, ret=%d",
			     data, address, ret);

	return ret;
}

static int fusb303b_i2c_read(struct fusb303b_chip *chip, u8 address, u8 *data)
{
	int ret = 0;

	ret = i2c_smbus_read_byte_data(chip->i2c_client, address);
	*data = (u8)ret;
	if (ret < 0)
		fusb303b_log(chip, "cannot read %02x, ret=%d", address, ret);

	return ret;
}

static int fusb303b_i2c_mask_write(struct fusb303b_chip *chip, u8 address,
				   u8 mask, u8 value)
{
	int ret = 0;
	u8 data;

	ret = fusb303b_i2c_read(chip, address, &data);
	if (ret < 0)
		return ret;
	data &= ~mask;
	data |= value;
	ret = fusb303b_i2c_write(chip, address, data);
	if (ret < 0)
		return ret;

	return ret;
}

static int fusb303b_i2c_clear_bits(struct fusb303b_chip *chip, u8 address,
				   u8 clear_bits)
{
	return fusb303b_i2c_mask_write(chip, address, clear_bits, 0x00);
}

static int fusb303b_sw_reset(struct fusb303b_chip *chip)
{
	int ret = 0;

	ret = fusb303b_i2c_write(chip, FUSB303B_REG_RESET, 1);
	if (ret < 0)
		fusb303b_log(chip, "cannot sw reset the chip, ret=%d", ret);
	else
		fusb303b_log(chip, "sw reset");

	return ret;
}

/*
 * initialize interrupt on the chip
 * - unmasked interrupt: VBUS_OK
 */
static int fusb303b_init_interrupt(struct fusb303b_chip *chip)
{
	int ret = 0;

	ret = fusb303b_i2c_write(chip, FUSB303B_REG_MASK,
				 0xFF & ~FUSB_REG_MASK_M_VBUS_CHG);
	if (ret < 0)
		return ret;
	ret = fusb303b_i2c_write(chip, FUSB303B_REG_MASK1, 0xFF);
	if (ret < 0)
		return ret;

	ret = fusb303b_i2c_clear_bits(chip, FUSB303B_REG_CONTROL,
				      FUSB303B_CONTROL_INT_MASK);
	if (ret < 0)
		return ret;

	return ret;
}

static int tcpm_init(struct tcpc_dev *dev)
{
	struct fusb303b_chip *chip =
		container_of(dev, struct fusb303b_chip, tcpc_dev);
	int ret = 0;
	u8 data;

	ret = fusb303b_sw_reset(chip);
	if (ret < 0)
		return ret;
	fusb303b_i2c_read(chip, FUSB303B_REG_STATUS, &data);
	fusb303b_i2c_mask_write(chip, FUSB303B_REG_CONTROL1,
				FUSB303B_CONTROL1_ENABLE,
				FUSB303B_CONTROL1_ENABLE);
	ret = fusb303b_init_interrupt(chip);
	if (ret < 0)
		return ret;

	ret = fusb303b_i2c_read(chip, FUSB303B_REG_STATUS, &data);
	if (ret < 0)
		return ret;
	chip->vbus_present = !!(data & FUSB303B_STATUS_VBUSOK);
	ret = fusb303b_i2c_read(chip, FUSB303B_REG_DEVICE_ID, &data);
	if (ret < 0)
		return ret;
	fusb303b_log(chip, "fusb303b device ID: 0x%02x", data);

	ret = fusb303b_i2c_read(chip, FUSB303B_REG_DEVICE_TYPE, &data);
	if (ret < 0)
		return ret;
	fusb303b_log(chip, "fusb303b type:0x%02x", data);

	return ret;
}

static int tcpm_get_vbus(struct tcpc_dev *dev)
{
	struct fusb303b_chip *chip =
		container_of(dev, struct fusb303b_chip, tcpc_dev);
	int ret = 0;

	mutex_lock(&chip->lock);
	ret = chip->vbus_present ? 1 : 0;
	mutex_unlock(&chip->lock);
	fusb303b_log(chip, "%s.%d vbus_present:%d\n", __FUNCTION__, __LINE__,
		     ret);
	return ret;
}

static const char *const typec_cc_status_name[] = {
	[TYPEC_CC_OPEN] = "Open",     [TYPEC_CC_RA] = "Ra",
	[TYPEC_CC_RD] = "Rd",	      [TYPEC_CC_RP_DEF] = "Rp-def",
	[TYPEC_CC_RP_1_5] = "Rp-1.5", [TYPEC_CC_RP_3_0] = "Rp-3.0",
};

static int tcpm_set_cc(struct tcpc_dev *dev, enum typec_cc_status cc)
{
	struct fusb303b_chip *chip =
		container_of(dev, struct fusb303b_chip, tcpc_dev);
	fusb303b_log(chip, "%s.%d cc:%s\n", __FUNCTION__, __LINE__,
		     typec_cc_status_name[cc]);

	return 0;
}

static int tcpm_get_cc(struct tcpc_dev *dev, enum typec_cc_status *cc1,
		       enum typec_cc_status *cc2)
{
	struct fusb303b_chip *chip =
		container_of(dev, struct fusb303b_chip, tcpc_dev);

	mutex_lock(&chip->lock);
	*cc1 = chip->cc1;
	*cc2 = chip->cc2;
	fusb303b_log(chip, "%s.%d,cc1=%s, cc2=%s", __FUNCTION__, __LINE__,
		     typec_cc_status_name[*cc1], typec_cc_status_name[*cc2]);
	mutex_unlock(&chip->lock);

	return 0;
}

static int tcpm_set_polarity(struct tcpc_dev *dev,
			     enum typec_cc_polarity polarity)
{
	struct fusb303b_chip *chip =
		container_of(dev, struct fusb303b_chip, tcpc_dev);
	fusb303b_log(chip, "%s.%d polarity:%d\n", __FUNCTION__, __LINE__,
		     polarity);
	return 0;
}

static int tcpm_set_vconn(struct tcpc_dev *dev, bool on)
{
	struct fusb303b_chip *chip =
		container_of(dev, struct fusb303b_chip, tcpc_dev);
	int ret = 0;

	fusb303b_log(chip, "%s.%d on:%d\n", __FUNCTION__, __LINE__, on);
	chip->vconn_on = on;

	return ret;
}

static int tcpm_set_vbus(struct tcpc_dev *dev, bool on, bool charge)
{
	struct fusb303b_chip *chip =
		container_of(dev, struct fusb303b_chip, tcpc_dev);

	fusb303b_log(chip, "%s.%d on:%d,charge:%d\n", __FUNCTION__, __LINE__,
		     on, charge);
	chip->vbus_on = on;
	chip->charge_on = charge;

	return 0;
}

static int tcpm_set_pd_rx(struct tcpc_dev *dev, bool on)
{
	struct fusb303b_chip *chip =
		container_of(dev, struct fusb303b_chip, tcpc_dev);

	fusb303b_log(chip, "%s.%d\n", __FUNCTION__, __LINE__);

	return 0;
}

static const char *const typec_role_name[] = {
	[TYPEC_SINK] = "Sink",
	[TYPEC_SOURCE] = "Source",
};

static const char *const typec_data_role_name[] = {
	[TYPEC_DEVICE] = "Device",
	[TYPEC_HOST] = "Host",
};

static int tcpm_set_roles(struct tcpc_dev *dev, bool attached,
			  enum typec_role pwr, enum typec_data_role data)
{
	struct fusb303b_chip *chip =
		container_of(dev, struct fusb303b_chip, tcpc_dev);
	fusb303b_log(chip, "%s.%d pwr:%d,%s data:%d,%s\n", __FUNCTION__,
		     __LINE__, pwr, typec_role_name[data], data,
		     typec_data_role_name[data]);

	return 0;
}

static s32 tcpm_start_toggling(struct tcpc_dev *dev,
			       enum typec_port_type port_type,
			       enum typec_cc_status cc)
{
	struct fusb303b_chip *chip =
		container_of(dev, struct fusb303b_chip, tcpc_dev);
	s32 ret = 0;

	fusb303b_log(chip, "%s.%d port_type:%d cc:%d\n", __FUNCTION__, __LINE__,
		     port_type, cc);
	switch (port_type) {
	case TYPEC_PORT_DRP:
		ret = fusb303b_i2c_write(chip, FUSB303B_REG_PORTROLE,
					 FUSB303B_PORTROLE_DRP);
		break;
	case TYPEC_PORT_SRC:
		ret = fusb303b_i2c_write(chip, FUSB303B_REG_PORTROLE,
					 FUSB303B_PORTROLE_SOURCE);
		break;
	default:
		ret = fusb303b_i2c_write(chip, FUSB303B_REG_PORTROLE,
					 FUSB303B_PORTROLE_SINK);
		break;
	}

	return ret;
}

static int tcpm_pd_transmit(struct tcpc_dev *dev, enum tcpm_transmit_type type,
			    const struct pd_message *msg,
			    unsigned int negotiated_rev)
{
	struct fusb303b_chip *chip =
		container_of(dev, struct fusb303b_chip, tcpc_dev);

	fusb303b_log(chip, "%s.%d negotiated_rev:%d\n", __FUNCTION__, __LINE__,
		     negotiated_rev);

	return 0;
}

static void init_tcpc_dev(struct tcpc_dev *fusb303b_tcpc_dev)
{
	fusb303b_tcpc_dev->init = tcpm_init;
	fusb303b_tcpc_dev->get_vbus = tcpm_get_vbus;
	fusb303b_tcpc_dev->set_cc = tcpm_set_cc;
	fusb303b_tcpc_dev->get_cc = tcpm_get_cc;
	fusb303b_tcpc_dev->set_polarity = tcpm_set_polarity;
	fusb303b_tcpc_dev->set_vconn = tcpm_set_vconn;
	fusb303b_tcpc_dev->set_vbus = tcpm_set_vbus;
	fusb303b_tcpc_dev->set_pd_rx = tcpm_set_pd_rx;
	fusb303b_tcpc_dev->set_roles = tcpm_set_roles;
	fusb303b_tcpc_dev->start_toggling = tcpm_start_toggling;
	fusb303b_tcpc_dev->pd_transmit = tcpm_pd_transmit;
}

enum fusb_cc_status {
	CC_NO_CONN,
	CC_CONN_CC1,
	CC_CONN_CC2,
	CC_CONN_FAULT,
};

static irqreturn_t fusb303b_irq(int irq, void *dev_id)
{
	struct fusb303b_chip *chip = dev_id;
	int ret = 0;
	u8 interrupt = 0, interrupt1;
	u8 status = 0;
	bool vbus_present = 0;
	u8 cc_status = 0;

	mutex_lock(&chip->lock);
	/* grab a snapshot of intr flags */

	ret = fusb303b_i2c_read(chip, FUSB303B_REG_INTERRUPT, &interrupt);
	if (ret < 0)
		goto done;
	ret = fusb303b_i2c_read(chip, FUSB303B_REG_INTERRUPT, &interrupt1);
	if (ret < 0)
		goto done;
	ret = fusb303b_i2c_read(chip, FUSB303B_REG_STATUS, &status);
	if (ret < 0)
		goto done;

	fusb303b_log(chip, "IRQ: 0x%02x, status: 0x%02x\n", interrupt, status);

	if (interrupt & FUSB303B_INT_I_VBUS_CHG) {
		vbus_present = !!(status & FUSB303B_STATUS_VBUSOK);
		fusb303b_log(chip, "IRQ: VBUS_OK, vbus=%s",
			     vbus_present ? "On" : "Off");
		if (vbus_present != chip->vbus_present) {
			chip->vbus_present = vbus_present;
			tcpm_vbus_change(chip->tcpm_port);
		}
	}
	cc_status = (status & FUSB303B_STATUS_MASK) >> 4;

	if (CC_CONN_CC1 == cc_status) {
		chip->cc_polarity = TYPEC_POLARITY_CC1;
		tcpm_cc_change(chip->tcpm_port);
	} else if (CC_CONN_CC2 == cc_status) {
		chip->cc_polarity = TYPEC_POLARITY_CC2;
		tcpm_cc_change(chip->tcpm_port);
	}

	if (0 != interrupt)
		fusb303b_i2c_write(chip, FUSB303B_REG_INTERRUPT, interrupt);
	if (0 != interrupt1)
		fusb303b_i2c_write(chip, FUSB303B_REG_INTERRUPT1, interrupt1);
done:
	mutex_unlock(&chip->lock);
	return IRQ_HANDLED;
}

static int init_gpio(struct fusb303b_chip *chip)
{
	struct device *dev = chip->dev;
	int ret = 0;

	chip->gpio_int_n = devm_gpiod_get(dev, "int", GPIOD_IN);
	if (IS_ERR(chip->gpio_int_n)) {
		dev_err(dev, "failed to request gpio_int_n\n");
		return PTR_ERR(chip->gpio_int_n);
	}
	ret = gpiod_to_irq(chip->gpio_int_n);
	if (ret < 0) {
		dev_err(dev, "cannot request IRQ for GPIO Int_N, ret=%d", ret);
		return ret;
	}
	chip->gpio_int_n_irq = ret;

	return 0;
}

static const struct property_entry port_props[] = {
	PROPERTY_ENTRY_STRING("data-role", "dual"),
	PROPERTY_ENTRY_STRING("power-role", "dual"),
	PROPERTY_ENTRY_STRING("try-power-role", "sink"),
	{}
};

static struct fwnode_handle *fusb303b_fwnode_get(struct device *dev)
{
	struct fwnode_handle *fwnode;
	fwnode = device_get_named_child_node(dev, "connector");
	if (!fwnode)
		fwnode = fwnode_create_software_node(port_props, NULL);

	return fwnode;
}

static int fusb303b_probe(struct i2c_client *client)
{
	struct fusb303b_chip *chip;
	struct device *dev = &client->dev;
	int ret = 0;
	struct regmap *regmap;
	int irq_sel_reg;
	int irq_sel_bit;
	regmap = syscon_regmap_lookup_by_phandle(dev->of_node, "eswin,syscfg");
	if (!IS_ERR(regmap)) {
		ret = of_property_read_u32_index(dev->of_node, "eswin,syscfg",
						 1, &irq_sel_reg);
		if (ret) {
			dev_err(dev,
				"can't get irq cfg reg offset in sys_con(errno:%d)\n",
				ret);
			return ret;
		}
		ret = of_property_read_u32_index(dev->of_node, "eswin,syscfg",
						 2, &irq_sel_bit);
		if (ret) {
			dev_err(dev,
				"can't get irq cfg bit offset in sys_con(errno:%d)\n",
				ret);
			return ret;
		}
		regmap_clear_bits(regmap, irq_sel_reg, BIT_ULL(irq_sel_bit));
	}

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->i2c_client = client;
	chip->dev = &client->dev;
	mutex_init(&chip->lock);
	spin_lock_init(&chip->irq_lock);
	init_tcpc_dev(&chip->tcpc_dev);
	fusb303b_debugfs_init(chip);
	if (client->irq) {
		chip->gpio_int_n_irq = client->irq;
	} else {
		ret = init_gpio(chip);
		if (ret < 0)
			goto destroy_workqueue;
	}
	chip->tcpc_dev.fwnode = fusb303b_fwnode_get(dev);
	if (IS_ERR(chip->tcpc_dev.fwnode)) {
		ret = PTR_ERR(chip->tcpc_dev.fwnode);
		goto destroy_workqueue;
	}
	chip->tcpm_port = tcpm_register_port(&client->dev, &chip->tcpc_dev);
	if (IS_ERR(chip->tcpm_port)) {
		fwnode_handle_put(chip->tcpc_dev.fwnode);
		ret = PTR_ERR(chip->tcpm_port);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "cannot register tcpm port, ret=%d", ret);
		goto destroy_workqueue;
	}

	ret = devm_request_threaded_irq(dev, chip->gpio_int_n_irq, NULL,
					fusb303b_irq,
					IRQF_ONESHOT | IRQF_TRIGGER_LOW,
					"fusb303b_interrupt_int_n", chip);
	if (ret < 0) {
		dev_err(dev, "cannot request IRQ for GPIO Int_N, ret=%d", ret);
		goto tcpm_unregister_port;
	}

	enable_irq_wake(chip->gpio_int_n_irq);
	i2c_set_clientdata(client, chip);

	fusb303b_log(chip, "Kernel thread created successfully\n");
	return ret;

tcpm_unregister_port:
	tcpm_unregister_port(chip->tcpm_port);
	fwnode_handle_put(chip->tcpc_dev.fwnode);
destroy_workqueue:
	fusb303b_debugfs_exit(chip);

	return ret;
}

static void fusb303b_remove(struct i2c_client *client)
{
	struct fusb303b_chip *chip = i2c_get_clientdata(client);

	disable_irq_wake(chip->gpio_int_n_irq);
	free_irq(chip->gpio_int_n_irq, chip);
	cancel_work_sync(&chip->irq_work);
	tcpm_unregister_port(chip->tcpm_port);
	fwnode_handle_put(chip->tcpc_dev.fwnode);
	fusb303b_debugfs_exit(chip);
}

static int fusb303b_pm_suspend(struct device *dev)
{
	struct fusb303b_chip *chip = dev->driver_data;
	unsigned long flags;

	spin_lock_irqsave(&chip->irq_lock, flags);
	chip->irq_suspended = true;
	spin_unlock_irqrestore(&chip->irq_lock, flags);

	flush_work(&chip->irq_work);
	return 0;
}

static int fufusb303b_pm_resume(struct device *dev)
{
	struct fusb303b_chip *chip = dev->driver_data;
	unsigned long flags;

	spin_lock_irqsave(&chip->irq_lock, flags);
	if (chip->irq_while_suspended) {
		schedule_work(&chip->irq_work);
		chip->irq_while_suspended = false;
	}
	chip->irq_suspended = false;
	spin_unlock_irqrestore(&chip->irq_lock, flags);

	return 0;
}

static const struct of_device_id fusb303b_dt_match[] = {
	{ .compatible = "fcs,fusb303b" },
	{},
};
MODULE_DEVICE_TABLE(of, fusb303b_dt_match);

static const struct i2c_device_id fusb303b_i2c_device_id[] = {
	{ "typec_fusb303b", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, fusb303b_i2c_device_id);

static const struct dev_pm_ops fusb303b_pm_ops = {
	.suspend = fusb303b_pm_suspend,
	.resume = fufusb303b_pm_resume,
};

static struct i2c_driver fusb303b_driver = {
	.driver = {
		   .name = "typec_fusb303b",
		   .pm = &fusb303b_pm_ops,
		   .of_match_table = of_match_ptr(fusb303b_dt_match),
		   },
	.probe = fusb303b_probe,
	.remove = fusb303b_remove,
	.id_table = fusb303b_i2c_device_id,
};
module_i2c_driver(fusb303b_driver);

MODULE_AUTHOR("Yang Wei <yangwei1@eswincomputing.com>");
MODULE_DESCRIPTION("Onsemi FUSB303B Type-C Chip Driver");
MODULE_LICENSE("GPL");
