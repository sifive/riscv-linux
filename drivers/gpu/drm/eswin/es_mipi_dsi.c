// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Eswin Holdings Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/iopoll.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/mfd/syscon.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <video/mipi_display.h>
#include <drm/drm_simple_kms_helper.h>
#include <drm/bridge/dw_mipi_dsi.h>

#include "es_crtc.h"
#include "es_mipi_dsi.h"
#include "es_panel.h"

#define MAX_LANE_COUNT 4
#define PLL_INPUT_REF_CLK 20000000

/* Defines the offset of the dphy register */
#define DSI_PHY_TST_CTRL0 0xb4
#define PHY_TESTCLK BIT(1)
#define PHY_UNTESTCLK 0
#define PHY_TESTCLR BIT(0)
#define PHY_UNTESTCLR 0

#define DSI_PHY_TST_CTRL1 0xb8
#define PHY_TESTEN BIT(16)
#define PHY_UNTESTEN 0
#define PHY_TESTDOUT(n) (((n) & 0xff) << 8)
#define PHY_TESTDIN(n) (((n) & 0xff) << 0)
#define INPUT_DIVIDER(val) (val & 0x7f)

#define LOW_PROGRAM_EN 0
#define HIGH_PROGRAM_EN BIT(7)
#define LOOP_DIV_LOW_SEL(val) (val & 0x1f)
#define LOOP_DIV_HIGH_SEL(val) ((val >> 5) & 0xf)
#define PLL_LOOP_DIV_EN BIT(5)
#define PLL_INPUT_DIV_EN BIT(4)

#define LPF_RESISTORS_15_5KOHM 0x1
#define LPF_RESISTORS_13KOHM 0x2
#define LPF_RESISTORS_11_5KOHM 0x4
#define LPF_RESISTORS_10_5KOHM 0x8
#define LPF_RESISTORS_8KOHM 0x10
#define LPF_PROGRAM_EN BIT(6)
#define LPF_RESISTORS_SEL(val) ((val) & 0x3f)

#define LOW_PROGRAM_EN 0
#define HIGH_PROGRAM_EN BIT(7)
#define LOOP_DIV_LOW_SEL(val) (val & 0x1f)
#define LOOP_DIV_HIGH_SEL(val) ((val >> 5) & 0xf)
#define PLL_LOOP_DIV_EN BIT(5)
#define PLL_INPUT_DIV_EN BIT(4)

#define PLL_BIAS_CUR_SEL_CAP_VCO_CONTROL 0x10
#define PLL_CP_CONTROL_PLL_LOCK_BYPASS 0x11
#define PLL_LPF_AND_CP_CONTROL 0x12
#define PLL_INPUT_DIVIDER_RATIO 0x17
#define PLL_LOOP_DIVIDER_RATIO 0x18
#define PLL_INPUT_AND_LOOP_DIVIDER_RATIOS_CONTROL 0x19

#define DSI_TO_CNT_CFG 0x78
#define HSTX_TO_CNT(p) (((p) & 0xffff) << 16)
#define LPRX_TO_CNT(p) ((p) & 0xffff)

struct hstt {
	unsigned int maxfreq;
	struct dw_mipi_dsi_dphy_timing timing;
};

struct pll_parameter {
	u32 max_freq;
	u32 actual_freq;
	u16 hs_freq_range;
	u16 pll_n;
	u16 pll_m;
	u8 vco_cntrl;
	u8 cpbias_cntrl;
	u8 gmp_cntrl;
	u8 int_cntrl;
	u8 prop_cntrl;
};

#define HSTT(_maxfreq, _c_lp2hs, _c_hs2lp, _d_lp2hs, _d_hs2lp)                 \
	{                                                                      \
		.maxfreq = _maxfreq, .timing = {                               \
			.clk_lp2hs = _c_lp2hs,                                 \
			.clk_hs2lp = _c_hs2lp,                                 \
			.data_lp2hs = _d_lp2hs,                                \
			.data_hs2lp = _d_hs2lp,                                \
		}                                                              \
	}

#define PLL_PARAM(_maxfreq, _vco_ctl, _cpbias_ctl, _gmp_ctl, _int_ctl,         \
		  _prop_ctl)                                                   \
	{                                                                      \
		.max_freq = _maxfreq, .vco_cntrl = _vco_ctl,                   \
		.cpbias_cntrl = _cpbias_ctl, .gmp_cntrl = _gmp_ctl,            \
		.int_cntrl = _int_ctl, .prop_cntrl = _prop_ctl,                \
	}

struct mipi_dsi_priv {
	u32 lanes;
	u32 format;
	unsigned long mode_flags;

	struct pll_parameter pll_param;

	void __iomem *dphy_base;

	struct clk *ivideo_clk;
	u64 pll_ref_clk;

	struct dw_mipi_dsi_plat_data plat_data;
	struct reset_control *rst_dsi_phyrstn;
	void *data;
};

struct es_mipi_dsi {
	struct mipi_dsi_priv dsi_priv;
	struct drm_encoder encoder;
	struct drm_crtc *crtc;
};

/* Table A-4 High-Speed Transition Times (High-Speed Entry LP->HS DATA LANE(Considers HS_CLK_LANE_ENTRY)d)*/
struct hstt hstt_table[] = {
	HSTT(80, 21, 17, 35, 10),      HSTT(90, 23, 17, 39, 10),
	HSTT(100, 22, 17, 37, 10),     HSTT(110, 25, 18, 43, 11),
	HSTT(120, 26, 20, 46, 11),     HSTT(130, 27, 19, 46, 11),
	HSTT(140, 27, 19, 46, 11),     HSTT(150, 28, 20, 47, 12),
	HSTT(160, 30, 21, 53, 13),     HSTT(170, 30, 21, 55, 13),
	HSTT(180, 31, 21, 53, 13),     HSTT(190, 32, 22, 58, 13),
	HSTT(205, 35, 22, 58, 13),     HSTT(220, 37, 26, 63, 15),
	HSTT(235, 38, 28, 65, 16),     HSTT(250, 41, 29, 71, 17),
	HSTT(275, 43, 29, 74, 18),     HSTT(300, 45, 32, 80, 19),
	HSTT(325, 48, 33, 86, 18),     HSTT(350, 51, 35, 91, 20),
	HSTT(400, 59, 37, 102, 21),    HSTT(450, 65, 40, 115, 23),
	HSTT(500, 71, 41, 126, 24),    HSTT(550, 77, 44, 135, 26),
	HSTT(600, 82, 46, 147, 27),    HSTT(650, 87, 48, 156, 28),
	HSTT(700, 94, 52, 166, 29),    HSTT(750, 99, 52, 175, 31),
	HSTT(800, 105, 55, 187, 32),   HSTT(850, 110, 58, 196, 32),
	HSTT(900, 115, 58, 206, 35),   HSTT(950, 120, 62, 213, 36),
	HSTT(1000, 128, 63, 225, 38),  HSTT(1050, 132, 65, 234, 38),
	HSTT(1100, 138, 67, 243, 39),  HSTT(1150, 146, 69, 259, 42),
	HSTT(1200, 151, 71, 269, 43),  HSTT(1250, 153, 74, 273, 45),
	HSTT(1300, 160, 73, 282, 46),  HSTT(1350, 165, 76, 294, 47),
	HSTT(1400, 172, 78, 304, 49),  HSTT(1450, 177, 80, 314, 49),
	HSTT(1500, 183, 81, 326, 52),  HSTT(1550, 191, 84, 339, 52),
	HSTT(1600, 194, 85, 345, 52),  HSTT(1650, 201, 86, 355, 53),
	HSTT(1700, 208, 88, 368, 53),  HSTT(1750, 212, 89, 378, 53),
	HSTT(1800, 220, 90, 389, 54),  HSTT(1850, 223, 92, 401, 54),
	HSTT(1900, 231, 91, 413, 55),  HSTT(1950, 236, 95, 422, 56),
	HSTT(2000, 243, 97, 432, 56),  HSTT(2050, 248, 99, 442, 58),
	HSTT(2100, 252, 100, 454, 59), HSTT(2150, 259, 102, 460, 61),
	HSTT(2200, 266, 105, 476, 62), HSTT(2250, 269, 109, 481, 63),
	HSTT(2300, 272, 109, 490, 65), HSTT(2350, 281, 112, 502, 66),
	HSTT(2400, 283, 115, 509, 66), HSTT(2450, 282, 115, 510, 67),
	HSTT(2500, 281, 118, 508, 67),
};

static struct pll_parameter pll_tbl[] = {
	PLL_PARAM(55, 0x3f, 0x10, 0x01, 0x00, 0x0D),
	PLL_PARAM(82, 0x37, 0x10, 0x01, 0x00, 0x0D),
	PLL_PARAM(110, 0x2f, 0x10, 0x01, 0x00, 0x0D),
	PLL_PARAM(165, 0x27, 0x10, 0x01, 0x00, 0x0D),
	PLL_PARAM(220, 0x1f, 0x10, 0x01, 0x00, 0x0D),
	PLL_PARAM(330, 0x17, 0x10, 0x01, 0x00, 0x0D),
	PLL_PARAM(440, 0x0f, 0x10, 0x01, 0x00, 0x0D),
	PLL_PARAM(660, 0x07, 0x10, 0x01, 0x00, 0x0D),
	PLL_PARAM(1149, 0x03, 0x10, 0x01, 0x00, 0x0D),
	PLL_PARAM(1152, 0x01, 0x10, 0x01, 0x00, 0x0D),
	PLL_PARAM(1250, 0x01, 0x10, 0x01, 0x00, 0x0E),
};

static const struct of_device_id es_mipi_dsi_dt_ids[] = {
	{
		.compatible = "eswin,dsi",
	},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, es_mipi_dsi_dt_ids);

static void dsi_write(struct mipi_dsi_priv *priv, u32 reg, u32 val)
{
	writel(val, priv->dphy_base + reg);
}

static u32 dsi_read(struct mipi_dsi_priv *priv, u32 reg)
{
	return readl(priv->dphy_base + reg);
}

static void es_dsi_encoder_disable(struct drm_encoder *encoder)
{
	struct es_mipi_dsi *es_dsi =
		container_of(encoder, struct es_mipi_dsi, encoder);
	struct mipi_dsi_priv *dsi_priv = &es_dsi->dsi_priv;

	DRM_INFO("enter, encoder = 0x%px\n", encoder);

	clk_disable_unprepare(dsi_priv->ivideo_clk);
}

static bool es_dsi_encoder_mode_fixup(struct drm_encoder *encoder,
				      const struct drm_display_mode *mode,
				      struct drm_display_mode *adj_mode)
{
	struct es_mipi_dsi *es_dsi =
		container_of(encoder, struct es_mipi_dsi, encoder);
	struct drm_crtc_state *crtc_state =
		container_of(mode, struct drm_crtc_state, mode);

	DRM_INFO("enter, bind crtc%d\n", drm_crtc_index(crtc_state->crtc));
	DRM_INFO("mode: " DRM_MODE_FMT "\n", DRM_MODE_ARG(mode));
	DRM_INFO("adj_mode: " DRM_MODE_FMT "\n", DRM_MODE_ARG(adj_mode));

	es_dsi->crtc = crtc_state->crtc;
	return true;
}

static void es_dsi_encoder_mode_set(struct drm_encoder *encoder,
				    struct drm_display_mode *mode,
				    struct drm_display_mode *adj_mode)
{
	DRM_INFO("mode: " DRM_MODE_FMT "\n", DRM_MODE_ARG(mode));
	DRM_INFO("adj_mode: " DRM_MODE_FMT "\n", DRM_MODE_ARG(adj_mode));
}

static void es_dsi_encoder_mode_enable(struct drm_encoder *encoder)
{
	int ret;
	struct es_mipi_dsi *es_dsi =
		container_of(encoder, struct es_mipi_dsi, encoder);
	struct mipi_dsi_priv *dsi_priv = &es_dsi->dsi_priv;

	DRM_INFO("enter, encoder = 0x%px\n", encoder);

	ret = clk_prepare_enable(dsi_priv->ivideo_clk);
	if (ret)
		DRM_ERROR("enable ivideo clk failed, ret = %d\n", ret);
}

static const struct drm_encoder_helper_funcs es_dsi_encoder_helper_funcs = {
	.mode_fixup = es_dsi_encoder_mode_fixup,
	.mode_set = es_dsi_encoder_mode_set,
	.enable = es_dsi_encoder_mode_enable,
	.disable = es_dsi_encoder_disable,
};

static void es_mipi_dsi_phy_write(struct mipi_dsi_priv *dsi, u8 test_code,
				  u8 test_data)
{
	/*
     * With the falling edge on TESTCLK, the TESTDIN[7:0] signal content
     * is latched internally as the current test code. Test data is
     * programmed internally by rising edge on TESTCLK.
     */
	dsi_write(dsi, DSI_PHY_TST_CTRL0, PHY_TESTCLK | PHY_UNTESTCLR);

	dsi_write(dsi, DSI_PHY_TST_CTRL1,
		  PHY_TESTEN | PHY_TESTDOUT(0) | PHY_TESTDIN(test_code));

	dsi_write(dsi, DSI_PHY_TST_CTRL0, PHY_UNTESTCLK | PHY_UNTESTCLR);

	dsi_write(dsi, DSI_PHY_TST_CTRL1,
		  PHY_UNTESTEN | PHY_TESTDOUT(0) | PHY_TESTDIN(test_data));

	dsi_write(dsi, DSI_PHY_TST_CTRL0, PHY_TESTCLK | PHY_UNTESTCLR);
}

static void es_mipi_dsi_phy_write_m(struct mipi_dsi_priv *dsi, u16 m)
{
	dsi_write(dsi, DSI_PHY_TST_CTRL0, PHY_UNTESTCLR);
	dsi_write(dsi, DSI_PHY_TST_CTRL1,
		  PHY_TESTEN | PHY_TESTDOUT(0) | PLL_LOOP_DIVIDER_RATIO);
	dsi_write(dsi, DSI_PHY_TST_CTRL0, PHY_TESTCLK | PHY_UNTESTCLR);
	dsi_write(dsi, DSI_PHY_TST_CTRL0, PHY_UNTESTCLK | PHY_UNTESTCLR);
	dsi_write(dsi, DSI_PHY_TST_CTRL1,
		  LOOP_DIV_HIGH_SEL(m) | HIGH_PROGRAM_EN);
	dsi_write(dsi, DSI_PHY_TST_CTRL0, PHY_TESTCLK | PHY_UNTESTCLR);
	dsi_write(dsi, DSI_PHY_TST_CTRL0, PHY_UNTESTCLK | PHY_UNTESTCLR);
	dsi_write(dsi, DSI_PHY_TST_CTRL1, LOOP_DIV_LOW_SEL(m) | LOW_PROGRAM_EN);
	dsi_write(dsi, DSI_PHY_TST_CTRL0, PHY_TESTCLK | PHY_UNTESTCLR);
	dsi_write(dsi, DSI_PHY_TST_CTRL0, PHY_UNTESTCLK | PHY_UNTESTCLR);
}

static int es_dsi_phy_init(void *priv_data)
{
	struct mipi_dsi_priv *dsi_priv = (struct mipi_dsi_priv *)priv_data;
	struct pll_parameter *pll_param = &dsi_priv->pll_param;

	dsi_write(dsi_priv, DSI_TO_CNT_CFG, HSTX_TO_CNT(0) | LPRX_TO_CNT(0));

	es_mipi_dsi_phy_write(dsi_priv,
			      PLL_INPUT_AND_LOOP_DIVIDER_RATIOS_CONTROL,
			      PLL_LOOP_DIV_EN | PLL_INPUT_DIV_EN);

	DRM_INFO(
		"set pll m:0x%x, n:0x%x, vco_cntrl:0x%x ,cpbias:0x%x, gmp:0x%x, int:0x%x, prop:0x%x\n",
		pll_param->pll_m, pll_param->pll_n, pll_param->vco_cntrl,
		pll_param->cpbias_cntrl, pll_param->gmp_cntrl,
		pll_param->int_cntrl, pll_param->prop_cntrl);
	//  0x17 cfg -> n
	es_mipi_dsi_phy_write(dsi_priv, PLL_INPUT_DIVIDER_RATIO,
			      INPUT_DIVIDER(pll_param->pll_n));

	es_mipi_dsi_phy_write_m(dsi_priv, pll_param->pll_m);

	// 0x12 cfg
	es_mipi_dsi_phy_write(dsi_priv, PLL_LPF_AND_CP_CONTROL,
			      LPF_PROGRAM_EN | pll_param->vco_cntrl);

	// 0x1c cfg
	es_mipi_dsi_phy_write(dsi_priv, 0x1c, pll_param->cpbias_cntrl);

	// 0x13 cfg
	es_mipi_dsi_phy_write(dsi_priv, 0x13, pll_param->gmp_cntrl << 4);

	// 0x0f cfg
	es_mipi_dsi_phy_write(dsi_priv, 0x0f, pll_param->int_cntrl);
	// 0x0f cfg
	es_mipi_dsi_phy_write(dsi_priv, 0x0e, pll_param->prop_cntrl);

	return 0;
}

static int dsi_phy_pll_get_params(struct mipi_dsi_priv *dsi_priv, u64 lane_bps)
{
	int i;
	unsigned int min_prediv, max_prediv;
	unsigned int _prediv0 = 1, _prediv, best_prediv = 0;
	unsigned long best_freq = 0;
	unsigned long fvco_min, fvco_max, fin, fout, fvco;
	unsigned long f_multi, best_multi;
	unsigned long min_delta = ULONG_MAX;
	struct pll_parameter *pll_param = &dsi_priv->pll_param;

	fin = dsi_priv->pll_ref_clk;
	fout = lane_bps / 2;

	/* constraint: 2Mhz <= Fref / N <= 8MHz */
	min_prediv = DIV_ROUND_UP(fin, 8 * USEC_PER_SEC);
	max_prediv = fin / (2 * USEC_PER_SEC);

	/* constraint: 320MHz <= Fvco <= 1250Mhz */
	fvco_min = 320 * USEC_PER_SEC;
	fvco_max = 1250 * USEC_PER_SEC;

	if (fout * 4 < fvco_min)
		_prediv0 = 8;
	else if (fout * 2 < fvco_min)
		_prediv0 = 4;
	else if (fout < fvco_min)
		_prediv0 = 2;

	fvco = fout * _prediv0;

	for (_prediv = min_prediv; _prediv <= max_prediv; _prediv++) {
		u64 tmp;
		u32 delta;

		/* Fvco = Fref * M / N */
		tmp = (u64)fvco * _prediv;
		do_div(tmp, fin);
		f_multi = tmp;
		/* M range */
		if (f_multi < 64 || f_multi > 625)
			continue;

		tmp = (u64)f_multi * fin;
		do_div(tmp, _prediv); // fvco
		if (tmp < fvco_min || tmp > fvco_max)
			continue;

		delta = abs(fvco - tmp);
		if (delta < min_delta) {
			best_prediv = _prediv;
			best_multi = f_multi; // M
			min_delta = delta;
			best_freq = tmp / _prediv0; // fout
		}
	}

	if (best_freq) {
		pll_param->pll_n = best_prediv - 1;
		pll_param->pll_m = best_multi - 2;
		pll_param->actual_freq = best_freq;
	} else {
		DRM_ERROR("Can not find best_freq for DPHY\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(pll_tbl); i++) {
		if (pll_param->actual_freq <=
		    pll_tbl[i].max_freq * USEC_PER_SEC) {
			pll_param->max_freq =
				pll_tbl[i].max_freq * USEC_PER_SEC;
			/* cfgclkfreqrange[5:0] = round[(Fcfg_clk(MHz)-17)*4] */
			pll_param->vco_cntrl = pll_tbl[i].vco_cntrl;
			pll_param->cpbias_cntrl = pll_tbl[i].cpbias_cntrl;
			pll_param->gmp_cntrl = pll_tbl[i].gmp_cntrl;
			pll_param->int_cntrl = pll_tbl[i].int_cntrl;
			pll_param->prop_cntrl = pll_tbl[i].prop_cntrl;
			break;
		}
	}

	DRM_INFO(
		"max_freq = %d, actual_freq = %d, pll_n = %d, pll_m = %d, i=%d\n",
		pll_param->max_freq, pll_param->actual_freq, pll_param->pll_n,
		pll_param->pll_m, i);
	return 0;
}

static int es_dsi_get_lane_mbps(void *priv_data,
				const struct drm_display_mode *mode,
				unsigned long mode_flags, u32 lanes, u32 format,
				unsigned int *lane_mbps)
{
	int ret, bpp;
	u64 tmp;
	struct mipi_dsi_priv *dsi_priv = (struct mipi_dsi_priv *)priv_data;

	DRM_INFO("mode: " DRM_MODE_FMT "\n", DRM_MODE_ARG(mode));
	DRM_INFO("lanes: %d, mode_flags: 0x%lx, format: %d\n", lanes,
		 mode_flags, format);

	dsi_priv->format = format;
	bpp = mipi_dsi_pixel_format_to_bpp(dsi_priv->format);
	if (bpp < 0) {
		DRM_DEV_ERROR(NULL, "failed to get bpp for pixel format %d\n",
			      dsi_priv->format);
		return bpp;
	}

	if (!lanes)
		lanes = dsi_priv->plat_data.max_data_lanes;

	dsi_priv->mode_flags = mode_flags;
	dsi_priv->lanes = lanes;

	tmp = (u64)mode->clock * 1000;

	tmp = tmp * bpp / lanes;

	ret = dsi_phy_pll_get_params(dsi_priv, tmp);
	if (ret)
		return ret;

	*lane_mbps =
		DIV_ROUND_UP(dsi_priv->pll_param.actual_freq * 2, USEC_PER_SEC);

	DRM_INFO("bpp = %d, lane_mbps = %d\n", bpp, *lane_mbps);

	return 0;
}

static int es_dsi_get_timing(void *priv_data, unsigned int lane_mbps,
			     struct dw_mipi_dsi_dphy_timing *timing)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hstt_table); i++)
		if (lane_mbps < hstt_table[i].maxfreq)
			break;

	if (i == ARRAY_SIZE(hstt_table))
		i--;

	*timing = hstt_table[i].timing;

	DRM_INFO(
		"lane_mbps: %d, c_lp2hs: %d, c_hs2lp: %d, d_lp2hs: %d, d_hs2lp: %d\n",
		lane_mbps, timing->clk_lp2hs, timing->clk_hs2lp,
		timing->data_lp2hs, timing->data_lp2hs);

	return 0;
}

struct dw_mipi_dsi_phy_ops dphy_ops = {
	.init = es_dsi_phy_init,
	.get_lane_mbps = es_dsi_get_lane_mbps,
	.get_timing = es_dsi_get_timing,
};

static int es_mipi_dsi_bind(struct device *dev, struct device *master,
			    void *data)
{
	int ret = 0;
	unsigned int max_data_lanes;
	struct platform_device *pdev = to_platform_device(dev);
	struct drm_device *drm_dev = (struct drm_device *)data;
	struct es_mipi_dsi *es_dsi;
	struct mipi_dsi_priv *dsi_priv;
	struct drm_encoder *encoder;
	struct resource *res;

	es_dsi = devm_kzalloc(&pdev->dev, sizeof(*es_dsi), GFP_KERNEL);
	if (!es_dsi) {
		ret = -ENODEV;
		goto exit0;
	}

	if (!pdev->dev.of_node) {
		DRM_ERROR("of_node is null, dev: %px\n", dev);
		ret = -ENODEV;
		goto exit0;
	}

	dsi_priv = &es_dsi->dsi_priv;

	ret = of_property_read_u32(pdev->dev.of_node, "max-lanes",
				   &max_data_lanes);
	if (ret) {
		DRM_WARN("max-lanes not defined, default max-lanes is 4\n");
		max_data_lanes = MAX_LANE_COUNT;
	}
	dsi_priv->plat_data.max_data_lanes = max_data_lanes;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dsi_priv->dphy_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(dsi_priv->dphy_base)) {
		DRM_ERROR("get mm_sys register base failed, of_node: %pOF\n",
			  dev->of_node);
		ret = PTR_ERR(dsi_priv->dphy_base);
		goto exit0;
	}

	dsi_priv->plat_data.base = dsi_priv->dphy_base;

	dsi_priv->plat_data.phy_ops = &dphy_ops;
	dsi_priv->plat_data.priv_data = dsi_priv;
	dsi_priv->pll_ref_clk = PLL_INPUT_REF_CLK;

	dsi_priv->ivideo_clk = devm_clk_get(dev, "pclk");
	if (IS_ERR(dsi_priv->ivideo_clk)) {
		DRM_ERROR("get ivideo clk failed, of_node: %pOF\n",
			  dev->of_node);
		ret = PTR_ERR(dsi_priv->ivideo_clk);
		goto exit0;
	}

	/* dsi phyctl reset */
	dsi_priv->rst_dsi_phyrstn =
		devm_reset_control_get_optional(dev, "phyrstn");
	if (IS_ERR_OR_NULL(dsi_priv->rst_dsi_phyrstn)) {
		DRM_ERROR("Failed to get dsi phyrstn reset handle\n");
		goto exit0;
	}
	if (dsi_priv->rst_dsi_phyrstn) {
		ret = reset_control_reset(dsi_priv->rst_dsi_phyrstn);
		WARN_ON(0 != ret);
	}

	encoder = &es_dsi->encoder;
	encoder->possible_crtcs =
		drm_of_find_possible_crtcs(drm_dev, dev->of_node);
	if (encoder->possible_crtcs == 0) {
		DRM_ERROR("Failed to find possible_crtcs:0\n");
	}

	ret = drm_simple_encoder_init(drm_dev, encoder, DRM_MODE_ENCODER_DSI);
	if (ret) {
		DRM_ERROR("Failed to initialize encoder with drm\n");
		return ret;
	}

	drm_encoder_helper_add(encoder, &es_dsi_encoder_helper_funcs);

	platform_set_drvdata(pdev, es_dsi);

	dsi_priv->data = dw_mipi_dsi_probe(pdev, &dsi_priv->plat_data);
	if (IS_ERR(dsi_priv->data)) {
		DRM_ERROR("probe dw-mipi dsi failed\n");
		ret = -ENODEV;
		goto exit2;
	}

	ret = dw_mipi_dsi_bind((struct dw_mipi_dsi *)dsi_priv->data, encoder);
	if (ret)
		goto exit3;

	DRM_INFO("mipi dsi bind done\n");

	return 0;

exit3:
	dw_mipi_dsi_remove((struct dw_mipi_dsi *)dsi_priv->data);
exit2:
	drm_encoder_cleanup(encoder);
exit0:
	DRM_INFO("mipi dsi bind failed, ret = %d\n", ret);
	return ret;
}

static void es_mipi_dsi_unbind(struct device *dev, struct device *master,
			       void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct es_mipi_dsi *es_dsi =
		(struct es_mipi_dsi *)platform_get_drvdata(pdev);
	struct mipi_dsi_priv *dsi_priv = &es_dsi->dsi_priv;

	DRM_INFO("mipi dsi unbind\n");

	dw_mipi_dsi_unbind((struct dw_mipi_dsi *)dsi_priv->data);
	dw_mipi_dsi_remove((struct dw_mipi_dsi *)dsi_priv->data);
	drm_encoder_cleanup(&es_dsi->encoder);
}

static const struct component_ops es_mipi_dsi_ops = {
	.bind = es_mipi_dsi_bind,
	.unbind = es_mipi_dsi_unbind,
};

static int es_mipi_dsi_probe(struct platform_device *pdev)
{
	mipi_dsi_driver_register(&es_panel_driver);

	return component_add(&pdev->dev, &es_mipi_dsi_ops);
}

static int es_mipi_dsi_remove(struct platform_device *pdev)
{
	DRM_INFO("mipi dsi remove\n");
	mipi_dsi_driver_unregister(&es_panel_driver);
	component_del(&pdev->dev, &es_mipi_dsi_ops);

	return 0;
}

struct platform_driver es_mipi_dsi_driver = {
	.probe = es_mipi_dsi_probe,
	.remove = es_mipi_dsi_remove,
	.driver = {
		   .of_match_table = es_mipi_dsi_dt_ids,
		   .name = "mipi-dsi-drv",
	},
};

MODULE_AUTHOR("lilijun@eswin.com");
MODULE_DESCRIPTION("Eswin dw dsi driver");
MODULE_LICENSE("GPL v2");
