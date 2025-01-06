// SPDX-License-Identifier: GPL-2.0
/*
 * ESWIN Reset Driver
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
 * Authors: HuangYiFeng<huangyifeng@eswincomputing.com>
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#define SYSCRG_CLEAR_BOOT_INFO_OFFSET    (0x30C)
#define CLEAR_BOOT_FLAG_BIT              BIT_ULL(0)
#define SYSCRG_RESET_OFFSET              (0x400)

/**
 * struct eswin_reset_data - reset controller information structure
 * @rcdev: reset controller entity
 * @dev: reset controller device pointer
 * @idr: idr structure for mapping ids to reset control structures
 */
struct eswin_reset_data {
	struct reset_controller_dev rcdev;
	struct device *dev;
	struct idr idr;
	struct regmap *regmap;
};

/**
 * struct eswin_reset_control - reset control structure
 * @dev_id: SoC-specific device identifier
 * @reset_bit: reset mask to use for toggling reset
 */
struct eswin_reset_control {
	u32 dev_id;
	u32 reset_bit;
};

#define to_eswin_reset_data(p)	\
	container_of((p), struct eswin_reset_data, rcdev)

/**
 * eswin_reset_set() - program a device's reset
 * @rcdev: reset controller entity
 * @id: ID of the reset to toggle
 * @assert: boolean flag to indicate assert or deassert
 *
 * This is a common internal function used to assert or deassert a device's
 * reset by clear and set the reset bit. The device's reset is asserted if the
 * @assert argument is true, or deasserted if @assert argument is false.
 *
 * Return: 0 for successful request, else a corresponding error value
 */
static int eswin_reset_set(struct reset_controller_dev *rcdev,
			    unsigned long id, bool assert)
{
	struct eswin_reset_data *data = to_eswin_reset_data(rcdev);
	struct eswin_reset_control *control;
	int ret;

	control = idr_find(&data->idr, id);

	dev_dbg(rcdev->dev, "dev_id 0x%x reset_bit 0x%x assert 0x%x\r\n",
		control->dev_id, control->reset_bit, assert);

	if (!control)
		return -EINVAL;

	if (assert) {
		ret = regmap_clear_bits(data->regmap, SYSCRG_RESET_OFFSET + control->dev_id * sizeof(u32),
			control->reset_bit);
	} else {
		ret = regmap_set_bits(data->regmap, SYSCRG_RESET_OFFSET + control->dev_id * sizeof(u32),
			control->reset_bit);
	}

	return ret;
}

static int eswin_reset_reset(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	int ret;

	ret = eswin_reset_set(rcdev, id, true);
	if (0 != ret) {
		return ret;
	}
	usleep_range(10, 15);
	ret = eswin_reset_set(rcdev, id, false);
	if (0 != ret) {
		return ret;
	}

	return 0;
}

static int eswin_reset_assert(struct reset_controller_dev *rcdev,
			      unsigned long id)
{
	return eswin_reset_set(rcdev, id, true);
}

static int eswin_reset_deassert(struct reset_controller_dev *rcdev,
				unsigned long id)
{
	return eswin_reset_set(rcdev, id, false);
}

static const struct reset_control_ops eswin_reset_ops = {
	.reset		= eswin_reset_reset,
	.assert		= eswin_reset_assert,
	.deassert	= eswin_reset_deassert,
};

static int eswin_reset_of_xlate_lookup_id(int id, void *p, void *data)
{
	struct of_phandle_args *reset_spec = (struct of_phandle_args *)data;
	struct eswin_reset_control *slot_control = (struct eswin_reset_control *)p;

	if (reset_spec->args[0] == slot_control->dev_id
		&& reset_spec->args[1] == slot_control->reset_bit) {
			return id;
	} else {
		return 0;
	}
}

/**
 * eswin_reset_of_xlate() - translate a set of OF arguments to a reset ID
 * @rcdev: reset controller entity
 * @reset_spec: OF reset argument specifier
 *
 * This function performs the translation of the reset argument specifier
 * values defined in a reset consumer device node. The function allocates a
 * reset control structure for that device reset, and will be used by the
 * driver for performing any reset functions on that reset. An idr structure
 * is allocated and used to map to the reset control structure. This idr
 * is used by the driver to do reset lookups.
 *
 * Return: 0 for successful request, else a corresponding error value
 */
static int eswin_reset_of_xlate(struct reset_controller_dev *rcdev,
				 const struct of_phandle_args *reset_spec)
{
	struct eswin_reset_data *data = to_eswin_reset_data(rcdev);
	struct eswin_reset_control *control;
	int ret;

	if (WARN_ON(reset_spec->args_count != rcdev->of_reset_n_cells))
		return -EINVAL;

	ret = idr_for_each(&data->idr, eswin_reset_of_xlate_lookup_id, (void *)reset_spec);
	if (0 != ret) {
		return ret;
	}

	control = devm_kzalloc(data->dev, sizeof(*control), GFP_KERNEL);
	if (!control)
		return -ENOMEM;

	control->dev_id = reset_spec->args[0];
	control->reset_bit = reset_spec->args[1];

	return idr_alloc(&data->idr, control, 0, 0, GFP_KERNEL);
}

static const struct of_device_id eswin_reset_dt_ids[] = {
	 { .compatible = "eswin,eic7700-reset", },
	 { /* sentinel */ },
};

static int eswin_reset_probe(struct platform_device *pdev)
{
	struct eswin_reset_data *data;
	struct device *parent;

	parent = pdev->dev.parent;
	if (!parent) {
		dev_err(&pdev->dev, "no parent\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->regmap = syscon_node_to_regmap(parent->of_node);
	if (IS_ERR(data->regmap)) {
		dev_err(&pdev->dev, "failed to get parent regmap\n");
		return PTR_ERR(data->regmap);
	}

	platform_set_drvdata(pdev, data);

	data->rcdev.owner = THIS_MODULE;
	data->rcdev.ops = &eswin_reset_ops;
	data->rcdev.of_node = pdev->dev.of_node;
	data->rcdev.of_reset_n_cells = 2;
	data->rcdev.of_xlate = eswin_reset_of_xlate;
	data->rcdev.dev = &pdev->dev;
	data->dev = &pdev->dev;
	idr_init(&data->idr);

	/*clear boot flag so u84 and scpu could be reseted by software*/
	regmap_set_bits(data->regmap, SYSCRG_CLEAR_BOOT_INFO_OFFSET, CLEAR_BOOT_FLAG_BIT);
	msleep(50);

	platform_set_drvdata(pdev, data);

	return devm_reset_controller_register(&pdev->dev, &data->rcdev);
}

static void eswin_reset_remove(struct platform_device *pdev)
{
	struct eswin_reset_data *data = platform_get_drvdata(pdev);

	idr_destroy(&data->idr);
}

static struct platform_driver eswin_reset_driver = {
	.probe	= eswin_reset_probe,
	.remove = eswin_reset_remove,
	.driver = {
		.name		= "eswin-reset",
		.of_match_table	= eswin_reset_dt_ids,
	},
};

static int __init eic7700_reset_init(void)
{
	return platform_driver_register(&eswin_reset_driver);
}
arch_initcall(eic7700_reset_init);

MODULE_AUTHOR("Huangyifeng <huangyifeng@eswincomputing.com>");
MODULE_DESCRIPTION("Eswin Reset Controller Driver for EIC770X SoCs");
MODULE_LICENSE("GPL v2");
