// SPDX-License-Identifier: GPL-2.0
/*
 * ESWIN Emmc Driver
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
 * Authors: Shuang Liang <liangshuang@eswincomputing.com>
 */
#include <linux/delay.h>
#include <linux/reset.h>
#include "sdhci-eswin.h"

static void eswin_mshc_coreclk_config(struct sdhci_host *host, uint16_t divisor,
					unsigned int flag_sel)
{
	struct sdhci_pltfm_host *pltfm_host;
	struct eswin_sdhci_data *eswin_sdhci;
	u32 val = 0;
	u32 delay = 0xfffff;

	pltfm_host = sdhci_priv(host);
	eswin_sdhci = sdhci_pltfm_priv(pltfm_host);

	val = readl(eswin_sdhci->core_clk_reg);
	val &= ~MSHC_CORE_CLK_ENABLE;
	writel(val, eswin_sdhci->core_clk_reg);
	while (delay--)
		;
	val &= ~(MSHC_CORE_CLK_FREQ_BIT_MASK << MSHC_CORE_CLK_FREQ_BIT_SHIFT);
	val |= (divisor & MSHC_CORE_CLK_FREQ_BIT_MASK)
	       << MSHC_CORE_CLK_FREQ_BIT_SHIFT;
	val &= ~(MSHC_CORE_CLK_SEL_BIT);
	val |= flag_sel;
	writel(val, eswin_sdhci->core_clk_reg);

	udelay(100);
	val |= MSHC_CORE_CLK_ENABLE;
	writel(val, eswin_sdhci->core_clk_reg);
	mdelay(1);
}

static void eswin_mshc_coreclk_disable(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host;
	struct eswin_sdhci_data *eswin_sdhci;
	u32 val = 0;

	pltfm_host = sdhci_priv(host);
	eswin_sdhci = sdhci_pltfm_priv(pltfm_host);

	val = readl(eswin_sdhci->core_clk_reg);
	val &= ~MSHC_CORE_CLK_ENABLE;
	writel(val, eswin_sdhci->core_clk_reg);
}

void eswin_sdhci_disable_card_clk(struct sdhci_host *host)
{
	unsigned int clk;

	/* Reset SD Clock Enable */
	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	clk &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
}

void eswin_sdhci_enable_card_clk(struct sdhci_host *host)
{
	ktime_t timeout;
	unsigned int clk;

	clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);

	clk |= SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	/* Wait max 150 ms */
	timeout = ktime_add_ms(ktime_get(), 150);
	while (1) {
		bool timedout = ktime_after(ktime_get(), timeout);
		clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
		if (clk & SDHCI_CLOCK_INT_STABLE)
			break;
		if (timedout) {
			pr_err("%s: Internal clock never stabilised.\n",
					mmc_hostname(host->mmc));
			return;
		}
		udelay(10);
	}

	clk |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
	mdelay(1);
}

void eswin_sdhci_set_core_clock(struct sdhci_host *host,
				       unsigned int clock)
{
	unsigned int div, divide;
	unsigned int flag_sel, max_clk;

	if (clock == 0) {
		eswin_mshc_coreclk_disable(host);
		return;
	}

	if (SDHCI_CLK_208M % clock == 0) {
		flag_sel = 1;
		max_clk = SDHCI_CLK_208M;
	} else {
		flag_sel = 0;
		max_clk = SDHCI_CLK_200M;
	}

	for (div = 1; div <= MAX_CORE_CLK_DIV; div++) {
		if ((max_clk / div) <= clock)
			break;
	}
	div--;

	if (div == 0 || div == 1) {
		divide = 2;
	} else {
		divide = (div + 1) * 2;
	}
	pr_debug("%s: clock:%d timing:%d\n", mmc_hostname(host->mmc), clock, host->timing);

	eswin_sdhci_disable_card_clk(host);
	eswin_mshc_coreclk_config(host, divide, flag_sel);
	eswin_sdhci_enable_card_clk(host);
	mdelay(2);
}

static void eswin_sdhci_set_clk_delays(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct eswin_sdhci_data *eswin_sdhci = sdhci_pltfm_priv(pltfm_host);
	struct eswin_sdhci_clk_data *clk_data = &eswin_sdhci->clk_data;

	clk_set_phase(clk_data->sampleclk,
			clk_data->clk_phase_in[host->timing]);
	clk_set_phase(clk_data->sdcardclk,
			clk_data->clk_phase_out[host->timing]);
}

static void eswin_sdhci_dt_read_clk_phase(struct device *dev,
					struct eswin_sdhci_clk_data *clk_data,
					unsigned int timing, const char *prop)
{
	struct device_node *np = dev->of_node;

	int clk_phase[2] = { 0 };

	/*
	 * Read Tap Delay values from DT, if the DT does not contain the
	 * Tap Values then use the pre-defined values.
	 */
	if (of_property_read_variable_u32_array(np, prop, &clk_phase[0], 2,
						0)) {
		dev_dbg(dev, "Using predefined clock phase for %s = %d %d\n",
			prop, clk_data->clk_phase_in[timing],
			clk_data->clk_phase_out[timing]);
		return;
	}

	/* The values read are Input and Output Clock Delays in order */
	clk_data->clk_phase_in[timing] = clk_phase[0];
	clk_data->clk_phase_out[timing] = clk_phase[1];
}

/**
 * eswin_dt_parse_clk_phases - Read Clock Delay values from DT
 *
 * @dev:        Pointer to our struct device.
 * @clk_data:       Pointer to the Clock Data structure
 *
 * Called at initialization to parse the values of Clock Delays.
 */
void eswin_sdhci_dt_parse_clk_phases(struct device *dev,
				struct eswin_sdhci_clk_data *clk_data)
{
	clk_data->set_clk_delays = eswin_sdhci_set_clk_delays;

	eswin_sdhci_dt_read_clk_phase(dev, clk_data, MMC_TIMING_LEGACY,
				      "clk-phase-legacy");
	eswin_sdhci_dt_read_clk_phase(dev, clk_data, MMC_TIMING_MMC_HS,
				      "clk-phase-mmc-hs");
	eswin_sdhci_dt_read_clk_phase(dev, clk_data, MMC_TIMING_SD_HS,
				      "clk-phase-sd-hs");
	eswin_sdhci_dt_read_clk_phase(dev, clk_data, MMC_TIMING_UHS_SDR12,
				      "clk-phase-uhs-sdr12");
	eswin_sdhci_dt_read_clk_phase(dev, clk_data, MMC_TIMING_UHS_SDR25,
				      "clk-phase-uhs-sdr25");
	eswin_sdhci_dt_read_clk_phase(dev, clk_data, MMC_TIMING_UHS_SDR50,
				      "clk-phase-uhs-sdr50");
	eswin_sdhci_dt_read_clk_phase(dev, clk_data, MMC_TIMING_UHS_SDR104,
				      "clk-phase-uhs-sdr104");
	eswin_sdhci_dt_read_clk_phase(dev, clk_data, MMC_TIMING_UHS_DDR50,
				      "clk-phase-uhs-ddr50");
	eswin_sdhci_dt_read_clk_phase(dev, clk_data, MMC_TIMING_MMC_DDR52,
				      "clk-phase-mmc-ddr52");
	eswin_sdhci_dt_read_clk_phase(dev, clk_data, MMC_TIMING_MMC_HS200,
				      "clk-phase-mmc-hs200");
	eswin_sdhci_dt_read_clk_phase(dev, clk_data, MMC_TIMING_MMC_HS400,
				      "clk-phase-mmc-hs400");
}

unsigned int eswin_convert_drive_impedance_ohm(struct platform_device *pdev,
					     unsigned int dr_ohm)
{
	switch (dr_ohm) {
	case 100:
		return PHYCTRL_DR_100OHM;
	case 66:
		return PHYCTRL_DR_66OHM;
	case 50:
		return PHYCTRL_DR_50OHM;
	case 40:
		return PHYCTRL_DR_40OHM;
	case 33:
		return PHYCTRL_DR_33OHM;
	}

	dev_warn(&pdev->dev, "Invalid value %u for drive-impedance-ohm.\n",
		 dr_ohm);
	return PHYCTRL_DR_50OHM;
}

static void eswin_sdhci_do_reset(struct eswin_sdhci_data *eswin_sdhci)
{
	int ret;

	ret = reset_control_assert(eswin_sdhci->txrx_rst);
	WARN_ON(0 != ret);
	ret = reset_control_assert(eswin_sdhci->phy_rst);
	WARN_ON(0 != ret);
	ret = reset_control_assert(eswin_sdhci->prstn);
	WARN_ON(0 != ret);
	ret = reset_control_assert(eswin_sdhci->arstn);
	WARN_ON(0 != ret);

	mdelay(2);

	ret = reset_control_deassert(eswin_sdhci->txrx_rst);
	WARN_ON(0 != ret);
	ret = reset_control_deassert(eswin_sdhci->phy_rst);
	WARN_ON(0 != ret);
	ret = reset_control_deassert(eswin_sdhci->prstn);
	WARN_ON(0 != ret);
	ret = reset_control_deassert(eswin_sdhci->arstn);
	WARN_ON(0 != ret);
}

int eswin_sdhci_reset_init(struct device *dev,
				 struct eswin_sdhci_data *eswin_sdhci)
{
	int ret = 0;
	eswin_sdhci->txrx_rst = devm_reset_control_get_optional(dev, "txrx_rst");
	if (IS_ERR_OR_NULL(eswin_sdhci->txrx_rst)) {
		dev_err_probe(dev, PTR_ERR(eswin_sdhci->txrx_rst),
			      "txrx_rst reset not found.\n");
		return -EFAULT;
	}

	eswin_sdhci->phy_rst = devm_reset_control_get_optional(dev, "phy_rst");
	if (IS_ERR_OR_NULL(eswin_sdhci->phy_rst)) {
		dev_err_probe(dev, PTR_ERR(eswin_sdhci->phy_rst),
			      "phy_rst reset not found.\n");
		return -EFAULT;
	}

	eswin_sdhci->prstn = devm_reset_control_get_optional(dev, "prstn");
	if (IS_ERR_OR_NULL(eswin_sdhci->prstn)) {
		dev_err_probe(dev, PTR_ERR(eswin_sdhci->prstn),
			      "prstn reset not found.\n");
		return -EFAULT;
	}

	eswin_sdhci->arstn = devm_reset_control_get_optional(dev, "arstn");
	if (IS_ERR_OR_NULL(eswin_sdhci->arstn)) {
		dev_err_probe(dev, PTR_ERR(eswin_sdhci->arstn),
			      "arstn reset not found.\n");
		return -EFAULT;
	}
	eswin_sdhci_do_reset(eswin_sdhci);

	return ret;
}
