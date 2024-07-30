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

#include <linux/clk-provider.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include "cqhci.h"
#include "sdhci-pltfm.h"
#include <linux/reset.h>
#include <linux/iommu.h>
#include <linux/bitfield.h>
#include <linux/regmap.h>
#include <linux/eic7700-sid-cfg.h>
#include "sdhci-eswin.h"

#define eswin_sdhci_VENDOR_REGISTER_BASEADDR 0x800
#define eswin_sdhci_VENDOR_EMMC_CTRL_REGISTER 0x2c
#define VENDOR_ENHANCED_STROBE BIT(8)

#define eswin_sdhci_CQE_BASE_ADDR eswin_sdhci_VENDOR_REGISTER_BASEADDR

/* Controller does not have CD wired and will not function normally without */
#define eswin_sdhci_QUIRK_FORCE_CDTEST BIT(0)

/* Controller immediately reports SDHCI_CLOCK_INT_STABLE after enabling the
 * internal clock even when the clock isn't stable */
#define eswin_sdhci_QUIRK_CLOCK_UNSTABLE BIT(1)

/*
 * On some SoCs the syscon area has a feature where the upper 16-bits of
 * each 32-bit register act as a write mask for the lower 16-bits.  This allows
 * atomic updates of the register without locking.  This macro is used on SoCs
 * that have that feature.
 */
#define HIWORD_UPDATE(val, mask, shift) \
	((val) << (shift) | (mask) << ((shift) + 16))

static void eswin_sdhci_set_clock(struct sdhci_host *host, unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct eswin_sdhci_data *eswin_sdhci = sdhci_pltfm_priv(pltfm_host);
	struct eswin_sdhci_clk_data *clk_data = &eswin_sdhci->clk_data;

	/* Set the Input and Output Clock Phase Delays */
	if (clk_data->set_clk_delays)
		clk_data->set_clk_delays(host);

	eswin_sdhci_set_core_clock(host, clock);

	/*
	 * Some controllers immediately report SDHCI_CLOCK_INT_STABLE
	 * after enabling the clock even though the clock is not
	 * stable. Trying to use a clock without waiting here results
	 * in EILSEQ while detecting some older/slower cards. The
	 * chosen delay is the maximum delay from sdhci_set_clock.
	 */
	if (eswin_sdhci->quirks & SDHCI_ESWIN_QUIRK_CLOCK_UNSTABLE)
		msleep(20);
}

static void eswin_sdhci_hs400_enhanced_strobe(struct mmc_host *mmc,
					      struct mmc_ios *ios)
{
	u32 vendor;
	struct sdhci_host *host = mmc_priv(mmc);

	vendor = sdhci_readl(host, eswin_sdhci_VENDOR_EMMC_CTRL_REGISTER);
	if (ios->enhanced_strobe)
		vendor |= VENDOR_ENHANCED_STROBE;
	else
		vendor &= ~VENDOR_ENHANCED_STROBE;

	sdhci_writel(host, vendor, eswin_sdhci_VENDOR_EMMC_CTRL_REGISTER);
}

static void eswin_sdhci_config_phy_delay(struct sdhci_host *host, int delay)
{
	delay &= PHY_CLK_MAX_DELAY_MASK;

	/*phy clk delay line config*/
	sdhci_writeb(host, PHY_UPDATE_DELAY_CODE, PHY_SDCLKDL_CNFG_R);
	sdhci_writeb(host, delay, PHY_SDCLKDL_DC_R);
	sdhci_writeb(host, 0x0, PHY_SDCLKDL_CNFG_R);
}

static void eswin_sdhci_config_phy(struct sdhci_host *host)
{
	unsigned int val = 0;
	unsigned int drv = 0;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct eswin_sdhci_data *eswin_sdhci = sdhci_pltfm_priv(pltfm_host);
	struct eswin_sdhci_phy_data *phy = &eswin_sdhci->phy;

	drv = phy->drive_impedance << PHY_PAD_SP_DRIVE_SHIF;

	val = sdhci_readw(host, VENDOR_EMMC_CTRL_R);
	val |= EMMC_CRAD_PRESENT;
	sdhci_writew(host, val, VENDOR_EMMC_CTRL_R);

	eswin_sdhci_disable_card_clk(host);

	/* reset phy,config phy's pad */
	sdhci_writel(host, drv | (~PHY_RSTN), PHY_CNFG_R);
	/*CMDPAD_CNFS*/
	val = (PHY_SLEW_2 << PHY_TX_SLEW_CTRL_P_BIT_SHIFT) |
		(PHY_SLEW_2 << PHY_TX_SLEW_CTRL_N_BIT_SHIFT) |
		(phy->enable_cmd_pullup << PHY_PULL_BIT_SHIF) | PHY_PAD_RXSEL_1;
	sdhci_writew(host, val, PHY_CMDPAD_CNFG_R);

	/*DATA PAD CNFG*/
	val = (PHY_SLEW_2 << PHY_TX_SLEW_CTRL_P_BIT_SHIFT) |
		(PHY_SLEW_2 << PHY_TX_SLEW_CTRL_N_BIT_SHIFT) |
		(phy->enable_data_pullup << PHY_PULL_BIT_SHIF) | PHY_PAD_RXSEL_1;
	sdhci_writew(host, val, PHY_DATAPAD_CNFG_R);

	/*Clock PAD Setting*/
	val = (PHY_SLEW_2 << PHY_TX_SLEW_CTRL_P_BIT_SHIFT) |
		(PHY_SLEW_2 << PHY_TX_SLEW_CTRL_N_BIT_SHIFT) | PHY_PAD_RXSEL_0;
	sdhci_writew(host, val, PHY_CLKPAD_CNFG_R);

	/*PHY strobe PAD setting*/
	val = (PHY_SLEW_2 << PHY_TX_SLEW_CTRL_P_BIT_SHIFT) |
		(PHY_SLEW_2 << PHY_TX_SLEW_CTRL_N_BIT_SHIFT) |
		((phy->enable_strobe_pulldown * PHY_PULL_DOWN) << PHY_PULL_BIT_SHIF) |
		PHY_PAD_RXSEL_1;
	sdhci_writew(host, val, PHY_STBPAD_CNFG_R);
	mdelay(2);

	/*PHY RSTN PAD setting*/
	val = (PHY_SLEW_2 << PHY_TX_SLEW_CTRL_P_BIT_SHIFT) |
		(PHY_SLEW_2 << PHY_TX_SLEW_CTRL_N_BIT_SHIFT) |
		(PHY_PULL_UP << PHY_PULL_BIT_SHIF) | PHY_PAD_RXSEL_1;
	sdhci_writew(host, val, PHY_RSTNPAD_CNFG_R);

	sdhci_writel(host, drv | PHY_RSTN, PHY_CNFG_R);

	eswin_sdhci_config_phy_delay(host, phy->delay_code);

	eswin_sdhci_enable_card_clk(host);
}

static void eswin_sdhci_reset(struct sdhci_host *host, u8 mask)
{
	u8 ctrl;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct eswin_sdhci_data *eswin_sdhci = sdhci_pltfm_priv(pltfm_host);

	/* Disable signal and interrupts before resetting the phy.
	 * Doing this avoids ISR to serve any undesired interrupts during
	 * reset and avoid producing the fake register dump during probe.
	 */
	sdhci_writel(host, 0, SDHCI_INT_ENABLE);
	sdhci_writel(host, 0, SDHCI_SIGNAL_ENABLE);
	sdhci_reset(host, mask);
	sdhci_writel(host, host->ier, SDHCI_INT_ENABLE);
	sdhci_writel(host, host->ier, SDHCI_SIGNAL_ENABLE);

	if (eswin_sdhci->quirks & SDHCI_ESWIN_QUIRK_FORCE_CDTEST) {
		ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
		ctrl |= SDHCI_CTRL_CDTEST_INS | SDHCI_CTRL_CDTEST_EN;
		sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);
	}

	if (mask == SDHCI_RESET_ALL) {
		/* after reset all,the phy`s config will be clear. */
		eswin_sdhci_config_phy(host);
	}
}

static u32 eswin_sdhci_cqhci_irq(struct sdhci_host *host, u32 intmask)
{
	int cmd_error = 0;
	int data_error = 0;

	if (!sdhci_cqe_irq(host, intmask, &cmd_error, &data_error))
		return intmask;

	cqhci_irq(host->mmc, intmask, cmd_error, data_error);

	return 0;
}

static void eswin_sdhci_dumpregs(struct mmc_host *mmc)
{
	sdhci_dumpregs(mmc_priv(mmc));
}

static void eswin_sdhci_cqe_enable(struct mmc_host *mmc)
{
	struct sdhci_host *host = mmc_priv(mmc);
	u32 reg;

	reg = sdhci_readl(host, SDHCI_PRESENT_STATE);
	while (reg & SDHCI_DATA_AVAILABLE) {
		sdhci_readl(host, SDHCI_BUFFER);
		reg = sdhci_readl(host, SDHCI_PRESENT_STATE);
	}

	sdhci_cqe_enable(mmc);
}

static int eswin_sdhci_delay_tuning(struct sdhci_host *host, u32 opcode)
{
	int ret;
	int delay = 0;
	int i = 0;
	int delay_min = -1;
	int delay_max = -1;
	int cmd_error = 0;
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct eswin_sdhci_data *eswin_sdhci = sdhci_pltfm_priv(pltfm_host);

	for (i = 0; i <= PHY_DELAY_CODE_MAX; i++) {
		eswin_sdhci_disable_card_clk(host);
		eswin_sdhci_config_phy_delay(host, i);
		eswin_sdhci_enable_card_clk(host);
		ret = mmc_send_tuning(host->mmc, opcode, &cmd_error);
		if (ret) {
			host->ops->reset(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);
			udelay(200);
			if (delay_min != -1 && delay_max != -1)
				break;
		} else {
			if (delay_min == -1) {
				delay_min = i;
				continue;
			} else {
				delay_max = i;
				continue;
			}
		}
	}
	if (delay_min == -1 && delay_max == -1) {
		pr_err("%s: delay code tuning failed!\n",
					mmc_hostname(host->mmc));
		eswin_sdhci_disable_card_clk(host);
		eswin_sdhci_config_phy_delay(host, eswin_sdhci->phy.delay_code);
		eswin_sdhci_enable_card_clk(host);

		return ret;
	}

	delay = (delay_min + delay_max) / 2;
	pr_debug("%s: set delay:0x%x\n", mmc_hostname(host->mmc), delay);
	eswin_sdhci_disable_card_clk(host);
	eswin_sdhci_config_phy_delay(host, delay);
	eswin_sdhci_enable_card_clk(host);

	return 0;
}

static int eswin_sdhci_phase_code_tuning(struct sdhci_host *host, u32 opcode)
{
	int cmd_error = 0;
	int ret = 0;
	int phase_code = 0;
	int code_min = -1;
	int code_max = -1;

	for (phase_code = 0; phase_code <= MAX_PHASE_CODE; phase_code++) {
		eswin_sdhci_disable_card_clk(host);
		sdhci_writew(host, phase_code, VENDOR_AT_SATA_R);
		eswin_sdhci_enable_card_clk(host);

		ret = mmc_send_tuning(host->mmc, opcode, &cmd_error);
		if (ret) {
			host->ops->reset(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);
			udelay(200);
			if (code_min != -1 && code_max != -1)
				break;
		} else {
			if (code_min == -1) {
				code_min = phase_code;
				continue;
			} else {
				code_max = phase_code;
				continue;
			}
		}
	}
	if (code_min == -1 && code_max == -1) {
		pr_err("%s: phase code tuning failed!\n",
					mmc_hostname(host->mmc));
		eswin_sdhci_disable_card_clk(host);
		sdhci_writew(host, 0, VENDOR_AT_SATA_R);
		eswin_sdhci_enable_card_clk(host);
		return -EIO;
	}

	phase_code = (code_min + code_max) / 2;
	pr_debug("%s: set phase_code:0x%x\n", mmc_hostname(host->mmc), phase_code);

	eswin_sdhci_disable_card_clk(host);
	sdhci_writew(host, phase_code, VENDOR_AT_SATA_R);
	eswin_sdhci_enable_card_clk(host);

	return 0;
}

static int eswin_sdhci_executing_tuning(struct sdhci_host *host, u32 opcode)
{
	u32 ctrl;
	u32 val;
	int ret = 0;

	eswin_sdhci_disable_card_clk(host);

	ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	ctrl &= ~SDHCI_CTRL_TUNED_CLK;
	sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);

	val = sdhci_readl(host, VENDOR_AT_CTRL_R);
	val |= SW_TUNE_ENABLE;
	sdhci_writew(host, val, VENDOR_AT_CTRL_R);
	sdhci_writew(host, 0, VENDOR_AT_SATA_R);

	eswin_sdhci_enable_card_clk(host);

	sdhci_writew(host, 0x0, SDHCI_CMD_DATA);

	ret = eswin_sdhci_delay_tuning(host, opcode);
	if (ret < 0) {
		return ret;
	}
	ret = eswin_sdhci_phase_code_tuning(host, opcode);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

static void eswin_sdhci_set_uhs_signaling(struct sdhci_host *host, unsigned timing)
{
	u32 val;
	u32 status;
	u32 timeout = 0;
	u16 ctrl_2;

	ctrl_2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	/* Select Bus Speed Mode for host */
	ctrl_2 &= ~SDHCI_CTRL_UHS_MASK;
	if ((timing == MMC_TIMING_MMC_HS200) ||
	    (timing == MMC_TIMING_UHS_SDR104))
		ctrl_2 |= SDHCI_CTRL_UHS_SDR104;
	else if (timing == MMC_TIMING_UHS_SDR12)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR12;
	else if (timing == MMC_TIMING_UHS_SDR25)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR25;
	else if (timing == MMC_TIMING_UHS_SDR50)
		ctrl_2 |= SDHCI_CTRL_UHS_SDR50;
	else if ((timing == MMC_TIMING_UHS_DDR50) ||
		 (timing == MMC_TIMING_MMC_DDR52))
		ctrl_2 |= SDHCI_CTRL_UHS_DDR50;
	else if (timing == MMC_TIMING_MMC_HS400)
		ctrl_2 |= ESWIN_SDHCI_CTRL_HS400; /* Non-standard */
	sdhci_writew(host, ctrl_2, SDHCI_HOST_CONTROL2);

	/*
	 * here need make dll locked when in hs400 at 200MHz
	 */
	if ((timing == MMC_TIMING_MMC_HS400) && (host->clock == 200000000)) {
		eswin_sdhci_disable_card_clk(host);

		val = sdhci_readl(host, VENDOR_AT_CTRL_R);
		val &= ~(LATENCY_LT_MASK << LATENCY_LT_BIT_OFFSET);
		val |= (LATENCY_LT_3 << LATENCY_LT_MASK);
		sdhci_writel(host, val, VENDOR_AT_CTRL_R);

		sdhci_writeb(host, 0x23, PHY_DLL_CNFG1_R);
		sdhci_writeb(host, 0x02, PHY_DLL_CNFG2_R);
		sdhci_writeb(host, 0x60, PHY_DLLDL_CNFG_R);
		sdhci_writeb(host, 0x00, PHY_DLL_OFFST_R);
		sdhci_writew(host, 0xffff, PHY_DLLBT_CNFG_R);

		eswin_sdhci_enable_card_clk(host);
		sdhci_writeb(host, DLL_ENABEL, PHY_DLL_CTRL_R);
		udelay(100);

		while (1) {
			status = sdhci_readb(host, PHY_DLL_STATUS_R);
			if (status & DLL_LOCK_STS) {
				pr_debug("%s: locked status:0x%x\n", mmc_hostname(host->mmc), status);
				break;
			}
			timeout++;
			udelay(100);
			if (timeout > 10000) {
				pr_err("%s: DLL lock failed!status:0x%x\n",
					mmc_hostname(host->mmc), status);
				return;
			}
		}

		status = sdhci_readb(host, PHY_DLL_STATUS_R);
		if (status & DLL_ERROR_STS) {
			pr_err("%s: DLL lock failed!err_status:0x%x\n",
				mmc_hostname(host->mmc), status);
		} else {
			pr_debug("%s: DLL lock is success\n", mmc_hostname(host->mmc));
		}
	}
}

static const struct cqhci_host_ops eswin_sdhci_cqhci_ops = {
	.enable = eswin_sdhci_cqe_enable,
	.disable = sdhci_cqe_disable,
	.dumpregs = eswin_sdhci_dumpregs,
};

static const struct sdhci_ops eswin_sdhci_cqe_ops = {
	.set_clock = eswin_sdhci_set_clock,
	.get_max_clock = sdhci_pltfm_clk_get_max_clock,
	.get_timeout_clock = sdhci_pltfm_clk_get_max_clock,
	.set_bus_width = sdhci_set_bus_width,
	.reset = eswin_sdhci_reset,
	.set_uhs_signaling = eswin_sdhci_set_uhs_signaling,
	.set_power = sdhci_set_power_and_bus_voltage,
	.irq = eswin_sdhci_cqhci_irq,
	.platform_execute_tuning = eswin_sdhci_executing_tuning,
};

static const struct sdhci_pltfm_data eswin_sdhci_cqe_pdata = {
	.ops = &eswin_sdhci_cqe_ops,
	.quirks = SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN |
		SDHCI_QUIRK_BROKEN_TIMEOUT_VAL,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
#if defined(__DISABLE_HS200)
		SDHCI_QUIRK2_BROKEN_HS200 |
#endif
		SDHCI_QUIRK2_CLOCK_DIV_ZERO_BROKEN,
};

#ifdef CONFIG_PM_SLEEP
/**
 * eswin_sdhci_suspend - Suspend method for the driver
 * @dev:    Address of the device structure
 *
 * Put the device in a low power state.
 *
 * Return: 0 on success and error value on error
 */
static int eswin_sdhci_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct eswin_sdhci_data *eswin_sdhci = sdhci_pltfm_priv(pltfm_host);
	int ret;

	if (host->tuning_mode != SDHCI_TUNING_MODE_3)
		mmc_retune_needed(host->mmc);

	ret = sdhci_suspend_host(host);
	if (ret)
		return ret;

	eic7700_tbu_power(dev, false);
	clk_disable_unprepare(pltfm_host->clk);
	clk_disable_unprepare(eswin_sdhci->clk_ahb);

	return 0;
}

/**
 * eswin_sdhci_resume - Resume method for the driver
 * @dev:    Address of the device structure
 *
 * Resume operation after suspend
 *
 * Return: 0 on success and error value on error
 */
static int eswin_sdhci_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct eswin_sdhci_data *eswin_sdhci = sdhci_pltfm_priv(pltfm_host);
	int ret;

	ret = clk_prepare_enable(eswin_sdhci->clk_ahb);
	if (ret) {
		dev_err(dev, "Cannot enable AHB clock.\n");
		return ret;
	}

	ret = clk_prepare_enable(pltfm_host->clk);
	if (ret) {
		dev_err(dev, "Cannot enable SD clock.\n");
		goto clk_ahb_disable;
	}

	eic7700_tbu_power(dev, true);

	ret = sdhci_resume_host(host);
	if (ret) {
		dev_err(dev, "Cannot resume host.\n");
		goto clk_disable;
	}

	return 0;
clk_disable:
	clk_disable_unprepare(pltfm_host->clk);
clk_ahb_disable:
	clk_disable_unprepare(eswin_sdhci->clk_ahb);

	return ret;
}
#endif /* ! CONFIG_PM_SLEEP */

/**
 * eswin_sdhci_sdcardclk_recalc_rate - Return the card clock rate
 *
 * @hw:         Pointer to the hardware clock structure.
 * @parent_rate:        The parent rate (should be rate of clk_xin).
 *
 * Return the current actual rate of the SD card clock.  This can be used
 * to communicate with out PHY.
 *
 * Return: The card clock rate.
 */
static unsigned long
eswin_sdhci_sdcardclk_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct eswin_sdhci_clk_data *clk_data =
		container_of(hw, struct eswin_sdhci_clk_data, sdcardclk_hw);
	struct eswin_sdhci_data *eswin_sdhci =
		container_of(clk_data, struct eswin_sdhci_data, clk_data);
	struct sdhci_host *host = eswin_sdhci->host;

	return host->mmc->actual_clock;
}

static const struct clk_ops eswin_sdcardclk_ops = {
	.recalc_rate = eswin_sdhci_sdcardclk_recalc_rate,
};

/**
 * eswin_sdhci_sampleclk_recalc_rate - Return the sampling clock rate
 *
 * @hw:         Pointer to the hardware clock structure.
 * @parent_rate:        The parent rate (should be rate of clk_xin).
 *
 * Return the current actual rate of the sampling clock.  This can be used
 * to communicate with out PHY.
 *
 * Return: The sample clock rate.
 */
static unsigned long
eswin_sdhci_sampleclk_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
	struct eswin_sdhci_clk_data *clk_data =
		container_of(hw, struct eswin_sdhci_clk_data, sampleclk_hw);
	struct eswin_sdhci_data *eswin_sdhci =
		container_of(clk_data, struct eswin_sdhci_data, clk_data);
	struct sdhci_host *host = eswin_sdhci->host;

	return host->mmc->actual_clock;
}

static const struct clk_ops eswin_sampleclk_ops = {
	.recalc_rate = eswin_sdhci_sampleclk_recalc_rate,
};

static const struct eswin_sdhci_clk_ops eswin_clk_ops = {
	.sdcardclk_ops = &eswin_sdcardclk_ops,
	.sampleclk_ops = &eswin_sampleclk_ops,
};

static struct eswin_sdhci_of_data eswin_sdhci_fu800_data = {
	.pdata = &eswin_sdhci_cqe_pdata,
	.clk_ops = &eswin_clk_ops,
};

static const struct of_device_id eswin_sdhci_of_match[] = {
	{
		.compatible = "eswin,emmc-sdhci-5.1",
		.data = &eswin_sdhci_fu800_data,
	},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, eswin_sdhci_of_match);

/**
 * eswin_sdhci_register_sdcardclk - Register the sdcardclk for a PHY to use
 *
 * @eswin_sdhci:    Our private data structure.
 * @clk_xin:        Pointer to the functional clock
 * @dev:        Pointer to our struct device.
 *
 * Some PHY devices need to know what the actual card clock is.  In order for
 * them to find out, we'll provide a clock through the common clock framework
 * for them to query.
 *
 * Return: 0 on success and error value on error
 */
static int eswin_sdhci_register_sdcardclk(struct eswin_sdhci_data *eswin_sdhci,
									struct clk *clk_xin, struct device *dev)
{
	struct eswin_sdhci_clk_data *clk_data = &eswin_sdhci->clk_data;
	struct device_node *np = dev->of_node;
	struct clk_init_data sdcardclk_init;
	const char *parent_clk_name;
	int ret;

	ret = of_property_read_string_index(np, "clock-output-names", 0,
						&sdcardclk_init.name);
	if (ret) {
		dev_err(dev, "DT has #clock-cells but no clock-output-names\n");
		return ret;
	}

	parent_clk_name = __clk_get_name(clk_xin);
	sdcardclk_init.parent_names = &parent_clk_name;
	sdcardclk_init.num_parents = 1;
	sdcardclk_init.flags = CLK_GET_RATE_NOCACHE;
	sdcardclk_init.ops = eswin_sdhci->clk_ops->sdcardclk_ops;

	clk_data->sdcardclk_hw.init = &sdcardclk_init;
	clk_data->sdcardclk = devm_clk_register(dev, &clk_data->sdcardclk_hw);
	if (IS_ERR(clk_data->sdcardclk))
		return PTR_ERR(clk_data->sdcardclk);
	clk_data->sdcardclk_hw.init = NULL;

	ret = of_clk_add_provider(np, of_clk_src_simple_get,
							clk_data->sdcardclk);
	if (ret)
		dev_err(dev, "Failed to add sdcard clock provider\n");

	return ret;
}

/**
 * eswin_sdhci_register_sampleclk - Register the sampleclk for a PHY to use
 *
 * @eswin_sdhci:    Our private data structure.
 * @clk_xin:        Pointer to the functional clock
 * @dev:        Pointer to our struct device.
 *
 * Some PHY devices need to know what the actual card clock is.  In order for
 * them to find out, we'll provide a clock through the common clock framework
 * for them to query.
 *
 * Return: 0 on success and error value on error
 */
static int eswin_sdhci_register_sampleclk(struct eswin_sdhci_data *eswin_sdhci,
						struct clk *clk_xin, struct device *dev)
{
	struct eswin_sdhci_clk_data *clk_data = &eswin_sdhci->clk_data;
	struct device_node *np = dev->of_node;
	struct clk_init_data sampleclk_init;
	const char *parent_clk_name;
	int ret;

	ret = of_property_read_string_index(np, "clock-output-names", 1,
						&sampleclk_init.name);
	if (ret) {
		dev_err(dev, "DT has #clock-cells but no clock-output-names\n");
		return ret;
	}

	parent_clk_name = __clk_get_name(clk_xin);
	sampleclk_init.parent_names = &parent_clk_name;
	sampleclk_init.num_parents = 1;
	sampleclk_init.flags = CLK_GET_RATE_NOCACHE;
	sampleclk_init.ops = eswin_sdhci->clk_ops->sampleclk_ops;

	clk_data->sampleclk_hw.init = &sampleclk_init;
	clk_data->sampleclk = devm_clk_register(dev, &clk_data->sampleclk_hw);
	if (IS_ERR(clk_data->sampleclk))
		return PTR_ERR(clk_data->sampleclk);
	clk_data->sampleclk_hw.init = NULL;

	ret = of_clk_add_provider(np, of_clk_src_simple_get,
				  clk_data->sampleclk);
	if (ret)
		dev_err(dev, "Failed to add sample clock provider\n");

	return ret;
}

/**
 * eswin_sdhci_unregister_sdclk - Undoes eswin_sdhci_register_sdclk()
 *
 * @dev:        Pointer to our struct device.
 *
 * Should be called any time we're exiting and eswin_sdhci_register_sdclk()
 * returned success.
 */
static void eswin_sdhci_unregister_sdclk(struct device *dev)
{
	struct device_node *np = dev->of_node;

	if (!of_find_property(np, "#clock-cells", NULL))
		return;

	of_clk_del_provider(dev->of_node);
}

/**
 * eswin_sdhci_register_sdclk - Register the sdcardclk for a PHY to use
 *
 * @eswin_sdhci:    Our private data structure.
 * @clk_xin:        Pointer to the functional clock
 * @dev:        Pointer to our struct device.
 *
 * Some PHY devices need to know what the actual card clock is.  In order for
 * them to find out, we'll provide a clock through the common clock framework
 * for them to query.
 *
 * Note: without seriously re-architecting SDHCI's clock code and testing on
 * all platforms, there's no way to create a totally beautiful clock here
 * with all clock ops implemented.  Instead, we'll just create a clock that can
 * be queried and set the CLK_GET_RATE_NOCACHE attribute to tell common clock
 * framework that we're doing things behind its back.  This should be sufficient
 * to create nice clean device tree bindings and later (if needed) we can try
 * re-architecting SDHCI if we see some benefit to it.
 *
 * Return: 0 on success and error value on error
 */
static int eswin_sdhci_register_sdclk(struct eswin_sdhci_data *eswin_sdhci,
				      struct clk *clk_xin, struct device *dev)
{
	struct device_node *np = dev->of_node;
	u32 num_clks = 0;
	int ret;

	/* Providing a clock to the PHY is optional; no error if missing */
	if (of_property_read_u32(np, "#clock-cells", &num_clks) < 0)
		return 0;

	ret = eswin_sdhci_register_sdcardclk(eswin_sdhci, clk_xin, dev);
	if (ret)
		return ret;

	if (num_clks) {
		ret = eswin_sdhci_register_sampleclk(eswin_sdhci, clk_xin, dev);
		if (ret) {
			eswin_sdhci_unregister_sdclk(dev);
			return ret;
		}
	}

	return 0;
}

static int eswin_sdhci_add_host(struct eswin_sdhci_data *eswin_sdhci)
{
	struct sdhci_host *host = eswin_sdhci->host;
	struct cqhci_host *cq_host;
	bool dma64;
	int ret;

	if (!eswin_sdhci->has_cqe)
		return sdhci_add_host(host);

	ret = sdhci_setup_host(host);
	if (ret)
		return ret;

	cq_host = devm_kzalloc(host->mmc->parent, sizeof(*cq_host), GFP_KERNEL);
	if (!cq_host) {
		ret = -ENOMEM;
		goto cleanup;
	}

	cq_host->mmio = host->ioaddr + eswin_sdhci_CQE_BASE_ADDR;
	cq_host->ops = &eswin_sdhci_cqhci_ops;

	dma64 = host->flags & SDHCI_USE_64_BIT_DMA;
	if (dma64)
		cq_host->caps |= CQHCI_TASK_DESC_SZ_128;

	ret = cqhci_init(cq_host, host->mmc, dma64);
	if (ret)
		goto cleanup;

	ret = __sdhci_add_host(host);
	if (ret)
		goto cleanup;

	return 0;

cleanup:
	sdhci_cleanup_host(host);
	return ret;
}

static int eswin_emmc_sid_cfg(struct device *dev)
{
	int ret;
	struct regmap *regmap;
	int hsp_mmu_emmc_reg;
	u32 rdwr_sid_ssid;
	u32 sid;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(dev);

	/* not behind smmu, use the default reset value(0x0) of the reg as streamID*/
	if (fwspec == NULL) {
		dev_dbg(dev,
			"dev is not behind smmu, skip configuration of sid\n");
		return 0;
	}
	sid = fwspec->ids[0];

	regmap = syscon_regmap_lookup_by_phandle(dev->of_node,
						 "eswin,hsp_sp_csr");
	if (IS_ERR(regmap)) {
		dev_dbg(dev, "No hsp_sp_csr phandle specified\n");
		return 0;
	}

	ret = of_property_read_u32_index(dev->of_node, "eswin,hsp_sp_csr", 1,
					 &hsp_mmu_emmc_reg);
	if (ret) {
		dev_err(dev, "can't get emmc sid cfg reg offset (%d)\n", ret);
		return ret;
	}

	/* make the reading sid the same as writing sid, ssid is fixed to zero */
	rdwr_sid_ssid = FIELD_PREP(AWSMMUSID, sid);
	rdwr_sid_ssid |= FIELD_PREP(ARSMMUSID, sid);
	rdwr_sid_ssid |= FIELD_PREP(AWSMMUSSID, 0);
	rdwr_sid_ssid |= FIELD_PREP(ARSMMUSSID, 0);
	regmap_write(regmap, hsp_mmu_emmc_reg, rdwr_sid_ssid);

	ret = eic7700_dynm_sid_enable(dev_to_node(dev));
	if (ret < 0)
		dev_err(dev, "failed to config emmc streamID(%d)!\n", sid);
	else
		dev_dbg(dev, "success to config emmc streamID(%d)!\n", sid);

	return ret;
}

static int eswin_sdhci_probe(struct platform_device *pdev)
{
	int ret;
	struct clk *clk_xin;
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;
	struct device *dev = &pdev->dev;
	struct eswin_sdhci_data *eswin_sdhci;
	const struct eswin_sdhci_of_data *data;
	unsigned int val = 0;

	data = of_device_get_match_data(dev);
	host = sdhci_pltfm_init(pdev, data->pdata, sizeof(*eswin_sdhci));
	if (IS_ERR(host))
		return PTR_ERR(host);

	pltfm_host = sdhci_priv(host);
	eswin_sdhci = sdhci_pltfm_priv(pltfm_host);
	eswin_sdhci->host = host;
	eswin_sdhci->clk_ops = data->clk_ops;

	eswin_sdhci->clk_ahb = devm_clk_get(dev, "clk_ahb");
	if (IS_ERR(eswin_sdhci->clk_ahb)) {
		ret = dev_err_probe(dev, PTR_ERR(eswin_sdhci->clk_ahb),
				    "clk_ahb clock not found.\n");
		goto err_pltfm_free;
	}

	clk_xin = devm_clk_get(dev, "clk_xin");
	if (IS_ERR(clk_xin)) {
		ret = dev_err_probe(dev, PTR_ERR(clk_xin),
				    "clk_xin clock not found.\n");
		goto err_pltfm_free;
	}

	ret = clk_prepare_enable(eswin_sdhci->clk_ahb);
	if (ret) {
		dev_err(dev, "Unable to enable AHB clock.\n");
		goto err_pltfm_free;
	}

	ret = clk_prepare_enable(clk_xin);
	if (ret) {
		dev_err(dev, "Unable to enable SD clock.\n");
		goto clk_dis_ahb;
	}

	ret = eswin_sdhci_reset_init(dev, eswin_sdhci);
	if (ret < 0) {
		dev_err(dev, "failed to reset\n");
		goto clk_disable_all;
	}

	eic7700_tbu_power(dev, true);

	eswin_sdhci->crg_regmap = syscon_regmap_lookup_by_phandle(pdev->dev.of_node, "eswin,syscrg_csr");
	if (IS_ERR(eswin_sdhci->crg_regmap)){
		dev_dbg(&pdev->dev, "No syscrg_csr phandle specified\n");
		goto clk_disable_all;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "eswin,syscrg_csr", 1,
                                    &eswin_sdhci->crg_core_clk);
	if (ret) {
		dev_err(&pdev->dev, "can't get crg_core_clk (%d)\n", ret);
		goto clk_disable_all;
	}
	ret = of_property_read_u32_index(pdev->dev.of_node, "eswin,syscrg_csr", 2,
                                    &eswin_sdhci->crg_aclk_ctrl);
	if (ret) {
		dev_err(&pdev->dev, "can't get crg_aclk_ctrl (%d)\n", ret);
		goto clk_disable_all;
	}
	ret = of_property_read_u32_index(pdev->dev.of_node, "eswin,syscrg_csr", 3,
                                    &eswin_sdhci->crg_cfg_ctrl);
	if (ret) {
		dev_err(&pdev->dev, "can't get crg_cfg_ctrl (%d)\n", ret);
		goto clk_disable_all;
	}

	eswin_sdhci->hsp_regmap = syscon_regmap_lookup_by_phandle(dev->of_node,
						 "eswin,hsp_sp_csr");
	if (IS_ERR(eswin_sdhci->hsp_regmap)) {
		dev_dbg(dev, "No hsp_sp_csr phandle specified\n");
		goto clk_disable_all;
	}

	ret = of_property_read_u32_index(pdev->dev.of_node, "eswin,hsp_sp_csr", 2,
                                    &eswin_sdhci->hsp_int_status);
	if (ret) {
		dev_err(&pdev->dev, "can't get hsp_int_status (%d)\n", ret);
		goto clk_disable_all;
	}
	ret = of_property_read_u32_index(pdev->dev.of_node, "eswin,hsp_sp_csr", 3,
                                    &eswin_sdhci->hsp_pwr_ctrl);
	if (ret) {
		dev_err(&pdev->dev, "can't get hsp_pwr_ctrl (%d)\n", ret);
		goto clk_disable_all;
	}

	regmap_write(eswin_sdhci->hsp_regmap, eswin_sdhci->hsp_int_status, MSHC_INT_CLK_STABLE);
	regmap_write(eswin_sdhci->hsp_regmap, eswin_sdhci->hsp_pwr_ctrl, MSHC_HOST_VAL_STABLE);

	/* smmu */
	eswin_emmc_sid_cfg(dev);

	if (!of_property_read_u32(dev->of_node, "delay_code", &val)) {
		eswin_sdhci->phy.delay_code = val;
	}

	if (!of_property_read_u32(dev->of_node, "drive-impedance-ohm", &val))
		eswin_sdhci->phy.drive_impedance =
			eswin_convert_drive_impedance_ohm(pdev, val);

	if (of_property_read_bool(dev->of_node, "enable-cmd-pullup"))
		eswin_sdhci->phy.enable_cmd_pullup = ENABLE;
	else
		eswin_sdhci->phy.enable_cmd_pullup = DISABLE;

	if (of_property_read_bool(dev->of_node, "enable-data-pullup"))
		eswin_sdhci->phy.enable_data_pullup = ENABLE;
	else
		eswin_sdhci->phy.enable_data_pullup = DISABLE;

	if (of_property_read_bool(dev->of_node, "enable-strobe-pulldown"))
		eswin_sdhci->phy.enable_strobe_pulldown = ENABLE;
	else
		eswin_sdhci->phy.enable_strobe_pulldown = DISABLE;

	sdhci_get_of_property(pdev);

	pltfm_host->clk = clk_xin;

	ret = eswin_sdhci_register_sdclk(eswin_sdhci, clk_xin, dev);
	if (ret)
		goto clk_disable_all;

	eswin_sdhci_dt_parse_clk_phases(dev, &eswin_sdhci->clk_data);

	ret = mmc_of_parse(host->mmc);
	if (ret) {
		ret = dev_err_probe(dev, ret, "parsing dt failed.\n");
		goto unreg_clk;
	}

	if (of_device_is_compatible(dev->of_node, "eswin,sdhci-5.1")) {
		host->mmc_host_ops.hs400_enhanced_strobe =
			eswin_sdhci_hs400_enhanced_strobe;
		eswin_sdhci->has_cqe = true;
		host->mmc->caps2 |= MMC_CAP2_CQE;

		if (!of_property_read_bool(dev->of_node, "disable-cqe-dcmd"))
			host->mmc->caps2 |= MMC_CAP2_CQE_DCMD;
	}

	sdhci_enable_v4_mode(eswin_sdhci->host);

	ret = eswin_sdhci_add_host(eswin_sdhci);
	if (ret)
		goto unreg_clk;

	return 0;

unreg_clk:
	eswin_sdhci_unregister_sdclk(dev);
clk_disable_all:
	clk_disable_unprepare(clk_xin);
clk_dis_ahb:
	clk_disable_unprepare(eswin_sdhci->clk_ahb);
err_pltfm_free:
	sdhci_pltfm_free(pdev);
	return ret;
}

static void eswin_sdhci_remove(struct platform_device *pdev)
{
	int ret;
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct eswin_sdhci_data *eswin_sdhci = sdhci_pltfm_priv(pltfm_host);
	struct clk *clk_ahb = eswin_sdhci->clk_ahb;

	sdhci_pltfm_remove(pdev);
	eic7700_tbu_power(&pdev->dev, false);

	if (eswin_sdhci->txrx_rst) {
		ret = reset_control_assert(eswin_sdhci->txrx_rst);
		WARN_ON(0 != ret);
	}

	if (eswin_sdhci->phy_rst) {
		ret = reset_control_assert(eswin_sdhci->phy_rst);
		WARN_ON(0 != ret);
	}

	if (eswin_sdhci->prstn) {
		ret = reset_control_assert(eswin_sdhci->prstn);
		WARN_ON(0 != ret);
	}

	if (eswin_sdhci->arstn) {
		ret = reset_control_assert(eswin_sdhci->arstn);
		WARN_ON(0 != ret);
	}
	eswin_sdhci_unregister_sdclk(&pdev->dev);
	clk_disable_unprepare(clk_ahb);
}

static void emmc_hard_reset(struct sdhci_host *host)
{
	unsigned int val;

	val = sdhci_readw(host, VENDOR_EMMC_CTRL_R);
	val |= EMMC_RST_N_OE;
	sdhci_writew(host, val, VENDOR_EMMC_CTRL_R);
	val &= ~EMMC_RST_N;
	sdhci_writew(host, val, VENDOR_EMMC_CTRL_R);
	mdelay(20);
	val |= EMMC_RST_N;
	sdhci_writew(host, val, VENDOR_EMMC_CTRL_R);
}

static void eswin_sdhci_shutdown(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);

	if (!host)
		return;

	emmc_hard_reset(host);
	host->ops->reset(host, SDHCI_RESET_ALL);
	platform_set_drvdata(pdev, NULL);
}

static const struct dev_pm_ops eswin_sdhci_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(eswin_sdhci_suspend, eswin_sdhci_resume)
};

static struct platform_driver eswin_sdhci_driver =
{
	.driver = {
		.name = "sdhci-eswin",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = eswin_sdhci_of_match,
		.pm = &eswin_sdhci_pmops,
	},
	.probe = eswin_sdhci_probe,
	.remove = eswin_sdhci_remove,
	.shutdown = eswin_sdhci_shutdown,
};

module_platform_driver(eswin_sdhci_driver);

MODULE_DESCRIPTION("Driver for the ESWIN SDHCI Controller");
MODULE_AUTHOR("Shuang Liang <liangshuang@eswincomputing.com>");
MODULE_LICENSE("GPL v2");
