// SPDX-License-Identifier: GPL-2.0
/*
 * ESWIN EIC7700 PCIe root complex driver
 *
 * Copyright 2025, Beijing ESWIN Computing Technology Co., Ltd.
 *
 * Authors: Yu Ning <ningyu@eswincomputing.com>
 *          Senchuan Zhang <zhangsenchuan@eswincomputing.com>
 *          Yanghui Ou <ouyanghui@eswincomputing.com>
 */

#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/reset.h>
#include <linux/types.h>

#include "../pci-host-common.h"
#include "pcie-designware.h"

/* ELBI registers */
#define PCIEELBI_CTRL0_OFFSET		0x0
#define PCIEELBI_STATUS0_OFFSET		0x100

/* LTSSM register fields */
#define PCIEELBI_APP_LTSSM_ENABLE	BIT(5)

/* APP_HOLD_PHY_RST register fields */
#define PCIEELBI_APP_HOLD_PHY_RST	BIT(6)

/* PM_SEL_AUX_CLK register fields */
#define PCIEELBI_PM_SEL_AUX_CLK		BIT(16)

/* DEV_TYPE register fields */
#define PCIEELBI_CTRL0_DEV_TYPE		GENMASK(3, 0)

/* Vendor and device ID value */
#define PCI_VENDOR_ID_ESWIN		0x1fe1
#define PCI_DEVICE_ID_ESWIN		0x2030

#define EIC7700_NUM_RSTS		ARRAY_SIZE(eic7700_pcie_rsts)

static const char * const eic7700_pcie_rsts[] = {
	"pwr",
	"dbi",
};

struct eic7700_pcie_data {
	bool msix_cap;
};

struct eic7700_pcie_port {
	struct list_head list;
	struct reset_control *perst;
	int num_lanes;
};

struct eic7700_pcie {
	struct dw_pcie pci;
	struct clk_bulk_data *clks;
	struct reset_control_bulk_data resets[EIC7700_NUM_RSTS];
	struct list_head ports;
	const struct eic7700_pcie_data *data;
	int num_clks;
	bool active_device;
};

#define to_eic7700_pcie(x) dev_get_drvdata((x)->dev)

static int eic7700_pcie_start_link(struct dw_pcie *pci)
{
	u32 val;

	/* Enable LTSSM */
	val = readl_relaxed(pci->elbi_base + PCIEELBI_CTRL0_OFFSET);
	val |= PCIEELBI_APP_LTSSM_ENABLE;
	writel_relaxed(val, pci->elbi_base + PCIEELBI_CTRL0_OFFSET);

	return 0;
}

static bool eic7700_pcie_link_up(struct dw_pcie *pci)
{
	u16 offset = dw_pcie_find_capability(pci, PCI_CAP_ID_EXP);
	u16 val = readw(pci->dbi_base + offset + PCI_EXP_LNKSTA);

	return val & PCI_EXP_LNKSTA_DLLLA;
}

static enum dw_pcie_ltssm eic7700_pcie_get_ltssm(struct dw_pcie *pci)
{
	dev_info(pci->dev, "LTSSM_L2 not supported\n");

	/* Return 0 only ensure skip read_poll_timeout function check */
	return 0;
}

static int eic7700_pcie_perst_deassert(struct eic7700_pcie_port *port,
				       struct eic7700_pcie *pcie)
{
	int ret;

	ret = reset_control_assert(port->perst);
	if (ret) {
		dev_err(pcie->pci.dev, "Failed to assert PERST#\n");
		return ret;
	}

	/* Ensure that PERST# has been asserted for at least 100 ms */
	msleep(PCIE_T_PVPERL_MS);

	ret = reset_control_deassert(port->perst);
	if (ret) {
		dev_err(pcie->pci.dev, "Failed to deassert PERST#\n");
		return ret;
	}

	return 0;
}

static int eic7700_pcie_parse_port(struct eic7700_pcie *pcie,
				   struct device_node *node)
{
	struct device *dev = pcie->pci.dev;
	struct eic7700_pcie_port *port;

	port = devm_kzalloc(dev, sizeof(*port), GFP_KERNEL);
	if (!port)
		return -ENOMEM;

	port->perst = of_reset_control_get_exclusive(node, "perst");
	if (IS_ERR(port->perst)) {
		dev_err(dev, "Failed to get PERST# reset\n");
		return PTR_ERR(port->perst);
	}

	/*
	 * Since the Root Port node is separated out by pcie devicetree, the
	 * DWC core initialization code cannot parse the num-lanes attribute
	 * in the Root Port. Before entering the DWC core initialization code,
	 * the platform driver code parses the Root Port node. The EIC7700 only
	 * supports one Root Port node, and the num-lanes attribute is suitable
	 * for the case of one Root Rort.
	 */
	of_property_read_u32(node, "num-lanes", &port->num_lanes);
	pcie->pci.num_lanes = port->num_lanes;

	INIT_LIST_HEAD(&port->list);
	list_add_tail(&port->list, &pcie->ports);

	return 0;
}

static int eic7700_pcie_parse_ports(struct eic7700_pcie *pcie)
{
	struct eic7700_pcie_port *port, *tmp;
	struct device *dev = pcie->pci.dev;
	int ret;

	for_each_available_child_of_node_scoped(dev->of_node, of_port) {
		ret = eic7700_pcie_parse_port(pcie, of_port);
		if (ret)
			goto err_port;
	}

	return 0;

err_port:
	list_for_each_entry_safe(port, tmp, &pcie->ports, list)
		list_del(&port->list);

	return ret;
}

static void eic7700_pcie_hide_broken_msix_cap(struct dw_pcie *pci)
{
	u16 offset, val;

	/*
	 * Hardware doesn't support MSI-X but it advertises MSI-X capability,
	 * to avoid this problem, the MSI-X capability in the PCIe capabilities
	 * linked-list needs to be disabled. Since the PCI Express capability
	 * structure's next pointer points to the MSI-X capability, and the
	 * MSI-X capability's next pointer is null (00H), so only the PCI
	 * Express capability structure's next pointer needs to be set 00H.
	 */
	offset = dw_pcie_find_capability(pci, PCI_CAP_ID_EXP);
	val = dw_pcie_readl_dbi(pci, offset);
	val &= ~PCI_CAP_LIST_NEXT_MASK;
	dw_pcie_writel_dbi(pci, offset, val);
}

static int eic7700_pcie_host_init(struct dw_pcie_rp *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct eic7700_pcie *pcie = to_eic7700_pcie(pci);
	struct eic7700_pcie_port *port;
	u8 msi_cap;
	u32 val;
	int ret;

	/*
	 * If resources are not released during suspend, resume does not
	 * require reinitializing them.
	 */
	if (pcie->active_device)
		return 0;

	pcie->num_clks = devm_clk_bulk_get_all_enabled(pci->dev, &pcie->clks);
	if (pcie->num_clks < 0)
		return dev_err_probe(pci->dev, pcie->num_clks,
				     "Failed to get pcie clocks\n");

	ret = reset_control_bulk_deassert(EIC7700_NUM_RSTS, pcie->resets);
	if (ret) {
		dev_err(pcie->pci.dev, "Failed to deassert resets\n");
		return ret;
	}

	/* Configure Root Port type */
	val = readl_relaxed(pci->elbi_base + PCIEELBI_CTRL0_OFFSET);
	val &= ~PCIEELBI_CTRL0_DEV_TYPE;
	val |= FIELD_PREP(PCIEELBI_CTRL0_DEV_TYPE, PCI_EXP_TYPE_ROOT_PORT);
	writel_relaxed(val, pci->elbi_base + PCIEELBI_CTRL0_OFFSET);

	list_for_each_entry(port, &pcie->ports, list) {
		ret = eic7700_pcie_perst_deassert(port, pcie);
		if (ret)
			goto err_perst;
	}

	/* Configure app_hold_phy_rst */
	val = readl_relaxed(pci->elbi_base + PCIEELBI_CTRL0_OFFSET);
	val &= ~PCIEELBI_APP_HOLD_PHY_RST;
	writel_relaxed(val, pci->elbi_base + PCIEELBI_CTRL0_OFFSET);

	/* The maximum waiting time for the clock switch lock is 20ms */
	ret = readl_poll_timeout(pci->elbi_base + PCIEELBI_STATUS0_OFFSET,
				 val, !(val & PCIEELBI_PM_SEL_AUX_CLK), 1000,
				 20000);
	if (ret) {
		dev_err(pci->dev, "Timeout waiting for PM_SEL_AUX_CLK ready\n");
		goto err_phy_init;
	}

	/*
	 * Configure ESWIN VID:DID for Root Port as the default values are
	 * invalid.
	 */
	dw_pcie_writew_dbi(pci, PCI_VENDOR_ID, PCI_VENDOR_ID_ESWIN);
	dw_pcie_writew_dbi(pci, PCI_DEVICE_ID, PCI_DEVICE_ID_ESWIN);

	/* Configure support 32 MSI vectors */
	msi_cap = dw_pcie_find_capability(pci, PCI_CAP_ID_MSI);
	val = dw_pcie_readw_dbi(pci, msi_cap + PCI_MSI_FLAGS);
	val &= ~PCI_MSI_FLAGS_QMASK;
	val |= FIELD_PREP(PCI_MSI_FLAGS_QMASK, 5);
	dw_pcie_writew_dbi(pci, msi_cap + PCI_MSI_FLAGS, val);

	/* Configure disable MSI-X cap */
	if (!pcie->data->msix_cap)
		eic7700_pcie_hide_broken_msix_cap(pci);

	return 0;

err_phy_init:
	list_for_each_entry(port, &pcie->ports, list)
		reset_control_assert(port->perst);
err_perst:
	reset_control_bulk_assert(EIC7700_NUM_RSTS, pcie->resets);

	return ret;
}

static void eic7700_pcie_host_deinit(struct dw_pcie_rp *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct eic7700_pcie *pcie = to_eic7700_pcie(pci);
	struct eic7700_pcie_port *port;

	if (pci_root_ports_have_device(pp->bridge->bus)) {
		pcie->active_device = true;
		return;
	}

	list_for_each_entry(port, &pcie->ports, list)
		reset_control_assert(port->perst);
	reset_control_bulk_assert(EIC7700_NUM_RSTS, pcie->resets);
	clk_bulk_disable_unprepare(pcie->num_clks, pcie->clks);
}

static void eic7700_pcie_pme_turn_off(struct dw_pcie_rp *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);

	/*
	 * Hardware doesn't support enter the D3code and L2/L3 states, send
	 * PME_Turn_Off message, which will then cause Vmain to be removed and
	 * controller stop working.
	 */
	dev_info(pci->dev, "Can't send PME_Turn_Off message\n");
}

static const struct dw_pcie_host_ops eic7700_pcie_host_ops = {
	.init = eic7700_pcie_host_init,
	.deinit = eic7700_pcie_host_deinit,
	.pme_turn_off = eic7700_pcie_pme_turn_off,
};

static const struct dw_pcie_ops dw_pcie_ops = {
	.start_link = eic7700_pcie_start_link,
	.link_up = eic7700_pcie_link_up,
	.get_ltssm = eic7700_pcie_get_ltssm,
};

static int eic7700_pcie_probe(struct platform_device *pdev)
{
	const struct eic7700_pcie_data *data;
	struct eic7700_pcie_port *port, *tmp;
	struct device *dev = &pdev->dev;
	struct eic7700_pcie *pcie;
	struct dw_pcie *pci;
	int ret, i;

	data = of_device_get_match_data(dev);
	if (!data)
		return dev_err_probe(dev, -EINVAL, "OF data missing\n");

	pcie = devm_kzalloc(dev, sizeof(*pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	INIT_LIST_HEAD(&pcie->ports);

	pci = &pcie->pci;
	pci->dev = dev;
	pci->ops = &dw_pcie_ops;
	pci->pp.ops = &eic7700_pcie_host_ops;
	pcie->data = data;

	for (i = 0; i < EIC7700_NUM_RSTS; i++)
		pcie->resets[i].id = eic7700_pcie_rsts[i];

	ret = devm_reset_control_bulk_get_exclusive(dev, EIC7700_NUM_RSTS,
						    pcie->resets);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get resets\n");

	ret = eic7700_pcie_parse_ports(pcie);
	if (ret)
		return dev_err_probe(dev, ret,
				     "Failed to parse Root Port: %d\n", ret);

	platform_set_drvdata(pdev, pcie);

	ret = dw_pcie_host_init(&pci->pp);
	if (ret) {
		dev_err(dev, "Failed to initialize host\n");
		goto err_init;
	}

	return 0;

err_init:
	list_for_each_entry_safe(port, tmp, &pcie->ports, list) {
		list_del(&port->list);
		reset_control_put(port->perst);
	}

	return ret;
}

static int eic7700_pcie_suspend_noirq(struct device *dev)
{
	struct eic7700_pcie *pcie = dev_get_drvdata(dev);

	return dw_pcie_suspend_noirq(&pcie->pci);
}

static int eic7700_pcie_resume_noirq(struct device *dev)
{
	struct eic7700_pcie *pcie = dev_get_drvdata(dev);

	return dw_pcie_resume_noirq(&pcie->pci);
}

static const struct dev_pm_ops eic7700_pcie_pm_ops = {
	NOIRQ_SYSTEM_SLEEP_PM_OPS(eic7700_pcie_suspend_noirq,
				  eic7700_pcie_resume_noirq)
};

static const struct eic7700_pcie_data eic7700_data = {
	.msix_cap = false,
};

static const struct of_device_id eic7700_pcie_of_match[] = {
	{ .compatible = "eswin,eic7700-pcie", .data = &eic7700_data },
	{},
};

static struct platform_driver eic7700_pcie_driver = {
	.probe = eic7700_pcie_probe,
	.driver = {
		.name = "eic7700-pcie",
		.of_match_table = eic7700_pcie_of_match,
		.suppress_bind_attrs = true,
		.pm = &eic7700_pcie_pm_ops,
	},
};
builtin_platform_driver(eic7700_pcie_driver);

MODULE_DESCRIPTION("Eswin EIC7700 PCIe host controller driver");
MODULE_AUTHOR("Yu Ning <ningyu@eswincomputing.com>");
MODULE_AUTHOR("Senchuan Zhang <zhangsenchuan@eswincomputing.com>");
MODULE_AUTHOR("Yanghui Ou <ouyanghui@eswincomputing.com>");
MODULE_LICENSE("GPL");
