// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2025, Beijing ESWIN Computing Technology Co., Ltd..
 * All rights reserved.
 *
 * ESWIN EIC7700 HSP Clock Driver
 *
 * Authors: Xuyang Dong <dongxuyang@eswincomputing.com>
 */

#include <linux/auxiliary_bus.h>
#include <linux/clk.h>
#include <linux/of.h>

#include <dt-bindings/clock/eswin,eic7700-hsp-clock.h>

#include "clk.h"

#define EIC7700_HSP_NR_CLKS (EIC7700_HSP_CLK_GATE_SATA + 1)

static const char *const mux_mmc_3mux1_p[] = {
	"gate_clk_hsp_cfg_clk", "factor_hsp_cfg_div2", "factor_hsp_cfg_div4"
};

static const char *const mux_mmc_2mux1_p[] = {
	"fixed_factor_clk_1m_div24", "factor_mmc_div10"
};

static u32 mux_mmc_3mux1_tbl[] = { 0x0, 0x1, 0x3 };

static struct eswin_fixed_factor_clock eic7700_fixed_factor_clks[] = {
	EIC7700_FACTOR(EIC7700_HSP_CLK_FAC_CFG_DIV2, "factor_hsp_cfg_div2",
		       "gate_clk_hsp_cfg_clk", 1, 2, 0),
	EIC7700_FACTOR(EIC7700_HSP_CLK_FAC_CFG_DIV4, "factor_hsp_cfg_div4",
		       "gate_clk_hsp_cfg_clk", 1, 4, 0),
	EIC7700_FACTOR(EIC7700_HSP_CLK_FAC_MMC_DIV10, "factor_mmc_div10",
		       "fixed_factor_clk_1m_div24", 1, 10, 0),
};

static struct eswin_mux_clock eic7700_mux_tbl_clks[] = {
	EIC7700_MUX_TBL(EIC7700_HSP_CLK_MUX_EMMC_3MUX1, "mux_emmc_3mux1",
			mux_mmc_3mux1_p, ARRAY_SIZE(mux_mmc_3mux1_p),
			CLK_SET_RATE_PARENT, 0x510, 16, 2, 0,
			mux_mmc_3mux1_tbl),
	EIC7700_MUX_TBL(EIC7700_HSP_CLK_MUX_SD0_3MUX1, "mux_sd0_3mux1",
			mux_mmc_3mux1_p, ARRAY_SIZE(mux_mmc_3mux1_p),
			CLK_SET_RATE_PARENT, 0x610, 16, 2, 0,
			mux_mmc_3mux1_tbl),
	EIC7700_MUX_TBL(EIC7700_HSP_CLK_MUX_SD1_3MUX1, "mux_sd1_3mux1",
			mux_mmc_3mux1_p, ARRAY_SIZE(mux_mmc_3mux1_p),
			CLK_SET_RATE_PARENT, 0x710, 16, 2, 0,
			mux_mmc_3mux1_tbl),
};

static struct eswin_mux_clock eic7700_mux_clks[] = {
	EIC7700_MUX(EIC7700_HSP_CLK_MUX_EMMC_CQE_2MUX1, "mux_emmc_cqe_2mux1",
		    mux_mmc_2mux1_p, ARRAY_SIZE(mux_mmc_2mux1_p),
		    CLK_SET_RATE_PARENT, 0x510, 0, 1, 0),
	EIC7700_MUX(EIC7700_HSP_CLK_MUX_SD0_CQE_2MUX1, "mux_sd0_cqe_2mux1",
		    mux_mmc_2mux1_p, ARRAY_SIZE(mux_mmc_2mux1_p),
		    CLK_SET_RATE_PARENT, 0x610, 0, 1, 0),
	EIC7700_MUX(EIC7700_HSP_CLK_MUX_SD1_CQE_2MUX1, "mux_sd1_cqe_2mux1",
		    mux_mmc_2mux1_p, ARRAY_SIZE(mux_mmc_2mux1_p),
		    CLK_SET_RATE_PARENT, 0x710, 0, 1, 0),
};

static struct eswin_gate_clock eic7700_gate_clks[] = {
	EIC7700_GATE(EIC7700_HSP_CLK_GATE_SATA, "gate_clk_sata",
		     "gate_hsp_sata_oob_clk", CLK_SET_RATE_PARENT, 0x300, 28,
		     0),
	EIC7700_GATE(EIC7700_HSP_CLK_GATE_MSHC0_TMR, "gate_clk_hsp_mshc0_tmr",
		     "fixed_factor_clk_1m_div24", CLK_SET_RATE_PARENT, 0x510, 8,
		     0),
	EIC7700_GATE(EIC7700_HSP_CLK_GATE_EMMC, "gate_clk_emmc",
		     "mux_emmc_3mux1", CLK_SET_RATE_PARENT, 0x510, 24, 0),
	EIC7700_GATE(EIC7700_HSP_CLK_GATE_MSHC1_TMR, "gate_clk_hsp_mshc1_tmr",
		     "fixed_factor_clk_1m_div24", CLK_SET_RATE_PARENT, 0x610, 8,
		     0),
	EIC7700_GATE(EIC7700_HSP_CLK_GATE_SD0, "gate_clk_sd0",
		     "mux_sd0_3mux1", CLK_SET_RATE_PARENT, 0x610, 24, 0),
	EIC7700_GATE(EIC7700_HSP_CLK_GATE_MSHC2_TMR, "gate_clk_hsp_mshc2_tmr",
		     "fixed_factor_clk_1m_div24", CLK_SET_RATE_PARENT, 0x710, 8,
		     0),
	EIC7700_GATE(EIC7700_HSP_CLK_GATE_SD1, "gate_clk_sd1",
		     "mux_sd1_3mux1", CLK_SET_RATE_PARENT, 0x710, 24, 0),
	EIC7700_GATE(EIC7700_HSP_CLK_GATE_USB0, "gate_clk_usb0",
		     "gate_hsp_sata_oob_clk", CLK_SET_RATE_PARENT, 0x800, 28,
		     0),
	EIC7700_GATE(EIC7700_HSP_CLK_GATE_USB1, "gate_clk_usb1",
		     "gate_hsp_sata_oob_clk", CLK_SET_RATE_PARENT, 0x900, 28,
		     0),
};

static int eic7700_hsp_clk_probe(struct platform_device *pdev)
{
	struct eswin_clock_data *clk_data;
	struct device *dev = &pdev->dev;
	struct auxiliary_device *adev;
	int i, ret;

	clk_data = eswin_clk_init(dev, EIC7700_HSP_NR_CLKS);
	if (!clk_data)
		return dev_err_probe(dev, -EAGAIN, "failed to get clk data!\n");

	eswin_clk_register_fixed_factor(eic7700_fixed_factor_clks,
					ARRAY_SIZE(eic7700_fixed_factor_clks),
					clk_data, dev);
	eswin_clk_register_mux(eic7700_mux_clks,
			       ARRAY_SIZE(eic7700_mux_clks), clk_data,
			       dev);
	eswin_clk_register_mux_tbl(eic7700_mux_tbl_clks,
				   ARRAY_SIZE(eic7700_mux_tbl_clks), clk_data,
				   dev);
	eswin_clk_register_gate(eic7700_gate_clks,
				ARRAY_SIZE(eic7700_gate_clks), clk_data, dev);

	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					  &clk_data->clk_data);
	if (ret) {
		dev_err(dev, "add clk provider failed\n");
		goto unregister_clk;
	}

	adev = devm_auxiliary_device_create(dev, "hsp-reset",
					    (__force void *)clk_data->base);
	if (!adev) {
		dev_err(dev, "register hsp-reset device failed\n");
		ret = -ENODEV;
		goto unregister_clk;
	}

	return 0;

unregister_clk:
	for (i = 0; i < ARRAY_SIZE(eic7700_mux_tbl_clks); i++)
		clk_hw_unregister_mux
			(clk_data->clk_data.hws[eic7700_mux_tbl_clks[i].id]);

	return ret;
}

static const struct of_device_id eic7700_hsp_clock_dt_ids[] = {
	{ .compatible = "eswin,eic7700-hspcrg", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, eic7700_hsp_clock_dt_ids);

static struct platform_driver eic7700_hsp_clock_driver = {
	.probe	= eic7700_hsp_clk_probe,
	.driver = {
		.name	= "eic7700-hsp-clock",
		.of_match_table	= eic7700_hsp_clock_dt_ids,
	},
};

module_platform_driver(eic7700_hsp_clock_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Xuyang Dong <dongxuyang@eswincomputing.com>");
MODULE_DESCRIPTION("ESWIN EIC7700 HSP clock controller driver");
