// SPDX-License-Identifier: GPL-2.0
/*
 * BUS error monitor of core driver
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
 * Authors: Yu Ning <ningyu@eswincomputing.com>
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

/**
 * @brief Bus error unit register map
 * 0x00 : cause of error event
 * 0x08 : physical address of error event
 * 0x10 : event enable mask
 * 0x18 : platform-level interrupt enable mask
 * 0x20 : accrued event mask
 * 0x28 : hart-local interrupt enable mask
 */

struct bus_error_device {
	struct device *dev;
	void __iomem *control;
	int plic_irq;
};

static irqreturn_t bus_error_handle(int irq, void *dev_id)
{
	struct bus_error_device *bus_err = dev_id;
	void __iomem *base = bus_err->control;

	printk(KERN_ERR "bus error of cause event: %d, accrued: 0x%x, physical address: 0x%llx\n",
		readl(base),readl(base+0x20),readq(base+0x8));

	/* clean interrupt */
	writel(0,base);
	writel(0,base+0x20);

	return IRQ_HANDLED;
}

static const struct of_device_id eswin_bus_error_of_match[] = {
	{.compatible = "sifive,buserror", },
	{ /* sentinel */ }
};

static int  bus_error_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	struct bus_error_device *bus_err_dev;
	int ret;
	struct resource *res;

	bus_err_dev = devm_kcalloc(dev, 1,
		sizeof(struct bus_error_device), GFP_KERNEL);
	if (!bus_err_dev)
		return -ENOMEM;

	bus_err_dev->dev = dev;
	dev_set_drvdata(dev, bus_err_dev);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "Error while get mem resource\n");
		return -ENODEV;
	}
	bus_err_dev->control = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR_OR_NULL(bus_err_dev->control)) {
		dev_err(dev, "Fail to get resource %s from 0x%llx!\n",
			node->name, res->start);
		ret = -EINVAL;
		goto free_bus_err_dev;
	}
	bus_err_dev->plic_irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(dev, bus_err_dev->plic_irq, bus_error_handle,
				IRQF_SHARED, dev_name(dev), bus_err_dev);
	if (ret) {
		dev_err(dev, "Fail to request irq %d \n",
				(int)bus_err_dev->plic_irq);
		return ret;
	}

	/* clean any interrupt before */
	writel(0,bus_err_dev->control);
	writel(0,bus_err_dev->control+0x20);

	/* enable interrupt */
	writel(0xee2,bus_err_dev->control+0x18);
	writel(0xee2,bus_err_dev->control+0x10);
	dev_dbg(dev, "Bus-err unit init OK\n");
	return 0;

free_bus_err_dev:
	return ret;

}

static struct platform_driver bus_error_driver = {
	.probe = bus_error_probe,
	.driver = {
		.name = "buserror",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(eswin_bus_error_of_match),},
};

static int __init init_bus_error_unit(void)
{
	return platform_driver_register(&bus_error_driver);
}

subsys_initcall(init_bus_error_unit);
