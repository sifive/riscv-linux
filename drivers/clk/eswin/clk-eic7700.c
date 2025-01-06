// SPDX-License-Identifier: GPL-2.0
/*
 * ESWIN Clk Provider Driver
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

#include <linux/kernel.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <dt-bindings/reset/eic7700-syscrg.h>
#include <dt-bindings/clock/eic7700-clock.h>

#include "clk.h"

/* clock parent list */
static const char *const mux_u_cpu_root_3mux1_gfree_p[] = {"clk_pll_cpu", "clk_clk_u84_core_lp", "fixed_rate_clk_xtal_24m"};
static u32  mux_u_cpu_root_3mux1_gfree_p_table[] = {0x000000, 0x000001, 0x000002};

static const char *const mux_u_cpu_aclk_2mux1_gfree_p[] = {"fixed_factor_u_cpu_div2", "mux_u_cpu_root_3mux1_gfree"};

static const char *const dsp_aclk_root_2mux1_gfree_mux_p[] = {"fixed_rate_clk_spll2_fout1", "fixed_rate_clk_spll0_fout1"};

static const char *const d2d_aclk_root_2mux1_gfree_mux_p[] = { "fixed_rate_clk_spll2_fout1", "fixed_rate_clk_spll0_fout1", };

static const char *const ddr_aclk_root_2mux1_gfree_mux_p[] = { "fixed_rate_clk_spll2_fout1", "fixed_rate_clk_spll0_fout1", };

static const char *const mshcore_root_3mux1_0_mux_p[] = {"fixed_rate_clk_spll0_fout3", "fixed_rate_clk_spll2_fout3", "fixed_rate_clk_xtal_24m"};
static u32  mshcore_root_3mux1_0_mux_p_table[] = {0x000000, 0x000001, 0x100000};

static const char *const mshcore_root_3mux1_1_mux_p[] = {"fixed_rate_clk_spll0_fout3", "fixed_rate_clk_spll2_fout3", "fixed_rate_clk_xtal_24m"};
static u32  mshcore_root_3mux1_1_mux_p_table[] = {0x000000, 0x000001, 0x100000};

static const char *const mshcore_root_3mux1_2_mux_p[] = {"fixed_rate_clk_spll0_fout3", "fixed_rate_clk_spll2_fout3", "fixed_rate_clk_xtal_24m"};
static u32  mshcore_root_3mux1_2_mux_p_table[] = {0x000000, 0x000001, 0x100000};

static const char *const npu_llclk_3mux1_gfree_mux_p[] = { "clk_clk_npu_llc_src0", "clk_clk_npu_llc_src1", "fixed_rate_clk_vpll_fout1"};
static u32  npu_llclk_3mux1_gfree_mux_p_table[] = {0x000000, 0x000001, 0x000002};

static const char *const npu_core_3mux1_gfree_mux_p[] = { "fixed_rate_clk_spll1_fout1", "fixed_rate_clk_vpll_fout1", "fixed_rate_clk_spll2_fout2"};
static u32  npu_core_3mux1_gfree_mux_p_table[] = {0x000000, 0x000001, 0x000002};

static const char *const npu_e31_3mux1_gfree_mux_p[] = { "fixed_rate_clk_spll1_fout1", "fixed_rate_clk_vpll_fout1", "fixed_rate_clk_spll2_fout2"};
static u32  npu_e31_3mux1_gfree_mux_p_table[] = {0x000000, 0x000100, 0x000200};

static const char *const vi_aclk_root_2mux1_gfree_mux_p[] = { "fixed_rate_clk_spll0_fout1", "fixed_rate_clk_spll2_fout1"};

static const char *const mux_u_vi_dw_root_2mux1_p[] = { "fixed_rate_clk_vpll_fout1", "fixed_rate_clk_spll0_fout1"};

static const char *const mux_u_vi_dvp_root_2mux1_gfree_p[] = { "fixed_rate_clk_vpll_fout1", "fixed_rate_clk_spll0_fout1"};

static const char *const mux_u_vi_dig_isp_root_2mux1_gfree_p[] = { "fixed_rate_clk_vpll_fout1", "fixed_rate_clk_spll0_fout1"};

static const char *const mux_u_vo_aclk_root_2mux1_gfree_p[] = { "fixed_rate_clk_spll0_fout1", "fixed_rate_clk_spll2_fout1"};

static const char *const mux_u_vo_pixel_root_2mux1_p[] = { "fixed_rate_clk_vpll_fout1", "fixed_rate_clk_spll2_fout2"};

static const char *const mux_u_vcdec_root_2mux1_gfree_p[] = { "fixed_rate_clk_spll0_fout1", "fixed_rate_clk_spll2_fout1"};

static const char *const mux_u_vcaclk_root_2mux1_gfree_p[] = { "fixed_rate_clk_spll0_fout1", "fixed_rate_clk_spll2_fout1"};

static const char *const mux_u_syscfg_clk_root_2mux1_gfree_p[] = { "divder_u_sys_cfg_div_dynm", "fixed_rate_clk_xtal_24m"};

static const char *const mux_u_bootspi_clk_2mux1_gfree_p[] = {  "divder_u_bootspi_div_dynm", "fixed_rate_clk_xtal_24m"};

static const char *const mux_u_scpu_core_clk_2mux1_gfree_p[] = { "divder_u_scpu_core_div_dynm", "fixed_rate_clk_xtal_24m"};

static const char *const mux_u_lpcpu_core_clk_2mux1_gfree_p[] = {"divder_u_lpcpu_core_div_dynm", "fixed_rate_clk_xtal_24m"};

static const char *const mux_u_vo_mclk_2mux_ext_mclk_p[] = {"divder_u_vo_mclk_div_dynm", "fixed_rate_ext_mclk"};

static const char *const mux_u_aondma_axi2mux1_gfree_p[] = { "divder_u_aondma_axi_div_dynm", "fixed_rate_clk_xtal_24m"};

static const char *const mux_u_rmii_ref_2mux1_p[] = { "fixed_factor_u_hsp_rmii_ref_div6", "fixed_rate_lpddr_ref_bak"};

static const char *const mux_u_eth_core_2mux1_p[] = { "fixed_rate_clk_spll1_fout3", "fixed_rate_lpddr_ref_bak"};

static const char *const mux_u_sata_phy_2mux1_p[] = { "divder_u_sata_phy_ref_div_dynm", "fixed_rate_lpddr_ref_bak"};

/* fixed rate clocks */
static struct eswin_fixed_rate_clock eic7700_fixed_rate_clks[] = {
	{ EIC7700_XTAL_24M,		"fixed_rate_clk_xtal_24m",	NULL, 0,	24000000, },
	{ EIC7700_XTAL_32K,		"fixed_rate_clk_xtal_32k",	NULL, 0,	32768, },
	{ EIC7700_SPLL0_FOUT1,		"fixed_rate_clk_spll0_fout1",	NULL, 0,	1600000000, },
	{ EIC7700_SPLL0_FOUT2,		"fixed_rate_clk_spll0_fout2",	NULL, 0,	800000000, },
	{ EIC7700_SPLL0_FOUT3,		"fixed_rate_clk_spll0_fout3",	NULL, 0,	400000000, },
	{ EIC7700_SPLL1_FOUT1,		"fixed_rate_clk_spll1_fout1",	NULL, 0,	1500000000, },
	{ EIC7700_SPLL1_FOUT2,  	"fixed_rate_clk_spll1_fout2",	NULL, 0,	300000000, },
	{ EIC7700_SPLL1_FOUT3,		"fixed_rate_clk_spll1_fout3",	NULL, 0,	250000000, },
	{ EIC7700_SPLL2_FOUT1,		"fixed_rate_clk_spll2_fout1",	NULL, 0,	2080000000, },
	{ EIC7700_SPLL2_FOUT2,		"fixed_rate_clk_spll2_fout2",	NULL, 0,	1040000000, },
	{ EIC7700_SPLL2_FOUT3,		"fixed_rate_clk_spll2_fout3",	NULL, 0,	416000000, },
	{ EIC7700_VPLL_FOUT1,		"fixed_rate_clk_vpll_fout1",	NULL, 0,	1188000000, },
	{ EIC7700_VPLL_FOUT2,		"fixed_rate_clk_vpll_fout2",	NULL, 0,	594000000, },
	{ EIC7700_VPLL_FOUT3,		"fixed_rate_clk_vpll_fout3",	NULL, 0,	49500000, },
	{ EIC7700_APLL_FOUT2,		"fixed_rate_clk_apll_fout2",	NULL, 0,	0, },
	{ EIC7700_APLL_FOUT3,		"fixed_rate_clk_apll_fout3",	NULL, 0,	0, },
	{ EIC7700_EXT_MCLK,		"fixed_rate_ext_mclk",		NULL, 0,	0, },
	{ EIC7700_LPDDR_REF_BAK,	"fixed_rate_lpddr_ref_bak",	NULL, 0,	50000000, },
};

static struct eswin_pll_clock eic7700_pll_clks[] = {
	{
		EIC7700_APLL_FOUT1, "clk_apll_fout1", NULL,
		EIC7700_REG_OFFSET_APLL_CFG_0, 0, 1, 12,6, 20,12,
		EIC7700_REG_OFFSET_APLL_CFG_1, 4,24,
		EIC7700_REG_OFFSET_APLL_CFG_2, 1,3, 16, 3,
		EIC7700_REG_OFFSET_PLL_STATUS, 4, 1,
	},
	{
		EIC7700_PLL_CPU, "clk_pll_cpu", NULL,
		EIC7700_REG_OFFSET_MCPUT_PLL_CFG_0, 0,1, 12,6, 20,12,
		EIC7700_REG_OFFSET_MCPUT_PLL_CFG_1, 4, 24,
		EIC7700_REG_OFFSET_MCPUT_PLL_CFG_2, 1,3, 16,3,
		EIC7700_REG_OFFSET_PLL_STATUS, 5, 1,
	},
};

/* fixed factor clocks */
static struct eswin_fixed_factor_clock eic7700_fixed_factor_clks[] = {
	{ EIC7700_FIXED_FACTOR_U_CPU_DIV2,	 "fixed_factor_u_cpu_div2",   "mux_u_cpu_root_3mux1_gfree", 1, 2, 0, },

	{ EIC7700_FIXED_FACTOR_U_CLK_1M_DIV24,   "fixed_factor_u_clk_1m_div24",   "fixed_rate_clk_xtal_24m", 1, 24, 0, },

	{ EIC7700_FIXED_FACTOR_U_MIPI_TXESC_DIV10, "fixed_factor_u_mipi_txesc_div10", "clk_clk_sys_cfg",  1, 10, 0, },

	{ EIC7700_FIXED_FACTOR_U_U84_CORE_LP_DIV2,   "fixed_factor_u_u84_core_lp_div2",   "gate_clk_spll0_fout2", 1, 2, 0, },

	{ EIC7700_FIXED_FACTOR_U_SCPU_BUS_DIV2,   "fixed_factor_u_scpu_bus_div2",   "mux_u_scpu_core_clk_2mux1_gfree", 1, 2, 0, },

	{ EIC7700_FIXED_FACTOR_U_LPCPU_BUS_DIV2, "fixed_factor_u_lpcpu_bus_div2", "mux_u_lpcpu_core_clk_2mux1_gfree",  1, 2, 0, },

	{ EIC7700_FIXED_FACTOR_U_PCIE_CR_DIV2,   "fixed_factor_u_pcie_cr_div2",   "clk_clk_sys_cfg", 1, 2, 0, },

	{ EIC7700_FIXED_FACTOR_U_PCIE_AUX_DIV4,   "fixed_factor_u_pcie_aux_div4",   "clk_clk_sys_cfg", 1, 4, 0, },

	{ EIC7700_FIXED_FACTOR_U_PVT_DIV20, "fixed_factor_u_pvt_div20", "fixed_rate_clk_xtal_24m",  1, 20, 0, },

	{ EIC7700_FIXED_FACTOR_U_HSP_RMII_REF_DIV6, "fixed_factor_u_hsp_rmii_ref_div6", "fixed_rate_clk_spll1_fout2",  1, 6, 0, },
};

static struct eswin_mux_clock eic7700_mux_clks[] = {
	{ EIC7700_MUX_U_CPU_ROOT_3MUX1_GFREE, "mux_u_cpu_root_3mux1_gfree", mux_u_cpu_root_3mux1_gfree_p,
		ARRAY_SIZE(mux_u_cpu_root_3mux1_gfree_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_U84_CLK_CTRL,
		0, BIT_MASK(0) | BIT_MASK(1), 0, mux_u_cpu_root_3mux1_gfree_p_table, },

	{ EIC7700_MUX_U_CPU_ACLK_2MUX1_GFREE, "mux_u_cpu_aclk_2mux1_gfree", mux_u_cpu_aclk_2mux1_gfree_p,
		ARRAY_SIZE(mux_u_cpu_aclk_2mux1_gfree_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_U84_CLK_CTRL,
		20, 1, 0,},

	{ EIC7700_MUX_U_DSP_ACLK_ROOT_2MUX1_GFREE, "mux_u_dsp_aclk_root_2mux1_gfree", dsp_aclk_root_2mux1_gfree_mux_p,
		ARRAY_SIZE(dsp_aclk_root_2mux1_gfree_mux_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_DSP_ACLK_CTRL,
		0, 1, 0,},

	{ EIC7700_MUX_U_D2D_ACLK_ROOT_2MUX1_GFREE, "mux_u_d2d_aclk_root_2mux1_gfree", d2d_aclk_root_2mux1_gfree_mux_p,
		ARRAY_SIZE(d2d_aclk_root_2mux1_gfree_mux_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_D2D_ACLK_CTRL,
		0, 1, 0,},

	{ EIC7700_MUX_U_DDR_ACLK_ROOT_2MUX1_GFREE, "mux_u_ddr_aclk_root_2mux1_gfree", ddr_aclk_root_2mux1_gfree_mux_p,
		ARRAY_SIZE(ddr_aclk_root_2mux1_gfree_mux_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_DDR_CLK_CTRL,
		16, 1, 0,},

	{ EIC7700_MUX_U_MSHCORE_ROOT_3MUX1_0, "mux_u_mshcore_root_3mux1_0", mshcore_root_3mux1_0_mux_p,
		ARRAY_SIZE(mshcore_root_3mux1_0_mux_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_MSHC0_CORECLK_CTRL,
		0, BIT_MASK(0) | BIT_MASK(20), 0, mshcore_root_3mux1_0_mux_p_table},

	{ EIC7700_MUX_U_MSHCORE_ROOT_3MUX1_1, "mux_u_mshcore_root_3mux1_1", mshcore_root_3mux1_1_mux_p,
		ARRAY_SIZE(mshcore_root_3mux1_1_mux_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_MSHC1_CORECLK_CTRL,
		0, BIT_MASK(0) | BIT_MASK(20), 0, mshcore_root_3mux1_1_mux_p_table, },

	{ EIC7700_MUX_U_MSHCORE_ROOT_3MUX1_2, "mux_u_mshcore_root_3mux1_2", mshcore_root_3mux1_2_mux_p,
		ARRAY_SIZE(mshcore_root_3mux1_2_mux_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_MSHC2_CORECLK_CTRL,
		0, BIT_MASK(0) | BIT_MASK(20), 0, mshcore_root_3mux1_2_mux_p_table },

	{ EIC7700_MUX_U_NPU_LLCLK_3MUX1_GFREE, "mux_u_npu_llclk_3mux1_gfree", npu_llclk_3mux1_gfree_mux_p,
		ARRAY_SIZE(npu_llclk_3mux1_gfree_mux_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_NPU_LLC_CTRL,
		0, BIT_MASK(0) | BIT_MASK(1), 0, npu_llclk_3mux1_gfree_mux_p_table},

	{ EIC7700_MUX_U_NPU_CORE_3MUX1_GFREE, "mux_u_npu_core_3mux1_gfree", npu_core_3mux1_gfree_mux_p,
		ARRAY_SIZE(npu_core_3mux1_gfree_mux_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_NPU_CORE_CTRL,
		0, BIT_MASK(0) | BIT_MASK(1), 0, npu_core_3mux1_gfree_mux_p_table},

	{ EIC7700_MUX_U_NPU_E31_3MUX1_GFREE, "mux_u_npu_e31_3mux1_gfree", npu_e31_3mux1_gfree_mux_p,
		ARRAY_SIZE(npu_e31_3mux1_gfree_mux_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_NPU_CORE_CTRL,
		0, BIT_MASK(8) | BIT_MASK(9), 0, npu_e31_3mux1_gfree_mux_p_table},

	{ EIC7700_MUX_U_VI_ACLK_ROOT_2MUX1_GFREE, "mux_u_vi_aclk_root_2mux1_gfree", vi_aclk_root_2mux1_gfree_mux_p,
		ARRAY_SIZE(vi_aclk_root_2mux1_gfree_mux_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_VI_ACLK_CTRL,
		0, 1, 0,},

	{ EIC7700_MUX_U_VI_DW_ROOT_2MUX1, "mux_u_vi_dw_root_2mux1", mux_u_vi_dw_root_2mux1_p,
		ARRAY_SIZE(mux_u_vi_dw_root_2mux1_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_VI_DWCLK_CTRL,
		0, 1, 0,},

	{ EIC7700_MUX_U_VI_DVP_ROOT_2MUX1_GFREE, "mux_u_vi_dvp_root_2mux1_gfree", mux_u_vi_dvp_root_2mux1_gfree_p,
		ARRAY_SIZE(mux_u_vi_dvp_root_2mux1_gfree_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_VI_DVP_CLK_CTRL,
		0, 1, 0, },

	{ EIC7700_MUX_U_VI_DIG_ISP_ROOT_2MUX1_GFREE, "mux_u_vi_dig_isp_root_2mux1_gfree", mux_u_vi_dig_isp_root_2mux1_gfree_p,
		ARRAY_SIZE(mux_u_vi_dig_isp_root_2mux1_gfree_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_VI_DIG_ISP_CLK_CTRL,
		0, 1, 0, },

	{ EIC7700_MUX_U_VO_ACLK_ROOT_2MUX1_GFREE, "mux_u_vo_aclk_root_2mux1_gfree", mux_u_vo_aclk_root_2mux1_gfree_p,
		ARRAY_SIZE(mux_u_vo_aclk_root_2mux1_gfree_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_VO_ACLK_CTRL,
		0, 1, 0, },

	{ EIC7700_MUX_U_VO_PIXEL_ROOT_2MUX1, "mux_u_vo_pixel_root_2mux1", mux_u_vo_pixel_root_2mux1_p,
		ARRAY_SIZE(mux_u_vo_pixel_root_2mux1_p), CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_VO_PIXEL_CTRL,
		0, 1, 0, },

	{ EIC7700_MUX_U_VCDEC_ROOT_2MUX1_GFREE,  "mux_u_vcdec_root_2mux1_gfree",  mux_u_vcdec_root_2mux1_gfree_p,
		ARRAY_SIZE(mux_u_vcdec_root_2mux1_gfree_p),  CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_VCDEC_ROOTCLK_CTRL,
		0, 1, 0, },

	{ EIC7700_MUX_U_VCACLK_ROOT_2MUX1_GFREE,  "mux_u_vcaclk_root_2mux1_gfree",  mux_u_vcaclk_root_2mux1_gfree_p,
		ARRAY_SIZE(mux_u_vcaclk_root_2mux1_gfree_p),  CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_VC_ACLK_CTRL,
		0, 1, 0, },

	{ EIC7700_MUX_U_SYSCFG_CLK_ROOT_2MUX1_GFREE,  "mux_u_syscfg_clk_root_2mux1_gfree",  mux_u_syscfg_clk_root_2mux1_gfree_p,
		ARRAY_SIZE(mux_u_syscfg_clk_root_2mux1_gfree_p),  CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_SYSCFG_CLK_CTRL,
		0, 1, 0, },

	{ EIC7700_MUX_U_BOOTSPI_CLK_2MUX1_GFREE,  "mux_u_bootspi_clk_2mux1_gfree",  mux_u_bootspi_clk_2mux1_gfree_p,
		ARRAY_SIZE(mux_u_bootspi_clk_2mux1_gfree_p),  CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_BOOTSPI_CLK_CTRL,
		0, 1, 0, },

	{ EIC7700_MUX_U_SCPU_CORE_CLK_2MUX1_GFREE,  "mux_u_scpu_core_clk_2mux1_gfree",  mux_u_scpu_core_clk_2mux1_gfree_p,
		ARRAY_SIZE(mux_u_scpu_core_clk_2mux1_gfree_p),  CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_SCPU_CORECLK_CTRL,
		0, 1, 0, },

	{ EIC7700_MUX_U_LPCPU_CORE_CLK_2MUX1_GFREE,  "mux_u_lpcpu_core_clk_2mux1_gfree",  mux_u_lpcpu_core_clk_2mux1_gfree_p,
		ARRAY_SIZE(mux_u_lpcpu_core_clk_2mux1_gfree_p),  CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_LPCPU_CORECLK_CTRL,
		0, 1, 0, },

	{ EIC7700_MUX_U_VO_MCLK_2MUX_EXT_MCLK,  "mux_u_vo_mclk_2mux_ext_mclk",  mux_u_vo_mclk_2mux_ext_mclk_p,
		ARRAY_SIZE(mux_u_vo_mclk_2mux_ext_mclk_p),  CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_VO_MCLK_CTRL,
		0, 1, 0, },

	{ EIC7700_MUX_U_AONDMA_AXI2MUX1_GFREE,  "mux_u_aondma_axi2mux1_gfree",  mux_u_aondma_axi2mux1_gfree_p,
		ARRAY_SIZE(mux_u_aondma_axi2mux1_gfree_p),  CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_AON_DMA_CLK_CTRL,
		20, 1, 0, },

	{ EIC7700_MUX_U_RMII_REF_2MUX,  "mux_u_rmii_ref_2mux1",  mux_u_rmii_ref_2mux1_p,
		ARRAY_SIZE(mux_u_rmii_ref_2mux1_p),  CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_ETH0_CTRL,
		2, 1, 0, },

	{ EIC7700_MUX_U_ETH_CORE_2MUX1,  "mux_u_eth_core_2mux1",  mux_u_eth_core_2mux1_p,
		ARRAY_SIZE(mux_u_eth_core_2mux1_p),  CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_ETH0_CTRL,
		1, 1, 0, },

	{ EIC7700_MUX_U_SATA_PHY_2MUX1,  "mux_u_sata_phy_2mux1",  mux_u_sata_phy_2mux1_p,
		ARRAY_SIZE(mux_u_sata_phy_2mux1_p),  CLK_SET_RATE_PARENT, EIC7700_REG_OFFSET_SATA_OOB_CTRL,
		9, 1, 0, },

};

/*The hardware decides vaule 0, 1 and 2 both means 2 divsor, so we have to add these ugly tables*/
static struct clk_div_table u_3_bit_special_div_table[8];
static struct clk_div_table u_4_bit_special_div_table[16];
static struct clk_div_table u_6_bit_special_div_table[64];
static struct clk_div_table u_7_bit_special_div_table[128];
static struct clk_div_table u_8_bit_special_div_table[256];
static struct clk_div_table u_11_bit_special_div_table[2048];
static struct clk_div_table u_16_bit_special_div_table[65536];

static struct eswin_divider_clock eic7700_div_clks[] = {
	{ EIC7700_DIVDER_U_SYS_CFG_DIV_DYNM, "divder_u_sys_cfg_div_dynm",   "fixed_rate_clk_spll0_fout3", 0,
		EIC7700_REG_OFFSET_SYSCFG_CLK_CTRL, 4, 3, CLK_DIVIDER_ROUND_CLOSEST, u_3_bit_special_div_table},

	{ EIC7700_DIVDER_U_NOC_NSP_DIV_DYNM,   "divder_u_noc_nsp_div_dynm", "fixed_rate_clk_spll2_fout1", 0,
		EIC7700_REG_OFFSET_NOC_CLK_CTRL, 0, 3, CLK_DIVIDER_ROUND_CLOSEST, u_3_bit_special_div_table},

	{ EIC7700_DIVDER_U_BOOTSPI_DIV_DYNM,       "divder_u_bootspi_div_dynm",     "gate_clk_spll0_fout2", 0,
		EIC7700_REG_OFFSET_BOOTSPI_CLK_CTRL, 4, 6, CLK_DIVIDER_ROUND_CLOSEST, u_6_bit_special_div_table},

	{ EIC7700_DIVDER_U_SCPU_CORE_DIV_DYNM,     "divder_u_scpu_core_div_dynm",   "fixed_rate_clk_spll0_fout1", 0,
		EIC7700_REG_OFFSET_SCPU_CORECLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_LPCPU_CORE_DIV_DYNM,     "divder_u_lpcpu_core_div_dynm",   "fixed_rate_clk_spll0_fout1", 0,
		EIC7700_REG_OFFSET_LPCPU_CORECLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_GPU_ACLK_DIV_DYNM,     "divder_u_gpu_aclk_div_dynm",   "fixed_rate_clk_spll0_fout1", 0,
		EIC7700_REG_OFFSET_GPU_ACLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_DSP_ACLK_DIV_DYNM,     "divder_u_dsp_aclk_div_dynm",   "clk_clk_dsp_root", 0,
		EIC7700_REG_OFFSET_DSP_ACLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_D2D_ACLK_DIV_DYNM, "divder_u_d2d_aclk_div_dynm",   "mux_u_d2d_aclk_root_2mux1_gfree", 0,
		EIC7700_REG_OFFSET_D2D_ACLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_DDR_ACLK_DIV_DYNM, "divder_u_ddr_aclk_div_dynm",   "mux_u_ddr_aclk_root_2mux1_gfree", 0,
		EIC7700_REG_OFFSET_DDR_CLK_CTRL, 20, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_HSP_ACLK_DIV_DYNM,   "divder_u_hsp_aclk_div_dynm", "fixed_rate_clk_spll0_fout1", 0,
		EIC7700_REG_OFFSET_HSP_ACLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_ETH_TXCLK_DIV_DYNM_0,     "divder_u_eth_txclk_div_dynm_0",   "mux_u_eth_core_2mux1", 0,
		EIC7700_REG_OFFSET_ETH0_CTRL, 4, 7, CLK_DIVIDER_ROUND_CLOSEST, u_7_bit_special_div_table},

	{ EIC7700_DIVDER_U_ETH_TXCLK_DIV_DYNM_1,     "divder_u_eth_txclk_div_dynm_1",   "mux_u_eth_core_2mux1", 0,
		EIC7700_REG_OFFSET_ETH1_CTRL, 4, 7, CLK_DIVIDER_ROUND_CLOSEST, u_7_bit_special_div_table},

	{ EIC7700_DIVDER_U_MSHC_CORE_DIV_DYNM_0,     "divder_u_mshc_core_div_dynm_0",   "mux_u_mshcore_root_3mux1_0", 0,
		EIC7700_REG_OFFSET_MSHC0_CORECLK_CTRL, 4, 12, CLK_DIVIDER_ONE_BASED},

	{ EIC7700_DIVDER_U_MSHC_CORE_DIV_DYNM_1,     "divder_u_mshc_core_div_dynm_1",   "mux_u_mshcore_root_3mux1_1", 0,
		EIC7700_REG_OFFSET_MSHC1_CORECLK_CTRL, 4, 12, CLK_DIVIDER_ONE_BASED},

	{ EIC7700_DIVDER_U_MSHC_CORE_DIV_DYNM_2,     "divder_u_mshc_core_div_dynm_2",   "mux_u_mshcore_root_3mux1_2",  0,
		EIC7700_REG_OFFSET_MSHC2_CORECLK_CTRL, 4, 12, CLK_DIVIDER_ONE_BASED},

	{ EIC7700_DIVDER_U_PCIE_ACLK_DIV_DYNM,   "divder_u_pcie_aclk_div_dynm", "fixed_rate_clk_spll2_fout2", 0,
		EIC7700_REG_OFFSET_PCIE_ACLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_NPU_ACLK_DIV_DYNM,        "divder_u_npu_aclk_div_dynm", "fixed_rate_clk_spll0_fout1", 0,
		EIC7700_REG_OFFSET_NPU_ACLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_NPU_LLC_SRC0_DIV_DYNM,    "divder_u_npu_llc_src0_div_dynm",  "fixed_rate_clk_spll0_fout1", 0,
		EIC7700_REG_OFFSET_NPU_LLC_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_NPU_LLC_SRC1_DIV_DYNM,    "divder_u_npu_llc_src1_div_dynm",   "fixed_rate_clk_spll2_fout1", 0,
		EIC7700_REG_OFFSET_NPU_LLC_CTRL, 8, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_NPU_CORECLK_DIV_DYNM,     "divder_u_npu_coreclk_div_dynm",   "mux_u_npu_core_3mux1_gfree", 0,
		EIC7700_REG_OFFSET_NPU_CORE_CTRL, 4, 4, CLK_DIVIDER_ONE_BASED},

	{ EIC7700_DIVDER_U_NPU_E31_DIV_DYNM,         "divder_u_npu_e31_div_dynm",   "mux_u_npu_e31_3mux1_gfree", 0,
		EIC7700_REG_OFFSET_NPU_CORE_CTRL, 12, 4, CLK_DIVIDER_ONE_BASED},

	{ EIC7700_DIVDER_U_VI_ACLK_DIV_DYNM,          "divder_u_vi_aclk_div_dynm",   "mux_u_vi_aclk_root_2mux1_gfree", 0,
		EIC7700_REG_OFFSET_VI_ACLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_VI_DW_DIV_DYNM,            "divder_u_vi_dw_div_dynm",   "mux_u_vi_dw_root_2mux1", 0,
		EIC7700_REG_OFFSET_VI_DWCLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_VI_DVP_DIV_DYNM,        "divder_u_vi_dvp_div_dynm",   "mux_u_vi_dig_root_2mux1_gfree", 0,
		EIC7700_REG_OFFSET_VI_DVP_CLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_VI_DIG_ISP_DIV_DYNM,       "divder_u_vi_dig_isp_div_dynm", "mux_u_vi_dig_isp_root_2mux1_gfree", 0,
		EIC7700_REG_OFFSET_VI_DIG_ISP_CLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_VI_SHUTTER_DIV_DYNM_0,     "divder_u_vi_shutter_div_dynm_0",   "fixed_rate_clk_vpll_fout2",0,
		EIC7700_REG_OFFSET_VI_SHUTTER0, 4, 7, CLK_DIVIDER_ROUND_CLOSEST, u_7_bit_special_div_table},

	{ EIC7700_DIVDER_U_VI_SHUTTER_DIV_DYNM_1,     "divder_u_vi_shutter_div_dynm_1",   "fixed_rate_clk_vpll_fout2", 0,
		EIC7700_REG_OFFSET_VI_SHUTTER1, 4, 7,  CLK_DIVIDER_ROUND_CLOSEST, u_7_bit_special_div_table},

	{ EIC7700_DIVDER_U_VI_SHUTTER_DIV_DYNM_2,     "divder_u_vi_shutter_div_dynm_2",   "fixed_rate_clk_vpll_fout2", 0,
		EIC7700_REG_OFFSET_VI_SHUTTER2, 4, 7, CLK_DIVIDER_ROUND_CLOSEST, u_7_bit_special_div_table},

	{ EIC7700_DIVDER_U_VI_SHUTTER_DIV_DYNM_3,     "divder_u_vi_shutter_div_dynm_3",   "fixed_rate_clk_vpll_fout2", 0,
		EIC7700_REG_OFFSET_VI_SHUTTER3, 4, 7, CLK_DIVIDER_ROUND_CLOSEST, u_7_bit_special_div_table},

	{ EIC7700_DIVDER_U_VI_SHUTTER_DIV_DYNM_4,     "divder_u_vi_shutter_div_dynm_4",   "fixed_rate_clk_vpll_fout2", 0,
		EIC7700_REG_OFFSET_VI_SHUTTER4, 4, 7, CLK_DIVIDER_ROUND_CLOSEST, u_7_bit_special_div_table},

	{ EIC7700_DIVDER_U_VI_SHUTTER_DIV_DYNM_5,     "divder_u_vi_shutter_div_dynm_5",   "fixed_rate_clk_vpll_fout2", 0,
		EIC7700_REG_OFFSET_VI_SHUTTER5, 4, 7, CLK_DIVIDER_ROUND_CLOSEST, u_7_bit_special_div_table},

	{ EIC7700_DIVDER_U_VO_ACLK_DIV_DYNM,         "divder_u_vo_aclk_div_dynm", "mux_u_vo_aclk_root_2mux1_gfree", 0,
		EIC7700_REG_OFFSET_VO_ACLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_IESMCLK_DIV_DYNM,       "divder_u_iesmclk_div_dynm",     "fixed_rate_clk_spll0_fout3", 0,
		EIC7700_REG_OFFSET_VO_IESMCLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_VO_PIXEL_DIV_DYNM,	 "divder_u_vo_pixel_div_dynm",	"mux_u_vo_pixel_root_2mux1", 0,
		EIC7700_REG_OFFSET_VO_PIXEL_CTRL, 4, 6, CLK_DIVIDER_ROUND_CLOSEST, u_6_bit_special_div_table},

	{ EIC7700_DIVDER_U_VO_MCLK_DIV_DYNM,     "divder_u_vo_mclk_div_dynm",   "clk_apll_fout1", 0,
		EIC7700_REG_OFFSET_VO_MCLK_CTRL, 4, 8, CLK_DIVIDER_ROUND_CLOSEST, u_8_bit_special_div_table},

	{ EIC7700_DIVDER_U_VO_CEC_DIV_DYNM,     "divder_u_vo_cec_div_dynm",   "fixed_rate_clk_vpll_fout2", 0,
		EIC7700_REG_OFFSET_VO_PHY_CLKCTRL, 16, 16, CLK_DIVIDER_ROUND_CLOSEST, u_16_bit_special_div_table},

	{ EIC7700_DIVDER_U_VC_ACLK_DIV_DYNM,     "divder_u_vc_aclk_div_dynm",   "mux_u_vcaclk_root_2mux1_gfree", 0,
		EIC7700_REG_OFFSET_VC_ACLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_JD_DIV_DYNM,     "divder_u_jd_div_dynm",   "clk_clk_vc_root", 0,
		EIC7700_REG_OFFSET_JD_CLK_CTRL, 4, 4,  CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_JE_DIV_DYNM,     "divder_u_je_div_dynm",   "clk_clk_vc_root", 0,
		EIC7700_REG_OFFSET_JE_CLK_CTRL, 4, 4,  CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_VE_DIV_DYNM,     "divder_u_ve_div_dynm",   "clk_clk_vc_root", 0,
		EIC7700_REG_OFFSET_VE_CLK_CTRL, 4, 4,  CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_VD_DIV_DYNM,     "divder_u_vd_div_dynm",    "clk_clk_vc_root", 0,
		EIC7700_REG_OFFSET_VD_CLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_G2D_DIV_DYNM,       "divder_u_g2d_div_dynm",     "clk_clk_dsp_root", 0,
		EIC7700_REG_OFFSET_G2D_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_AONDMA_AXI_DIV_DYNM,     "divder_u_aondma_axi_div_dynm",   "fixed_rate_clk_spll0_fout1", 0,
		EIC7700_REG_OFFSET_AON_DMA_CLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_CRYPTO_DIV_DYNM,     "divder_u_crypto_div_dynm",   "fixed_rate_clk_spll0_fout1", 0,
		EIC7700_REG_OFFSET_SPACC_CLK_CTRL, 4, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_SATA_PHY_REF_DIV_DYNM,     "divder_u_sata_phy_ref_div_dynm",   "fixed_rate_clk_spll1_fout2", 0,
		EIC7700_REG_OFFSET_SATA_OOB_CTRL, 0, 4, CLK_DIVIDER_ROUND_CLOSEST, u_4_bit_special_div_table},

	{ EIC7700_DIVDER_U_DSP_0_ACLK_DIV_DYNM,     "divder_u_dsp_0_aclk_div_dynm",   "gate_dspt_aclk", 0,
		EIC7700_REG_OFFSET_DSP_CFG_CTRL, 19, 1, },

	{ EIC7700_DIVDER_U_DSP_1_ACLK_DIV_DYNM,     "divder_u_dsp_1_aclk_div_dynm",   "gate_dspt_aclk", 0,
		EIC7700_REG_OFFSET_DSP_CFG_CTRL, 20, 1, },

	{ EIC7700_DIVDER_U_DSP_2_ACLK_DIV_DYNM,     "divder_u_dsp_2_aclk_div_dynm",   "gate_dspt_aclk", 0,
		EIC7700_REG_OFFSET_DSP_CFG_CTRL, 21, 1, },

	{ EIC7700_DIVDER_U_DSP_3_ACLK_DIV_DYNM,     "divder_u_dsp_3_aclk_div_dynm",   "gate_dspt_aclk", 0,
		EIC7700_REG_OFFSET_DSP_CFG_CTRL, 22, 1, },

	{ EIC7700_DIVDER_U_AON_RTC_DIV_DYNM,     "divder_u_aon_rtc_div_dynm",   "clk_clk_1m", 0,
		EIC7700_REG_OFFSET_RTC_CLK_CTRL, 21, 11, CLK_DIVIDER_ROUND_CLOSEST, u_11_bit_special_div_table},

	{ EIC7700_DIVDER_U_U84_RTC_TOGGLE_DIV_DYNM,     "divder_u_u84_rtc_toggle_dynm",   "fixed_rate_clk_xtal_24m", 0,
		EIC7700_REG_OFFSET_RTC_CLK_CTRL, 16, 5, CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ROUND_CLOSEST},

};

/*
	these clks should init early to cost down the whole clk module init time
*/
static struct eswin_clock eic7700_clks_early_0[] = {
	{ EIC7700_CLK_CLK_DSP_ROOT , "clk_clk_dsp_root", "mux_u_dsp_aclk_root_2mux1_gfree", CLK_SET_RATE_PARENT,},
	{ EIC7700_CLK_CLK_VC_ROOT, "clk_clk_vc_root", "mux_u_vcdec_root_2mux1_gfree", CLK_SET_RATE_PARENT,},
};

static struct eswin_clock eic7700_clks_early_1[] = {
	{ EIC7700_CLK_CLK_SYS_CFG	,"clk_clk_sys_cfg", "mux_u_syscfg_clk_root_2mux1_gfree", CLK_SET_RATE_PARENT,},
	{ EIC7700_CLK_CLK_D2DDR_ACLK	,"clk_clk_d2ddr_aclk", "divder_u_ddr_aclk_div_dynm", CLK_SET_RATE_PARENT,},
	{ EIC7700_CLK_CLK_AONDMA_AXI_ST3,"clk_clk_aondma_axi_st3","mux_u_aondma_axi2mux1_gfree", CLK_SET_RATE_PARENT,},
	{ EIC7700_CLK_CLK_G2D_ST2	,"clk_clk_g2d_st2",	"divder_u_g2d_div_dynm", CLK_SET_RATE_PARENT,},
	{ EIC7700_CLK_CLK_MIPI_TXESC	,"clk_clk_mipi_txesc",	"fixed_factor_u_mipi_txesc_div10", CLK_SET_RATE_PARENT,},
	{ EIC7700_CLK_CLK_VI_ACLK_ST1	,"clk_clk_vi_aclk_st1",	"divder_u_vi_aclk_div_dynm", CLK_SET_RATE_PARENT,},
};

static struct eswin_gate_clock eic7700_gate_clks[] = {

	{ EIC7700_GATE_CLK_CPU_EXT_SRC_CORE_CLK_0 ,	"gate_clk_cpu_ext_src_core_clk_0", "mux_u_cpu_root_3mux1_gfree", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_U84_CLK_CTRL, 28, 0, },

	{ EIC7700_GATE_CLK_CPU_EXT_SRC_CORE_CLK_1 ,	"gate_clk_cpu_ext_src_core_clk_1", "mux_u_cpu_root_3mux1_gfree", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_U84_CLK_CTRL, 29, 0, },

	{ EIC7700_GATE_CLK_CPU_EXT_SRC_CORE_CLK_2 ,	"gate_clk_cpu_ext_src_core_clk_2", "mux_u_cpu_root_3mux1_gfree", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_U84_CLK_CTRL, 30, 0, },

	{ EIC7700_GATE_CLK_CPU_EXT_SRC_CORE_CLK_3 ,	"gate_clk_cpu_ext_src_core_clk_3", "mux_u_cpu_root_3mux1_gfree", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_U84_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_CLK_CPU_TRACE_CLK_0 ,		"gate_clk_cpu_trace_clk_0", "mux_u_cpu_root_3mux1_gfree", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_U84_CLK_CTRL, 24, 0, },

	{ EIC7700_GATE_CLK_CPU_TRACE_CLK_1 ,		"gate_clk_cpu_trace_clk_1", "mux_u_cpu_root_3mux1_gfree", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_U84_CLK_CTRL, 25, 0, },

	{ EIC7700_GATE_CLK_CPU_TRACE_CLK_2 ,		"gate_clk_cpu_trace_clk_2", "mux_u_cpu_root_3mux1_gfree", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_U84_CLK_CTRL, 26, 0, },

	{ EIC7700_GATE_CLK_CPU_TRACE_CLK_3 ,		"gate_clk_cpu_trace_clk_3", "mux_u_cpu_root_3mux1_gfree", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_U84_CLK_CTRL, 27, 0, },

	{ EIC7700_GATE_CLK_CPU_TRACE_COM_CLK ,		"gate_clk_cpu_trace_com_clk", "mux_u_cpu_aclk_2mux1_gfree", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_U84_CLK_CTRL, 23, 0, },

	{ EIC7700_GATE_CLK_CPU_CLK ,			"gate_clk_cpu_clk", "mux_u_cpu_aclk_2mux1_gfree", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_U84_CLK_CTRL, 28, 0, }, /*same as EIC7700_GATE_CLK_CPU_EXT_SRC_CORE_CLK_0 */

	{ EIC7700_GATE_CLK_SPLL0_FOUT2 ,		"gate_clk_spll0_fout2", "fixed_rate_clk_spll0_fout2", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_SPLL0_CFG_2, 31, 0, },

	{ EIC7700_GATE_NOC_NSP_CLK ,			"gate_noc_nsp_clk", "divder_u_noc_nsp_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_NOC_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_CLK_BOOTSPI ,			"gate_clk_bootspi", "mux_u_bootspi_clk_2mux1_gfree", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_BOOTSPI_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_CLK_BOOTSPI_CFG	,		"gate_clk_bootspi_cfg",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_BOOTSPI_CFGCLK_CTRL, 31, 0, },

	{ EIC7700_GATE_CLK_SCPU_CORE	,		"gate_clk_scpu_core",   "mux_u_scpu_core_clk_2mux1_gfree", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_SCPU_CORECLK_CTRL, 31, 0, },

	{ EIC7700_GATE_CLK_SCPU_BUS 			,"gate_clk_scpu_bus",   "fixed_factor_u_scpu_bus_div2", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_SCPU_BUSCLK_CTRL, 31, 0, },

	{ EIC7700_GATE_CLK_LPCPU_CORE			,"gate_clk_lpcpu_core",   "mux_u_lpcpu_core_clk_2mux1_gfree", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LPCPU_CORECLK_CTRL, 31, 0, },

	{ EIC7700_GATE_CLK_LPCPU_BUS			,"gate_clk_lpcpu_bus",   "fixed_factor_u_lpcpu_bus_div2", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LPCPU_BUSCLK_CTRL, 31, 0, },

	{ EIC7700_GATE_GPU_ACLK 			,"gate_gpu_aclk",   "divder_u_gpu_aclk_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_GPU_ACLK_CTRL, 31, 0, },

	{ EIC7700_GATE_GPU_GRAY_CLK 			,"gate_gpu_gray_clk",   "fixed_rate_clk_xtal_24m", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_GPU_GRAY_CTRL, 31, 0, },

	{ EIC7700_GATE_GPU_CFG_CLK			,"gate_gpu_cfg_clk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_GPU_CFG_CTRL, 31, 0, },

	{ EIC7700_GATE_DSPT_ACLK			,"gate_dspt_aclk",   "divder_u_dsp_aclk_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_DSP_ACLK_CTRL, 31, 0, },

	{ EIC7700_GATE_DSPT_CFG_CLK 			,"gate_dspt_cfg_clk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_DSP_CFG_CTRL, 31, 0, },

	{ EIC7700_GATE_D2D_ACLK 			,"gate_d2d_aclk",   "divder_u_d2d_aclk_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_D2D_ACLK_CTRL, 31, 0, },

	{ EIC7700_GATE_D2D_CFG_CLK			,"gate_d2d_cfg_clk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_D2D_CFG_CTRL, 31, 0, },

	{ EIC7700_GATE_TCU_ACLK 			,"gate_tcu_aclk",   "clk_clk_d2ddr_aclk", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_TCU_ACLK_CTRL, 31, 0, },

	{ EIC7700_GATE_TCU_CFG_CLK			,"gate_tcu_cfg_clk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_TCU_CFG_CTRL, 31, 0, },

	{ EIC7700_GATE_DDRT_CFG_CLK			,"gate_ddrt_cfg_clk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_DDR_CLK_CTRL, 9, 0, },

	{ EIC7700_GATE_DDRT0_P0_ACLK			,"gate_ddrt0_p0_aclk",   "clk_clk_d2ddr_aclk", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_DDR_CLK_CTRL, 4, 0, },

	{ EIC7700_GATE_DDRT0_P1_ACLK			,"gate_ddrt0_p1_aclk",   "clk_clk_d2ddr_aclk", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_DDR_CLK_CTRL, 5, 0, },

	{ EIC7700_GATE_DDRT0_P2_ACLK			,"gate_ddrt0_p2_aclk",   "clk_clk_d2ddr_aclk", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_DDR_CLK_CTRL, 6, 0, },

	{ EIC7700_GATE_DDRT0_P3_ACLK			,"gate_ddrt0_p3_aclk",   "clk_clk_d2ddr_aclk", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_DDR_CLK_CTRL, 7, 0, },

	{ EIC7700_GATE_DDRT0_P4_ACLK			,"gate_ddrt0_p4_aclk",   "clk_clk_d2ddr_aclk", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_DDR_CLK_CTRL, 8, 0, },

	{ EIC7700_GATE_DDRT1_P0_ACLK			,"gate_ddrt1_p0_aclk",   "clk_clk_d2ddr_aclk", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_DDR1_CLK_CTRL, 4, 0, },

	{ EIC7700_GATE_DDRT1_P1_ACLK			,"gate_ddrt1_p1_aclk",   "clk_clk_d2ddr_aclk", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_DDR1_CLK_CTRL, 5, 0, },

	{ EIC7700_GATE_DDRT1_P2_ACLK			,"gate_ddrt1_p2_aclk",   "clk_clk_d2ddr_aclk", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_DDR1_CLK_CTRL, 6, 0, },

	{ EIC7700_GATE_DDRT1_P3_ACLK			,"gate_ddrt1_p3_aclk",   "clk_clk_d2ddr_aclk", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_DDR1_CLK_CTRL, 7, 0, },

	{ EIC7700_GATE_DDRT1_P4_ACLK			,"gate_ddrt1_p4_aclk",   "clk_clk_d2ddr_aclk", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_DDR1_CLK_CTRL, 8, 0, },

	{ EIC7700_GATE_CLK_HSP_ACLK			,"gate_clk_hsp_aclk",   "divder_u_hsp_aclk_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_HSP_ACLK_CTRL, 31, 0, },

	{ EIC7700_GATE_CLK_HSP_CFGCLK			,"gate_clk_hsp_cfgclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_HSP_CFG_CTRL, 31, 0, },

	{ EIC7700_GATE_PCIET_ACLK			,"gate_pciet_aclk",   "divder_u_pcie_aclk_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_PCIE_ACLK_CTRL, 31, 0, },

	{ EIC7700_GATE_PCIET_CFG_CLK			,"gate_pciet_cfg_clk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_PCIE_CFG_CTRL, 31, 0, },

	{ EIC7700_GATE_PCIET_CR_CLK 			,"gate_pciet_cr_clk",   "fixed_factor_u_pcie_cr_div2", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_PCIE_CFG_CTRL, 0, 0, },

	{ EIC7700_GATE_PCIET_AUX_CLK			,"gate_pciet_aux_clk",   "fixed_factor_u_pcie_aux_div4", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_PCIE_CFG_CTRL, 1, 0, },

	{ EIC7700_GATE_NPU_ACLK 			,"gate_npu_aclk",   "divder_u_npu_aclk_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_NPU_ACLK_CTRL, 31, 0, },

	{ EIC7700_GATE_NPU_CFG_CLK			,"gate_npu_cfg_clk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_NPU_ACLK_CTRL, 30, 0, },

	{ EIC7700_GATE_NPU_LLC_ACLK 			,"gate_npu_llc_aclk",   "mux_u_npu_llclk_3mux1_gfree", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_NPU_LLC_CTRL, 31, 0, },

	{ EIC7700_GATE_NPU_CLK				,"gate_npu_clk",   "divder_u_npu_coreclk_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_NPU_CORE_CTRL, 31, 0, },

	{ EIC7700_GATE_NPU_E31_CLK			,"gate_npu_e31_clk",   "divder_u_npu_e31_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_NPU_CORE_CTRL, 30, 0, },

	{ EIC7700_GATE_VI_ACLK				,"gate_vi_aclk",   "clk_clk_vi_aclk_st1", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VI_ACLK_CTRL, 31, 0, },

	{ EIC7700_GATE_VI_CFG_CLK			,"gate_vi_cfg_clk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VI_ACLK_CTRL, 30, 0, },

	{ EIC7700_GATE_VI_DIG_DW_CLK			,"gate_vi_dig_dw_clk",   "divder_u_vi_dw_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VI_DWCLK_CTRL, 31, 0, },

	{ EIC7700_GATE_VI_DVP_CLK			,"gate_vi_dvp_clk",   "divder_u_vi_dvp_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VI_DVP_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_VI_DIG_ISP_CLK			,"gate_vi_dig_isp_clk",   "divder_u_vi_dig_isp_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VI_DIG_ISP_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_VI_SHUTTER_0			,"gate_vi_shutter_0",   "divder_u_vi_shutter_div_dynm_0", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VI_SHUTTER0, 31, 0, },

	{ EIC7700_GATE_VI_SHUTTER_1			,"gate_vi_shutter_1",   "divder_u_vi_shutter_div_dynm_1", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VI_SHUTTER1, 31, 0, },

	{ EIC7700_GATE_VI_SHUTTER_2			,"gate_vi_shutter_2",   "divder_u_vi_shutter_div_dynm_2", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VI_SHUTTER2, 31, 0, },

	{ EIC7700_GATE_VI_SHUTTER_3			,"gate_vi_shutter_3",   "divder_u_vi_shutter_div_dynm_3", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VI_SHUTTER3, 31, 0, },

	{ EIC7700_GATE_VI_SHUTTER_4			,"gate_vi_shutter_4",   "divder_u_vi_shutter_div_dynm_4", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VI_SHUTTER4, 31, 0, },

	{ EIC7700_GATE_VI_SHUTTER_5			,"gate_vi_shutter_5",   "divder_u_vi_shutter_div_dynm_5", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VI_SHUTTER5, 31, 0, },

	{ EIC7700_GATE_VI_PHY_TXCLKESC			,"gate_vi_phy_txclkesc",   "clk_clk_mipi_txesc", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VI_PHY_CLKCTRL, 0, 0, },

	{ EIC7700_GATE_VI_PHY_CFG			,"gate_vi_phy_cfg",   "fixed_rate_clk_xtal_24m", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VI_PHY_CLKCTRL, 1, 0, },

	{ EIC7700_GATE_VO_ACLK				,"gate_vo_aclk",   "divder_u_vo_aclk_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VO_ACLK_CTRL, 31, 0, },

	{ EIC7700_GATE_VO_CFG_CLK			,"gate_vo_cfg_clk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VO_ACLK_CTRL, 30, 0, },

	{ EIC7700_GATE_VO_HDMI_IESMCLK			,"gate_vo_hdmi_iesmclk",   "divder_u_iesmclk_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VO_IESMCLK_CTRL, 31, 0, },

	{ EIC7700_GATE_VO_PIXEL_CLK 			,"gate_vo_pixel_clk",   "divder_u_vo_pixel_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VO_PIXEL_CTRL, 31, 0, },

	{ EIC7700_GATE_VO_I2S_MCLK			,"gate_vo_i2s_mclk",   "mux_u_vo_mclk_2mux_ext_mclk", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VO_MCLK_CTRL, 31, 0, },

	{ EIC7700_GATE_VO_CR_CLK			,"gate_vo_cr_clk",   "clk_clk_mipi_txesc", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VO_PHY_CLKCTRL, 1, 0, },

	{ EIC7700_GATE_VC_ACLK				,"gate_vc_aclk",   "divder_u_vc_aclk_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VC_ACLK_CTRL, 31, 0, },

	{ EIC7700_GATE_VC_CFG_CLK			,"gate_vc_cfg_clk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VC_CLKEN_CTRL, 0, 0, },

	{ EIC7700_GATE_VC_JE_CLK			,"gate_vc_je_clk",   "divder_u_je_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_JE_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_VC_JD_CLK			,"gate_vc_jd_clk",   "divder_u_jd_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_JD_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_VC_VE_CLK			,"gate_vc_ve_clk",   "divder_u_ve_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VE_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_VC_VD_CLK			,"gate_vc_vd_clk",   "divder_u_vd_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VD_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_G2D_CFG_CLK			,"gate_g2d_cfg_clk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_G2D_CTRL, 28, 0, },

	{ EIC7700_GATE_G2D_CLK				,"gate_g2d_clk",   "clk_clk_g2d_st2", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_G2D_CTRL, 30, 0, },

	{ EIC7700_GATE_G2D_ACLK 			,"gate_g2d_aclk",   "clk_clk_g2d_st2", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_G2D_CTRL, 31, 0, },

	{ EIC7700_GATE_CLK_AONDMA_CFG			,"gate_clk_aondma_cfg",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_AON_DMA_CLK_CTRL, 30, 0, },

	{ EIC7700_GATE_AONDMA_ACLK			,"gate_aondma_aclk",   "clk_clk_aondma_axi_st3", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_AON_DMA_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_AON_ACLK 			,"gate_aon_aclk",   "clk_clk_aondma_axi_st3", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_AON_DMA_CLK_CTRL, 29, 0, },

	{ EIC7700_GATE_TIMER_CLK_0 			,"gate_time_clk_0",   "fixed_rate_clk_xtal_24m", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_TIMER_CLK_CTRL, 0, 0, },

	{ EIC7700_GATE_TIMER_CLK_1 			,"gate_time_clk_1",   "fixed_rate_clk_xtal_24m", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_TIMER_CLK_CTRL, 1, 0, },

	{ EIC7700_GATE_TIMER_CLK_2 			,"gate_time_clk_2",   "fixed_rate_clk_xtal_24m", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_TIMER_CLK_CTRL, 2, 0, },

	{ EIC7700_GATE_TIMER_CLK_3 			,"gate_time_clk_3",   "fixed_rate_clk_xtal_24m", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_TIMER_CLK_CTRL, 3, 0, },

	{ EIC7700_GATE_TIMER_PCLK_0			,"gate_timer_pclk_0",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_TIMER_CLK_CTRL, 4, 0, },

	{ EIC7700_GATE_TIMER_PCLK_1			,"gate_timer_pclk_1",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_TIMER_CLK_CTRL, 5, 0, },

	{ EIC7700_GATE_TIMER_PCLK_2			,"gate_timer_pclk_2",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_TIMER_CLK_CTRL, 6, 0, },

	{ EIC7700_GATE_TIMER_PCLK_3			,"gate_timer_pclk_3",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_TIMER_CLK_CTRL, 7, 0, },

	{ EIC7700_GATE_TIMER3_CLK8			,"gate_timer3_clk8",   "fixed_rate_clk_vpll_fout3", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_TIMER_CLK_CTRL, 8, 0, },

	{ EIC7700_GATE_CLK_RTC_CFG			,"gate_clk_rtc_cfg",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_RTC_CLK_CTRL, 2, 0, },

	{ EIC7700_GATE_CLK_RTC				,"gate_clk_rtc",   "divder_u_aon_rtc_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_RTC_CLK_CTRL, 1, 0, },

	{ EIC7700_GATE_CLK_PKA_CFG			,"gate_clk_pka_cfg",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_PKA_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_CLK_SPACC_CFG			,"gate_clk_spacc_cfg",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_SPACC_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_CLK_CRYPTO			,"gate_clk_crypto",   "divder_u_crypto_div_dynm", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_SPACC_CLK_CTRL, 30, 0, },

	{ EIC7700_GATE_CLK_TRNG_CFG 			,"gate_clk_trng_cfg",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_TRNG_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_CLK_OTP_CFG			,"gate_clk_otp_cfg",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_OTP_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_0			,"gate_clk_mailbox_0",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 0, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_1			,"gate_clk_mailbox_1",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 1, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_2			,"gate_clk_mailbox_2",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 2, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_3			,"gate_clk_mailbox_3",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 3, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_4			,"gate_clk_mailbox_4",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 4, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_5			,"gate_clk_mailbox_5",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 5, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_6			,"gate_clk_mailbox_6",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 6, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_7			,"gate_clk_mailbox_7",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 7, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_8			,"gate_clk_mailbox_8",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 8, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_9			,"gate_clk_mailbox_9",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 9, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_10			,"gate_clk_mailbox_10",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 10, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_11			,"gate_clk_mailbox_11",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 11, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_12			,"gate_clk_mailbox_12",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 12, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_13			,"gate_clk_mailbox_13",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 13, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_14			,"gate_clk_mailbox_14",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 14, 0, },

	{ EIC7700_GATE_CLK_MAILBOX_15			,"gate_clk_mailbox_15",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 15, 0, },

	{ EIC7700_GATE_LSP_I2C0_PCLK			,"gate_i2c0_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 7, 0, },

	{ EIC7700_GATE_LSP_I2C1_PCLK			,"gate_i2c1_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 8, 0, },

	{ EIC7700_GATE_LSP_I2C2_PCLK			,"gate_i2c2_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 9, 0, },

	{ EIC7700_GATE_LSP_I2C3_PCLK			,"gate_i2c3_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 10, 0, },

	{ EIC7700_GATE_LSP_I2C4_PCLK			,"gate_i2c4_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 11, 0, },

	{ EIC7700_GATE_LSP_I2C5_PCLK			,"gate_i2c5_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 12, 0, },

	{ EIC7700_GATE_LSP_I2C6_PCLK			,"gate_i2c6_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 13, 0, },

	{ EIC7700_GATE_LSP_I2C7_PCLK			,"gate_i2c7_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 14, 0, },

	{ EIC7700_GATE_LSP_I2C8_PCLK			,"gate_i2c8_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 15, 0, },

	{ EIC7700_GATE_LSP_I2C9_PCLK			,"gate_i2c9_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 16, 0, },

	{ EIC7700_GATE_LSP_WDT0_PCLK			,"gate_lsp_wdt0_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 28, 0, },

	{ EIC7700_GATE_LSP_WDT1_PCLK			,"gate_lsp_wdt1_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 29, 0, },

	{ EIC7700_GATE_LSP_WDT2_PCLK			,"gate_lsp_wdt2_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 30, 0, },

	{ EIC7700_GATE_LSP_WDT3_PCLK			,"gate_lsp_wdt3_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 31, 0, },

	{ EIC7700_GATE_LSP_SSI0_PCLK			,"gate_lsp_ssi0_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 26, 0, },

	{ EIC7700_GATE_LSP_SSI1_PCLK			,"gate_lsp_ssi1_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 27, 0, },

	{ EIC7700_GATE_LSP_UART0_PCLK			,"gate_lsp_uart0_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 17, 0, },

	{ EIC7700_GATE_LSP_UART1_PCLK			,"gate_lsp_uart1_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 18, 0, },

	{ EIC7700_GATE_LSP_UART2_PCLK			,"gate_lsp_uart2_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 19, 0, },

	{ EIC7700_GATE_LSP_UART3_PCLK			,"gate_lsp_uart3_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 20, 0, },

	{ EIC7700_GATE_LSP_UART4_PCLK			,"gate_lsp_uart4_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 21, 0, },

	{ EIC7700_GATE_LSP_TIMER_PCLK			,"gate_lsp_timer_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 25, 0, },

	{ EIC7700_GATE_LSP_FAN_PCLK			,"gate_lsp_fan_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 0, 0, },

	{ EIC7700_GATE_LSP_PVT_PCLK			,"gate_lsp_pvt_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN0, 1, 0, },

	{ EIC7700_GATE_LSP_PVT0_CLK			,"gate_pvt0_clk",   "fixed_factor_u_pvt_div20", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 16, 0, },

	{ EIC7700_GATE_LSP_PVT1_CLK			,"gate_pvt1_clk",   "fixed_factor_u_pvt_div20", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_LSP_CLK_EN1, 17, 0, },

	{ EIC7700_GATE_VC_JE_PCLK			,"gate_vc_je_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VC_CLKEN_CTRL, 2, 0, },

	{ EIC7700_GATE_VC_JD_PCLK			,"gate_vc_jd_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VC_CLKEN_CTRL, 1, 0, },

	{ EIC7700_GATE_VC_VE_PCLK			,"gate_vc_ve_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VC_CLKEN_CTRL, 5, 0, },

	{ EIC7700_GATE_VC_VD_PCLK			,"gate_vc_vd_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VC_CLKEN_CTRL, 4, 0, },

	{ EIC7700_GATE_VC_MON_PCLK			,"gate_vc_mon_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_VC_CLKEN_CTRL, 3, 0, },

	{ EIC7700_GATE_HSP_MSHC0_CORE_CLK		,"gate_hsp_mshc0_core_clk",   "divder_u_mshc_core_div_dynm_0", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_MSHC0_CORECLK_CTRL, 16, 0, },

	{ EIC7700_GATE_HSP_MSHC1_CORE_CLK		,"gate_hsp_mshc1_core_clk",   "divder_u_mshc_core_div_dynm_1", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_MSHC1_CORECLK_CTRL, 16, 0, },

	{ EIC7700_GATE_HSP_MSHC2_CORE_CLK		,"gate_hsp_mshc2_core_clk",   "divder_u_mshc_core_div_dynm_2", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_MSHC2_CORECLK_CTRL, 16, 0, },

	{ EIC7700_GATE_HSP_SATA_RBC_CLK			,"gate_hsp_sata_rbc_clk",   "fixed_rate_clk_spll1_fout2", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_SATA_RBC_CTRL, 0, 0, },

	{ EIC7700_GATE_HSP_SATA_OOB_CLK			,"gate_hsp_sata_oob_clk",   "mux_u_sata_phy_2mux1", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_SATA_OOB_CTRL, 31, 0, },

	{ EIC7700_GATE_HSP_DMA0_CLK			,"gate_hsp_dma0_clk",   "gate_clk_hsp_aclk", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_HSP_ACLK_CTRL, 0, 0, },

	{ EIC7700_GATE_HSP_ETH0_CORE_CLK		,"gate_hsp_eth0_core_clk",   "divder_u_eth_txclk_div_dynm_0", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_ETH0_CTRL, 0, 0, },

	{ EIC7700_GATE_HSP_ETH1_CORE_CLK		,"gate_hsp_eth1_core_clk",   "divder_u_eth_txclk_div_dynm_1", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_ETH1_CTRL, 0, 0, },

	{ EIC7700_GATE_HSP_RMII_REF_0			,"gate_hsp_rmii_ref_0",   "mux_u_rmii_ref_2mux1", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_ETH0_CTRL, 31, 0, },

	{ EIC7700_GATE_HSP_RMII_REF_1			,"gate_hsp_rmii_ref_1",   "mux_u_rmii_ref_2mux1", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_ETH1_CTRL, 31, 0, },

	{ EIC7700_GATE_AON_I2C0_PCLK			,"gate_aon_i2c0_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_I2C0_CLK_CTRL, 31, 0, },

	{ EIC7700_GATE_AON_I2C1_PCLK			,"gate_aon_i2c1_pclk",   "clk_clk_sys_cfg", CLK_SET_RATE_PARENT,
		EIC7700_REG_OFFSET_I2C1_CLK_CTRL, 31, 0, },
};

/* eic7700 clocks */
static struct eswin_clock eic7700_clks[] = {
	{  EIC7700_CLK_CPU_EXT_SRC_CORE_CLK_0	,"clk_cpu_ext_src_core_clk_0",	"gate_clk_cpu_ext_src_core_clk_0", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CPU_EXT_SRC_CORE_CLK_1	,"clk_cpu_ext_src_core_clk_1",	"gate_clk_cpu_ext_src_core_clk_1", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CPU_EXT_SRC_CORE_CLK_2	,"clk_cpu_ext_src_core_clk_2",	"gate_clk_cpu_ext_src_core_clk_2", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CPU_EXT_SRC_CORE_CLK_3	,"clk_cpu_ext_src_core_clk_3",	"gate_clk_cpu_ext_src_core_clk_3", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CPU_TRACE_CLK_0		,"clk_cpu_trace_clk_0",	 	"gate_clk_cpu_trace_clk_0", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CPU_TRACE_CLK_1		,"clk_cpu_trace_clk_1",	 	"gate_clk_cpu_trace_clk_1", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CPU_TRACE_CLK_2		,"clk_cpu_trace_clk_2",	 	"gate_clk_cpu_trace_clk_2", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CPU_TRACE_CLK_3		,"clk_cpu_trace_clk_3",	 	"gate_clk_cpu_trace_clk_3", CLK_SET_RATE_PARENT,},

	{  EIC7700_CLK_CPU_TRACE_COM_CLK 	,"clk_cpu_trace_com_clk",	"gate_clk_cpu_trace_com_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CPU_CLK			,"clk_cpu_clk",			"gate_clk_cpu_clk", CLK_SET_RATE_PARENT,},

	{  EIC7700_CLK_CLK_1M			,"clk_clk_1m",			"fixed_factor_u_clk_1m_div24", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_NOC_CFG_CLK		,"clk_noc_cfg_clk",		"clk_clk_sys_cfg", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_NOC_NSP_CLK		,"clk_noc_nsp_clk",		"gate_noc_nsp_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_BOOTSPI		,"clk_clk_bootspi",		"gate_clk_bootspi", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_BOOTSPI_CFG		,"clk_clk_bootspi_cfg",		"gate_clk_bootspi_cfg", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_U84_CORE_LP		,"clk_clk_u84_core_lp",		"fixed_factor_u_u84_core_lp_div2", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_SCPU_CORE		,"clk_clk_scpu_core",		"gate_clk_scpu_core", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_SCPU_BUS 		,"clk_clk_scpu_bus",		"gate_clk_scpu_bus", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_LPCPU_CORE		,"clk_clk_lpcpu_core",		"gate_clk_lpcpu_core", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_LPCPU_BUS		,"clk_clk_lpcpu_bus",		"gate_clk_lpcpu_bus", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_GPU_ACLK 		,"clk_gpu_aclk",		"gate_gpu_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_GPU_GRAY_CLK 		,"clk_gpu_gray_clk",		"gate_gpu_gray_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_GPU_CFG_CLK		,"clk_gpu_cfg_clk",		"gate_gpu_cfg_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DSPT_ACLK		,"clk_dspt_aclk",		"gate_dspt_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DSPT_CFG_CLK 		,"clk_dspt_cfg_clk",		"gate_dspt_cfg_clk", CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,},
	{  EIC7700_CLK_D2D_ACLK 		,"clk_d2d_aclk",		"gate_d2d_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_D2D_CFG_CLK		,"clk_d2d_cfg_clk",	 	"gate_d2d_cfg_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_TCU_ACLK 		,"clk_tcu_aclk",		"gate_tcu_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_TCU_CFG_CLK		,"clk_tcu_cfg_clk",		"gate_tcu_cfg_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DDRT_CFG_CLK 		,"clk_ddrt_cfg_clk",		"gate_ddrt_cfg_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DDRT0_P0_ACLK		,"clk_ddrt0_p0_aclk",		"gate_ddrt0_p0_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DDRT0_P1_ACLK		,"clk_ddrt0_p1_aclk",		"gate_ddrt0_p1_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DDRT0_P2_ACLK		,"clk_ddrt0_p2_aclk",		"gate_ddrt0_p2_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DDRT0_P3_ACLK		,"clk_ddrt0_p3_aclk",		"gate_ddrt0_p3_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DDRT0_P4_ACLK		,"clk_ddrt0_p4_aclk",		"gate_ddrt0_p4_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DDRT1_P0_ACLK		,"clk_ddrt1_p0_aclk",		"gate_ddrt1_p0_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DDRT1_P1_ACLK		,"clk_ddrt1_p1_aclk",		"gate_ddrt1_p1_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DDRT1_P2_ACLK		,"clk_ddrt1_p2_aclk",		"gate_ddrt1_p2_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DDRT1_P3_ACLK		,"clk_ddrt1_p3_aclk",		"gate_ddrt1_p3_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DDRT1_P4_ACLK		,"clk_ddrt1_p4_aclk",		"gate_ddrt1_p4_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_ACLK 		,"clk_hsp_aclk",		"gate_clk_hsp_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_CFG_CLK		,"clk_hsp_cfg_clk",		"gate_clk_hsp_cfgclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_SATA_RBC_CLK 	,"clk_hsp_sata_rbc_clk",	"gate_hsp_sata_rbc_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_SATA_OOB_CLK 	,"clk_hsp_sata_oob_clk",	"gate_hsp_sata_oob_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_SATA_PHY_REF		,"clk_hsp_sata_phy_ref",	"gate_hsp_sata_oob_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_SATA_PMALIVE_CLK 	,"clk_hsp_sata_pmalive_clk",  	"gate_hsp_sata_oob_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_ETH_APP_CLK		,"clk_hsp_eth_app_clk",		"clk_clk_sys_cfg", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_ETH_CSR_CLK		,"clk_hsp_eth_csr_clk",		"clk_clk_sys_cfg", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_ETH0_CORE_CLK	,"clk_hsp_eth0_core_clk",	"gate_hsp_eth0_core_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_ETH1_CORE_CLK	,"clk_hsp_eth1_core_clk",	"gate_hsp_eth1_core_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_MSHC0_CORE_CLK	,"clk_hsp_mshc0_core_clk",	"gate_hsp_mshc0_core_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_MSHC1_CORE_CLK	,"clk_hsp_mshc1_core_clk",	"gate_hsp_mshc1_core_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_MSHC2_CORE_CLK	,"clk_hsp_mshc2_core_clk",	"gate_hsp_mshc2_core_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_MSHC0_TMR_CLK	,"clk_hsp_mshc0_tmr_clk",	"clk_clk_1m", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_MSHC1_TMR_CLK	,"clk_hsp_mshc1_tmr_clk",	"clk_clk_1m", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_MSHC2_TMR_CLK	,"clk_hsp_mshc2_tmr_clk",	"clk_clk_1m", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_USB0_SUSPEND_CLK 	,"clk_hsp_usb0_suspend_clk",	"clk_clk_1m", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_USB1_SUSPEND_CLK 	,"clk_hsp_usb1_suspend_clk",	"clk_clk_1m", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_PCIET_ACLK		,"clk_pciet_aclk",		"gate_pciet_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_PCIET_CFG_CLK		,"clk_pciet_cfg_clk",		"gate_pciet_cfg_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_PCIET_CR_CLK 		,"clk_pciet_cr_clk",		"gate_pciet_cr_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_PCIET_AUX_CLK		,"clk_pciet_aux_clk",		"gate_pciet_aux_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_NPU_ACLK 		,"clk_npu_aclk",		"gate_npu_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_NPU_CFG_CLK		,"clk_npu_cfg_clk",		"gate_npu_cfg_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_NPU_LLC_SRC0 	,"clk_clk_npu_llc_src0",	"divder_u_npu_llc_src0_div_dynm", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_NPU_LLC_SRC1 	,"clk_clk_npu_llc_src1",	"divder_u_npu_llc_src1_div_dynm", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_NPU_LLC_ACLK 		,"clk_npu_llc_aclk",		"gate_npu_llc_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_NPU_CLK			,"clk_npu_clk",			"gate_npu_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_NPU_E31_CLK		,"clk_npu_e31_clk",		"gate_npu_e31_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VI_ACLK			,"clk_vi_aclk",			"gate_vi_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VI_DIG_DW_CLK		,"clk_vi_dig_dw_clk",		"gate_vi_dig_dw_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VI_CFG_CLK		,"clk_vi_cfg_clk",		"gate_vi_cfg_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VI_DVP_CLK		,"clk_vi_dvp_clk",		"gate_vi_dvp_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VI_DIG_ISP_CLK		,"clk_vi_dig_isp_clk",		"gate_vi_dig_isp_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VI_SHUTTER_0 		,"clk_vi_shutter_0",		"gate_vi_shutter_0", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VI_SHUTTER_1 		,"clk_vi_shutter_1",		"gate_vi_shutter_1", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VI_SHUTTER_2 		,"clk_vi_shutter_2",		"gate_vi_shutter_2", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VI_SHUTTER_3 		,"clk_vi_shutter_3",		"gate_vi_shutter_3", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VI_SHUTTER_4 		,"clk_vi_shutter_4",		"gate_vi_shutter_4", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VI_SHUTTER_5 		,"clk_vi_shutter_5",		"gate_vi_shutter_5", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VI_PHY_TXCLKESC		,"clk_vi_phy_txclkesc",		"gate_vi_phy_txclkesc", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VI_PHY_CFG		,"clk_vi_phy_cfg",		"gate_vi_phy_cfg", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VO_ACLK			,"clk_vo_aclk",			"gate_vo_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VO_CFG_CLK		,"clk_vo_cfg_clk",		"gate_vo_cfg_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VO_HDMI_IESMCLK		,"clk_vo_hdmi_iesmclk",		"gate_vo_hdmi_iesmclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VO_PIXEL_CLK 		,"clk_vo_pixel_clk",		"gate_vo_pixel_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VO_I2S_MCLK		,"clk_vo_i2s_mclk",		"gate_vo_i2s_mclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VO_CR_CLK		,"clk_vo_cr_clk",		"gate_vo_cr_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VO_CEC_CLK		,"clk_vo_cec_clk",		"divder_u_vo_cec_div_dynm", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VC_ACLK			,"clk_vc_aclk",			"gate_vc_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VC_CFG_CLK		,"clk_vc_cfg_clk",		"gate_vc_cfg_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VC_JE_CLK		,"clk_vc_je_clk",		"gate_vc_je_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VC_JD_CLK		,"clk_vc_jd_clk",		"gate_vc_jd_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VC_VE_CLK		,"clk_vc_ve_clk",		"gate_vc_ve_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VC_VD_CLK		,"clk_vc_vd_clk",		"gate_vc_vd_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_G2D_CFG_CLK		,"clk_g2d_cfg_clk",		"gate_g2d_cfg_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_G2D_CLK			,"clk_g2d_clk",			"gate_g2d_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_G2D_ACLK 		,"clk_g2d_aclk",		"gate_g2d_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_AONDMA_CFG		,"clk_aondma_cfg",		"gate_clk_aondma_cfg", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_AONDMA_ACLK		,"clk_aondma_aclk",		"gate_aondma_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_AON_ACLK 		,"clk_aon_aclk",		"gate_aon_aclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_TIMER_CLK_0 		,"clk_timer_clk_0",		"gate_time_clk_0", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_TIMER_CLK_1 		,"clk_timer_clk_1",		"gate_time_clk_1", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_TIMER_CLK_2 		,"clk_timer_clk_2",		"gate_time_clk_2", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_TIMER_CLK_3 		,"clk_timer_clk_3",		"gate_time_clk_3", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_TIMER_PCLK_0		,"clk_timer_pclk_0",		"gate_timer_pclk_0", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_TIMER_PCLK_1		,"clk_timer_pclk_1",		"gate_timer_pclk_1", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_TIMER_PCLK_2		,"clk_timer_pclk_2",		"gate_timer_pclk_2", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_TIMER_PCLK_3		,"clk_timer_pclk_3",		"gate_timer_pclk_3", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_TIMER3_CLK8		,"clk_timer3_clk8",		"gate_timer3_clk8", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_RTC_CFG		,"clk_clk_rtc_cfg",		"gate_clk_rtc_cfg", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_RTC			,"clk_clk_rtc",			"gate_clk_rtc", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_PKA_CFG		,"clk_clk_pka_cfg",	 	"gate_clk_pka_cfg", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_SPACC_CFG		,"clk_clk_spacc_cfg",		"gate_clk_spacc_cfg", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_CRYPTO		,"clk_clk_crypto",		"gate_clk_crypto", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_TRNG_CFG 		,"clk_clk_trng_cfg",		"gate_clk_trng_cfg", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_OTP_CFG		,"clk_clk_otp_cfg",	 	"gate_clk_otp_cfg", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLMM_CFG_CLK 		,"clk_clmm_cfg_clk",		"clk_clk_sys_cfg", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLMM_DEB_CLK 		,"clk_clmm_deb_clk",		"clk_clk_1m", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_0 		,"clk_mailbox0",		"gate_clk_mailbox_0", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_1 		,"clk_mailbox1",		"gate_clk_mailbox_1", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_2 		,"clk_mailbox2",		"gate_clk_mailbox_2", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_3 		,"clk_mailbox3",		"gate_clk_mailbox_3", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_4 		,"clk_mailbox4",		"gate_clk_mailbox_4", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_5 		,"clk_mailbox5",		"gate_clk_mailbox_5", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_6 		,"clk_mailbox6",		"gate_clk_mailbox_6", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_7 		,"clk_mailbox7",		"gate_clk_mailbox_7", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_8 		,"clk_mailbox8",		"gate_clk_mailbox_8", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_9 		,"clk_mailbox9",		"gate_clk_mailbox_9", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_10 		,"clk_mailbox10",		"gate_clk_mailbox_10", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_11 		,"clk_mailbox11",		"gate_clk_mailbox_11", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_12 		,"clk_mailbox12",		"gate_clk_mailbox_12", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_13 		,"clk_mailbox13",		"gate_clk_mailbox_13", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_14 		,"clk_mailbox14",		"gate_clk_mailbox_14", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_MAILBOX_15 		,"clk_mailbox15",		"gate_clk_mailbox_15", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_I2C0_PCLK		,"clk_i2c0_pclk",		"gate_i2c0_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_I2C1_PCLK		,"clk_i2c1_pclk",		"gate_i2c1_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_I2C2_PCLK		,"clk_i2c2_pclk",		"gate_i2c2_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_I2C3_PCLK		,"clk_i2c3_pclk",		"gate_i2c3_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_I2C4_PCLK		,"clk_i2c4_pclk",		"gate_i2c4_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_I2C5_PCLK		,"clk_i2c5_pclk",		"gate_i2c5_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_I2C6_PCLK		,"clk_i2c6_pclk",		"gate_i2c6_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_I2C7_PCLK		,"clk_i2c7_pclk",		"gate_i2c7_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_I2C8_PCLK		,"clk_i2c8_pclk",		"gate_i2c8_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_I2C9_PCLK		,"clk_i2c9_pclk",		"gate_i2c9_pclk", CLK_SET_RATE_PARENT,},

	{  EIC7700_CLK_LSP_WDT0_PCLK		,"clk_lsp_wdt0_pclk",		"gate_lsp_wdt0_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_WDT1_PCLK		,"clk_lsp_wdt1_pclk",		"gate_lsp_wdt1_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_WDT2_PCLK		,"clk_lsp_wdt2_pclk",		"gate_lsp_wdt2_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_WDT3_PCLK		,"clk_lsp_wdt3_pclk",		"gate_lsp_wdt3_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_SSI0_PCLK		,"clk_lsp_ssi0_pclk",		"gate_lsp_ssi0_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_SSI1_PCLK		,"clk_lsp_ssi1_pclk",		"gate_lsp_ssi1_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_UART0_PCLK		,"clk_lsp_uart0_pclk",		"gate_lsp_uart0_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_UART1_PCLK		,"clk_lsp_uart1_pclk",		"gate_lsp_uart1_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_UART2_PCLK		,"clk_lsp_uart2_pclk",		"gate_lsp_uart2_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_UART3_PCLK		,"clk_lsp_uart3_pclk",		"gate_lsp_uart3_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_UART4_PCLK		,"clk_lsp_uart4_pclk",		"gate_lsp_uart4_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_TIMER_PCLK		,"clk_lsp_timer_pclk",		"gate_lsp_timer_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_FAN_PCLK		,"clk_lsp_fan_pclk",		"gate_lsp_fan_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_LSP_PVT_PCLK		,"clk_lsp_pvt_pclk",		"gate_lsp_pvt_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_PVT_CLK_0		,"clk_pvt0_clk",		"gate_pvt0_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_PVT_CLK_1		,"clk_pvt1_clk",		"gate_pvt1_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VC_JE_PCLK		,"clk_vc_je_pclk",		"gate_vc_je_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VC_JD_PCLK		,"clk_vc_jd_pclk",		"gate_vc_jd_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VC_VE_PCLK		,"clk_vc_ve_pclk",		"gate_vc_ve_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VC_VD_PCLK		,"clk_vc_vd_pclk",		"gate_vc_vd_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_VC_MON_PCLK		,"clk_vc_mon_pclk",		"gate_vc_mon_pclk", CLK_SET_RATE_PARENT,},

	{  EIC7700_CLK_HSP_DMA0_CLK		,"clk_hsp_dma0_clk",		"gate_hsp_dma0_clk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_RMII_REF_0		,"clk_hsp_rmii_ref_0",		"gate_hsp_rmii_ref_0", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_HSP_RMII_REF_1		,"clk_hsp_rmii_ref_1",		"gate_hsp_rmii_ref_1", CLK_SET_RATE_PARENT,},

	{  EIC7700_CLK_DSP_ACLK_0		,"clk_dsp_aclk_0",	"divder_u_dsp_0_aclk_div_dynm", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DSP_ACLK_1		,"clk_dsp_aclk_1",	"divder_u_dsp_1_aclk_div_dynm", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DSP_ACLK_2		,"clk_dsp_aclk_2",	"divder_u_dsp_2_aclk_div_dynm", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_DSP_ACLK_3		,"clk_dsp_aclk_3",	"divder_u_dsp_3_aclk_div_dynm", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_CLK_U84_RTC_TOGGLE		,"clk_clk_u84_rtc_toggle",	"divder_u_u84_rtc_toggle_dynm", CLK_SET_RATE_PARENT,},

	{  EIC7700_CLK_AON_I2C0_PCLK		,"clk_aon_i2c0_pclk",	"gate_aon_i2c0_pclk", CLK_SET_RATE_PARENT,},
	{  EIC7700_CLK_AON_I2C1_PCLK		,"clk_aon_i2c1_pclk",	"gate_aon_i2c1_pclk", CLK_SET_RATE_PARENT,},
};

static void special_div_table_init(struct clk_div_table *table, int table_size)
{
	int i;

	if (table_size < 3) {
		return;
	}
	if (!table) {
		return;
	}
	/*The hardware decides vaule 0, 1 and 2 both means 2 divsor*/
	for (i = 0; i < 3; i++) {
		table[i].val = i;
		table[i].div = 2;
	}
	for (i = 3; i < table_size; i++) {
		table[i].val = i;
		table[i].div = i;
	}
	return;
}

static int eswin_clk_probe(struct platform_device *pdev)
{
	struct eswin_clock_data *clk_data;

	clk_data = eswin_clk_init(pdev, EIC7700_NR_CLKS);
	if (!clk_data)
		return -EAGAIN;

	special_div_table_init(u_3_bit_special_div_table, ARRAY_SIZE(u_3_bit_special_div_table));
	special_div_table_init(u_4_bit_special_div_table, ARRAY_SIZE(u_4_bit_special_div_table));
	special_div_table_init(u_6_bit_special_div_table, ARRAY_SIZE(u_6_bit_special_div_table));
	special_div_table_init(u_7_bit_special_div_table, ARRAY_SIZE(u_7_bit_special_div_table));
	special_div_table_init(u_8_bit_special_div_table, ARRAY_SIZE(u_8_bit_special_div_table));
	special_div_table_init(u_11_bit_special_div_table, ARRAY_SIZE(u_11_bit_special_div_table));
	special_div_table_init(u_16_bit_special_div_table, ARRAY_SIZE(u_16_bit_special_div_table));

	eswin_clk_register_fixed_rate(eic7700_fixed_rate_clks,
				ARRAY_SIZE(eic7700_fixed_rate_clks),
				clk_data);
	eswin_clk_register_pll(eic7700_pll_clks,
			ARRAY_SIZE(eic7700_pll_clks), clk_data, &pdev->dev);

	eswin_clk_register_fixed_factor(eic7700_fixed_factor_clks,
				ARRAY_SIZE(eic7700_fixed_factor_clks),
				clk_data);
	eswin_clk_register_mux(eic7700_mux_clks, ARRAY_SIZE(eic7700_mux_clks),
				clk_data);
	eswin_clk_register_clk(eic7700_clks_early_0, ARRAY_SIZE(eic7700_clks_early_0),
				clk_data);
	eswin_clk_register_divider(eic7700_div_clks, ARRAY_SIZE(eic7700_div_clks),
				clk_data);
	eswin_clk_register_clk(eic7700_clks_early_1, ARRAY_SIZE(eic7700_clks_early_1),
				clk_data);
	eswin_clk_register_gate(eic7700_gate_clks, ARRAY_SIZE(eic7700_gate_clks),
				clk_data);
	eswin_clk_register_clk(eic7700_clks, ARRAY_SIZE(eic7700_clks), clk_data);
	return 0;
}

static const struct of_device_id eswin_clock_dt_ids[] = {
	 { .compatible = "eswin,eic7700-clock", },
	 { /* sentinel */ },
};

static struct platform_driver eswin_clock_driver = {
	.probe	= eswin_clk_probe,
	.driver = {
		.name	= "eswin-clock",
		.of_match_table	= eswin_clock_dt_ids,
	},
};
static int __init eic7700_clk_init(void)
{
	return platform_driver_register(&eswin_clock_driver);
}
arch_initcall(eic7700_clk_init);
