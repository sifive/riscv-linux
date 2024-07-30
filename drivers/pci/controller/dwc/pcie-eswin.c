// SPDX-License-Identifier: GPL-2.0
/*
 * ESWIN PCIe root complex driver
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/resource.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/reset.h>
#include <linux/eic7700-sid-cfg.h>
#include <linux/gpio/consumer.h>
#include <linux/property.h>
#include "pcie-designware.h"

struct eswin_pcie {
	struct dw_pcie pci;
	void __iomem *mgmt_base;
	struct gpio_desc *reset;
	struct clk *pcie_aux;
	struct clk *pcie_cfg;
	struct clk *pcie_cr;
	struct clk *pcie_aclk;
	struct reset_control *powerup_rst;
	struct reset_control *cfg_rst;
	struct reset_control *perst;
};

#define PCIE_PM_SEL_AUX_CLK		BIT(16)
#define PCIEMGMT_APP_HOLD_PHY_RST	BIT(6)
#define PCIEMGMT_APP_LTSSM_ENABLE	BIT(5)
#define PCIEMGMT_DEVICE_TYPE_MASK	0xf

#define PCIEMGMT_CTRL0_OFFSET	0x0
#define PCIEMGMT_STATUS0_OFFSET	0x100

#define PCIE_TYPE_DEV_VEND_ID	0x0
#define PCIE_DSP_PF0_MSI_CAP	0x50
#define PCIE_NEXT_CAP_PTR		0x70
#define DEVICE_CONTROL_DEVICE_STATUS	0x78

#define PCIE_MSI_MULTIPLE_MSG_32	(0x5<<17)
#define PCIE_MSI_MULTIPLE_MSG_MASK	(0x7<<17)

#define PCIEMGMT_LINKUP_STATE_VALIDATE  ((0x11<<2)|0x3)
#define PCIEMGMT_LINKUP_STATE_MASK      0xff

static void eswin_pcie_shutdown(struct platform_device *pdev)
{
	struct eswin_pcie *pcie = platform_get_drvdata(pdev);

	/* Bring down link, so bootloader gets clean state in case of reboot */
	reset_control_assert(pcie->perst);
}

static int eswin_pcie_start_link(struct dw_pcie *pci)
{
	struct device *dev = pci->dev;
	struct eswin_pcie *pcie = dev_get_drvdata(dev);
	u32 val;

	/* Enable LTSSM */
	val = readl_relaxed(pcie->mgmt_base + PCIEMGMT_CTRL0_OFFSET);
	val |= PCIEMGMT_APP_LTSSM_ENABLE;
	writel_relaxed(val, pcie->mgmt_base + PCIEMGMT_CTRL0_OFFSET);
	return 0;
}

static int eswin_pcie_link_up(struct dw_pcie *pci)
{
	struct device *dev = pci->dev;
	struct eswin_pcie *pcie = dev_get_drvdata(dev);
	u32 val;

	val = readl_relaxed(pcie->mgmt_base + PCIEMGMT_STATUS0_OFFSET);
	if ((val & PCIEMGMT_LINKUP_STATE_MASK) == PCIEMGMT_LINKUP_STATE_VALIDATE)
		return 1;
	else
		return 0;
}

static int eswin_pcie_clk_enable(struct eswin_pcie *pcie)
{
	int ret;

	ret = clk_prepare_enable(pcie->pcie_cr);
	if (ret) {
		pr_err("PCIe: failed to enable cr clk: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(pcie->pcie_aclk);
	if (ret) {
		pr_err("PCIe: failed to enable aclk: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(pcie->pcie_cfg);
	if (ret) {
		pr_err("PCIe: failed to enable cfg_clk: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(pcie->pcie_aux);
	if (ret) {
		pr_err("PCIe: failed to enable aux_clk: %d\n", ret);
		return ret;
	}

	return 0;
}

static int eswin_pcie_clk_disable(struct eswin_pcie *eswin_pcie)
{
	clk_disable_unprepare(eswin_pcie->pcie_aux);
	clk_disable_unprepare(eswin_pcie->pcie_cfg);
	clk_disable_unprepare(eswin_pcie->pcie_cr);
	clk_disable_unprepare(eswin_pcie->pcie_aclk);

	return 0;
}

static int eswin_pcie_power_on(struct eswin_pcie *pcie)
{
	int ret = 0;

	/* pciet_cfg_rstn */
	ret = reset_control_reset(pcie->cfg_rst);
	WARN_ON(0 != ret);

	/* pciet_powerup_rstn */
	ret = reset_control_reset(pcie->powerup_rst);
	WARN_ON(0 != ret);

	return ret;
}

static int eswin_pcie_power_off(struct eswin_pcie *eswin_pcie)
{
	reset_control_assert(eswin_pcie->perst);

	reset_control_assert(eswin_pcie->powerup_rst);

	reset_control_assert(eswin_pcie->cfg_rst);

	return 0;
}

static int eswin_evb_socket_power_on(struct device *dev)
{
	int err_desc=0;
	struct gpio_desc *gpio;
	gpio = devm_gpiod_get(dev, "pci-socket", GPIOD_OUT_LOW);
	err_desc = IS_ERR(gpio);

	if (err_desc) {
		pr_debug("No power control gpio found, maybe not needed\n");
		return 0;
	}

	gpiod_set_value(gpio,1);

	return err_desc;
}

static int eswin_pcie_host_init(struct dw_pcie_rp *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct eswin_pcie *pcie = dev_get_drvdata(pci->dev);
	int ret;
	u32 val;

	/* pciet_aux_clken, pcie_cfg_clken */
	ret = eswin_pcie_clk_enable(pcie);
	if (ret)
		return ret;

	ret = eswin_pcie_power_on(pcie);
	if (ret)
		return ret;

	ret = eic7700_tbu_power(pcie->pci.dev, true);
	if (ret)
		return ret;

	/* set device type : rc */
	val = readl_relaxed(pcie->mgmt_base + PCIEMGMT_CTRL0_OFFSET);
	val &= 0xfffffff0;
	writel_relaxed(val|0x4, pcie->mgmt_base + PCIEMGMT_CTRL0_OFFSET);

	ret = reset_control_assert(pcie->perst);
	WARN_ON(0 != ret);

	eswin_evb_socket_power_on(pcie->pci.dev);
	msleep(100);
	ret = reset_control_deassert(pcie->perst);
	WARN_ON(0 != ret);

	/* app_hold_phy_rst */
	val = readl_relaxed(pcie->mgmt_base + PCIEMGMT_CTRL0_OFFSET);
	val &= ~(0x40);
	writel_relaxed(val, pcie->mgmt_base + PCIEMGMT_CTRL0_OFFSET);

	/* wait pm_sel_aux_clk to 0 */
	for (ret = 50; ret > 0; ret--) {
		val = readl_relaxed(pcie->mgmt_base + PCIEMGMT_STATUS0_OFFSET);
		if (!(val & PCIE_PM_SEL_AUX_CLK)) {
			break;
		}
		msleep(2);
	}

	if(!ret) {
		dev_err(pcie->pci.dev, "No clock present\n");
		eic7700_tbu_power(pcie->pci.dev, false);
		eswin_pcie_power_off(pcie);
		eswin_pcie_clk_disable(pcie);
		return -ENODEV;
	}

	/* config eswin vendor id and eic7700 device id */
	dw_pcie_writel_dbi(pci, PCIE_TYPE_DEV_VEND_ID, 0x20301fe1);

	/* lane fix config, real driver NOT need, default x4 */
	val = dw_pcie_readl_dbi(pci, PCIE_PORT_MULTI_LANE_CTRL);
	val &= 0xffffff80;
	val |= 0x44;
	dw_pcie_writel_dbi(pci, PCIE_PORT_MULTI_LANE_CTRL, val);

	val = dw_pcie_readl_dbi(pci, DEVICE_CONTROL_DEVICE_STATUS);
	val &= ~(0x7<<5);
	val |= (0x2<<5);
	dw_pcie_writel_dbi(pci, DEVICE_CONTROL_DEVICE_STATUS, val);

	/*  config support 32 msi vectors */
	val = dw_pcie_readl_dbi(pci, PCIE_DSP_PF0_MSI_CAP);
	val &= ~PCIE_MSI_MULTIPLE_MSG_MASK;
	val |= PCIE_MSI_MULTIPLE_MSG_32;
	dw_pcie_writel_dbi(pci, PCIE_DSP_PF0_MSI_CAP, val);

	/* disable msix cap */
	val = dw_pcie_readl_dbi(pci, PCIE_NEXT_CAP_PTR);
	val &= 0xffff00ff;
	dw_pcie_writel_dbi(pci, PCIE_NEXT_CAP_PTR, val);

	return 0;
}

static const struct dw_pcie_host_ops eswin_pcie_host_ops = {
	.init = eswin_pcie_host_init,
};

static const struct dw_pcie_ops dw_pcie_ops = {
	.start_link = eswin_pcie_start_link,
	.link_up = eswin_pcie_link_up,
};

static void __exit eswin_pcie_remove(struct platform_device *pdev)
{
	struct eswin_pcie *pcie = platform_get_drvdata(pdev);

	dw_pcie_host_deinit(&pcie->pci.pp);

	eic7700_tbu_power(&pdev->dev, false);
	eswin_pcie_power_off(pcie);
	eswin_pcie_clk_disable(pcie);
}

static int eswin_pcie_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dw_pcie *pci;
	struct eswin_pcie *pcie;

	pcie = devm_kzalloc(dev, sizeof(*pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;
	pci = &pcie->pci;
	pci->dev = dev;
	pci->ops = &dw_pcie_ops;
	pci->pp.ops = &eswin_pcie_host_ops;

	/* SiFive specific region: mgmt */
	pcie->mgmt_base = devm_platform_ioremap_resource_byname(pdev, "mgmt");
	if (IS_ERR(pcie->mgmt_base))
		return PTR_ERR(pcie->mgmt_base);

	/* Fetch clocks */
	pcie->pcie_aux = devm_clk_get(dev, "pcie_aux_clk");
	if (IS_ERR(pcie->pcie_aux)) {
		dev_err(dev, "pcie_aux clock source missing or invalid\n");
		return PTR_ERR(pcie->pcie_aux);
	}

	pcie->pcie_cfg = devm_clk_get(dev, "pcie_cfg_clk");
	if (IS_ERR(pcie->pcie_cfg)) {
		dev_err(dev, "pcie_cfg_clk clock source missing or invalid\n");
		return PTR_ERR(pcie->pcie_cfg);
	}

	pcie->pcie_cr = devm_clk_get(dev, "pcie_cr_clk");
	if (IS_ERR(pcie->pcie_cr)) {
		dev_err(dev, "pcie_cr_clk clock source missing or invalid\n");
		return PTR_ERR(pcie->pcie_cr);
	}

	pcie->pcie_aclk = devm_clk_get(dev, "pcie_aclk");

	if (IS_ERR(pcie->pcie_aclk)) {
		dev_err(dev, "pcie_aclk clock source missing or invalid\n");
		return PTR_ERR(pcie->pcie_aclk);
	}

	/* Fetch reset */
	pcie->powerup_rst = devm_reset_control_get_optional(&pdev->dev, "pcie_powerup");
	if (IS_ERR_OR_NULL(pcie->powerup_rst)) {
		dev_err_probe(dev, PTR_ERR(pcie->powerup_rst), "unable to get powerup reset\n");
	}

	pcie->cfg_rst = devm_reset_control_get_optional(&pdev->dev, "pcie_cfg");
	if (IS_ERR_OR_NULL(pcie->cfg_rst)) {
		dev_err_probe(dev, PTR_ERR(pcie->cfg_rst), "unable to get cfg reset\n");
	}

	pcie->perst = devm_reset_control_get_optional(&pdev->dev, "pcie_pwren");
	if (IS_ERR_OR_NULL(pcie->perst)) {
		dev_err_probe(dev, PTR_ERR(pcie->perst), "unable to get perst\n");
	}

	platform_set_drvdata(pdev, pcie);

	return dw_pcie_host_init(&pci->pp);
}

static const struct of_device_id eswin_pcie_of_match[] = {
	{ .compatible = "eswin,eic7700-pcie", },
	{},
};

static struct platform_driver eswin_pcie_driver = {
	.driver = {
		   .name = "eswin-pcie",
		   .of_match_table = eswin_pcie_of_match,
		   .suppress_bind_attrs = true,
	},
	.probe = eswin_pcie_probe,
	.remove = __exit_p(eswin_pcie_remove),
	.shutdown = eswin_pcie_shutdown,
};

module_platform_driver(eswin_pcie_driver);

MODULE_DEVICE_TABLE(of, eswin_pcie_of_match);
MODULE_DESCRIPTION("PCIe host controller driver for ESWIN EIC7700 SoCs");
MODULE_AUTHOR("Ning Yu <ningyu@eswincomputing.com>");
MODULE_LICENSE("GPL v2");
