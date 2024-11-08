// SPDX-License-Identifier: GPL-2.0-only
/*
 * Eswin DWC Ethernet linux driver
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/ethtool.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_net.h>
#include <linux/mfd/syscon.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/stmmac.h>
#include <linux/iommu.h>
#include "stmmac_platform.h"
#include "dwmac4.h"
#include <linux/mfd/syscon.h>
#include <linux/bitfield.h>
#include <linux/regmap.h>
#include <linux/eic7700-sid-cfg.h>
#include <linux/gpio/consumer.h>

/* eth_phy_ctrl_offset eth0:0x100; eth1:0x200 */
#define ETH_TX_CLK_SEL          BIT(16)
#define ETH_PHY_INTF_SELI       BIT(0)

/* eth_axi_lp_ctrl_offset eth0:0x108; eth1:0x208 */
#define ETH_CSYSREQ_VAL         BIT(0)

/* hsp_aclk_ctrl_offset (0x148) */
#define HSP_ACLK_CLKEN           BIT(31)
#define HSP_ACLK_DIVSOR          (0x2 << 4)

/* hsp_cfg_ctrl_offset (0x14c) */
#define HSP_CFG_CLKEN            BIT(31)
#define SCU_HSP_PCLK_EN          BIT(30)
#define HSP_CFG_CTRL_REGSET      (HSP_CFG_CLKEN | SCU_HSP_PCLK_EN)

/* RTL8211F PHY Configurations for LEDs */
#define PHY_ADDR                 0
#define PHY_PAGE_SWITCH_REG      31
#define PHY_LED_CFG_REG          16
#define PHY_LED_PAGE_CFG         0xd04

#define AWSMMUSID   GENMASK(31, 24) // The sid of write operation
#define AWSMMUSSID  GENMASK(23, 16) // The ssid of write operation
#define ARSMMUSID   GENMASK(15, 8)  // The sid of read operation
#define ARSMMUSSID  GENMASK(7, 0)   // The ssid of read operation

struct dwc_qos_priv {
	struct device *dev;
	int dev_id;
	struct regmap *crg_regmap;
	struct regmap *hsp_regmap;
	struct reset_control *rst;
	struct clk *clk_app;
	struct clk *clk_tx;
	struct regmap *rgmii_sel;
	struct gpio_desc *phy_reset;
	struct stmmac_priv *stmpriv;
	int phyled_cfgs[3];
};

static int dwc_eth_dwmac_config_dt(struct platform_device *pdev,
				   struct plat_stmmacenet_data *plat_dat)
{
	struct device *dev = &pdev->dev;
	u32 burst_map = 0;
	u32 bit_index = 0;
	u32 a_index = 0;

	if (!plat_dat->axi) {
		plat_dat->axi = kzalloc(sizeof(struct stmmac_axi), GFP_KERNEL);

		if (!plat_dat->axi)
			return -ENOMEM;
	}

	plat_dat->axi->axi_lpi_en = device_property_read_bool(dev,
							      "snps,en-lpi");
	if (device_property_read_u32(dev, "snps,write-requests",
				     &plat_dat->axi->axi_wr_osr_lmt)) {
		/**
		 * Since the register has a reset value of 1, if property
		 * is missing, default to 1.
		 */
		plat_dat->axi->axi_wr_osr_lmt = 1;
	} else {
		/**
		 * If property exists, to keep the behavior from dwc_eth_qos,
		 * subtract one after parsing.
		 */
		plat_dat->axi->axi_wr_osr_lmt--;
	}

	if (device_property_read_u32(dev, "snps,read-requests",
				     &plat_dat->axi->axi_rd_osr_lmt)) {
		/**
		 * Since the register has a reset value of 1, if property
		 * is missing, default to 1.
		 */
		plat_dat->axi->axi_rd_osr_lmt = 1;
	} else {
		/**
		 * If property exists, to keep the behavior from dwc_eth_qos,
		 * subtract one after parsing.
		 */
		plat_dat->axi->axi_rd_osr_lmt--;
	}
	device_property_read_u32(dev, "snps,burst-map", &burst_map);

	/* converts burst-map bitmask to burst array */
	for (bit_index = 0; bit_index < 7; bit_index++) {
		if (burst_map & (1 << bit_index)) {
			switch (bit_index) {
			case 0:
			plat_dat->axi->axi_blen[a_index] = 4; break;
			case 1:
			plat_dat->axi->axi_blen[a_index] = 8; break;
			case 2:
			plat_dat->axi->axi_blen[a_index] = 16; break;
			case 3:
			plat_dat->axi->axi_blen[a_index] = 32; break;
			case 4:
			plat_dat->axi->axi_blen[a_index] = 64; break;
			case 5:
			plat_dat->axi->axi_blen[a_index] = 128; break;
			case 6:
			plat_dat->axi->axi_blen[a_index] = 256; break;
			default:
			break;
			}
			a_index++;
		}
	}

	/* dwc-qos needs GMAC4, AAL, TSO and PMT */
	plat_dat->has_gmac4 = 1;
	plat_dat->dma_cfg->aal = 1;
	plat_dat->flags |= STMMAC_FLAG_TSO_EN;
	plat_dat->pmt = 1;

	return 0;
}

static int eswin_eth_sid_cfg(struct device *dev)
{
	int ret;
	struct regmap *regmap;
	int hsp_mmu_eth_reg;
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
				&hsp_mmu_eth_reg);
	if (ret) {
		dev_err(dev, "can't get eth sid cfg reg offset (%d)\n", ret);
		return ret;
	}

	/* make the reading sid the same as writing sid, ssid is fixed to zero */
	rdwr_sid_ssid  = FIELD_PREP(AWSMMUSID, sid);
	rdwr_sid_ssid |= FIELD_PREP(ARSMMUSID, sid);
	rdwr_sid_ssid |= FIELD_PREP(AWSMMUSSID, 0);
	rdwr_sid_ssid |= FIELD_PREP(ARSMMUSSID, 0);
	regmap_write(regmap, hsp_mmu_eth_reg, rdwr_sid_ssid);

	ret = eic7700_dynm_sid_enable(dev_to_node(dev));
	if (ret < 0)
		dev_err(dev, "failed to config eth streamID(%d)!\n", sid);
	else
		dev_dbg(dev, "success to config eth streamID(%d)!\n", sid);

	return ret;
}

static void dwc_qos_fix_speed(void *priv, unsigned int speed, unsigned int mode)
{
	unsigned long rate = 125000000;
	int err, data = 0;
	struct dwc_qos_priv *dwc_priv = (struct dwc_qos_priv *)priv;

	switch (speed) {
	case SPEED_1000:
		rate = 125000000;

		if (dwc_priv->dev_id == 0) {
			regmap_write(dwc_priv->hsp_regmap, 0x118, 0x800c8023);
			regmap_write(dwc_priv->hsp_regmap, 0x11c, 0x0c0c0c0c);
			regmap_write(dwc_priv->hsp_regmap, 0x114, 0x23232323);
		} else {
			regmap_write(dwc_priv->hsp_regmap, 0x218, 0x80268025);
			regmap_write(dwc_priv->hsp_regmap, 0x21c, 0x26262626);
			regmap_write(dwc_priv->hsp_regmap, 0x214, 0x25252525);
		}

		if (dwc_priv->stmpriv) {
			data = mdiobus_read(dwc_priv->stmpriv->mii, PHY_ADDR, PHY_PAGE_SWITCH_REG);
			mdiobus_write(dwc_priv->stmpriv->mii, PHY_ADDR, PHY_PAGE_SWITCH_REG, PHY_LED_PAGE_CFG);
			mdiobus_write(dwc_priv->stmpriv->mii, PHY_ADDR, PHY_LED_CFG_REG, dwc_priv->phyled_cfgs[0]);
			mdiobus_write(dwc_priv->stmpriv->mii, PHY_ADDR, PHY_PAGE_SWITCH_REG, data);
		}

		break;
	case SPEED_100:
		rate = 25000000;

		if (dwc_priv->dev_id == 0) {
			regmap_write(dwc_priv->hsp_regmap, 0x118, 0x803f8050);
			regmap_write(dwc_priv->hsp_regmap, 0x11c, 0x3f3f3f3f);
			regmap_write(dwc_priv->hsp_regmap, 0x114, 0x50505050);
		} else {
			regmap_write(dwc_priv->hsp_regmap, 0x218, 0x80588048);
			regmap_write(dwc_priv->hsp_regmap, 0x21c, 0x58585858);
			regmap_write(dwc_priv->hsp_regmap, 0x214, 0x48484848);
		}

		if (dwc_priv->stmpriv) {
			data = mdiobus_read(dwc_priv->stmpriv->mii, PHY_ADDR, PHY_PAGE_SWITCH_REG);
			mdiobus_write(dwc_priv->stmpriv->mii, PHY_ADDR, PHY_PAGE_SWITCH_REG, PHY_LED_PAGE_CFG);
			mdiobus_write(dwc_priv->stmpriv->mii, PHY_ADDR, PHY_LED_CFG_REG, dwc_priv->phyled_cfgs[1]);
			mdiobus_write(dwc_priv->stmpriv->mii, PHY_ADDR, PHY_PAGE_SWITCH_REG, data);
		}

		break;
	case SPEED_10:
		rate = 2500000;

		if (dwc_priv->dev_id == 0) {
			regmap_write(dwc_priv->hsp_regmap, 0x118, 0x0);
			regmap_write(dwc_priv->hsp_regmap, 0x11c, 0x0);
			regmap_write(dwc_priv->hsp_regmap, 0x114, 0x0);
		} else {
			regmap_write(dwc_priv->hsp_regmap, 0x218, 0x0);
			regmap_write(dwc_priv->hsp_regmap, 0x21c, 0x0);
			regmap_write(dwc_priv->hsp_regmap, 0x214, 0x0);
		}

		if (dwc_priv->stmpriv) {
			data = mdiobus_read(dwc_priv->stmpriv->mii, PHY_ADDR, PHY_PAGE_SWITCH_REG);
			mdiobus_write(dwc_priv->stmpriv->mii, PHY_ADDR, PHY_PAGE_SWITCH_REG, PHY_LED_PAGE_CFG);
			mdiobus_write(dwc_priv->stmpriv->mii, PHY_ADDR, PHY_LED_CFG_REG, dwc_priv->phyled_cfgs[2]);
			mdiobus_write(dwc_priv->stmpriv->mii, PHY_ADDR, PHY_PAGE_SWITCH_REG, data);
		}

		break;
	default:
		dev_err(dwc_priv->dev, "invalid speed %u\n", speed);
		break;
	}

	err = clk_set_rate(dwc_priv->clk_tx, rate);
	if (err < 0)
	{
		dev_err(dwc_priv->dev, "failed to set TX rate: %d\n", err);
	}
}

static int dwc_qos_probe(struct platform_device *pdev,
			 struct plat_stmmacenet_data *plat_dat,
			 struct stmmac_resources *stmmac_res)
{
	struct dwc_qos_priv *dwc_priv;
	int ret;
	int err;
	u32 hsp_aclk_ctrl_offset;
	u32 hsp_aclk_ctrl_regset;
	u32 hsp_cfg_ctrl_offset;
	u32 eth_axi_lp_ctrl_offset;
	u32 eth_phy_ctrl_offset;
	u32 eth_phy_ctrl_regset;
	u32 rgmiisel_offset;
	u32 rgmiisel_regset;

	dwc_priv = devm_kzalloc(&pdev->dev, sizeof(*dwc_priv), GFP_KERNEL);
	if (!dwc_priv)
		return -ENOMEM;

	if (device_property_read_u32(&pdev->dev, "id", &dwc_priv->dev_id)) {
		dev_err(&pdev->dev, "Can not read device id!\n");
		return -EINVAL;
	}

	dwc_priv->dev = &pdev->dev;
	dwc_priv->phy_reset = devm_gpiod_get(&pdev->dev, "rst", GPIOD_OUT_LOW);
	if (IS_ERR(dwc_priv->phy_reset)) {
		dev_err(&pdev->dev, "Reset gpio not specified\n");
		return -EINVAL;
	}

	gpiod_set_value(dwc_priv->phy_reset, 0);

	dwc_priv->rgmii_sel = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, "eswin,rgmiisel");
	if (IS_ERR(dwc_priv->rgmii_sel)){
		dev_dbg(&pdev->dev, "rgmiisel not specified\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "eswin,led-cfgs", 0, &dwc_priv->phyled_cfgs[0]);
	if (ret) {
		dev_warn(&pdev->dev, "can't get led cfgs for 1Gbps mode (%d)\n", ret);
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "eswin,led-cfgs", 1, &dwc_priv->phyled_cfgs[1]);
	if (ret) {
		dev_warn(&pdev->dev, "can't get led cfgs for 100Mbps mode (%d)\n", ret);
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "eswin,led-cfgs", 2, &dwc_priv->phyled_cfgs[2]);
	if (ret) {
		dev_warn(&pdev->dev, "can't get led cfgs for 10Mbps mode (%d)\n", ret);
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "eswin,rgmiisel", 1, &rgmiisel_offset);
	if (ret) {
		dev_err(&pdev->dev, "can't get rgmiisel_offset (%d)\n", ret);
		return ret;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "eswin,rgmiisel", 2, &rgmiisel_regset);
	if (ret) {
		dev_err(&pdev->dev, "can't get rgmiisel_regset (%d)\n", ret);
		return ret;
	}

	regmap_write(dwc_priv->rgmii_sel, rgmiisel_offset, rgmiisel_regset);

	dwc_priv->crg_regmap = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, "eswin,syscrg_csr");
	if (IS_ERR(dwc_priv->crg_regmap)){
		dev_dbg(&pdev->dev, "No syscrg_csr phandle specified\n");
		return 0;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "eswin,syscrg_csr", 1,
                                    &hsp_aclk_ctrl_offset);
	if (ret) {
		dev_err(&pdev->dev, "can't get hsp_aclk_ctrl_offset (%d)\n", ret);
		return ret;
	}
	regmap_read(dwc_priv->crg_regmap, hsp_aclk_ctrl_offset, &hsp_aclk_ctrl_regset);
	hsp_aclk_ctrl_regset |= (HSP_ACLK_CLKEN | HSP_ACLK_DIVSOR);
	regmap_write(dwc_priv->crg_regmap, hsp_aclk_ctrl_offset, hsp_aclk_ctrl_regset);

	ret = of_property_read_u32_index(pdev->dev.of_node, "eswin,syscrg_csr", 2,
                                    &hsp_cfg_ctrl_offset);
	if (ret) {
		dev_err(&pdev->dev, "can't get hsp_cfg_ctrl_offset (%d)\n", ret);
		return ret;
	}
	regmap_write(dwc_priv->crg_regmap, hsp_cfg_ctrl_offset, HSP_CFG_CTRL_REGSET);

	dwc_priv->hsp_regmap = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, "eswin,hsp_sp_csr");
	if (IS_ERR(dwc_priv->hsp_regmap)){
		dev_dbg(&pdev->dev, "No hsp_sp_csr phandle specified\n");
		return 0;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "eswin,hsp_sp_csr", 2,
                                    &eth_phy_ctrl_offset);
	if (ret) {
		dev_err(&pdev->dev, "can't get eth_phy_ctrl_offset (%d)\n", ret);
		return ret;
	}
	regmap_read(dwc_priv->hsp_regmap, eth_phy_ctrl_offset, &eth_phy_ctrl_regset);
	eth_phy_ctrl_regset |= (ETH_TX_CLK_SEL | ETH_PHY_INTF_SELI);
	regmap_write(dwc_priv->hsp_regmap, eth_phy_ctrl_offset, eth_phy_ctrl_regset);

	ret = of_property_read_u32_index(pdev->dev.of_node, "eswin,hsp_sp_csr", 3,
                                    &eth_axi_lp_ctrl_offset);
	if (ret) {
		dev_err(&pdev->dev, "can't get eth_axi_lp_ctrl_offset (%d)\n", ret);
		return ret;
	}
	regmap_write(dwc_priv->hsp_regmap, eth_axi_lp_ctrl_offset, ETH_CSYSREQ_VAL);

	dwc_priv->clk_app = devm_clk_get(&pdev->dev, "app");
	if (IS_ERR(dwc_priv->clk_app)) {
		dev_err(&pdev->dev, "app clock not found.\n");
		return PTR_ERR(dwc_priv->clk_app);
	}

	err = clk_prepare_enable(dwc_priv->clk_app);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to enable app clock: %d\n",
			err);
		return err;
	}

	dwc_priv->clk_tx = devm_clk_get(&pdev->dev, "tx");
	if (IS_ERR(plat_dat->pclk)) {
		dev_err(&pdev->dev, "tx clock not found.\n");
		return PTR_ERR(dwc_priv->clk_tx);
	}

	err = clk_prepare_enable(dwc_priv->clk_tx);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to enable tx clock: %d\n",
            err);
		return err;
	}
	dwc_priv->rst = devm_reset_control_get_optional_exclusive(&pdev->dev, "ethrst");
	if (IS_ERR(dwc_priv->rst)) {
		return PTR_ERR(dwc_priv->rst);
	}

	ret = reset_control_assert(dwc_priv->rst);
	WARN_ON(0 != ret);
	ret = reset_control_deassert(dwc_priv->rst);
	WARN_ON(0 != ret);

	ret = eic7700_tbu_power(&pdev->dev, true);
	if (ret) {
		dev_err(&pdev->dev, "failed to power on tbu\n");
		return ret;
	}

	plat_dat->fix_mac_speed = dwc_qos_fix_speed;
	plat_dat->bsp_priv = dwc_priv;
	plat_dat->phy_addr = PHY_ADDR;

	return 0;
}

static int dwc_qos_remove(struct platform_device *pdev)
{
	int ret;
	struct dwc_qos_priv *dwc_priv = get_stmmac_bsp_priv(&pdev->dev);

	ret = eic7700_tbu_power(&pdev->dev, false);
	if (ret) {
		dev_err(&pdev->dev, "failed to power down tbu\n");
		return ret;
	}

	reset_control_assert(dwc_priv->rst);
	clk_disable_unprepare(dwc_priv->clk_tx);
	clk_disable_unprepare(dwc_priv->clk_app);

	devm_gpiod_put(&pdev->dev, dwc_priv->phy_reset);

	return 0;
}

struct dwc_eth_dwmac_data {
	int (*probe)(struct platform_device *pdev,
		     struct plat_stmmacenet_data *data,
		     struct stmmac_resources *res);
	int (*remove)(struct platform_device *pdev);
};

static const struct dwc_eth_dwmac_data dwc_qos_data = {
	.probe = dwc_qos_probe,
	.remove = dwc_qos_remove,
};

static int dwc_eth_dwmac_probe(struct platform_device *pdev)
{
	const struct dwc_eth_dwmac_data *data;
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	struct net_device *ndev = NULL;
	struct stmmac_priv *stmpriv = NULL;
	struct dwc_qos_priv *dwc_priv = NULL;
	int ret;

	data = device_get_match_data(&pdev->dev);

	memset(&stmmac_res, 0, sizeof(struct stmmac_resources));

	/**
	 * Since stmmac_platform supports name IRQ only, basic platform
	 * resource initialization is done in the glue logic.
	 */
	stmmac_res.irq = platform_get_irq(pdev, 0);
	if (stmmac_res.irq < 0)
		return stmmac_res.irq;
	stmmac_res.wol_irq = stmmac_res.irq;
	stmmac_res.addr = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(stmmac_res.addr))
		return PTR_ERR(stmmac_res.addr);

	plat_dat = stmmac_probe_config_dt(pdev, stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	ret = data->probe(pdev, plat_dat, &stmmac_res);
	if (ret < 0) {
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "failed to probe subdriver: %d\n",
				ret);

		goto remove_config;
	}

	ret = dwc_eth_dwmac_config_dt(pdev, plat_dat);
	if (ret)
		goto remove;

    ret =  eswin_eth_sid_cfg(&pdev->dev);
	if (ret)
		goto remove;

	ret = stmmac_dvr_probe(&pdev->dev, plat_dat, &stmmac_res);
	if (ret)
		goto remove;

	ndev = dev_get_drvdata(&pdev->dev);
	stmpriv = netdev_priv(ndev);
	dwc_priv = (struct dwc_qos_priv *)plat_dat->bsp_priv;
	dwc_priv->stmpriv = stmpriv;

	return ret;

remove:
	data->remove(pdev);
remove_config:
	stmmac_remove_config_dt(pdev, plat_dat);

	return ret;
}

static int dwc_eth_dwmac_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct stmmac_priv *priv = netdev_priv(ndev);
	const struct dwc_eth_dwmac_data *data;
	int err;

	data = device_get_match_data(&pdev->dev);

	stmmac_dvr_remove(&pdev->dev);

	err = data->remove(pdev);
	if (err < 0)
		dev_err(&pdev->dev, "failed to remove subdriver: %d\n", err);

	stmmac_remove_config_dt(pdev, priv->plat);

	return err;
}

static const struct of_device_id dwc_eth_dwmac_match[] = {
	{ .compatible = "eswin,eic7700-qos-eth", .data = &dwc_qos_data },
	{ }
};
MODULE_DEVICE_TABLE(of, dwc_eth_dwmac_match);

static struct platform_driver eic7700_eth_dwmac_driver = {
	.probe  = dwc_eth_dwmac_probe,
	.remove = dwc_eth_dwmac_remove,
	.driver = {
		.name           = "eic7700-eth-dwmac",
		.pm             = &stmmac_pltfr_pm_ops,
		.of_match_table = dwc_eth_dwmac_match,
	},
};
module_platform_driver(eic7700_eth_dwmac_driver);

MODULE_AUTHOR("Eswin");
MODULE_DESCRIPTION("Eswin eic7700 qos ethernet driver");
MODULE_LICENSE("GPL v2");
