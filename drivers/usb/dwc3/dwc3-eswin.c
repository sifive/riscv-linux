// SPDX-License-Identifier: GPL-2.0
/*
 * eswin Specific Glue layer
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
 * Authors: Han Min <hanmin@eswincomputing.com>
 */

#include <linux/async.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/extcon.h>
#include <linux/freezer.h>
#include <linux/iopoll.h>
#include <linux/reset.h>
#include <linux/usb.h>
#include <linux/pm.h>
#include <linux/usb/hcd.h>
#include <linux/usb/ch9.h>
#include <linux/extcon-provider.h>
#include <linux/iommu.h>
#include <linux/mfd/syscon.h>
#include <linux/bitfield.h>
#include <linux/eic7700-sid-cfg.h>
#include <linux/regmap.h>
#include <linux/gpio/consumer.h>
#include "core.h"
#include "io.h"

#define AWSMMUSID GENMASK(31, 24) // The sid of write operation
#define AWSMMUSSID GENMASK(23, 16) // The ssid of write operation
#define ARSMMUSID GENMASK(15, 8) // The sid of read operation
#define ARSMMUSSID GENMASK(7, 0) // The ssid of read operation

#define HSP_USB_VBUS_FSEL 0x2a
#define HSP_USB_MPLL_DEFAULT 0x0

#define HSP_USB_BUS_FILTER_EN (0x1 << 0)
#define HSP_USB_BUS_CLKEN_GM (0x1 << 9)
#define HSP_USB_BUS_CLKEN_GS (0x1 << 16)
#define HSP_USB_BUS_SW_RST (0x1 << 24)
#define HSP_USB_BUS_CLK_EN (0x1 << 28)

#define HSP_USB_AXI_LP_XM_CSYSREQ (0x1 << 0)
#define HSP_USB_AXI_LP_XS_CSYSREQ (0x1 << 16)

struct dwc3_eswin {
	int num_clocks;
	bool connected;
	bool suspended;
	bool force_mode;
	bool is_phy_on;
	struct device *dev;
	struct clk **clks;
	struct dwc3 *dwc;
	struct extcon_dev *edev;
	struct usb_hcd *hcd;
	struct notifier_block device_nb;
	struct notifier_block host_nb;
	struct work_struct otg_work;
	struct mutex lock;
	struct reset_control *vaux_rst;
	struct device *child_dev;
	enum usb_role new_usb_role;
};

static ssize_t dwc3_mode_show(struct device *device,
			      struct device_attribute *attr, char *buf)
{
	struct dwc3_eswin *eswin = dev_get_drvdata(device);
	struct dwc3 *dwc = eswin->dwc;
	int ret;

	switch (dwc->current_dr_role) {
	case USB_DR_MODE_HOST:
		ret = sprintf(buf, "host\n");
		break;
	case USB_DR_MODE_PERIPHERAL:
		ret = sprintf(buf, "peripheral\n");
		break;
	case USB_DR_MODE_OTG:
		ret = sprintf(buf, "otg\n");
		break;
	default:
		ret = sprintf(buf, "UNKNOWN\n");
	}

	return ret;
}

static ssize_t dwc3_mode_store(struct device *device,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct dwc3_eswin *eswin = dev_get_drvdata(device);
	struct dwc3 *dwc = eswin->dwc;
	enum usb_role new_role;
	struct usb_role_switch *role_sw = dwc->role_sw;

	if (!strncmp(buf, "1", 1) || !strncmp(buf, "host", 4)) {
		new_role = USB_ROLE_HOST;
	} else if (!strncmp(buf, "0", 1) || !strncmp(buf, "peripheral", 10)) {
		new_role = USB_ROLE_DEVICE;
	} else {
		dev_info(eswin->dev, "illegal dr_mode\n");
		return count;
	}
	eswin->force_mode = true;

	mutex_lock(&eswin->lock);
	usb_role_switch_set_role(role_sw, new_role);
	mutex_unlock(&eswin->lock);

	return count;
}

static DEVICE_ATTR_RW(dwc3_mode);

static struct attribute *dwc3_eswin_attrs[] = {
	&dev_attr_dwc3_mode.attr,
	NULL,
};

static struct attribute_group dwc3_eswin_attr_group = {
	.name = NULL, /* we want them in the same directory */
	.attrs = dwc3_eswin_attrs,
};

static int dwc3_eswin_device_notifier(struct notifier_block *nb,
					 unsigned long event, void *ptr)
{
	struct dwc3_eswin *eswin =
		container_of(nb, struct dwc3_eswin, device_nb);

	mutex_lock(&eswin->lock);
	eswin->new_usb_role = USB_ROLE_DEVICE;
	mutex_unlock(&eswin->lock);
	if (!eswin->suspended)
		schedule_work(&eswin->otg_work);

	return NOTIFY_DONE;
}

static int dwc3_eswin_host_notifier(struct notifier_block *nb,
				    unsigned long event, void *ptr)
{
	struct dwc3_eswin *eswin = container_of(nb, struct dwc3_eswin, host_nb);
	mutex_lock(&eswin->lock);
	eswin->new_usb_role = USB_ROLE_HOST;
	mutex_unlock(&eswin->lock);
	if (!eswin->suspended)
		schedule_work(&eswin->otg_work);

	return NOTIFY_DONE;
}

static void dwc3_eswin_otg_extcon_evt_work(struct work_struct *work)
{
	struct dwc3_eswin *eswin =
		container_of(work, struct dwc3_eswin, otg_work);
	struct usb_role_switch *role_sw = eswin->dwc->role_sw;
	if (true == eswin->force_mode) {
		return;
	}
	mutex_lock(&eswin->lock);
	usb_role_switch_set_role(role_sw, eswin->new_usb_role);
	mutex_unlock(&eswin->lock);
}

static int dwc3_eswin_get_extcon_dev(struct dwc3_eswin *eswin)
{
	struct device *dev = eswin->dev;
	struct extcon_dev *edev;
	s32 ret = 0;

	if (device_property_read_bool(dev, "extcon")) {
		edev = extcon_get_edev_by_phandle(dev, 0);
		if (IS_ERR(edev)) {
			if (PTR_ERR(edev) != -EPROBE_DEFER)
				dev_err(dev, "couldn't get extcon device\n");
			return PTR_ERR(edev);
		}
		eswin->edev = edev;
		eswin->device_nb.notifier_call = dwc3_eswin_device_notifier;
		ret = devm_extcon_register_notifier(dev, edev, EXTCON_USB,
						    &eswin->device_nb);
		if (ret < 0)
			dev_err(dev, "failed to register notifier for USB\n");

		eswin->host_nb.notifier_call = dwc3_eswin_host_notifier;
		ret = devm_extcon_register_notifier(dev, edev, EXTCON_USB_HOST,
						    &eswin->host_nb);
		if (ret < 0)
			dev_err(dev, "failed to register notifier for USB-HOST\n");
	}

	return 0;
}

static int dwc3_eswin_deassert(struct dwc3_eswin *eswin)
{
	int rc;

	if (eswin->vaux_rst) {
		rc = reset_control_deassert(eswin->vaux_rst);
		WARN_ON(0 != rc);
	}

	return 0;
}

static int dwc3_eswin_assert(struct dwc3_eswin *eswin)
{
	int rc = 0;

	if (eswin->vaux_rst) {
		rc = reset_control_assert(eswin->vaux_rst);
		WARN_ON(0 != rc);
	}

	return 0;
}

static int dwc_usb_clk_init(struct device *dev)
{
	struct regmap *regmap;
	u32 hsp_usb_bus;
	u32 hsp_usb_axi_lp;
	u32 hsp_usb_vbus_freq;
	u32 hsp_usb_mpll;
	int ret;

	regmap = syscon_regmap_lookup_by_phandle(dev->of_node, "eswin,hsp_sp_csr");
	if (IS_ERR(regmap)) {
		dev_dbg(dev, "No hsp_sp_csr phandle specified\n");
		return -1;
	}
	ret = of_property_read_u32_index(dev->of_node, "eswin,hsp_sp_csr", 1,
				&hsp_usb_bus);
	if (ret) {
		dev_err(dev, "can't get usb sid cfg reg offset (%d)\n", ret);
		return ret;
	}
	ret = of_property_read_u32_index(dev->of_node, "eswin,hsp_sp_csr", 2,
				&hsp_usb_axi_lp);
	if (ret) {
		dev_err(dev, "can't get usb sid cfg reg offset (%d)\n", ret);
		return ret;
	}
	ret = of_property_read_u32_index(dev->of_node, "eswin,hsp_sp_csr", 3,
				&hsp_usb_vbus_freq);
	if (ret) {
		dev_err(dev, "can't get usb sid cfg reg offset (%d)\n", ret);
		return ret;
	}
	ret = of_property_read_u32_index(dev->of_node, "eswin,hsp_sp_csr", 4,
				&hsp_usb_mpll);
	if (ret) {
		dev_err(dev, "can't get usb sid cfg reg offset (%d)\n", ret);
		return ret;
	}

	/*
	 * usb1 clock init
	 * ref clock is 24M, below need to be set to satisfy usb phy requirement(125M)
	 */
	regmap_write(regmap, hsp_usb_vbus_freq, HSP_USB_VBUS_FSEL);
	regmap_write(regmap, hsp_usb_mpll, HSP_USB_MPLL_DEFAULT);

	/*
	 * reset usb core and usb phy
	 */
	regmap_write(regmap, hsp_usb_bus,
		     HSP_USB_BUS_FILTER_EN | HSP_USB_BUS_CLKEN_GM |
			     HSP_USB_BUS_CLKEN_GS | HSP_USB_BUS_SW_RST |
			     HSP_USB_BUS_CLK_EN);
	regmap_write(regmap, hsp_usb_axi_lp,
		     HSP_USB_AXI_LP_XM_CSYSREQ | HSP_USB_AXI_LP_XS_CSYSREQ);

	return 0;
}

int dwc3_sid_cfg(struct device *dev)
{
	int ret;
	struct regmap *regmap;
	int hsp_mmu_usb_reg;
	u32 rdwr_sid_ssid;
	u32 sid;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);

	/* not behind smmu, use the default reset value(0x0) of the reg as streamID*/
	if (fwspec == NULL) {
		dev_dbg(dev, "dev is not behind smmu, skip configuration of sid\n");
		return 0;
	}
	sid = fwspec->ids[0];

	regmap = syscon_regmap_lookup_by_phandle(dev->of_node, "eswin,hsp_sp_csr");
	if (IS_ERR(regmap)) {
		dev_dbg(dev, "No hsp_sp_csr phandle specified\n");
		return 0;
	}

	ret = of_property_read_u32_index(dev->of_node, "eswin,hsp_sp_csr", 1,
				&hsp_mmu_usb_reg);
	if (ret) {
		dev_err(dev, "can't get usb sid cfg reg offset (%d)\n", ret);
		return ret;
	}

	/* make the reading sid the same as writing sid, ssid is fixed to zero */
	rdwr_sid_ssid = FIELD_PREP(AWSMMUSID, sid);
	rdwr_sid_ssid |= FIELD_PREP(ARSMMUSID, sid);
	rdwr_sid_ssid |= FIELD_PREP(AWSMMUSSID, 0);
	rdwr_sid_ssid |= FIELD_PREP(ARSMMUSSID, 0);
	regmap_write(regmap, hsp_mmu_usb_reg, rdwr_sid_ssid);

	ret = eic7700_dynm_sid_enable(dev_to_node(dev));
	if (ret < 0)
	{
		dev_err(dev, "failed to config usb streamID(%d)!\n", sid);
	}
	else
	{
		dev_dbg(dev, "success to config usb streamID(%d)!\n", sid);
	}

	return ret;
}

static int dwc3_eswin_probe(struct platform_device *pdev)
{
	struct dwc3_eswin *eswin;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node, *child;
	struct platform_device *child_pdev;
	unsigned int count;
	int ret;
	int i;
	int err_desc = 0;
	struct gpio_desc *hub_gpio;

	hub_gpio = devm_gpiod_get(dev, "hub-rst", GPIOD_OUT_HIGH);
	err_desc = IS_ERR(hub_gpio);

	if (!err_desc) {
		gpiod_set_raw_value(hub_gpio, 1);
	}

	eswin = devm_kzalloc(dev, sizeof(*eswin), GFP_KERNEL);
	if (!eswin)
		return -ENOMEM;

	count = of_clk_get_parent_count(np);
	if (!count)
		return -ENOENT;

	eswin->num_clocks = count;
	eswin->force_mode = false;
	eswin->clks = devm_kcalloc(dev, eswin->num_clocks, sizeof(struct clk *),
				   GFP_KERNEL);
	if (!eswin->clks)
		return -ENOMEM;

	platform_set_drvdata(pdev, eswin);

	mutex_init(&eswin->lock);

	eswin->dev = dev;

	mutex_lock(&eswin->lock);

	for (i = 0; i < eswin->num_clocks; i++) {
		struct clk *clk;
		clk = of_clk_get(np, i);
		if (IS_ERR(clk)) {
			ret = PTR_ERR(clk);
			goto err0;
		}
		ret = clk_prepare_enable(clk);
		if (ret < 0) {
			clk_put(clk);
			goto err0;
		}

		eswin->clks[i] = clk;
	}

	eswin->vaux_rst = devm_reset_control_get(dev, "vaux");
	if (IS_ERR_OR_NULL(eswin->vaux_rst)) {
		dev_err(dev, "Failed to asic0_rst handle\n");
		return -EFAULT;
	}

	dwc3_eswin_deassert(eswin);
	dwc_usb_clk_init(dev);

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		dev_err(dev, "get_sync failed with err %d\n", ret);
		goto err1;
	}

	child = of_get_child_by_name(np, "dwc3");
	if (!child) {
		dev_err(dev, "failed to find dwc3 core node\n");
		ret = -ENODEV;
		goto err1;
	}

	/* Allocate and initialize the core */
	ret = of_platform_populate(np, NULL, NULL, dev);
	if (ret) {
		dev_err(dev, "failed to create dwc3 core\n");
		goto err1;
	}

	INIT_WORK(&eswin->otg_work, dwc3_eswin_otg_extcon_evt_work);

	child_pdev = of_find_device_by_node(child);
	if (!child_pdev) {
		dev_err(dev, "failed to find dwc3 core device\n");
		ret = -ENODEV;
		goto err2;
	}

	eswin->dwc = platform_get_drvdata(child_pdev);
	if (!eswin->dwc) {
		dev_err(dev, "failed to get drvdata dwc3\n");
		ret = -EPROBE_DEFER;
		goto err2;
	}
	eswin->child_dev = &child_pdev->dev;

	ret = eic7700_tbu_power(eswin->child_dev, true);
	if (ret)
	{
		dev_err(dev, "tbu power on failed %d\n", ret);
		goto err2;
	}

	ret = dwc3_sid_cfg(&child_pdev->dev);
	if (ret)
		goto err3;

	ret = dwc3_eswin_get_extcon_dev(eswin);
	if (ret < 0)
		goto err3;

	mutex_unlock(&eswin->lock);
	ret = sysfs_create_group(&dev->kobj, &dwc3_eswin_attr_group);
	if (ret)
		dev_err(dev, "failed to create sysfs group: %d\n", ret);

	return ret;

err3:
	ret = eic7700_tbu_power(eswin->child_dev, false);
	if (ret)
	{
		dev_err(dev, "tbu power2 off failed %d\n", ret);
	}

err2:
	cancel_work_sync(&eswin->otg_work);
	of_platform_depopulate(dev);

err1:
	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);
	dwc3_eswin_assert(eswin);

err0:
	for (i = 0; i < eswin->num_clocks && eswin->clks[i]; i++) {
		if (!pm_runtime_status_suspended(dev))
			clk_disable(eswin->clks[i]);
		clk_unprepare(eswin->clks[i]);
		clk_put(eswin->clks[i]);
	}

	mutex_unlock(&eswin->lock);

	return ret;
}

static int dwc3_eswin_remove(struct platform_device *pdev)
{
	struct dwc3_eswin *eswin = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;
	int i = 0;
	int ret = 0;
	cancel_work_sync(&eswin->otg_work);

	sysfs_remove_group(&dev->kobj, &dwc3_eswin_attr_group);

	/* Restore hcd state before unregistering xhci */
	if (eswin->edev && !eswin->connected) {
		struct usb_hcd *hcd = dev_get_drvdata(&eswin->dwc->xhci->dev);

		pm_runtime_get_sync(dev);

		/*
		 * The xhci code does not expect that HCDs have been removed.
		 * It will unconditionally call usb_remove_hcd() when the xhci
		 * driver is unloaded in of_platform_depopulate(). This results
		 * in a crash if the HCDs were already removed. To avoid this
		 * crash, add the HCDs here as dummy operation.
		 * This code should be removed after pm runtime support
		 * has been added to xhci.
		 */
		if (hcd->state == HC_STATE_HALT) {
			usb_add_hcd(hcd, hcd->irq, IRQF_SHARED);
			usb_add_hcd(hcd->shared_hcd, hcd->irq, IRQF_SHARED);
		}
	}

	of_platform_depopulate(dev);

	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);

	ret = eic7700_tbu_power(eswin->child_dev, false);
	if (ret)
	{
		dev_err(dev, "tbu power off failed %d\n", ret);
	}

	dwc3_eswin_assert(eswin);
	for (i = 0; i < eswin->num_clocks; i++) {
		if (!pm_runtime_status_suspended(dev))
			clk_disable(eswin->clks[i]);
		clk_unprepare(eswin->clks[i]);
		clk_put(eswin->clks[i]);
	}

	return 0;
}

#ifdef CONFIG_PM
static int dwc3_eswin_runtime_suspend(struct device *dev)
{
	struct dwc3_eswin *eswin = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < eswin->num_clocks; i++)
		clk_disable(eswin->clks[i]);

	device_init_wakeup(dev, false);

	return 0;
}

static int dwc3_eswin_runtime_resume(struct device *dev)
{
	struct dwc3_eswin *eswin = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < eswin->num_clocks; i++)
		clk_enable(eswin->clks[i]);

	device_init_wakeup(dev, true);

	return 0;
}

static int __maybe_unused dwc3_eswin_suspend(struct device *dev)
{
	struct dwc3_eswin *eswin = dev_get_drvdata(dev);
	struct dwc3 *dwc = eswin->dwc;

	eswin->suspended = true;
	cancel_work_sync(&eswin->otg_work);

	/*
	 * The flag of is_phy_on is only true if
	 * the DWC3 is in Host mode.
	 */
	if (eswin->is_phy_on) {
		phy_power_off(dwc->usb2_generic_phy);

		/*
		 * If link state is Rx.Detect, it means that
		 * no usb device is connecting with the DWC3
		 * Host, and need to power off the USB3 PHY.
		 */
		dwc->link_state = dwc3_gadget_get_link_state(dwc);
		if (dwc->link_state == DWC3_LINK_STATE_RX_DET)
			phy_power_off(dwc->usb3_generic_phy);
	}

	return 0;
}

static int __maybe_unused dwc3_eswin_resume(struct device *dev)
{
	struct dwc3_eswin *eswin = dev_get_drvdata(dev);
	struct dwc3 *dwc = eswin->dwc;

	eswin->suspended = false;

	if (eswin->is_phy_on) {
		phy_power_on(dwc->usb2_generic_phy);

		if (dwc->link_state == DWC3_LINK_STATE_RX_DET)
			phy_power_on(dwc->usb3_generic_phy);
	}

	if (eswin->edev)
		schedule_work(&eswin->otg_work);

	return 0;
}

static const struct dev_pm_ops dwc3_eswin_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(dwc3_eswin_suspend, dwc3_eswin_resume)
	SET_RUNTIME_PM_OPS(dwc3_eswin_runtime_suspend,
			   dwc3_eswin_runtime_resume, NULL)
};

#define DEV_PM_OPS      (&dwc3_eswin_dev_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif /* CONFIG_PM */

static const struct of_device_id eswin_dwc3_match[] = {
	{ .compatible = "eswin,eic7700-dwc3" },
	{ /* Sentinel */ }
};

MODULE_DEVICE_TABLE(of, eswin_dwc3_match);

static struct platform_driver dwc3_eswin_driver = {
	.probe		= dwc3_eswin_probe,
	.remove		= dwc3_eswin_remove,
	.driver		= {
		.name	= "eswin-dwc3",
		.pm	= DEV_PM_OPS,
		.of_match_table = eswin_dwc3_match,
	},
};

module_platform_driver(dwc3_eswin_driver);

MODULE_ALIAS("platform:eswin-dwc3");
MODULE_AUTHOR("Han Min <hanmin@eswin.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 ESWIN Glue Layer");
