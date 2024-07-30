// SPDX-License-Identifier: GPL-2.0
/*
 * ESWIN AHCI SATA Driver
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
 * Authors: Yulin Lu <luyulin@eswincomputing.com>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/libata.h>
#include <linux/ahci_platform.h>
#include <linux/acpi.h>
#include <linux/pci_ids.h>
#include <linux/iommu.h>
#include <linux/eic7700-sid-cfg.h>
#include  <linux/mfd/syscon.h>
#include <linux/bitfield.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include "ahci.h"

#define DRV_NAME "ahci"

#define AWSMMUSID                     GENMASK(31, 24) // The sid of write operation
#define AWSMMUSSID                    GENMASK(23, 16) // The ssid of write operation
#define ARSMMUSID                     GENMASK(15, 8)  // The sid of read operation
#define ARSMMUSSID                    GENMASK(7, 0)   // The ssid of read operation
#define SATA_REF_CTRL1                0x338
#define SATA_PHY_CTRL0                0x328
#define SATA_PHY_CTRL1                0x32c
#define SATA_LOS_IDEN                 0x33c
#define SATA_AXI_LP_CTRL              0x308
#define SATA_REG_CTRL                 0x334
#define SATA_MPLL_CTRL                0x320
#define SATA_RESET_CTRL               0x340
#define SATA_RESET_CTRL_ASSERT        0x3
#define SATA_RESET_CTRL_DEASSERT      0x0
#define SATA_PHY_RESET                BIT(0)
#define SATA_P0_RESET                 BIT(1)
#define SATA_LOS_LEVEL                0x9
#define SATA_LOS_BIAS                 (0x02 << 16)
#define SATA_REF_REPEATCLK_EN         BIT(0)
#define SATA_REF_USE_PAD              BIT(20)
#define SATA_P0_AMPLITUDE_GEN1        0x42
#define SATA_P0_AMPLITUDE_GEN2        (0x46 << 8)
#define SATA_P0_AMPLITUDE_GEN3        (0x73 << 16)
#define SATA_P0_PHY_TX_PREEMPH_GEN1   0x05
#define SATA_P0_PHY_TX_PREEMPH_GEN2   (0x05 << 8)
#define SATA_P0_PHY_TX_PREEMPH_GEN3   (0x08 << 16)
#define SATA_MPLL_MULTIPLIER          (0x3c << 16)
#define SATA_M_CSYSREQ                BIT(0)
#define SATA_S_CSYSREQ                BIT(16)

struct eswin_ahci_plat {
	struct reset_control *apb_rst;
};

static const struct ata_port_info ahci_port_info = {
	.flags		= AHCI_FLAG_COMMON,
	.pio_mask	= ATA_PIO4,
	.udma_mask	= ATA_UDMA6,
	.port_ops	= &ahci_platform_ops,
};

static struct scsi_host_template ahci_platform_sht = {
	AHCI_SHT(DRV_NAME),
};

static int eswin_sata_sid_cfg(struct device *dev)
{
	int ret;
	struct regmap *regmap;
	int hsp_mmu_sata_reg;
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
			&hsp_mmu_sata_reg);
	if (ret) {
		dev_err(dev, "can't get sata sid cfg reg offset (%d)\n", ret);
		return ret;
	}

	/* make the reading sid the same as writing sid, ssid is fixed to zero */
	rdwr_sid_ssid  = FIELD_PREP(AWSMMUSID, sid);
	rdwr_sid_ssid |= FIELD_PREP(ARSMMUSID, sid);
	rdwr_sid_ssid |= FIELD_PREP(AWSMMUSSID, 0);
	rdwr_sid_ssid |= FIELD_PREP(ARSMMUSSID, 0);
	regmap_write(regmap, hsp_mmu_sata_reg, rdwr_sid_ssid);

	ret = eic7700_dynm_sid_enable(dev_to_node(dev));
	if (ret < 0)
		dev_err(dev, "failed to config sata streamID(%d)!\n", sid);
	else
		dev_dbg(dev, "success to config sata streamID(%d)!\n", sid);
	pr_err("eswin_sata_sid_cfg success\n");

	return ret;
}

static int eswin_sata_init(struct device *dev)
{
	struct regmap *regmap;
	regmap = syscon_regmap_lookup_by_phandle(dev->of_node, "eswin,hsp_sp_csr");
	if (IS_ERR(regmap)) {
		dev_dbg(dev, "No hsp_sp_csr phandle specified\n");
		return -1;
	}

	regmap_write(regmap, SATA_REF_CTRL1, 0x1);
	regmap_write(regmap, SATA_PHY_CTRL0, (SATA_P0_AMPLITUDE_GEN1|SATA_P0_AMPLITUDE_GEN2|SATA_P0_AMPLITUDE_GEN3));
	regmap_write(regmap, SATA_PHY_CTRL1, (SATA_P0_PHY_TX_PREEMPH_GEN1|SATA_P0_PHY_TX_PREEMPH_GEN2|SATA_P0_PHY_TX_PREEMPH_GEN3));
	regmap_write(regmap, SATA_LOS_IDEN, SATA_LOS_LEVEL|SATA_LOS_BIAS);
	regmap_write(regmap, SATA_AXI_LP_CTRL, (SATA_M_CSYSREQ|SATA_S_CSYSREQ));
	regmap_write(regmap, SATA_REG_CTRL, (SATA_REF_REPEATCLK_EN|SATA_REF_USE_PAD));
	regmap_write(regmap, SATA_MPLL_CTRL, SATA_MPLL_MULTIPLIER);
	udelay(20);

	return 0;
}

static int eswin_ahci_platform_resets(struct ahci_host_priv *hpriv,
				struct device *dev)
{
	struct eswin_ahci_plat *plat = hpriv->plat_data;
	struct regmap *regmap;
	int ret;

	regmap = syscon_regmap_lookup_by_phandle(dev->of_node, "eswin,hsp_sp_csr");
	if (IS_ERR(regmap)) {
		dev_dbg(dev, "No hsp_sp_csr phandle specified\n");
		return -1;
	}

	plat->apb_rst = devm_reset_control_get_optional(dev, "apb");
	if (PTR_ERR(plat->apb_rst) == -EPROBE_DEFER)
		return PTR_ERR(plat->apb_rst);

	ret = reset_control_assert(plat->apb_rst);
	if (ret) {
		dev_err(dev, "failed to assert apb_rst\n");
		return ret;
	}
	regmap_write(regmap, SATA_RESET_CTRL, SATA_RESET_CTRL_ASSERT);

	regmap_write(regmap, SATA_RESET_CTRL, SATA_RESET_CTRL_DEASSERT);
	ret = reset_control_deassert(plat->apb_rst);
	if (ret) {
		dev_err(dev, "failed to deassert apb_rst\n");
		return ret;
	}

	return 0;
}

static int ahci_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct ahci_host_priv *hpriv;
	struct eswin_ahci_plat *plat;
	const struct ata_port_info *port;
	int ret;

	plat = devm_kzalloc(dev, sizeof(*plat), GFP_KERNEL);
	if(!plat)
		return -ENOMEM;

	hpriv = ahci_platform_get_resources(pdev,
			0);
	if (IS_ERR(hpriv))
		return PTR_ERR(hpriv);

	eswin_sata_init(dev);

	hpriv->plat_data = plat;
	ret = eswin_ahci_platform_resets(hpriv, dev);
	if(ret)
		return ret;

	ret = ahci_platform_enable_resources(hpriv);
	if(ret)
		return ret;

	eswin_sata_sid_cfg(dev);

	eic7700_tbu_power(&pdev->dev, true);

	ret = dma_set_mask_and_coherent(dev,DMA_BIT_MASK(64));
	if(ret)
		return ret;

	port = acpi_device_get_match_data(dev);
	if (!port){
		port = &ahci_port_info;
	}
	ret = ahci_platform_init_host(pdev, hpriv, port,
			&ahci_platform_sht);
	if (ret)
		goto disable_resources;

	return 0;

disable_resources:
	ahci_platform_disable_resources(hpriv);
	return ret;
}

static void ahci_remove(struct platform_device *pdev)
{
	eic7700_tbu_power(&pdev->dev, false);

	ata_platform_remove_one(pdev);
}

static SIMPLE_DEV_PM_OPS(ahci_pm_ops, ahci_platform_suspend,
		ahci_platform_resume);

static const struct of_device_id ahci_of_match[] = {
	{ .compatible = "snps,eswin-ahci", },
	{},
};
MODULE_DEVICE_TABLE(of, ahci_of_match);

static struct platform_driver ahci_driver = {
	.probe = ahci_probe,
	.remove_new = ahci_remove,
	.shutdown = ahci_platform_shutdown,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = ahci_of_match,
		.pm = &ahci_pm_ops,
	},
};
module_platform_driver(ahci_driver);

MODULE_DESCRIPTION("ESWIN AHCI SATA driver");
MODULE_AUTHOR("Lu Yulin <luyulin@eswincomputing.com>");
MODULE_LICENSE("GPL");
