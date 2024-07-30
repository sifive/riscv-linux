// SPDX-License-Identifier: GPL-2.0
/*
 * ESWIN SDHCI Driver
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
#ifndef _DRIVERS_MMC_SDHCI_ESWIN_H
#define _DRIVERS_MMC_SDHCI_ESWIN_H

#include <linux/reset.h>
#include <linux/clk-provider.h>
#include "sdhci-pltfm.h"

#define MSHC_CARD_CLK_STABLE BIT(28)
#define MSHC_INT_BCLK_STABLE BIT(16)
#define MSHC_INT_ACLK_STABLE BIT(8)
#define MSHC_INT_TMCLK_STABLE BIT(0)
#define MSHC_INT_CLK_STABLE                             \
	(MSHC_CARD_CLK_STABLE | MSHC_INT_ACLK_STABLE | \
	 MSHC_INT_BCLK_STABLE | MSHC_INT_TMCLK_STABLE)
#define MSHC_HOST_VAL_STABLE BIT(0)
#define EMMC0_CARD_DETECT BIT(9)
#define EMMC0_CARD_WRITE_PROT BIT(8)

#define MSHC_CORE_CLK_ENABLE BIT(16)
#define MSHC_CORE_CLK_FREQ_BIT_SHIFT 4
#define MSHC_CORE_CLK_FREQ_BIT_MASK 0xfffu
#define MSHC_CORE_CLK_SEL_BIT BIT(0)

/* Controller does not have CD wired and will not function normally without */
#define SDHCI_ESWIN_QUIRK_FORCE_CDTEST BIT(0)
/* Controller immediately reports SDHCI_CLOCK_INT_STABLE after enabling the
 * internal clock even when the clock isn't stable */
#define SDHCI_ESWIN_QUIRK_CLOCK_UNSTABLE BIT(1)

#define ESWIN_SDHCI_CTRL_HS400 0x0007 // Non-standard, for eswin,these bits are 0x7

#define SDHCI_CLK_208M   208000000
#define SDHCI_CLK_200M   200000000

#define AWSMMUSID GENMASK(31, 24)  // The sid of write operation
#define AWSMMUSSID GENMASK(23, 16)  // The ssid of write operation
#define ARSMMUSID GENMASK(15, 8)  // The sid of read operation
#define ARSMMUSSID GENMASK(7, 0)  // The ssid of read operation

/* DWC_mshc_map/DWC_mshc_phy_block register */
#define DWC_MSHC_PTR_PHY_R 0x300
#define PHY_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x00)
#define PHY_CMDPAD_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x04)
#define PHY_DATAPAD_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x06)
#define PHY_CLKPAD_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x08)
#define PHY_STBPAD_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x0a)
#define PHY_RSTNPAD_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x0c)
#define PHY_PADTEST_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x0e)
#define PHY_PADTEST_OUT_R (DWC_MSHC_PTR_PHY_R + 0x10)
#define PHY_PADTEST_IN_R (DWC_MSHC_PTR_PHY_R + 0x12)
#define PHY_PRBS_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x18)
#define PHY_PHYLBK_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x1a)
#define PHY_COMMDL_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x1c)
#define PHY_SDCLKDL_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x1d)
#define PHY_SDCLKDL_DC_R (DWC_MSHC_PTR_PHY_R + 0x1e)
#define PHY_SMPLDL_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x20)
#define PHY_ATDL_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x21)
#define PHY_DLL_CTRL_R (DWC_MSHC_PTR_PHY_R + 0x24)
#define PHY_DLL_CNFG1_R (DWC_MSHC_PTR_PHY_R + 0x25)
#define PHY_DLL_CNFG2_R (DWC_MSHC_PTR_PHY_R + 0x26)
#define PHY_DLLDL_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x28)
#define PHY_DLL_OFFST_R (DWC_MSHC_PTR_PHY_R + 0x29)
#define PHY_DLLMST_TSTDC_R (DWC_MSHC_PTR_PHY_R + 0x2a)
#define PHY_DLLBT_CNFG_R (DWC_MSHC_PTR_PHY_R + 0x2c)
#define PHY_DLL_STATUS_R (DWC_MSHC_PTR_PHY_R + 0x2e)
#define PHY_DLLDBG_MLKDC_R (DWC_MSHC_PTR_PHY_R + 0x30)
#define PHY_DLLDBG_SLKDC_R (DWC_MSHC_PTR_PHY_R + 0x32)

#define ENABLE 1
#define DISABLE 0
/* strength definition */
#define PHYCTRL_DR_33OHM 0xee
#define PHYCTRL_DR_40OHM 0xcc
#define PHYCTRL_DR_50OHM 0x88
#define PHYCTRL_DR_66OHM 0x44
#define PHYCTRL_DR_100OHM 0x00

#define PHY_PAD_MAX_DRIVE_STRENGTH 0xf
#define PHY_CLK_MAX_DELAY_MASK 0x7f
#define PHY_PAD_SP_DRIVE_SHIF 16
#define PHY_PAD_SN_DRIVE_SHIF 20

#define PHY_RSTN BIT(0)
#define PHY_UPDATE_DELAY_CODE BIT(4)

#define VENDOR_EMMC_CTRL_R 0x52c
#define EMMC_CRAD_PRESENT BIT(0)
#define EMMC_RST_N_OE BIT(3)
#define EMMC_RST_N  BIT(2)

#define PHY_SLEW_0 0x0
#define PHY_SLEW_1 0x1
#define PHY_SLEW_2 0x2
#define PHY_SLEW_3 0x3
#define PHY_TX_SLEW_CTRL_P_BIT_SHIFT 5
#define PHY_TX_SLEW_CTRL_N_BIT_SHIFT 9

#define PHY_PULL_BIT_SHIF 0x3
#define PHY_PULL_DISABLED 0x0
#define PHY_PULL_UP 0x1
#define PHY_PULL_DOWN 0x2
#define PHY_PULL_MASK 0x3

#define PHY_PAD_RXSEL_0 0x0
#define PHY_PAD_RXSEL_1 0x1

#define VENDOR_AT_CTRL_R 0x540
#define LATENCY_LT_BIT_OFFSET 19
#define LATENCY_LT_MASK 0x3

#define LATENCY_LT_1 0x0
#define LATENCY_LT_2 0x1
#define LATENCY_LT_3 0x2
#define LATENCY_LT_4 0x3
#define SW_TUNE_ENABLE BIT(4)

#define VENDOR_AT_SATA_R 0x544
#define MAX_PHASE_CODE 0xff

#define DLL_ENABEL BIT(0)
#define DLL_LOCK_STS BIT(0)
#define DLL_ERROR_STS BIT(1)
#define PHY_DELAY_CODE_MASK 0x7f
#define PHY_DELAY_CODE_MAX 0x7f

#define MAX_CORE_CLK_DIV 0xfff

/**
 * struct eswin_sdhci_clk_ops - Clock Operations for eswin SD controller
 *
 * @sdcardclk_ops:  The output clock related operations
 * @sampleclk_ops:  The sample clock related operations
 */
struct eswin_sdhci_clk_ops {
	const struct clk_ops *sdcardclk_ops;
	const struct clk_ops *sampleclk_ops;
};

/**
 * struct eswin_sdhci_clk_data - ESWIN Controller Clock Data.
 *
 * @sdcardclk_hw:   Struct for the clock we might provide to a PHY.
 * @sdcardclk:      Pointer to normal 'struct clock' for sdcardclk_hw.
 * @sampleclk_hw:   Struct for the clock we might provide to a PHY.
 * @sampleclk:      Pointer to normal 'struct clock' for sampleclk_hw.
 * @clk_phase_in:   Array of Input Clock Phase Delays for all speed modes
 * @clk_phase_out:  Array of Output Clock Phase Delays for all speed modes
 * @set_clk_delays: Function pointer for setting Clock Delays
 * @clk_of_data:    Platform specific runtime clock data storage pointer
 */
struct eswin_sdhci_clk_data {
	struct clk_hw sdcardclk_hw;
	struct clk *sdcardclk;
	struct clk_hw sampleclk_hw;
	struct clk *sampleclk;
	int clk_phase_in[MMC_TIMING_MMC_HS400 + 1];
	int clk_phase_out[MMC_TIMING_MMC_HS400 + 1];
	void (*set_clk_delays)(struct sdhci_host *host);
	void *clk_of_data;
};

struct eswin_sdhci_phy_data {
	unsigned int drive_impedance;
	unsigned int enable_strobe_pulldown;
	unsigned int enable_data_pullup;
	unsigned int enable_cmd_pullup;
	unsigned int delay_code;
};

/**
 * struct eswin_sdhci_data - ESWIN Controller Data
 *
 * @host:       Pointer to the main SDHCI host structure.
 * @clk_ahb:        Pointer to the AHB clock
 * @has_cqe:        True if controller has command queuing engine.
 * @clk_data:       Struct for the ESWIN Controller Clock Data.
 * @clk_ops:        Struct for the ESWIN Controller Clock Operations.
 * @soc_ctl_base:   Pointer to regmap for syscon for soc_ctl registers.
 * @soc_ctl_map:    Map to get offsets into soc_ctl registers.
 * @quirks:     ESWIN deviations from spec.
 * @phy:        ESWIN sdhci phy configs.
 * @private:    private for spec driver.
 */
struct eswin_sdhci_data {
	struct sdhci_host *host;
	struct clk *clk_ahb;
	bool has_cqe;
	struct eswin_sdhci_clk_data clk_data;
	const struct eswin_sdhci_clk_ops *clk_ops;
	unsigned int quirks;

	struct regmap *crg_regmap;
	unsigned int crg_core_clk;
	unsigned int crg_aclk_ctrl;
	unsigned int crg_cfg_ctrl;

	struct regmap *hsp_regmap;
	unsigned int hsp_int_status;
	unsigned int hsp_pwr_ctrl;

	struct reset_control *txrx_rst;
	struct reset_control *phy_rst;
	struct reset_control *prstn;
	struct reset_control *arstn;
	struct eswin_sdhci_phy_data phy;
	unsigned long private[] ____cacheline_aligned;
};

struct eswin_sdhci_of_data {
	const struct sdhci_pltfm_data *pdata;
	const struct eswin_sdhci_clk_ops *clk_ops;
};

void eswin_sdhci_set_core_clock(struct sdhci_host *host,
		unsigned int clock);
void eswin_sdhci_disable_card_clk(struct sdhci_host *host);
void eswin_sdhci_enable_card_clk(struct sdhci_host *host);
void eswin_sdhci_dt_parse_clk_phases(struct device *dev,
		struct eswin_sdhci_clk_data *clk_data);
unsigned int eswin_convert_drive_impedance_ohm(struct platform_device *pdev,
		unsigned int dr_ohm);
int eswin_sdhci_reset_init(struct device *dev,
		struct eswin_sdhci_data *eswin_sdhci);

#endif /* _DRIVERS_MMC_SDHCI_ESWIN_H */
