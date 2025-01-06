// SPDX-License-Identifier: GPL-2.0
/*
 * ESWIN SysCrg Definition
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

#ifndef __DT_ESWIN_EIC7700_SYSCRG_H__
#define __DT_ESWIN_EIC7700_SYSCRG_H__

/*REG OFFSET OF SYS-CRG*/

#define EIC7700_REG_OFFSET_SPLL0_CFG_0                          0X0000
#define EIC7700_REG_OFFSET_SPLL0_CFG_1                          0X0004
#define EIC7700_REG_OFFSET_SPLL0_CFG_2                          0X0008
#define EIC7700_REG_OFFSET_SPLL0_DSKEWCAL                       0X000C
#define EIC7700_REG_OFFSET_SPLL0_SSC                            0X0010
#define EIC7700_REG_OFFSET_SPLL1_CFG_0                          0X0014
#define EIC7700_REG_OFFSET_SPLL1_CFG_1                          0X0018
#define EIC7700_REG_OFFSET_SPLL1_CFG_2                          0X001C
#define EIC7700_REG_OFFSET_SPLL1_DSKEWCAL                       0X0020
#define EIC7700_REG_OFFSET_SPLL1_SSC                            0X0024
#define EIC7700_REG_OFFSET_SPLL2_CFG_0                          0X0028
#define EIC7700_REG_OFFSET_SPLL2_CFG_1                          0X002C
#define EIC7700_REG_OFFSET_SPLL2_CFG_2                          0X0030
#define EIC7700_REG_OFFSET_SPLL2_DSKEWCAL                       0X0034
#define EIC7700_REG_OFFSET_SPLL2_SSC                            0X0038
#define EIC7700_REG_OFFSET_VPLL_CFG_0                           0X003C
#define EIC7700_REG_OFFSET_VPLL_CFG_1                           0X0040
#define EIC7700_REG_OFFSET_VPLL_CFG_2                           0X0044
#define EIC7700_REG_OFFSET_VPLL_DSKEWCAL                        0X0048
#define EIC7700_REG_OFFSET_VPLL_SSC                             0X004C
#define EIC7700_REG_OFFSET_APLL_CFG_0                           0X0050
#define EIC7700_REG_OFFSET_APLL_CFG_1                           0X0054
#define EIC7700_REG_OFFSET_APLL_CFG_2                           0X0058
#define EIC7700_REG_OFFSET_APLL_DSKEWCAL                        0X005C
#define EIC7700_REG_OFFSET_APLL_SSC                             0X0060
#define EIC7700_REG_OFFSET_MCPUT_PLL_CFG_0                      0X0064
#define EIC7700_REG_OFFSET_MCPUT_PLL_CFG_1                      0X0068
#define EIC7700_REG_OFFSET_MCPUT_PLL_CFG_2                      0X006C
#define EIC7700_REG_OFFSET_MCPUT_PLL_DSKEWCAL                   0X0070
#define EIC7700_REG_OFFSET_MCPUT_PLL_SSC                        0X0074
#define EIC7700_REG_OFFSET_DDRT_PLL_CFG_0                       0X0078
#define EIC7700_REG_OFFSET_DDRT_PLL_CFG_1                       0X007C
#define EIC7700_REG_OFFSET_DDRT_PLL_CFG_2                       0X0080
#define EIC7700_REG_OFFSET_DDRT_PLL_DSKEWCAL                    0X0084
#define EIC7700_REG_OFFSET_DDRT_PLL_SSC                         0X0088
#define EIC7700_REG_OFFSET_PLL_STATUS                           0X00A4
#define EIC7700_REG_OFFSET_NOC_CLK_CTRL                         0X100
#define EIC7700_REG_OFFSET_BOOTSPI_CLK_CTRL                     0X104
#define EIC7700_REG_OFFSET_BOOTSPI_CFGCLK_CTRL                  0X108
#define EIC7700_REG_OFFSET_SCPU_CORECLK_CTRL                    0X10C
#define EIC7700_REG_OFFSET_SCPU_BUSCLK_CTRL                     0X110
#define EIC7700_REG_OFFSET_LPCPU_CORECLK_CTRL                   0X114
#define EIC7700_REG_OFFSET_LPCPU_BUSCLK_CTRL                    0X118
#define EIC7700_REG_OFFSET_TCU_ACLK_CTRL                        0X11C
#define EIC7700_REG_OFFSET_TCU_CFG_CTRL                         0X120
#define EIC7700_REG_OFFSET_DDR_CLK_CTRL                         0X124
#define EIC7700_REG_OFFSET_DDR1_CLK_CTRL                        0X128
#define EIC7700_REG_OFFSET_GPU_ACLK_CTRL                        0X12C
#define EIC7700_REG_OFFSET_GPU_CFG_CTRL                         0X130
#define EIC7700_REG_OFFSET_GPU_GRAY_CTRL                        0X134
#define EIC7700_REG_OFFSET_DSP_ACLK_CTRL                        0X138
#define EIC7700_REG_OFFSET_DSP_CFG_CTRL                         0X13C
#define EIC7700_REG_OFFSET_D2D_ACLK_CTRL                        0X140
#define EIC7700_REG_OFFSET_D2D_CFG_CTRL                         0X144
#define EIC7700_REG_OFFSET_HSP_ACLK_CTRL                        0X148
#define EIC7700_REG_OFFSET_HSP_CFG_CTRL                         0X14C
#define EIC7700_REG_OFFSET_SATA_RBC_CTRL                        0X150
#define EIC7700_REG_OFFSET_SATA_OOB_CTRL                        0X154
#define EIC7700_REG_OFFSET_ETH0_CTRL                            0X158
#define EIC7700_REG_OFFSET_ETH1_CTRL                            0X15C
#define EIC7700_REG_OFFSET_MSHC0_CORECLK_CTRL                   0X160
#define EIC7700_REG_OFFSET_MSHC1_CORECLK_CTRL                   0X164
#define EIC7700_REG_OFFSET_MSHC2_CORECLK_CTRL                   0X168
#define EIC7700_REG_OFFSET_MSHC_USB_SLWCLK                      0X16C
#define EIC7700_REG_OFFSET_PCIE_ACLK_CTRL                       0X170
#define EIC7700_REG_OFFSET_PCIE_CFG_CTRL                        0X174
#define EIC7700_REG_OFFSET_NPU_ACLK_CTRL                        0X178
#define EIC7700_REG_OFFSET_NPU_LLC_CTRL                         0X17C
#define EIC7700_REG_OFFSET_NPU_CORE_CTRL                        0X180
#define EIC7700_REG_OFFSET_VI_DWCLK_CTRL                        0X184
#define EIC7700_REG_OFFSET_VI_ACLK_CTRL                         0X188
#define EIC7700_REG_OFFSET_VI_DIG_ISP_CLK_CTRL                  0X18C
#define EIC7700_REG_OFFSET_VI_DVP_CLK_CTRL                      0X190
#define EIC7700_REG_OFFSET_VI_SHUTTER0                          0X194
#define EIC7700_REG_OFFSET_VI_SHUTTER1                          0X198
#define EIC7700_REG_OFFSET_VI_SHUTTER2                          0X19C
#define EIC7700_REG_OFFSET_VI_SHUTTER3                          0X1A0
#define EIC7700_REG_OFFSET_VI_SHUTTER4                          0X1A4
#define EIC7700_REG_OFFSET_VI_SHUTTER5                          0X1A8
#define EIC7700_REG_OFFSET_VI_PHY_CLKCTRL                       0X1AC
#define EIC7700_REG_OFFSET_VO_ACLK_CTRL                         0X1B0
#define EIC7700_REG_OFFSET_VO_IESMCLK_CTRL                      0X1B4
#define EIC7700_REG_OFFSET_VO_PIXEL_CTRL                        0X1B8
#define EIC7700_REG_OFFSET_VO_MCLK_CTRL                         0X1BC
#define EIC7700_REG_OFFSET_VO_PHY_CLKCTRL                       0X1C0
#define EIC7700_REG_OFFSET_VC_ACLK_CTRL                         0X1C4
#define EIC7700_REG_OFFSET_VCDEC_ROOTCLK_CTRL                   0X1C8
#define EIC7700_REG_OFFSET_G2D_CTRL                             0X1CC
#define EIC7700_REG_OFFSET_VC_CLKEN_CTRL                        0X1D0
#define EIC7700_REG_OFFSET_JE_CLK_CTRL                          0X1D4
#define EIC7700_REG_OFFSET_JD_CLK_CTRL                          0X1D8
#define EIC7700_REG_OFFSET_VD_CLK_CTRL                          0X1DC
#define EIC7700_REG_OFFSET_VE_CLK_CTRL                          0X1E0
#define EIC7700_REG_OFFSET_AON_DMA_CLK_CTRL                     0X1E4
#define EIC7700_REG_OFFSET_TIMER_CLK_CTRL                       0X1E8
#define EIC7700_REG_OFFSET_RTC_CLK_CTRL                         0X1EC
#define EIC7700_REG_OFFSET_PKA_CLK_CTRL                         0X1F0
#define EIC7700_REG_OFFSET_SPACC_CLK_CTRL                       0X1F4
#define EIC7700_REG_OFFSET_TRNG_CLK_CTRL                        0X1F8
#define EIC7700_REG_OFFSET_OTP_CLK_CTRL                         0X1FC
#define EIC7700_REG_OFFSET_LSP_CLK_EN0                          0X200
#define EIC7700_REG_OFFSET_LSP_CLK_EN1                          0X204
#define EIC7700_REG_OFFSET_U84_CLK_CTRL                         0X208
#define EIC7700_REG_OFFSET_SYSCFG_CLK_CTRL                      0X20C
#define EIC7700_REG_OFFSET_I2C0_CLK_CTRL                        0X210
#define EIC7700_REG_OFFSET_I2C1_CLK_CTRL                        0X214
#define EIC7700_REG_OFFSET_DFT_CLK_CTRL                         0X280
#define EIC7700_REG_OFFSET_SYS_SWRST_VALUE                      0X300
#define EIC7700_REG_OFFSET_CLR_RST_STATUS                       0X304
#define EIC7700_REG_OFFSET_DIE_STATUS                           0X308
#define EIC7700_REG_OFFSET_CLR_BOOT_INFO                        0X30C
#define EIC7700_REG_OFFSET_SCPU_BOOT_ADDRESS                    0X310
#define EIC7700_REG_OFFSET_LPCPU_BOOT_ADDRESS                   0X314
#define EIC7700_REG_OFFSET_NPUE31_BOOT_ADDRESS                  0X318
#define EIC7700_REG_OFFSET_U84_BOOT_ADDRESS0_HI                 0X31C
#define EIC7700_REG_OFFSET_U84_BOOT_ADDRESS0_LOW                0X320
#define EIC7700_REG_OFFSET_U84_BOOT_ADDRESS1_HI                 0X324
#define EIC7700_REG_OFFSET_U84_BOOT_ADDRESS1_LOW                0X328
#define EIC7700_REG_OFFSET_U84_BOOT_ADDRESS2_HI                 0X32C
#define EIC7700_REG_OFFSET_U84_BOOT_ADDRESS2_LOW                0X330
#define EIC7700_REG_OFFSET_U84_BOOT_ADDRESS3_HI                 0X334
#define EIC7700_REG_OFFSET_U84_BOOT_ADDRESS3_LOW                0X338
#define EIC7700_REG_OFFSET_BOOT_SEL_STAT                        0X33C
#define EIC7700_REG_OFFSET_BOOT_SPI_CFG                         0X340
#define EIC7700_REG_OFFSET_SNOC_RST_CTRL                        0X400
#define EIC7700_REG_OFFSET_GPU_RST_CTRL                         0X404
#define EIC7700_REG_OFFSET_DSP_RST_CTRL                         0X408
#define EIC7700_REG_OFFSET_D2D_RST_CTRL                         0X40C
#define EIC7700_REG_OFFSET_DDR_RST_CTRL                         0X410
#define EIC7700_REG_OFFSET_TCU_RST_CTRL                         0X414
#define EIC7700_REG_OFFSET_NPU_RST_CTRL                         0X418
#define EIC7700_REG_OFFSET_HSPDMA_RST_CTRL                      0X41C
#define EIC7700_REG_OFFSET_PCIE_RST_CTRL                        0X420
#define EIC7700_REG_OFFSET_I2C_RST_CTRL                         0X424
#define EIC7700_REG_OFFSET_FAN_RST_CTRL                         0X428
#define EIC7700_REG_OFFSET_PVT_RST_CTRL                         0X42C
#define EIC7700_REG_OFFSET_MBOX_RST_CTRL                        0X430
#define EIC7700_REG_OFFSET_UART_RST_CTRL                        0X434
#define EIC7700_REG_OFFSET_GPIO_RST_CTRL                        0X438
#define EIC7700_REG_OFFSET_TIMER_RST_CTRL                       0X43C
#define EIC7700_REG_OFFSET_SSI_RST_CTRL                         0X440
#define EIC7700_REG_OFFSET_WDT_RST_CTRL                         0X444
#define EIC7700_REG_OFFSET_LSP_CFGRST_CTRL                      0X448
#define EIC7700_REG_OFFSET_U84_RST_CTRL                         0X44C
#define EIC7700_REG_OFFSET_SCPU_RST_CTRL                        0X450
#define EIC7700_REG_OFFSET_LPCPU_RST_CTRL                       0X454
#define EIC7700_REG_OFFSET_VC_RST_CTRL                          0X458
#define EIC7700_REG_OFFSET_JD_RST_CTRL                          0X45C
#define EIC7700_REG_OFFSET_JE_RST_CTRL                          0X460
#define EIC7700_REG_OFFSET_VD_RST_CTRL                          0X464
#define EIC7700_REG_OFFSET_VE_RST_CTRL                          0X468
#define EIC7700_REG_OFFSET_G2D_RST_CTRL                         0X46C
#define EIC7700_REG_OFFSET_VI_RST_CTRL                          0X470
#define EIC7700_REG_OFFSET_DVP_RST_CTRL                         0X474
#define EIC7700_REG_OFFSET_ISP0_RST_CTRL                        0X478
#define EIC7700_REG_OFFSET_ISP1_RST_CTRL                        0X47C
#define EIC7700_REG_OFFSET_SHUTTER_RST_CTRL                     0X480
#define EIC7700_REG_OFFSET_VO_PHYRST_CTRL                       0X484
#define EIC7700_REG_OFFSET_VO_I2SRST_CTRL                       0X488
#define EIC7700_REG_OFFSET_VO_RST_CTRL                          0X48C
#define EIC7700_REG_OFFSET_BOOTSPI_RST_CTRL                     0X490
#define EIC7700_REG_OFFSET_I2C1_RST_CTRL                        0X494
#define EIC7700_REG_OFFSET_I2C0_RST_CTRL                        0X498
#define EIC7700_REG_OFFSET_DMA1_RST_CTRL                        0X49C
#define EIC7700_REG_OFFSET_FPRT_RST_CTRL                        0X4A0
#define EIC7700_REG_OFFSET_HBLOCK_RST_CTRL                      0X4A4
#define EIC7700_REG_OFFSET_SECSR_RST_CTRL                       0X4A8
#define EIC7700_REG_OFFSET_OTP_RST_CTRL                         0X4AC
#define EIC7700_REG_OFFSET_PKA_RST_CTRL                         0X4B0
#define EIC7700_REG_OFFSET_SPACC_RST_CTRL                       0X4B4
#define EIC7700_REG_OFFSET_TRNG_RST_CTRL                        0X4B8
#define EIC7700_REG_OFFSET_TIMER0_RST_CTRL                      0X4C0
#define EIC7700_REG_OFFSET_TIMER1_RST_CTRL                      0X4C4
#define EIC7700_REG_OFFSET_TIMER2_RST_CTRL                      0X4C8
#define EIC7700_REG_OFFSET_TIMER3_RST_CTRL                      0X4CC
#define EIC7700_REG_OFFSET_RTC_RST_CTRL                         0X4D0
#define EIC7700_REG_OFFSET_MNOC_RST_CTRL                        0X4D4
#define EIC7700_REG_OFFSET_RNOC_RST_CTRL                        0X4D8
#define EIC7700_REG_OFFSET_CNOC_RST_CTRL                        0X4DC
#define EIC7700_REG_OFFSET_LNOC_RST_CTRL                        0X4E0

/*
 * RESET DEV ID  FOR EACH RESET CONSUMER
 *
 */
#define		SNOC_RST_CTRL					0X00
#define		GPU_RST_CTRL					0X01
#define		DSP_RST_CTRL					0X02
#define		D2D_RST_CTRL					0X03
#define		DDR_RST_CTRL					0X04
#define		TCU_RST_CTRL					0X05
#define		NPU_RST_CTRL					0X06
#define		HSPDMA_RST_CTRL					0X07
#define		PCIE_RST_CTRL					0X08
#define		I2C_RST_CTRL					0X09
#define		FAN_RST_CTRL					0X0A
#define		PVT_RST_CTRL					0X0B
#define		MBOX_RST_CTRL					0X0C
#define		UART_RST_CTRL					0X0D
#define		GPIO_RST_CTRL					0X0E
#define		TIMER_RST_CTRL					0X0F
#define		SSI_RST_CTRL					0X10
#define		WDT_RST_CTRL					0X11
#define		LSP_CFGRST_CTRL					0X12
#define		U84_RST_CTRL					0X13
#define		SCPU_RST_CTRL					0X14
#define		LPCPU_RST_CTRL					0X15
#define		VC_RST_CTRL					0X16
#define		JD_RST_CTRL					0X17
#define		JE_RST_CTRL					0X18
#define		VD_RST_CTRL					0X19
#define		VE_RST_CTRL					0X1A
#define		G2D_RST_CTRL					0X1B
#define		VI_RST_CTRL					0X1C
#define		DVP_RST_CTRL					0X1D
#define		ISP0_RST_CTRL					0X1E
#define		ISP1_RST_CTRL					0X1F
#define		SHUTTER_RST_CTRL				0X20
#define		VO_PHYRST_CTRL					0X21
#define		VO_I2SRST_CTRL					0X22
#define		VO_RST_CTRL					0X23
#define		BOOTSPI_RST_CTRL				0X24
#define		I2C1_RST_CTRL					0X25
#define		I2C0_RST_CTRL					0X26
#define		DMA1_RST_CTRL					0X27
#define		FPRT_RST_CTRL					0X28
#define		HBLOCK_RST_CTRL					0X29
#define		SECSR_RST_CTRL					0X2A
#define		OTP_RST_CTRL					0X2B
#define		PKA_RST_CTRL					0X2C
#define		SPACC_RST_CTRL					0X2D
#define		TRNG_RST_CTRL					0X2E
#define		RESERVED					0X2F
#define		TIMER0_RST_CTRL					0X30
#define		TIMER1_RST_CTRL					0X31
#define		TIMER2_RST_CTRL					0X32
#define		TIMER3_RST_CTRL					0X33
#define		RTC_RST_CTRL					0X34
#define		MNOC_RST_CTRL					0X35
#define		RNOC_RST_CTRL					0X36
#define		CNOC_RST_CTRL					0X37
#define		LNOC_RST_CTRL					0X38

#define BIT0                              (1 << 0)
#define BIT1                              (1 << 1)
#define BIT2                              (1 << 2)
#define BIT3                              (1 << 3)
#define BIT4                              (1 << 4)
#define BIT5                              (1 << 5)
#define BIT6                              (1 << 6)
#define BIT7                              (1 << 7)
#define BIT8                              (1 << 8)
#define BIT9                              (1 << 9)
#define BIT10                             (1 << 10)
#define BIT11                             (1 << 11)
#define BIT12                             (1 << 12)
#define BIT13                             (1 << 13)
#define BIT14                             (1 << 14)
#define BIT15                             (1 << 15)
#define BIT16                             (1 << 16)
#define BIT17                             (1 << 17)
#define BIT18                             (1 << 18)
#define BIT19                             (1 << 19)
#define BIT20                             (1 << 20)
#define BIT21                             (1 << 21)
#define BIT22                             (1 << 22)
#define BIT23                             (1 << 23)
#define BIT24                             (1 << 24)
#define BIT25                             (1 << 25)
#define BIT26                             (1 << 26)
#define BIT27                             (1 << 27)
#define BIT28                             (1 << 28)
#define BIT29                             (1 << 29)
#define BIT30                             (1 << 30)
#define BIT31                             (1 << 31)

/*
	CONSUMER RESET CONTROL BIT
*/
/*SNOC*/
#define		SW_NOC_NSP_RSTN						BIT0
#define		SW_NOC_CFG_RSTN						BIT1
#define		SW_RNOC_NSP_RSTN					BIT2
#define		SW_SNOC_TCU_ARSTN					BIT3
#define		SW_SNOC_U84_ARSTN					BIT4
#define		SW_SNOC_PCIET_XSRSTN					BIT5
#define		SW_SNOC_PCIET_XMRSTN					BIT6
#define		SW_SNOC_PCIET_PRSTN					BIT7
#define		SW_SNOC_NPU_ARSTN					BIT8
#define		SW_SNOC_JTAG_ARSTN					BIT9
#define		SW_SNOC_DSPT_ARSTN					BIT10
#define		SW_SNOC_DDRC1_P2_ARSTN					BIT11
#define		SW_SNOC_DDRC1_P1_ARSTN					BIT12
#define		SW_SNOC_DDRC0_P2_ARSTN					BIT13
#define		SW_SNOC_DDRC0_P1_ARSTN					BIT14
#define		SW_SNOC_D2D_ARSTN					BIT15
#define		SW_SNOC_AON_ARSTN					BIT16

/*GPU*/
#define		SW_GPU_AXI_RSTN						BIT0
#define		SW_GPU_CFG_RSTN						BIT1
#define		SW_GPU_GRAY_RSTN					BIT2
#define		SW_GPU_JONES_RSTN					BIT3
#define		SW_GPU_SPU_RSTN						BIT4

/*DSP*/
#define		SW_DSP_AXI_RSTN						BIT0
#define		SW_DSP_CFG_RSTN						BIT1
#define		SW_DSP_DIV4_RSTN					BIT2
#define		SW_DSP_DIV_RSTN_0					BIT4
#define		SW_DSP_DIV_RSTN_1					BIT5
#define		SW_DSP_DIV_RSTN_2					BIT6
#define		SW_DSP_DIV_RSTN_3					BIT7

/*D2D*/
#define		SW_D2D_AXI_RSTN						BIT0
#define		SW_D2D_CFG_RSTN						BIT1
#define		SW_D2D_PRST_N						BIT2
#define		SW_D2D_RAW_PCS_RST_N					BIT4
#define		SW_D2D_RX_RST_N						BIT5
#define		SW_D2D_TX_RST_N						BIT6
#define		SW_D2D_CORE_RST_N					BIT7

/*TCU*/
#define		SW_TCU_AXI_RSTN						BIT0
#define		SW_TCU_CFG_RSTN						BIT1
#define		TBU_RSTN_0						BIT4
#define		TBU_RSTN_1						BIT5
#define		TBU_RSTN_2						BIT6
#define		TBU_RSTN_3						BIT7
#define		TBU_RSTN_4						BIT8
#define		TBU_RSTN_5						BIT9
#define		TBU_RSTN_6						BIT10
#define		TBU_RSTN_7						BIT11
#define		TBU_RSTN_8						BIT12
#define		TBU_RSTN_9						BIT13
#define		TBU_RSTN_10						BIT14
#define		TBU_RSTN_11						BIT15
#define		TBU_RSTN_12						BIT16
#define		TBU_RSTN_13						BIT17
#define		TBU_RSTN_14						BIT18
#define		TBU_RSTN_15						BIT19
#define		TBU_RSTN_16						BIT20

/*NPU*/
#define		SW_NPU_AXI_RSTN						BIT0
#define		SW_NPU_CFG_RSTN						BIT1
#define		SW_NPU_CORE_RSTN					BIT2
#define		SW_NPU_E31CORE_RSTN					BIT3
#define		SW_NPU_E31BUS_RSTN					BIT4
#define		SW_NPU_E31DBG_RSTN					BIT5
#define		SW_NPU_LLC_RSTN						BIT6

/*HSP DMA*/
#define		SW_HSP_AXI_RSTN						BIT0
#define		SW_HSP_CFG_RSTN						BIT1
#define		SW_HSP_POR_RSTN						BIT2
#define		SW_MSHC0_PHY_RSTN					BIT3
#define		SW_MSHC1_PHY_RSTN					BIT4
#define		SW_MSHC2_PHY_RSTN					BIT5
#define		SW_MSHC0_TXRX_RSTN					BIT6
#define		SW_MSHC1_TXRX_RSTN					BIT7
#define		SW_MSHC2_TXRX_RSTN					BIT8
#define		SW_SATA_ASIC0_RSTN					BIT9
#define		SW_SATA_OOB_RSTN					BIT10
#define		SW_SATA_PMALIVE_RSTN					BIT11
#define		SW_SATA_RBC_RSTN					BIT12
#define		SW_DMA0_RST_N						BIT13
#define		SW_HSP_DMA0_RSTN					BIT14
#define		SW_USB0_VAUX_RSTN					BIT15
#define		SW_USB1_VAUX_RSTN					BIT16
#define		SW_HSP_SD1_PRSTN					BIT17
#define		SW_HSP_SD0_PRSTN					BIT18
#define		SW_HSP_EMMC_PRSTN					BIT19
#define		SW_HSP_DMA_PRSTN					BIT20
#define		SW_HSP_SD1_ARSTN					BIT21
#define		SW_HSP_SD0_ARSTN					BIT22
#define		SW_HSP_EMMC_ARSTN					BIT23
#define		SW_HSP_DMA_ARSTN					BIT24
#define		SW_HSP_ETH1_ARSTN					BIT25
#define		SW_HSP_ETH0_ARSTN					BIT26
#define		SW_HSP_SATA_ARSTN					BIT27

/*PCIE*/
#define		SW_PCIE_CFG_RSTN					BIT0
#define		SW_PCIE_POWERUP_RSTN					BIT1
#define		SW_PCIE_PERST_N						BIT2

/*I2C*/
#define		SW_I2C_RST_N_0						BIT0
#define		SW_I2C_RST_N_1						BIT1
#define		SW_I2C_RST_N_2						BIT2
#define		SW_I2C_RST_N_3						BIT3
#define		SW_I2C_RST_N_4						BIT4
#define		SW_I2C_RST_N_5						BIT5
#define		SW_I2C_RST_N_6						BIT6
#define		SW_I2C_RST_N_7						BIT7
#define		SW_I2C_RST_N_8						BIT8
#define		SW_I2C_RST_N_9						BIT9

/*FAN*/
#define		SW_FAN_RST_N						BIT0

/*PVT*/
#define		SW_PVT_RST_N_0						BIT0
#define		SW_PVT_RST_N_1						BIT1

/*MBOX*/
#define		SW_MBOX_RST_N_0					BIT0
#define		SW_MBOX_RST_N_1					BIT1
#define		SW_MBOX_RST_N_2					BIT2
#define		SW_MBOX_RST_N_3					BIT3
#define		SW_MBOX_RST_N_4					BIT4
#define		SW_MBOX_RST_N_5					BIT5
#define		SW_MBOX_RST_N_6					BIT6
#define		SW_MBOX_RST_N_7					BIT7
#define		SW_MBOX_RST_N_8					BIT8
#define		SW_MBOX_RST_N_9					BIT9
#define		SW_MBOX_RST_N_10				BIT10
#define		SW_MBOX_RST_N_11				BIT11
#define		SW_MBOX_RST_N_12				BIT12
#define		SW_MBOX_RST_N_13				BIT13
#define		SW_MBOX_RST_N_14				BIT14
#define		SW_MBOX_RST_N_15				BIT15

/*UART*/
#define		SW_UART_RST_N_0					BIT0
#define		SW_UART_RST_N_1					BIT1
#define		SW_UART_RST_N_2					BIT2
#define		SW_UART_RST_N_3					BIT3
#define		SW_UART_RST_N_4					BIT4

/*GPIO*/
/*
#define		SW_GPIO_RST_N_0					BIT0
#define		SW_GPIO_RST_N_1					BIT1
*/

/*TIMER*/
#define		SW_TIMER_RST_N					BIT0

/*SSI*/
#define		SW_SSI_RST_N_0					BIT0
#define		SW_SSI_RST_N_1					BIT1

/*WDT*/
#define		SW_WDT_RST_N_0					BIT0
#define		SW_WDT_RST_N_1					BIT1
#define		SW_WDT_RST_N_2					BIT2
#define		SW_WDT_RST_N_3					BIT3

/*LSP CFG*/
#define		SW_LSP_CFG_RSTN					BIT0

/*U84 CFG*/
#define		SW_U84_CORE_RSTN_0				BIT0
#define		SW_U84_CORE_RSTN_1				BIT1
#define		SW_U84_CORE_RSTN_2				BIT2
#define		SW_U84_CORE_RSTN_3				BIT3
#define		SW_U84_BUS_RSTN					BIT4
#define		SW_U84_DBG_RSTN					BIT5
#define		SW_U84_TRACECOM_RSTN				BIT6
#define		SW_U84_TRACE_RSTN_0				BIT8
#define		SW_U84_TRACE_RSTN_1				BIT9
#define		SW_U84_TRACE_RSTN_2				BIT10
#define		SW_U84_TRACE_RSTN_3				BIT11

/*SCPU*/
#define		SW_SCPU_CORE_RSTN				BIT0
#define		SW_SCPU_BUS_RSTN				BIT1
#define		SW_SCPU_DBG_RSTN				BIT2

/*LPCPU*/
#define		SW_LPCPU_CORE_RSTN				BIT0
#define		SW_LPCPU_BUS_RSTN				BIT1
#define		SW_LPCPU_DBG_RSTN				BIT2

/*VC*/
#define		SW_VC_CFG_RSTN					BIT0
#define		SW_VC_AXI_RSTN					BIT1
#define		SW_VC_MONCFG_RSTN				BIT2

/*JD*/
#define		SW_JD_CFG_RSTN					BIT0
#define		SW_JD_AXI_RSTN					BIT1

/*JE*/
#define		SW_JE_CFG_RSTN					BIT0
#define		SW_JE_AXI_RSTN					BIT1

/*VD*/
#define		SW_VD_CFG_RSTN					BIT0
#define		SW_VD_AXI_RSTN					BIT1

/*VE*/
#define		SW_VE_AXI_RSTN					BIT0
#define		SW_VE_CFG_RSTN					BIT1

/*G2D*/
#define		SW_G2D_CORE_RSTN				BIT0
#define		SW_G2D_CFG_RSTN					BIT1
#define		SW_G2D_AXI_RSTN					BIT2

/*VI*/
#define		SW_VI_AXI_RSTN					BIT0
#define		SW_VI_CFG_RSTN					BIT1
#define		SW_VI_DWE_RSTN					BIT2

/*DVP*/
#define		SW_VI_DVP_RSTN					BIT0

/*ISP0*/
#define		SW_VI_ISP0_RSTN					BIT0

/*ISP1*/
#define		SW_VI_ISP1_RSTN					BIT0

/*SHUTTR*/
#define		SW_VI_SHUTTER_RSTN_0				BIT0
#define		SW_VI_SHUTTER_RSTN_1				BIT1
#define		SW_VI_SHUTTER_RSTN_2				BIT2
#define		SW_VI_SHUTTER_RSTN_3				BIT3
#define		SW_VI_SHUTTER_RSTN_4				BIT4
#define		SW_VI_SHUTTER_RSTN_5				BIT5

/*VO PHY*/
#define		SW_VO_MIPI_PRSTN				BIT0
#define		SW_VO_PRSTN					BIT1
#define		SW_VO_HDMI_PRSTN				BIT3
#define		SW_HDMI_PHYCTRL_RSTN				BIT4
#define		SW_VO_HDMI_RSTN					BIT5

/*VO I2S*/
#define		SW_VO_I2S_RSTN					BIT0
#define		SW_VO_I2S_PRSTN					BIT1

/*VO*/
#define		SW_VO_AXI_RSTN					BIT0
#define		SW_VO_CFG_RSTN					BIT1
#define		SW_VO_DC_RSTN					BIT2
#define		SW_VO_DC_PRSTN					BIT3

/*BOOTSPI*/
#define		SW_BOOTSPI_HRSTN				BIT0
#define		SW_BOOTSPI_RSTN					BIT1

/*I2C1*/
#define		SW_I2C1_PRSTN					BIT0

/*I2C0*/
#define		SW_I2C0_PRSTN					BIT0

/*DMA1*/
#define		SW_DMA1_ARSTN					BIT0
#define		SW_DMA1_HRSTN					BIT1

/*FPRT*/
#define		SW_FP_PRT_HRSTN					BIT0

/*HBLOCK*/
#define		SW_HBLOCK_HRSTN					BIT0

/*SECSR*/
#define		SW_SECSR_HRSTN					BIT0

/*OTP*/
#define		SW_OTP_PRSTN					BIT0

/*PKA*/
#define		SW_PKA_HRSTN					BIT0

/*SPACC*/
#define		SW_SPACC_RSTN					BIT0

/*TRNG*/
#define		SW_TRNG_HRSTN					BIT0

/*TIMER0*/
#define		SW_TIMER0_RSTN_0				BIT0
#define		SW_TIMER0_RSTN_1				BIT1
#define		SW_TIMER0_RSTN_2				BIT2
#define		SW_TIMER0_RSTN_3				BIT3
#define		SW_TIMER0_RSTN_4				BIT4
#define		SW_TIMER0_RSTN_5				BIT5
#define		SW_TIMER0_RSTN_6				BIT6
#define		SW_TIMER0_RSTN_7				BIT7
#define		SW_TIMER0_PRSTN					BIT8

/*TIMER1*/
#define		SW_TIMER1_RSTN_0				BIT0
#define		SW_TIMER1_RSTN_1				BIT1
#define		SW_TIMER1_RSTN_2				BIT2
#define		SW_TIMER1_RSTN_3				BIT3
#define		SW_TIMER1_RSTN_4				BIT4
#define		SW_TIMER1_RSTN_5				BIT5
#define		SW_TIMER1_RSTN_6				BIT6
#define		SW_TIMER1_RSTN_7				BIT7
#define		SW_TIMER1_PRSTN					BIT8

/*TIMER2*/
#define		SW_TIMER2_RSTN_0				BIT0
#define		SW_TIMER2_RSTN_1				BIT1
#define		SW_TIMER2_RSTN_2				BIT2
#define		SW_TIMER2_RSTN_3				BIT3
#define		SW_TIMER2_RSTN_4				BIT4
#define		SW_TIMER2_RSTN_5				BIT5
#define		SW_TIMER2_RSTN_6				BIT6
#define		SW_TIMER2_RSTN_7				BIT7
#define		SW_TIMER2_PRSTN					BIT8

/*TIMER3*/
#define		SW_TIMER3_RSTN_0				BIT0
#define		SW_TIMER3_RSTN_1				BIT1
#define		SW_TIMER3_RSTN_2				BIT2
#define		SW_TIMER3_RSTN_3				BIT3
#define		SW_TIMER3_RSTN_4				BIT4
#define		SW_TIMER3_RSTN_5				BIT5
#define		SW_TIMER3_RSTN_6				BIT6
#define		SW_TIMER3_RSTN_7				BIT7
#define		SW_TIMER3_PRSTN					BIT8

/*RTC*/
#define		SW_RTC_RSTN					BIT0

/*MNOC*/
#define		SW_MNOC_SNOC_NSP_RSTN				BIT0
#define		SW_MNOC_VC_ARSTN				BIT1
#define		SW_MNOC_CFG_RSTN				BIT2
#define		SW_MNOC_HSP_ARSTN				BIT3
#define		SW_MNOC_GPU_ARSTN				BIT4
#define		SW_MNOC_DDRC1_P3_ARSTN				BIT5
#define		SW_MNOC_DDRC0_P3_ARSTN				BIT6

/*RNOC*/
#define		SW_RNOC_VO_ARSTN				BIT0
#define		SW_RNOC_VI_ARSTN				BIT1
#define		SW_RNOC_SNOC_NSP_RSTN				BIT2
#define		SW_RNOC_CFG_RSTN				BIT3
#define		SW_MNOC_DDRC1_P4_ARSTN				BIT4
#define		SW_MNOC_DDRC0_P4_ARSTN				BIT5

/*CNOC*/
#define		SW_CNOC_VO_CFG_RSTN				BIT0
#define		SW_CNOC_VI_CFG_RSTN				BIT1
#define		SW_CNOC_VC_CFG_RSTN				BIT2
#define		SW_CNOC_TCU_CFG_RSTN				BIT3
#define		SW_CNOC_PCIET_CFG_RSTN				BIT4
#define		SW_CNOC_NPU_CFG_RSTN				BIT5
#define		SW_CNOC_LSP_CFG_RSTN				BIT6
#define		SW_CNOC_HSP_CFG_RSTN				BIT7
#define		SW_CNOC_GPU_CFG_RSTN				BIT8
#define		SW_CNOC_DSPT_CFG_RSTN				BIT9
#define		SW_CNOC_DDRT1_CFG_RSTN				BIT10
#define		SW_CNOC_DDRT0_CFG_RSTN				BIT11
#define		SW_CNOC_D2D_CFG_RSTN				BIT12
#define		SW_CNOC_CFG_RSTN				BIT13
#define		SW_CNOC_CLMM_CFG_RSTN				BIT14
#define		SW_CNOC_AON_CFG_RSTN				BIT15

/*LNOC*/
#define		SW_LNOC_CFG_RSTN				BIT0
#define		SW_LNOC_NPU_LLC_ARSTN				BIT1
#define		SW_LNOC_DDRC1_P0_ARSTN				BIT2
#define		SW_LNOC_DDRC0_P0_ARSTN				BIT3

#endif /*endif __DT_ESWIN_EIC7700_SYSCRG_H__*/
