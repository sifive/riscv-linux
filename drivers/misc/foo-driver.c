/*
 * foo-drvier.c - foo driver used to bypass smmu of PCIe
 *
 * Copyright 2024, Beijing ESWIN Computing Technology Co., Ltd.. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 *
 * Author: Lin Min  <linmin@eswincomputing.com>
 *
 */

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>

static struct miscdevice foo_dev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "foo_a_misc_dev",
};

static int foo_device_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int err = 0;

	dev_info(dev, "---------%s:%d\n", __func__, __LINE__);
	err = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(48));
	if (err)
		dev_warn(dev, "Unable to set coherent mask\n");


	err = misc_register(&foo_dev);
	if (err) {
		dev_err(dev, "Failed to register misc device: %d\n", err);
	}

	return err;
}

static int foo_device_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s remove!\n", pdev->name);

	misc_deregister(&foo_dev);

	return 0;
}

static const struct of_device_id foo_of_match[] = {
	{ .compatible = "riscv,dev-foo", },
	{ },
};
MODULE_DEVICE_TABLE(of, foo_of_match);


static struct platform_driver foo_driver = {
	.driver	= {
		.name			= "riscv-dev-foo",
		.of_match_table		= foo_of_match,
		.suppress_bind_attrs	= true,
	},
	.probe	= foo_device_probe,
	.remove	= foo_device_remove,
};


static struct platform_driver * const drivers[] = {
	&foo_driver,
};
static int __init foo_dev_init(void)
{
	int err;


	err = platform_register_drivers(drivers, ARRAY_SIZE(drivers));

	return err;
}
module_init(foo_dev_init);

static void __exit foo_dev_exit(void)
{
	platform_unregister_drivers(drivers, ARRAY_SIZE(drivers));
}
module_exit(foo_dev_exit);

MODULE_DESCRIPTION("foo driver used to bypass smmu for PCIe");
MODULE_AUTHOR("Lin MIn <linmin@eswincomputing.com>");
MODULE_LICENSE("GPL v2");
