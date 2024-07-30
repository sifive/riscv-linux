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
 * Authors: Wei Yang <yangwei1@eswincomputing.com>
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
#define FUSB303B_REG_RESET 0XA
#define FUSB303B_REG_MASK 0XE
#define FUSB303B_REG_MASK1 0XF
#define FUSB303B_REG_STATUS 0X11
#define FUSB303B_REG_TYPE 0X13
#define FUSB303B_REG_INTERRUPT 0X14
#define FUSB303B_REG_INTERRUPT1 0X15

#define FUSB303B_PORTROLE_DRP BIT(2)
#define FUSB303B_PORTROLE_SINK BIT(1)
#define FUSB303B_PORTROLE_SOURCE BIT(0)

#define FUSB303B_CONTROL_DCABLE_EN BIT(3)
#define FUSB303B_CONTROL_INT_MASK BIT(0)

#define FUSB303B_CONTROL1_REMEDY_EN BIT(7)
#define FUSB303B_CONTROL1_ENABLE BIT(3)

#define FUSB303B_STATUS_VBUSOK BIT(3)
#define FUSB303B_STATUS_ATTACH BIT(0)

#define FUSB303B_INT_I_VBUS_CHG BIT(4)
#define FUSB303B_INT_I_DETACH BIT(1)
#define FUSB303B_INT_I_ATTACH BIT(0)

#define FUSB303B_TYPE_SINK BIT(4)
#define FUSB303B_TYPE_SOURCE BIT(3)

#define FUSB_REG_MASK_M_VBUS_CHG BIT(4)
#define FUSB_REG_MASK_M_DETACH BIT(1)
#define FUSB_REG_MASK_M_ATTACH BIT(0)

#define LOG_BUFFER_ENTRIES 1024
#define LOG_BUFFER_ENTRY_SIZE 128

struct fusb303b_chip
{
	struct device *dev;
	struct i2c_client *i2c_client;
	struct fwnode_handle *fwnode;
	spinlock_t irq_lock;
	struct work_struct irq_work;
	struct gpio_desc *gpio_int_n;
	int gpio_int_n_irq;
	struct usb_role_switch *role_sw;
	/* lock for sharing chip states */
	struct mutex lock;
	bool vbus_ok;
	bool attch_ok;
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
		strscpy(tmpbuffer, "overflow", sizeof(tmpbuffer));
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
	u8 int_unmask = FUSB_REG_MASK_M_VBUS_CHG |
                        FUSB_REG_MASK_M_DETACH | FUSB_REG_MASK_M_ATTACH;
	ret = fusb303b_i2c_write(chip, FUSB303B_REG_MASK,
                                0xFF & ~(int_unmask));
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

static int fusb303b_init(struct fusb303b_chip *chip)
{
	int ret = 0;
	u8 data;

	ret = fusb303b_sw_reset(chip);
	if (ret < 0)
		return ret;
	fusb303b_i2c_read(chip, FUSB303B_REG_STATUS, &data);
	fusb303b_i2c_mask_write(chip, FUSB303B_REG_CONTROL,
                                FUSB303B_CONTROL_DCABLE_EN,
                                FUSB303B_CONTROL_DCABLE_EN);
	fusb303b_i2c_mask_write(chip, FUSB303B_REG_CONTROL1,
                                FUSB303B_CONTROL1_ENABLE | FUSB303B_CONTROL1_REMEDY_EN,
                                FUSB303B_CONTROL1_ENABLE | FUSB303B_CONTROL1_REMEDY_EN);
	ret = fusb303b_init_interrupt(chip);
	if (ret < 0)
		return ret;

	ret = fusb303b_i2c_read(chip, FUSB303B_REG_STATUS, &data);
	if (ret < 0)
		return ret;
	chip->vbus_ok = !!(data & FUSB303B_STATUS_VBUSOK);
	chip->attch_ok = !!(data & FUSB303B_STATUS_ATTACH);
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

static s32 fusb303b_set_port_check(struct fusb303b_chip *chip,
                                    enum typec_port_type port_type)
{
	s32 ret = 0;

	fusb303b_log(chip, "%s.%d port_type:%d",
                    __FUNCTION__, __LINE__, port_type);
	switch (port_type)
	{
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

static int fusb303b_set_usb_role(struct fusb303b_chip *chip)
{
	u8 type = 0;
	int ret = 0;

	if ((true == chip->attch_ok) && (true == chip->vbus_ok))
	{
		ret = fusb303b_i2c_read(chip, FUSB303B_REG_TYPE, &type);
		if (ret < 0)
		{
			fusb303b_log(chip, "read type error:%d", ret);
			return ret;
		}
		fusb303b_log(chip, "%s type: 0x%02x", __func__, type);
		if (FUSB303B_TYPE_SOURCE == (FUSB303B_TYPE_SOURCE & type))
		{
			usb_role_switch_set_role(chip->role_sw, USB_ROLE_HOST);
			fusb303b_log(chip, "set usb to host");
		}
		else
		{
			usb_role_switch_set_role(chip->role_sw, USB_ROLE_DEVICE);
			fusb303b_log(chip, "set usb to device");
			if (FUSB303B_TYPE_SINK != (FUSB303B_TYPE_SINK & type))
			{
				fusb303b_log(chip, "illegel type:0x%02x,set usb to device", type);
			}
		}
	}

	return 0;
}

static irqreturn_t fusb303b_irq_intn(int irq, void *dev_id)
{
	struct fusb303b_chip *chip = dev_id;
	int ret = 0;
	u8 interrupt = 0, interrupt1 = 0, status = 0;

	mutex_lock(&chip->lock);
	ret = fusb303b_i2c_read(chip, FUSB303B_REG_INTERRUPT, &interrupt);
	if (ret < 0)
		goto done;
	ret = fusb303b_i2c_read(chip, FUSB303B_REG_INTERRUPT1, &interrupt1);
	if (ret < 0)
		goto done;
	ret = fusb303b_i2c_read(chip, FUSB303B_REG_STATUS, &status);
	if (ret < 0)
		goto done;

	fusb303b_log(chip, "IRQ: 0x%02x,0x%02x status: 0x%02x",
				 interrupt, interrupt1, status);

	if (interrupt & FUSB303B_INT_I_VBUS_CHG)
	{
		chip->vbus_ok = !!(status & FUSB303B_STATUS_VBUSOK);
		fusb303b_log(chip, "IRQ: VBUS_OK, vbus=%s",
                    chip->vbus_ok ? "On" : "Off");
	}
	if (interrupt & (FUSB303B_INT_I_ATTACH | FUSB303B_INT_I_DETACH))
	{
		chip->attch_ok = !!(status & FUSB303B_STATUS_ATTACH);
		fusb303b_log(chip, "IRQ: attach OK, attach=%s",
                        chip->attch_ok ? "On" : "Off");
	}
	fusb303b_set_usb_role(chip);
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
	if (IS_ERR(chip->gpio_int_n))
	{
		fusb303b_log(chip, "failed to request gpio_int_n\n");
		return PTR_ERR(chip->gpio_int_n);
	}
	ret = gpiod_to_irq(chip->gpio_int_n);
	if (ret < 0)
	{
		fusb303b_log(chip, "cannot request IRQ for GPIO Int_N, ret=%d", ret);
		return ret;
	}
	chip->gpio_int_n_irq = ret;

	return 0;
}

static const struct property_entry port_props[] = {
	PROPERTY_ENTRY_STRING("data-role", "dual"),
	PROPERTY_ENTRY_STRING("power-role", "dual"),
	PROPERTY_ENTRY_STRING("try-power-role", "sink"),
	{}};

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
	const char *cap_str;
	int usb_data_role = 0;
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
	fusb303b_init(chip);
	fusb303b_debugfs_init(chip);
	if (client->irq)
	{
		chip->gpio_int_n_irq = client->irq;
	}
	else
	{
		ret = init_gpio(chip);
		if (ret < 0)
			goto destroy_workqueue;
	}
	chip->fwnode = fusb303b_fwnode_get(dev);
	if (IS_ERR(chip->fwnode))
	{
		ret = PTR_ERR(chip->fwnode);
		goto destroy_workqueue;
	}
	/*
	 * This fwnode has a "compatible" property, but is never populated as a
	 * struct device. Instead we simply parse it to read the properties.
	 * This it breaks fw_devlink=on. To maintain backward compatibility
	 * with existing DT files, we work around this by deleting any
	 * fwnode_links to/from this fwnode.
	 */
	fw_devlink_purge_absent_suppliers(chip->fwnode);

	/* USB data support is optional */
	ret = fwnode_property_read_string(chip->fwnode, "data-role", &cap_str);
	if (ret == 0)
	{
		ret = typec_find_port_data_role(cap_str);
		if (ret < 0)
		{
			fusb303b_log(chip, "%s is not leage data-role\n", cap_str);
			goto put_fwnode;
		}
		usb_data_role = ret;
	}
	else
	{
		fusb303b_log(chip, "cannot find data-role in dts\n");
	}
	chip->role_sw = usb_role_switch_get(chip->dev);
	if (IS_ERR(chip->role_sw))
	{
		ret = PTR_ERR(chip->role_sw);
		fusb303b_log(chip, "get role_sw error");
		goto put_fwnode;
	}

	ret = devm_request_threaded_irq(dev, chip->gpio_int_n_irq, NULL,
									fusb303b_irq_intn,
									IRQF_ONESHOT | IRQF_TRIGGER_LOW,
									"fusb303b_interrupt_int_n", chip);
	if (ret < 0)
	{
		fusb303b_log(chip, "cannot request IRQ for GPIO Int_N, ret=%d", ret);
		goto err_put_role;
	}

	enable_irq_wake(chip->gpio_int_n_irq);
	fusb303b_set_port_check(chip, usb_data_role);

	i2c_set_clientdata(client, chip);
	fusb303b_log(chip, "Kernel thread created successfully");
	return ret;
err_put_role:
	usb_role_switch_put(chip->role_sw);
put_fwnode:
	fwnode_handle_put(chip->fwnode);
destroy_workqueue:
	fusb303b_debugfs_exit(chip);

	return ret;
}

static void fusb303b_remove(struct i2c_client *client)
{
	struct fusb303b_chip *chip = i2c_get_clientdata(client);

	disable_irq_wake(chip->gpio_int_n_irq);
	free_irq(chip->gpio_int_n_irq, chip);
	usb_role_switch_put(chip->role_sw);
	fwnode_handle_put(chip->fwnode);
	fusb303b_debugfs_exit(chip);
}

static const struct of_device_id fusb303b_dt_match[] = {
	{.compatible = "fcs,fusb303b"},
	{},
};
MODULE_DEVICE_TABLE(of, fusb303b_dt_match);

static const struct i2c_device_id fusb303b_i2c_device_id[] = {
	{"typec_fusb303b", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, fusb303b_i2c_device_id);

static struct i2c_driver fusb303b_driver = {
	.driver = {
		.name = "typec_fusb303b",
		.of_match_table = of_match_ptr(fusb303b_dt_match),
	},
	.probe = fusb303b_probe,
	.remove = fusb303b_remove,
	.id_table = fusb303b_i2c_device_id,
};
module_i2c_driver(fusb303b_driver);

MODULE_AUTHOR("Wei Yang <yangwei1@eswincomputing.com>");
MODULE_DESCRIPTION("Onsemi FUSB303B Type-C Chip Driver");
MODULE_LICENSE("GPL");
