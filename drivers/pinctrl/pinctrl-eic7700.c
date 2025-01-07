// SPDX-License-Identifier: GPL-2.0
/*
 * ESWIN Pinctrl Controller Platform Device Driver
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
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/regmap.h>
#include "pinctrl-utils.h"
#include "core.h"

#define ESWIN_MIO_NUM 164

#define ESWIN_PINCONF_IE	   BIT(0)
#define ESWIN_PINCONF_PULLUP   BIT(1)
#define ESWIN_PINCONF_PULLDOWN BIT(2)
#define ESWIN_PINCONF_DRIVER_STRENGTH_MASK  0xf  // 0111 1000
#define ESWIN_PINCONF_DRIVER_SHIFT 3
#define ESWIN_PINCONF_SMT BIT(7)

struct eswin_function_desc {
	const char *name;
	const char * const *groups;
	int ngroups;
	u32 mux_val;
	u32 mux_mask;
};

struct eswin_group_desc{
	const char *name;
	const unsigned int *pins;
	const unsigned int npins;

};

struct eswin_pinctrl {
	struct pinctrl_dev *pctrl;
	void __iomem		*base;
	const struct eswin_group_desc *groups;
	unsigned int ngroups;
	const struct eswin_function_desc *funcs;
	unsigned int nfuncs;
};

static const struct pinctrl_pin_desc eswin_pins[] = {

	PINCTRL_PIN(0, "CHIP_MODE"),
	PINCTRL_PIN(1, "MODE_SET0"),
	PINCTRL_PIN(2, "MODE_SET1"),
	PINCTRL_PIN(3, "MODE_SET2"),
	PINCTRL_PIN(4, "MODE_SET3"),
	PINCTRL_PIN(5, "XIN"),
	PINCTRL_PIN(6, "RTC_XIN"),
	PINCTRL_PIN(7, "RST_OUT_N"),
	PINCTRL_PIN(8,"KEY_RESET_N"),
	PINCTRL_PIN(9,"RST_IN_N"),
	PINCTRL_PIN(10,"POR_IN_N"),
	PINCTRL_PIN(11,"POR_OUT_N"),
	PINCTRL_PIN(12,"GPIO0"),
	PINCTRL_PIN(13,"POR_SEL"),
	PINCTRL_PIN(14,"JTAG0_TCK"),
	PINCTRL_PIN(15,"JTAG0_TMS"),
	PINCTRL_PIN(16,"JTAG0_TDI"),
	PINCTRL_PIN(17,"JTAG0_TDO"),
	PINCTRL_PIN(18,"JTAG0_TRST"),
	PINCTRL_PIN(19,"SPI2_CS0_N"),
	PINCTRL_PIN(20,"JTAG1_TCK"),
	PINCTRL_PIN(21,"JTAG1_TMS"),
	PINCTRL_PIN(22,"JTAG1_TDI"),
	PINCTRL_PIN(23,"JTAG1_TDO"),
	PINCTRL_PIN(24,"JTAG1_TRST"),
	PINCTRL_PIN(25,"SPI2_CS1_N"),
	PINCTRL_PIN(26,"PCIE_CLKREQ_N"),
	PINCTRL_PIN(27,"PCIE_WAKE_N"),
	PINCTRL_PIN(28,"PCIE_PERST_N"),
	PINCTRL_PIN(29,"HDMI_SCL"),
	PINCTRL_PIN(30,"HDMI_SDA"),
	PINCTRL_PIN(31,"HDMI_CEC"),
	PINCTRL_PIN(32,"JTAG2_TRST"),
	PINCTRL_PIN(33,"RGMII0_CLK_125"),
	PINCTRL_PIN(34,"RGMII0_TXEN"),
	PINCTRL_PIN(35,"RGMII0_TXCLK"),
	PINCTRL_PIN(36,"RGMII0_TXD0"),
	PINCTRL_PIN(37,"RGMII0_TXD1"),
	PINCTRL_PIN(38,"RGMII0_TXD2"),
	PINCTRL_PIN(39,"RGMII0_TXD3"),
	PINCTRL_PIN(40,"I2S0_BCLK"),
	PINCTRL_PIN(41,"I2S0_WCLK"),
	PINCTRL_PIN(42,"I2S0_SDI"),
	PINCTRL_PIN(43,"I2S0_SDO"),
	PINCTRL_PIN(44,"I2S_MCLK"),
	PINCTRL_PIN(45,"RGMII0_RXCLK"),
	PINCTRL_PIN(46,"RGMII0_RXDV"),
	PINCTRL_PIN(47,"RGMII0_RXD0"),
	PINCTRL_PIN(48,"RGMII0_RXD1"),
	PINCTRL_PIN(49,"RGMII0_RXD2"),
	PINCTRL_PIN(50,"RGMII0_RXD3"),
	PINCTRL_PIN(51,"I2S2_BCLK"),
	PINCTRL_PIN(52,"I2S2_WCLK"),
	PINCTRL_PIN(53,"I2S2_SDI"),
	PINCTRL_PIN(54,"I2S2_SDO"),
	PINCTRL_PIN(55,"GPIO27"),
	PINCTRL_PIN(56,"GPIO28"),
	PINCTRL_PIN(57,"GPIO29"),
	PINCTRL_PIN(58,"RGMII0_MDC"),
	PINCTRL_PIN(59,"RGMII0_MDIO"),
	PINCTRL_PIN(60,"RGMII0_INTB"),
	PINCTRL_PIN(61,"RGMII1_CLK_125"),
	PINCTRL_PIN(62,"RGMII1_TXEN"),
	PINCTRL_PIN(63,"RGMII1_TXCLK"),
	PINCTRL_PIN(64,"RGMII1_TXD0"),
	PINCTRL_PIN(65,"RGMII1_TXD1"),
	PINCTRL_PIN(66,"RGMII1_TXD2"),
	PINCTRL_PIN(67,"RGMII1_TXD3"),
	PINCTRL_PIN(68,"I2S1_BCLK"),
	PINCTRL_PIN(69,"I2S1_WCLK"),
	PINCTRL_PIN(70,"I2S1_SDI"),
	PINCTRL_PIN(71,"I2S1_SDO"),
	PINCTRL_PIN(72,"GPIO34"),
	PINCTRL_PIN(73,"RGMII1_RXCLK"),
	PINCTRL_PIN(74,"RGMII1_RXDV"),
	PINCTRL_PIN(75,"RGMII1_RXD0"),
	PINCTRL_PIN(76,"RGMII1_RXD1"),
	PINCTRL_PIN(77,"RGMII1_RXD2"),
	PINCTRL_PIN(78,"RGMII1_RXD3"),
	PINCTRL_PIN(79,"SPI1_CS0_N"),
	PINCTRL_PIN(80,"SPI1_CLK"),
	PINCTRL_PIN(81,"SPI1_D0"),
	PINCTRL_PIN(82,"SPI1_D1"),
	PINCTRL_PIN(83,"SPI1_D2"),
	PINCTRL_PIN(84,"SPI1_D3"),
	PINCTRL_PIN(85,"SPI1_CS1_N"),
	PINCTRL_PIN(86,"RGMII1_MDC"),
	PINCTRL_PIN(87,"RGMII1_MDIO"),
	PINCTRL_PIN(88,"RGMII1_INTB"),
	PINCTRL_PIN(89,"USB0_PWREN"),
	PINCTRL_PIN(90,"USB1_PWREN"),
	PINCTRL_PIN(91,"I2C0_SCL"),
	PINCTRL_PIN(92,"I2C0_SDA"),
	PINCTRL_PIN(93,"I2C1_SCL"),
	PINCTRL_PIN(94,"I2C1_SDA"),
	PINCTRL_PIN(95,"I2C2_SCL"),
	PINCTRL_PIN(96,"I2C2_SDA"),
	PINCTRL_PIN(97,"I2C3_SCL"),
	PINCTRL_PIN(98,"I2C3_SDA"),
	PINCTRL_PIN(99,"I2C4_SCL"),
	PINCTRL_PIN(100,"I2C4_SDA"),
	PINCTRL_PIN(101,"I2C5_SCL"),
	PINCTRL_PIN(102,"I2C5_SDA"),
	PINCTRL_PIN(103,"UART0_TX"),
	PINCTRL_PIN(104,"UART0_RX"),
	PINCTRL_PIN(105,"UART1_TX"),
	PINCTRL_PIN(106,"UART1_RX"),
	PINCTRL_PIN(107,"UART1_CTS"),
	PINCTRL_PIN(108,"UART1_RTS"),
	PINCTRL_PIN(109,"UART2_TX"),
	PINCTRL_PIN(110,"UART2_RX"),
	PINCTRL_PIN(111,"JTAG2_TCK"),
	PINCTRL_PIN(112,"JTAG2_TMS"),
	PINCTRL_PIN(113,"JTAG2_TDI"),
	PINCTRL_PIN(114,"JTAG2_TDO"),
	PINCTRL_PIN(115,"FAN_PWM"),
	PINCTRL_PIN(116,"FAN_TACH"),
	PINCTRL_PIN(117,"MIPI_CSI0_XVS"),
	PINCTRL_PIN(118,"MIPI_CSI0_XHS"),
	PINCTRL_PIN(119,"MIPI_CSI0_MCLK"),
	PINCTRL_PIN(120,"MIPI_CSI1_XVS"),
	PINCTRL_PIN(121,"MIPI_CSI1_XHS"),
	PINCTRL_PIN(122,"MIPI_CSI1_MCLK"),
	PINCTRL_PIN(123,"MIPI_CSI2_XVS"),
	PINCTRL_PIN(124,"MIPI_CSI2_XHS"),
	PINCTRL_PIN(125,"MIPI_CSI2_MCLK"),
	PINCTRL_PIN(126,"MIPI_CSI3_XVS"),
	PINCTRL_PIN(127,"MIPI_CSI3_XHS"),
	PINCTRL_PIN(128,"MIPI_CSI3_MCLK"),
	PINCTRL_PIN(129,"MIPI_CSI4_XVS"),
	PINCTRL_PIN(130,"MIPI_CSI4_XHS"),
	PINCTRL_PIN(131,"MIPI_CSI4_MCLK"),
	PINCTRL_PIN(132,"MIPI_CSI5_XVS"),
	PINCTRL_PIN(133,"MIPI_CSI5_XHS"),
	PINCTRL_PIN(134,"MIPI_CSI5_MCLK"),
	PINCTRL_PIN(135,"SPI3_CS_N"),
	PINCTRL_PIN(136,"SPI3_CLK"),
	PINCTRL_PIN(137,"SPI3_DI"),
	PINCTRL_PIN(138,"SPI3_DO"),
	PINCTRL_PIN(139,"GPIO92"),
	PINCTRL_PIN(140,"GPIO93"),
	PINCTRL_PIN(141,"S_MODE"),
	PINCTRL_PIN(142,"GPIO95"),
	PINCTRL_PIN(143,"SPI0_CS_N"),
	PINCTRL_PIN(144,"SPI0_CLK"),
	PINCTRL_PIN(145,"SPI0_D0"),
	PINCTRL_PIN(146,"SPI0_D1"),
	PINCTRL_PIN(147,"SPI0_D2"),
	PINCTRL_PIN(148,"SPI0_D3"),
	PINCTRL_PIN(149,"I2C10_SCL"),
	PINCTRL_PIN(150,"I2C10_SDA"),
	PINCTRL_PIN(151,"I2C11_SCL"),
	PINCTRL_PIN(152,"I2C11_SDA"),
	PINCTRL_PIN(153,"GPIO106"),
	PINCTRL_PIN(154,"BOOT_SEL0"),
	PINCTRL_PIN(155,"BOOT_SEL1"),
	PINCTRL_PIN(156,"BOOT_SEL2"),
	PINCTRL_PIN(157,"BOOT_SEL3"),
	PINCTRL_PIN(158,"GPIO111"),
	PINCTRL_PIN(159,"D2D_SERDES_STATUS_IN"),
	PINCTRL_PIN(160,"D2D_SERDES_STATUS_OUT"),
	PINCTRL_PIN(161,"LPDDR_REF_CLK"),

};

#define ESWIN_PINCTRL_GRP(_name) \
	{ \
		.name = #_name "_group", \
		.pins = _name ## _pins, \
		.npins = ARRAY_SIZE(_name ## _pins), \
	}

//func0
static const char * const sdio0_group[] = {"sdio0_group"};
static const char * const sdio1_group[] = {"sdio1_group"};
static const char * const por_sel_group[] = {"por_sel_group"};
static const char * const jtag0_group[] = {"jtag0_group"};
static const char * const jtag1_group[] = {"jtag1_group"};
static const char * const spi2_cs_group[] = {"spi2_cs_group"};
static const char * const pcie_group[] = {"pcie_group"};
static const char * const hdmi_group[] = {"hdmi_group"};
static const char * const jtag2_group[] = {"jtag2_group"};
static const char * const rgmii0_group[] = {"rgmii0_group"};
static const char * const i2s0_group[] = {"i2s0_group"};
static const char * const i2s1_group[] = {"i2s1_group"};
static const char * const i2s2_group[] = {"i2s2_group"};
static const char * const por_time_sel0_group[] = {"por_time_sel0_group"};
static const char * const por_time_sel1_group[] = {"por_time_sel1_group"};
static const char * const rgmii1_group[] = {"rgmii1_group"};
static const char * const spi1_group[] = {"spi1_group"};
static const char * const usb0_pwren_group[] = {"usb0_pwren_group"};
static const char * const usb1_pwren_group[] = {"usb1_pwren_group"};
static const char * const i2c0_group[] = {"i2c0_group"};
static const char * const i2c1_group[] = {"i2c1_group"};
static const char * const i2c2_group[] = {"i2c2_group"};
static const char * const i2c3_group[] = {"i2c3_group"};
static const char * const i2c4_group[] = {"i2c4_group"};
static const char * const i2c5_group[] = {"i2c5_group"};
static const char * const uart0_group[] = {"uart0_group"};
static const char * const uart1_group[] = {"uart1_group"};
static const char * const uart2_group[] = {"uart2_group"};

static const char * const pwm0_group[] = {"pwm0_group"};
static const char * const fan_tach_group[] = {"fan_tach_group"};
static const char * const mipi_csi0_group[] = {"mipi_csi0_group"};
static const char * const mipi_csi1_group[] = {"mipi_csi1_group"};
static const char * const mipi_csi2_group[] = {"mipi_csi2_group"};
static const char * const mipi_csi3_group[] = {"mipi_csi3_group"};
static const char * const mipi_csi4_group[] = {"mipi_csi4_group"};
static const char * const mipi_csi5_group[] = {"mipi_csi5_group"};
static const char * const spi3_group[] = {"spi3_group"};
static const char * const i2c8_group[] = {"i2c8_group"};
static const char * const s_mode_group[] = {"s_mode_group"};
static const char * const pinmux_ddr_refclk_sel_group[] = {"pinmux_ddr_refclk_sel_group"};
static const char * const spi0_group[] = {"spi0_group"};
static const char * const i2c10_group[] = {"i2c10_group"};
static const char * const i2c11_group[] = {"i2c11_group"};
static const char * const boot_sel_group[] = {"boot_sel_group"};
static const char * const lpddr_ref_clk_group[] = {"lpddr_ref_clk_group"};

//func1
static const char * const spi2_clk_group[] = {"spi2_clk_group"};
static const char * const spi2_d0_group[] = {"spi2_d0_group"};
static const char * const spi2_d1_d2_d3_group[] = {"spi2_d1_d2_d3_group"};

static const char * const sata_act_led_group[] = {"sata_act_led_group"};
static const char * const emmc_led_control_group[] = {"emmc_led_control_group"};
static const char * const sd0_led_control_group[] = {"sd0_led_control_group"};
static const char * const i2c9_group[] = {"i2c9_group"};
static const char * const sd1_led_control_group[] = {"sd1_led_control_group"};
static const char * const pwm1_group[] = {"pwm1_group"};
static const char * const pwm2_group[] = {"pwm2_group"};
static const char * const i2c6_group[] = {"i2c6_group"};
static const char * const i2c7_group[] = {"i2c7_group"};
static const char * const mipi_csi_xtrig_group[] = {"mipi_csi_xtrig_group"};

//gpio
static const char * const gpio0_group[] = {"gpio0_group"};
static const char * const gpio1_group[] = {"gpio1_group"};
static const char * const gpio2_group[] = {"gpio2_group"};
static const char * const gpio3_group[] = {"gpio3_group"};
static const char * const gpio4_group[] = {"gpio4_group"};
static const char * const gpio5_group[] = {"gpio5_group"};
static const char * const gpio6_group[] = {"gpio6_group"};
static const char * const gpio7_group[] = {"gpio7_group"};
static const char * const gpio8_group[] = {"gpio8_group"};
static const char * const gpio9_group[] = {"gpio9_group"};
static const char * const gpio10_group[] = {"gpio10_group"};
static const char * const gpio11_group[] = {"gpio11_group"};
static const char * const gpio12_group[] = {"gpio12_group"};
static const char * const gpio13_group[] = {"gpio13_group"};
static const char * const gpio14_group[] = {"gpio14_group"};
static const char * const gpio15_group[] = {"gpio15_group"};
static const char * const gpio16_group[] = {"gpio16_group"};
static const char * const gpio17_group[] = {"gpio17_group"};
static const char * const gpio18_group[] = {"gpio18_group"};
static const char * const gpio19_group[] = {"gpio19_group"};
static const char * const gpio20_group[] = {"gpio20_group"};
static const char * const gpio21_group[] = {"gpio21_group"};
static const char * const gpio22_group[] = {"gpio22_group"};
static const char * const gpio23_group[] = {"gpio23_group"};
static const char * const gpio24_group[] = {"gpio24_group"};
static const char * const gpio25_group[] = {"gpio25_group"};
static const char * const gpio26_group[] = {"gpio26_group"};
static const char * const gpio27_group[] = {"gpio27_group"};
static const char * const gpio28_group[] = {"gpio28_group"};
static const char * const gpio29_group[] = {"gpio29_group"};
static const char * const gpio30_group[] = {"gpio30_group"};
static const char * const gpio31_group[] = {"gpio31_group"};
static const char * const gpio32_group[] = {"gpio32_group"};
static const char * const gpio33_group[] = {"gpio33_group"};
static const char * const gpio34_group[] = {"gpio34_group"};
static const char * const gpio35_group[] = {"gpio35_group"};
static const char * const gpio36_group[] = {"gpio36_group"};
static const char * const gpio37_group[] = {"gpio37_group"};
static const char * const gpio38_group[] = {"gpio38_group"};
static const char * const gpio39_group[] = {"gpio39_group"};
static const char * const gpio40_group[] = {"gpio40_group"};
static const char * const gpio41_group[] = {"gpio41_group"};
static const char * const gpio42_group[] = {"gpio42_group"};
static const char * const gpio43_group[] = {"gpio43_group"};
static const char * const gpio44_group[] = {"gpio44_group"};
static const char * const gpio45_group[] = {"gpio45_group"};
static const char * const gpio46_group[] = {"gpio46_group"};
static const char * const gpio47_group[] = {"gpio47_group"};
static const char * const gpio48_group[] = {"gpio48_group"};
static const char * const gpio49_group[] = {"gpio49_group"};

static const char * const gpio50_group[] = {"gpio50_group"};
static const char * const gpio51_group[] = {"gpio51_group"};
static const char * const gpio52_group[] = {"gpio52_group"};
static const char * const gpio53_group[] = {"gpio53_group"};
static const char * const gpio54_group[] = {"gpio54_group"};
static const char * const gpio55_group[] = {"gpio55_group"};
static const char * const gpio56_group[] = {"gpio56_group"};
static const char * const gpio57_group[] = {"gpio57_group"};
static const char * const gpio58_group[] = {"gpio58_group"};
static const char * const gpio59_group[] = {"gpio59_group"};

static const char * const gpio60_group[] = {"gpio60_group"};
static const char * const gpio61_group[] = {"gpio61_group"};
static const char * const gpio62_group[] = {"gpio62_group"};
static const char * const gpio63_group[] = {"gpio63_group"};
static const char * const gpio64_group[] = {"gpio64_group"};
static const char * const gpio65_group[] = {"gpio65_group"};
static const char * const gpio66_group[] = {"gpio66_group"};
static const char * const gpio67_group[] = {"gpio67_group"};
static const char * const gpio68_group[] = {"gpio68_group"};
static const char * const gpio69_group[] = {"gpio69_group"};

static const char * const gpio70_group[] = {"gpio70_group"};
static const char * const gpio71_group[] = {"gpio71_group"};
static const char * const gpio72_group[] = {"gpio72_group"};
static const char * const gpio73_group[] = {"gpio73_group"};
static const char * const gpio74_group[] = {"gpio74_group"};
static const char * const gpio75_group[] = {"gpio75_group"};
static const char * const gpio76_group[] = {"gpio76_group"};
static const char * const gpio77_group[] = {"gpio77_group"};
static const char * const gpio78_group[] = {"gpio78_group"};
static const char * const gpio79_group[] = {"gpio79_group"};

static const char * const gpio80_group[] = {"gpio80_group"};
static const char * const gpio81_group[] = {"gpio81_group"};
static const char * const gpio82_group[] = {"gpio82_group"};
static const char * const gpio83_group[] = {"gpio83_group"};
static const char * const gpio84_group[] = {"gpio84_group"};
static const char * const gpio85_group[] = {"gpio85_group"};
static const char * const gpio86_group[] = {"gpio86_group"};
static const char * const gpio87_group[] = {"gpio87_group"};
static const char * const gpio88_group[] = {"gpio88_group"};
static const char * const gpio89_group[] = {"gpio89_group"};

static const char * const gpio90_group[] = {"gpio90_group"};
static const char * const gpio91_group[] = {"gpio91_group"};
static const char * const gpio92_group[] = {"gpio92_group"};
static const char * const gpio93_group[] = {"gpio93_group"};
static const char * const gpio94_group[] = {"gpio94_group"};
static const char * const gpio95_group[] = {"gpio95_group"};
static const char * const gpio96_group[] = {"gpio96_group"};
static const char * const gpio97_group[] = {"gpio97_group"};
static const char * const gpio98_group[] = {"gpio98_group"};
static const char * const gpio99_group[] = {"gpio99_group"};

static const char * const gpio100_group[] = {"gpio100_group"};
static const char * const gpio101_group[] = {"gpio101_group"};
static const char * const gpio102_group[] = {"gpio102_group"};
static const char * const gpio103_group[] = {"gpio103_group"};
static const char * const gpio104_group[] = {"gpio104_group"};
static const char * const gpio105_group[] = {"gpio105_group"};
static const char * const gpio106_group[] = {"gpio106_group"};
static const char * const gpio107_group[] = {"gpio107_group"};
static const char * const gpio108_group[] = {"gpio108_group"};
static const char * const gpio109_group[] = {"gpio109_group"};
static const char * const gpio110_group[] = {"gpio110_group"};
static const char * const gpio111_group[] = {"gpio111_group"};

//func3
static const char * const uart4_group[] = {"uart4_group"};
static const char * const uart3_group[] = {"uart3_group"};

//func6
static const char * const csi_mon_out_group[] = {"csi_mon_out_group"};
static const char * const csi_ocla_clk_group[] = {"csi_ocla_clk_group"};
static const char * const csi_mon_out_valid_group[] = {"csi_mon_out_valid_group"};
static const char * const csi_parity_error_group[] = {"csi_parity_error_group"};
static const char * const csi_dtb_out_group[] = {"csi_dtb_out_group"};
static const char * const csi_phy_sel_group[] = {"csi_phy_sel_group"};
static const char * const vc_g2d0_debug_out_group[] = {"vc_g2d0_debug_out_group"};
static const char * const vc_g2d1_debug_out_group[] = {"vc_g2d1_debug_out_group"};
static const char * const sata_mpll_clk_group[] = {"sata_mpll_clk_group"};
static const char * const sata_ref_repeat_clk_m_group[] = {"sata_ref_repeat_clk_m_group"};
static const char * const sata_ref_repeat_clk_p_group[] = {"sata_ref_repeat_clk_p_group"};

//func0
static const unsigned int sdio0_pins[] = {1,2};
static const unsigned int sdio1_pins[] = {3,4};
static const unsigned int por_sel_pins[] = {13};
static const unsigned int jtag0_pins[] = {14,15,16,17};
static const unsigned int jtag1_pins[] = {20,21,22,23};
static const unsigned int spi2_cs_pins[] = {19,25};
static const unsigned int pcie_pins[] = {26,27,28};
static const unsigned int hdmi_pins[] = {29,30,31};
static const unsigned int jtag2_pins[] = {32,111,112,113,114};
static const unsigned int rgmii0_pins[] = {33,34,35,36,37,38,39,45,46,47,48,49,50,58,59,60};
static const unsigned int i2s0_pins[] = {40,41,42,43,44};
static const unsigned int i2s1_pins[] = {68,69,70,71,44};
static const unsigned int i2s2_pins[] = {51,52,53,54,44};
static const unsigned int por_time_sel0_pins[] = {57};
static const unsigned int por_time_sel1_pins[] = {72};
static const unsigned int rgmii1_pins[] = {61,62,63,64,65,66,67,73,74,75,76,77,78,86,87,88};
static const unsigned int spi1_pins[] = {79,80,81,82,83,84,85};
static const unsigned int usb0_pwren_pins[] = {89};
static const unsigned int usb1_pwren_pins[] = {90};
static const unsigned int i2c0_pins[] = {91,92};
static const unsigned int i2c1_pins[] = {93,94};
static const unsigned int i2c2_pins[] = {95,96};
static const unsigned int i2c3_pins[] = {97,98};
static const unsigned int i2c4_pins[] = {99,100};
static const unsigned int i2c5_pins[] = {101,102};
static const unsigned int uart0_pins[] = {103,104};
static const unsigned int uart1_pins[] = {105,106,107,108};
static const unsigned int uart2_pins[] = {109,110};
static const unsigned int pwm0_pins[] = {115};
static const unsigned int fan_tach_pins[] = {116};
static const unsigned int mipi_csi0_pins[] = {117,118,119};
static const unsigned int mipi_csi1_pins[] = {120,121,122};
static const unsigned int mipi_csi2_pins[] = {123,124,125};
static const unsigned int mipi_csi3_pins[] = {126,127,128};
static const unsigned int mipi_csi4_pins[] = {129,130,131};
static const unsigned int mipi_csi5_pins[] = {132,133,134};
static const unsigned int spi3_pins[] = {135,136,137,138};
static const unsigned int i2c8_pins[] = {139,140};
static const unsigned int s_mode_pins[] = {141};
static const unsigned int pinmux_ddr_refclk_sel_pins[] = {142};
static const unsigned int spi0_pins[] = {143,144,145,146,147,148};
static const unsigned int i2c10_pins[] = {149,150};
static const unsigned int i2c11_pins[] = {151,152};
static const unsigned int boot_sel_pins[] = {154,155,156,157};
static const unsigned int lpddr_ref_clk_pins[] = {158};

//func1
static const unsigned int spi2_clk_pins[] = {14};
static const unsigned int spi2_d0_pins[] = {15};
static const unsigned int spi2_d1_d2_d3_pins[] = {16,17,18};

static const unsigned int sata_act_led_pins[] = {55};
static const unsigned int emmc_led_control_pins[] = {57};
static const unsigned int sd0_led_control_pins[] = {72};
static const unsigned int i2c9_pins[] = {81,82};
static const unsigned int sd1_led_control_pins[] = {83};
static const unsigned int pwm1_pins[] = {84};
static const unsigned int pwm2_pins[] = {85};
static const unsigned int i2c6_pins[] = {107,108};
static const unsigned int i2c7_pins[] = {109,110};
static const unsigned int mipi_csi_xtrig_pins[] = {139,140};

//gpio
static const unsigned int gpio0_pins[] = {12};
static const unsigned int gpio1_pins[] = {14};
static const unsigned int gpio2_pins[] = {15};
static const unsigned int gpio3_pins[] = {16};
static const unsigned int gpio4_pins[] = {17};
static const unsigned int gpio5_pins[] = {18};
static const unsigned int gpio6_pins[] = {19};
static const unsigned int gpio7_pins[] = {20};
static const unsigned int gpio8_pins[] = {21};
static const unsigned int gpio9_pins[] = {22};

static const unsigned int gpio10_pins[] = {23};
static const unsigned int gpio11_pins[] = {24};
static const unsigned int gpio12_pins[] = {25};
static const unsigned int gpio13_pins[] = {1};
static const unsigned int gpio14_pins[] = {2};
static const unsigned int gpio15_pins[] = {3};
static const unsigned int gpio16_pins[] = {4};
static const unsigned int gpio17_pins[] = {32};
static const unsigned int gpio18_pins[] = {40};
static const unsigned int gpio19_pins[] = {41};

static const unsigned int gpio20_pins[] = {42};
static const unsigned int gpio21_pins[] = {43};
static const unsigned int gpio22_pins[] = {44};
static const unsigned int gpio23_pins[] = {51};
static const unsigned int gpio24_pins[] = {52};
static const unsigned int gpio25_pins[] = {53};
static const unsigned int gpio26_pins[] = {54};
static const unsigned int gpio27_pins[] = {55};
static const unsigned int gpio28_pins[] = {56};
static const unsigned int gpio29_pins[] = {57};

static const unsigned int gpio30_pins[] = {68};
static const unsigned int gpio31_pins[] = {69};
static const unsigned int gpio32_pins[] = {70};
static const unsigned int gpio33_pins[] = {71};
static const unsigned int gpio34_pins[] = {72};
static const unsigned int gpio35_pins[] = {79};
static const unsigned int gpio36_pins[] = {80};
static const unsigned int gpio37_pins[] = {81};
static const unsigned int gpio38_pins[] = {82};
static const unsigned int gpio39_pins[] = {83};

static const unsigned int gpio40_pins[] = {84};
static const unsigned int gpio41_pins[] = {85};
static const unsigned int gpio42_pins[] = {89};
static const unsigned int gpio43_pins[] = {90};
static const unsigned int gpio44_pins[] = {91};
static const unsigned int gpio45_pins[] = {92};
static const unsigned int gpio46_pins[] = {93};
static const unsigned int gpio47_pins[] = {94};
static const unsigned int gpio48_pins[] = {95};
static const unsigned int gpio49_pins[] = {96};

static const unsigned int gpio50_pins[] = {97};
static const unsigned int gpio51_pins[] = {98};
static const unsigned int gpio52_pins[] = {99};
static const unsigned int gpio53_pins[] = {100};
static const unsigned int gpio54_pins[] = {101};
static const unsigned int gpio55_pins[] = {102};
static const unsigned int gpio56_pins[] = {103};
static const unsigned int gpio57_pins[] = {104};
static const unsigned int gpio58_pins[] = {105};
static const unsigned int gpio59_pins[] = {106};

static const unsigned int gpio60_pins[] = {107};
static const unsigned int gpio61_pins[] = {108};
static const unsigned int gpio62_pins[] = {109};
static const unsigned int gpio63_pins[] = {110};
static const unsigned int gpio64_pins[] = {111};
static const unsigned int gpio65_pins[] = {112};
static const unsigned int gpio66_pins[] = {113};
static const unsigned int gpio67_pins[] = {114};
static const unsigned int gpio68_pins[] = {115};
static const unsigned int gpio69_pins[] = {116};

static const unsigned int gpio70_pins[] = {117};
static const unsigned int gpio71_pins[] = {118};
static const unsigned int gpio72_pins[] = {119};
static const unsigned int gpio73_pins[] = {120};
static const unsigned int gpio74_pins[] = {121};
static const unsigned int gpio75_pins[] = {122};
static const unsigned int gpio76_pins[] = {123};
static const unsigned int gpio77_pins[] = {124};
static const unsigned int gpio78_pins[] = {125};
static const unsigned int gpio79_pins[] = {126};

static const unsigned int gpio80_pins[] = {127};
static const unsigned int gpio81_pins[] = {128};
static const unsigned int gpio82_pins[] = {129};
static const unsigned int gpio83_pins[] = {130};
static const unsigned int gpio84_pins[] = {131};
static const unsigned int gpio85_pins[] = {132};
static const unsigned int gpio86_pins[] = {133};
static const unsigned int gpio87_pins[] = {134};
static const unsigned int gpio88_pins[] = {135};
static const unsigned int gpio89_pins[] = {136};

static const unsigned int gpio90_pins[] = {137};
static const unsigned int gpio91_pins[] = {138};
static const unsigned int gpio92_pins[] = {139};
static const unsigned int gpio93_pins[] = {140};
static const unsigned int gpio94_pins[] = {141};
static const unsigned int gpio95_pins[] = {142};
static const unsigned int gpio96_pins[] = {143};
static const unsigned int gpio97_pins[] = {144};
static const unsigned int gpio98_pins[] = {145};
static const unsigned int gpio99_pins[] = {146};

static const unsigned int gpio100_pins[] = {147};
static const unsigned int gpio101_pins[] = {148};
static const unsigned int gpio102_pins[] = {149};
static const unsigned int gpio103_pins[] = {150};
static const unsigned int gpio104_pins[] = {151};
static const unsigned int gpio105_pins[] = {152};
static const unsigned int gpio106_pins[] = {153};
static const unsigned int gpio107_pins[] = {154};
static const unsigned int gpio108_pins[] = {155};
static const unsigned int gpio109_pins[] = {156};
static const unsigned int gpio110_pins[] = {157};
static const unsigned int gpio111_pins[] = {158};

//func3
static const unsigned int uart4_pins[] = {81,82};
static const unsigned int uart3_pins[] = {139,140};

//func6
static const unsigned int csi_mon_out_pins[] = {32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55};
static const unsigned int csi_ocla_clk_pins[] = {96};
static const unsigned int csi_mon_out_valid_pins[] = {97};
static const unsigned int csi_parity_error_pins[] = {98};
static const unsigned int csi_dtb_out_pins[] = {99,100,101,102,129,130,131,132};
static const unsigned int csi_phy_sel_pins[] = {133,134,109};
static const unsigned int vc_g2d0_debug_out_pins[] = {110,111,112,113,114,115,116,117};
static const unsigned int vc_g2d1_debug_out_pins[] = {118,119,120,121,122,123,124,125};
static const unsigned int sata_mpll_clk_pins[] = {126};
static const unsigned int sata_ref_repeat_clk_m_pins[] = {127};
static const unsigned int sata_ref_repeat_clk_p_pins[] = {128};

static const struct eswin_group_desc eswin_pinctrl_groups[] =
{
	//func0
	ESWIN_PINCTRL_GRP(sdio0),
	ESWIN_PINCTRL_GRP(sdio1),
	ESWIN_PINCTRL_GRP(por_sel),
	ESWIN_PINCTRL_GRP(jtag0),
	ESWIN_PINCTRL_GRP(jtag1),
	ESWIN_PINCTRL_GRP(spi2_cs),
	ESWIN_PINCTRL_GRP(pcie),
	ESWIN_PINCTRL_GRP(hdmi),
	ESWIN_PINCTRL_GRP(jtag2),
	ESWIN_PINCTRL_GRP(rgmii0),
	ESWIN_PINCTRL_GRP(i2s0),
	ESWIN_PINCTRL_GRP(i2s1),
	ESWIN_PINCTRL_GRP(i2s2),
	ESWIN_PINCTRL_GRP(por_time_sel0),
	ESWIN_PINCTRL_GRP(por_time_sel1),
	ESWIN_PINCTRL_GRP(rgmii1),
	ESWIN_PINCTRL_GRP(spi1),
	ESWIN_PINCTRL_GRP(usb0_pwren),
	ESWIN_PINCTRL_GRP(usb1_pwren),
	ESWIN_PINCTRL_GRP(i2c0),
	ESWIN_PINCTRL_GRP(i2c1),
	ESWIN_PINCTRL_GRP(i2c2),
	ESWIN_PINCTRL_GRP(i2c3),
	ESWIN_PINCTRL_GRP(i2c4),
	ESWIN_PINCTRL_GRP(i2c5),
	ESWIN_PINCTRL_GRP(uart0),
	ESWIN_PINCTRL_GRP(uart1),
	ESWIN_PINCTRL_GRP(uart2),
	ESWIN_PINCTRL_GRP(pwm0),
	ESWIN_PINCTRL_GRP(fan_tach),
	ESWIN_PINCTRL_GRP(mipi_csi0),
	ESWIN_PINCTRL_GRP(mipi_csi1),
	ESWIN_PINCTRL_GRP(mipi_csi2),
	ESWIN_PINCTRL_GRP(mipi_csi3),
	ESWIN_PINCTRL_GRP(mipi_csi4),
	ESWIN_PINCTRL_GRP(mipi_csi5),
	ESWIN_PINCTRL_GRP(spi3),
	ESWIN_PINCTRL_GRP(i2c8),
	ESWIN_PINCTRL_GRP(s_mode),
	ESWIN_PINCTRL_GRP(pinmux_ddr_refclk_sel),
	ESWIN_PINCTRL_GRP(spi0),
	ESWIN_PINCTRL_GRP(i2c10),
	ESWIN_PINCTRL_GRP(i2c11),
	ESWIN_PINCTRL_GRP(boot_sel),
	ESWIN_PINCTRL_GRP(lpddr_ref_clk),

	//func1
	ESWIN_PINCTRL_GRP(spi2_clk),
	ESWIN_PINCTRL_GRP(spi2_d0),
	ESWIN_PINCTRL_GRP(spi2_d1_d2_d3),

	ESWIN_PINCTRL_GRP(sata_act_led),
	ESWIN_PINCTRL_GRP(emmc_led_control),
	ESWIN_PINCTRL_GRP(sd0_led_control),
	ESWIN_PINCTRL_GRP(i2c9),
	ESWIN_PINCTRL_GRP(sd1_led_control),
	ESWIN_PINCTRL_GRP(pwm1),
	ESWIN_PINCTRL_GRP(pwm2),
	ESWIN_PINCTRL_GRP(i2c6),
	ESWIN_PINCTRL_GRP(i2c7),
	ESWIN_PINCTRL_GRP(mipi_csi_xtrig),

	//gpio
	ESWIN_PINCTRL_GRP(gpio0),
	ESWIN_PINCTRL_GRP(gpio1),
	ESWIN_PINCTRL_GRP(gpio2),
	ESWIN_PINCTRL_GRP(gpio3),
	ESWIN_PINCTRL_GRP(gpio4),
	ESWIN_PINCTRL_GRP(gpio5),
	ESWIN_PINCTRL_GRP(gpio6),
	ESWIN_PINCTRL_GRP(gpio7),
	ESWIN_PINCTRL_GRP(gpio8),
	ESWIN_PINCTRL_GRP(gpio9),
	ESWIN_PINCTRL_GRP(gpio10),
	ESWIN_PINCTRL_GRP(gpio11),
	ESWIN_PINCTRL_GRP(gpio12),
	ESWIN_PINCTRL_GRP(gpio13),
	ESWIN_PINCTRL_GRP(gpio14),
	ESWIN_PINCTRL_GRP(gpio15),
	ESWIN_PINCTRL_GRP(gpio16),
	ESWIN_PINCTRL_GRP(gpio17),
	ESWIN_PINCTRL_GRP(gpio18),
	ESWIN_PINCTRL_GRP(gpio19),
	ESWIN_PINCTRL_GRP(gpio20),
	ESWIN_PINCTRL_GRP(gpio21),
	ESWIN_PINCTRL_GRP(gpio22),
	ESWIN_PINCTRL_GRP(gpio23),
	ESWIN_PINCTRL_GRP(gpio24),
	ESWIN_PINCTRL_GRP(gpio25),
	ESWIN_PINCTRL_GRP(gpio26),
	ESWIN_PINCTRL_GRP(gpio27),
	ESWIN_PINCTRL_GRP(gpio28),
	ESWIN_PINCTRL_GRP(gpio29),
	ESWIN_PINCTRL_GRP(gpio30),
	ESWIN_PINCTRL_GRP(gpio31),
	ESWIN_PINCTRL_GRP(gpio32),
	ESWIN_PINCTRL_GRP(gpio33),
	ESWIN_PINCTRL_GRP(gpio34),
	ESWIN_PINCTRL_GRP(gpio35),
	ESWIN_PINCTRL_GRP(gpio36),
	ESWIN_PINCTRL_GRP(gpio37),
	ESWIN_PINCTRL_GRP(gpio38),
	ESWIN_PINCTRL_GRP(gpio39),
	ESWIN_PINCTRL_GRP(gpio40),
	ESWIN_PINCTRL_GRP(gpio41),
	ESWIN_PINCTRL_GRP(gpio42),
	ESWIN_PINCTRL_GRP(gpio43),
	ESWIN_PINCTRL_GRP(gpio44),
	ESWIN_PINCTRL_GRP(gpio45),
	ESWIN_PINCTRL_GRP(gpio46),
	ESWIN_PINCTRL_GRP(gpio47),
	ESWIN_PINCTRL_GRP(gpio48),
	ESWIN_PINCTRL_GRP(gpio49),

	ESWIN_PINCTRL_GRP(gpio50),
	ESWIN_PINCTRL_GRP(gpio51),
	ESWIN_PINCTRL_GRP(gpio52),
	ESWIN_PINCTRL_GRP(gpio53),
	ESWIN_PINCTRL_GRP(gpio54),
	ESWIN_PINCTRL_GRP(gpio55),
	ESWIN_PINCTRL_GRP(gpio56),
	ESWIN_PINCTRL_GRP(gpio57),
	ESWIN_PINCTRL_GRP(gpio58),
	ESWIN_PINCTRL_GRP(gpio59),

	ESWIN_PINCTRL_GRP(gpio60),
	ESWIN_PINCTRL_GRP(gpio61),
	ESWIN_PINCTRL_GRP(gpio62),
	ESWIN_PINCTRL_GRP(gpio63),
	ESWIN_PINCTRL_GRP(gpio64),
	ESWIN_PINCTRL_GRP(gpio65),
	ESWIN_PINCTRL_GRP(gpio66),
	ESWIN_PINCTRL_GRP(gpio67),
	ESWIN_PINCTRL_GRP(gpio68),
	ESWIN_PINCTRL_GRP(gpio69),

	ESWIN_PINCTRL_GRP(gpio70),
	ESWIN_PINCTRL_GRP(gpio71),
	ESWIN_PINCTRL_GRP(gpio72),
	ESWIN_PINCTRL_GRP(gpio73),
	ESWIN_PINCTRL_GRP(gpio74),
	ESWIN_PINCTRL_GRP(gpio75),
	ESWIN_PINCTRL_GRP(gpio76),
	ESWIN_PINCTRL_GRP(gpio77),
	ESWIN_PINCTRL_GRP(gpio78),
	ESWIN_PINCTRL_GRP(gpio79),

	ESWIN_PINCTRL_GRP(gpio80),
	ESWIN_PINCTRL_GRP(gpio81),
	ESWIN_PINCTRL_GRP(gpio82),
	ESWIN_PINCTRL_GRP(gpio83),
	ESWIN_PINCTRL_GRP(gpio84),
	ESWIN_PINCTRL_GRP(gpio85),
	ESWIN_PINCTRL_GRP(gpio86),
	ESWIN_PINCTRL_GRP(gpio87),
	ESWIN_PINCTRL_GRP(gpio88),
	ESWIN_PINCTRL_GRP(gpio89),

	ESWIN_PINCTRL_GRP(gpio90),
	ESWIN_PINCTRL_GRP(gpio91),
	ESWIN_PINCTRL_GRP(gpio92),
	ESWIN_PINCTRL_GRP(gpio93),
	ESWIN_PINCTRL_GRP(gpio94),
	ESWIN_PINCTRL_GRP(gpio95),
	ESWIN_PINCTRL_GRP(gpio96),
	ESWIN_PINCTRL_GRP(gpio97),
	ESWIN_PINCTRL_GRP(gpio98),
	ESWIN_PINCTRL_GRP(gpio99),

	ESWIN_PINCTRL_GRP(gpio100),
	ESWIN_PINCTRL_GRP(gpio101),
	ESWIN_PINCTRL_GRP(gpio102),
	ESWIN_PINCTRL_GRP(gpio103),
	ESWIN_PINCTRL_GRP(gpio104),
	ESWIN_PINCTRL_GRP(gpio105),
	ESWIN_PINCTRL_GRP(gpio106),
	ESWIN_PINCTRL_GRP(gpio107),
	ESWIN_PINCTRL_GRP(gpio108),
	ESWIN_PINCTRL_GRP(gpio109),
	ESWIN_PINCTRL_GRP(gpio110),
	ESWIN_PINCTRL_GRP(gpio111),

	//func3
	ESWIN_PINCTRL_GRP(uart4),
	ESWIN_PINCTRL_GRP(uart3),

	//func6
	ESWIN_PINCTRL_GRP(csi_mon_out),
	ESWIN_PINCTRL_GRP(csi_ocla_clk),
	ESWIN_PINCTRL_GRP(csi_mon_out_valid),
	ESWIN_PINCTRL_GRP(csi_parity_error),
	ESWIN_PINCTRL_GRP(csi_dtb_out),
	ESWIN_PINCTRL_GRP(csi_phy_sel),
	ESWIN_PINCTRL_GRP(vc_g2d0_debug_out),
	ESWIN_PINCTRL_GRP(vc_g2d1_debug_out),
	ESWIN_PINCTRL_GRP(sata_mpll_clk),
	ESWIN_PINCTRL_GRP(sata_ref_repeat_clk_m),
	ESWIN_PINCTRL_GRP(sata_ref_repeat_clk_p),
};

#define ESWIN_PINMUX_FUNCTION(_func_name, _mux_val, _mask)\
	{				   \
		.name = #_func_name"_func",			 \
		.groups = _func_name##_group,	  \
		.ngroups = ARRAY_SIZE(_func_name##_group),  \
		.mux_val = _mux_val,			\
		.mux_mask = _mask,		  \
	}

#define ESWIN_PINMUX_SHIFT  16
#define ESWIN_PINMUX_MASK   (0x07 << ESWIN_PINMUX_SHIFT)

static const struct eswin_function_desc eswin_pinmux_functions[] = {

	//func0
	ESWIN_PINMUX_FUNCTION(sdio0, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(sdio1, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(por_sel, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(jtag0, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(jtag1, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(spi2_cs, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(pcie, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(hdmi, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(jtag2, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(rgmii0, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2s0, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2s1, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2s2, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(por_time_sel0, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(por_time_sel1, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(rgmii1, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(spi1, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(usb0_pwren, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(usb1_pwren, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2c0, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2c1, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2c2, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2c3, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2c4, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2c5, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(uart0, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(uart1, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(uart2, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(pwm0, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(fan_tach, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(mipi_csi0, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(mipi_csi1, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(mipi_csi2, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(mipi_csi3, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(mipi_csi4, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(mipi_csi5, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(spi3, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2c8, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(s_mode, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(pinmux_ddr_refclk_sel, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(spi0, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2c10, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2c11, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(boot_sel, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(lpddr_ref_clk, 0, ESWIN_PINMUX_MASK),

	//func1
	ESWIN_PINMUX_FUNCTION(spi2_clk, 1, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(spi2_d0, 1, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(spi2_d1_d2_d3, 1, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(sata_act_led, 1, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(emmc_led_control, 1, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(sd0_led_control, 1, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2c9, 1, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(sd1_led_control, 1, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(pwm1, 1, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(pwm2, 1, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2c6, 1, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(i2c7, 1, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(mipi_csi_xtrig, 1, ESWIN_PINMUX_MASK),

	//gpio
	ESWIN_PINMUX_FUNCTION(gpio0, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio1, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio2, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio3, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio4, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio5, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio6, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio7, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio8, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio9, 2, ESWIN_PINMUX_MASK),

	ESWIN_PINMUX_FUNCTION(gpio10, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio11, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio12, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio13, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio14, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio15, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio16, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio17, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio18, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio19, 2, ESWIN_PINMUX_MASK),

	ESWIN_PINMUX_FUNCTION(gpio20, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio21, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio22, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio23, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio24, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio25, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio26, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio27, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio28, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio29, 2, ESWIN_PINMUX_MASK),

	ESWIN_PINMUX_FUNCTION(gpio30, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio31, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio32, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio33, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio34, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio35, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio36, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio37, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio38, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio39, 2, ESWIN_PINMUX_MASK),

	ESWIN_PINMUX_FUNCTION(gpio40, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio41, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio42, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio43, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio44, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio45, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio46, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio47, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio48, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio49, 2, ESWIN_PINMUX_MASK),

	ESWIN_PINMUX_FUNCTION(gpio50, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio51, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio52, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio53, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio54, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio55, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio56, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio57, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio58, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio59, 2, ESWIN_PINMUX_MASK),

	ESWIN_PINMUX_FUNCTION(gpio60, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio61, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio62, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio63, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio64, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio65, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio66, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio67, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio68, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio69, 2, ESWIN_PINMUX_MASK),

	ESWIN_PINMUX_FUNCTION(gpio70, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio71, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio72, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio73, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio74, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio75, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio76, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio77, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio78, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio79, 2, ESWIN_PINMUX_MASK),

	ESWIN_PINMUX_FUNCTION(gpio80, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio81, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio82, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio83, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio84, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio85, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio86, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio87, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio88, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio89, 2, ESWIN_PINMUX_MASK),

	ESWIN_PINMUX_FUNCTION(gpio90, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio91, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio92, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio93, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio94, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio95, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio96, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio97, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio98, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio99, 2, ESWIN_PINMUX_MASK),

	ESWIN_PINMUX_FUNCTION(gpio100, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio101, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio102, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio103, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio104, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio105, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio106, 0, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio107, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio108, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio109, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio110, 2, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(gpio111, 0, ESWIN_PINMUX_MASK),

	//func3
	ESWIN_PINMUX_FUNCTION(uart4, 3, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(uart3, 3, ESWIN_PINMUX_MASK),

	//func6
	ESWIN_PINMUX_FUNCTION(csi_mon_out, 6, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(csi_ocla_clk, 6, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(csi_mon_out_valid, 6, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(csi_parity_error, 6, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(csi_dtb_out, 6, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(csi_phy_sel, 6, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(vc_g2d0_debug_out, 6, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(vc_g2d1_debug_out, 6, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(sata_mpll_clk, 6, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(sata_ref_repeat_clk_m, 6, ESWIN_PINMUX_MASK),
	ESWIN_PINMUX_FUNCTION(sata_ref_repeat_clk_p, 6, ESWIN_PINMUX_MASK),
};

/* pinctrl */
static int eswin_pctrl_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct eswin_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);
	return pctrl->ngroups;
}

static const char *eswin_pctrl_get_group_name(struct pinctrl_dev *pctldev,
						 unsigned int selector)
{
	struct eswin_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);
	return pctrl->groups[selector].name;
}

static int eswin_pctrl_get_group_pins(struct pinctrl_dev *pctldev,
					 unsigned int selector,
					 const unsigned int **pins,
					 unsigned int *num_pins)
{
	struct eswin_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);
	*pins = pctrl->groups[selector].pins;
	*num_pins = pctrl->groups[selector].npins;
	return 0;
}

static const struct pinctrl_ops eswin_pinctrl_ops = {
	.get_groups_count = eswin_pctrl_get_groups_count,
	.get_group_name = eswin_pctrl_get_group_name,
	.get_group_pins = eswin_pctrl_get_group_pins,
	.dt_node_to_map = pinconf_generic_dt_node_to_map_all,
	.dt_free_map = pinctrl_utils_free_map,
};

/* pinmux */
static int eswin_pmux_get_functions_count(struct pinctrl_dev *pctldev)
{
	struct eswin_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);

	return pctrl->nfuncs;
}

static const char *eswin_pmux_get_function_name(struct pinctrl_dev *pctldev,
						   unsigned int selector)
{
	struct eswin_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);

	return pctrl->funcs[selector].name;
}

static int eswin_pmux_get_function_groups(struct pinctrl_dev *pctldev,
					 unsigned int selector,
					 const char * const **groups,
					 unsigned * const num_groups)
{
	struct eswin_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);
	*groups = pctrl->funcs[selector].groups;
	*num_groups = pctrl->funcs[selector].ngroups;
	return 0;
}

static int eswin_pinmux_set_mux(struct pinctrl_dev *pctldev,
				   unsigned int function,
				   unsigned int  group)
{
	int i ;
	struct eswin_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);
	const struct eswin_group_desc *pgrp = &pctrl->groups[group];
	const struct eswin_function_desc *func = &pctrl->funcs[function];

	for(i = 0 ;i< pgrp->npins;i++){
		u32 reg ;
		unsigned int pin = pgrp->pins[i];
		reg = readl(pctrl->base + 4*pin);
		reg &= ~ESWIN_PINMUX_MASK;
		reg |= (func->mux_val << ESWIN_PINMUX_SHIFT);
		writel(reg,pctrl->base + 4*pin);
	}
	return 0;
}

static const struct pinmux_ops eswin_pinmux_ops = {
	.get_functions_count = eswin_pmux_get_functions_count,
	.get_function_name = eswin_pmux_get_function_name,
	.get_function_groups = eswin_pmux_get_function_groups,
	.set_mux = eswin_pinmux_set_mux,
};

/* pinconfig */
static int eswin_pinconf_cfg_get(struct pinctrl_dev *pctldev,
		   unsigned pin,
			  unsigned long *config)
{
	u32 reg=0;
	int ret=0;
	unsigned int arg = 0;
	unsigned int param = pinconf_to_config_param(*config);
	struct eswin_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);

	if (pin >= ESWIN_MIO_NUM){
		return -ENOTSUPP;
	}
	reg = readl(pctrl->base + 4*pin);
	if (ret)
		return -EIO;

	switch (param) {
	case PIN_CONFIG_BIAS_PULL_UP:
	if (!(arg=(reg & ESWIN_PINCONF_PULLUP))){
		return -EINVAL;}
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
	if (!(arg = (reg & ESWIN_PINCONF_PULLDOWN))){
		return -EINVAL;}
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
	arg = (reg & (ESWIN_PINCONF_DRIVER_STRENGTH_MASK
						   <<ESWIN_PINCONF_DRIVER_SHIFT));
		break;
	case PIN_CONFIG_INPUT_ENABLE:
	if (!(arg = (reg & ESWIN_PINCONF_IE))){
		return -EINVAL;}
	break;
	case PIN_CONFIG_INPUT_SCHMITT:
	if (!(arg = (reg & ESWIN_PINCONF_SMT))){
		return -EINVAL;}
	break;
	default:
	return -ENOTSUPP;
	}
	*config = pinconf_to_config_packed(param, arg);
	return 0;
}

static int eswin_pinconf_cfg_set(struct pinctrl_dev *pctldev,
			unsigned pin,
			unsigned long *configs,
			unsigned num_configs)
{
	int i=0, ret=0;
	u32 reg = 0;
	struct eswin_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);

	if (pin >= ESWIN_MIO_NUM)
		return -ENOTSUPP;
	reg = readl(pctrl->base + 4*pin);

	if (ret)
		return -EIO;

	for (i = 0; i < num_configs; i++) {
		unsigned int param = pinconf_to_config_param(configs[i]);
		unsigned int arg = pinconf_to_config_argument(configs[i]);

		switch (param) {
			case PIN_CONFIG_INPUT_ENABLE:
				reg &=~ESWIN_PINCONF_IE;
				reg |= (arg<<0);
				break;
			case PIN_CONFIG_BIAS_PULL_UP:
				reg &=~ESWIN_PINCONF_PULLUP;
				reg |= (arg<<1);
				break;
			case PIN_CONFIG_BIAS_PULL_DOWN:
				reg &=~ESWIN_PINCONF_PULLDOWN;
				reg |= (arg<<2);
				break;
			case PIN_CONFIG_DRIVE_STRENGTH:
				reg &= ~(ESWIN_PINCONF_DRIVER_STRENGTH_MASK<<3);
				reg |= (arg<<3);
				break;
			case PIN_CONFIG_INPUT_SCHMITT:
				reg &= ~ESWIN_PINCONF_SMT;
				reg |= (arg<<7);
				break;
			default:
				dev_warn(pctldev->dev,
					"unsupported configuration parameter '%u'\n",
					param);
				continue;
		}


	}
		writel(reg,pctrl->base + 4*pin);

	if (ret)
		return -EIO;
	return 0;
}

static int eswin_pinconf_group_set(struct pinctrl_dev *pctldev,
			unsigned selector,
			unsigned long *configs,
			unsigned num_configs)

{
	int i=0, ret=0;
	struct eswin_pinctrl *pctrl = pinctrl_dev_get_drvdata(pctldev);
	const struct eswin_group_desc *pgrp = &pctrl->groups[selector];

	for (i = 0; i < pgrp->npins; i++) {
		ret = eswin_pinconf_cfg_set(pctldev, pgrp->pins[i], configs,
					 num_configs);
	if (ret)
		  return ret;
	}
	return 0;
}

static const struct pinconf_ops eswin_pinconf_ops = {
	.is_generic = true,
	.pin_config_get = eswin_pinconf_cfg_get,
	.pin_config_set = eswin_pinconf_cfg_set,
	.pin_config_group_set = eswin_pinconf_group_set,
};

static struct pinctrl_desc eswin_desc = {
	.name = "eswin_pinctrl",
	.pins = eswin_pins,
	.npins = ARRAY_SIZE(eswin_pins),
	.pctlops = &eswin_pinctrl_ops,
	.pmxops = &eswin_pinmux_ops,
	.confops = &eswin_pinconf_ops,
	.owner = THIS_MODULE,
};

static int eswin_pinctrl_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct eswin_pinctrl *pctrl;
	pctrl = devm_kzalloc(&pdev->dev, sizeof(*pctrl), GFP_KERNEL);
	if (!pctrl)
		return -ENOMEM;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;
	pctrl->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(pctrl->base))
		return PTR_ERR(pctrl->base);

	pctrl->groups = eswin_pinctrl_groups;
	pctrl->ngroups = ARRAY_SIZE(eswin_pinctrl_groups);
	pctrl->funcs = eswin_pinmux_functions;
	pctrl->nfuncs = ARRAY_SIZE(eswin_pinmux_functions);

	pctrl->pctrl = devm_pinctrl_register(&pdev->dev, &eswin_desc, pctrl);
	if (IS_ERR(pctrl->pctrl)){
		return PTR_ERR(pctrl->pctrl);
	}

	platform_set_drvdata(pdev, pctrl);

	dev_info(&pdev->dev, "eswin pinctrl initialized\n");

	return 0;
}

static void eswin_pinctrl_remove(struct platform_device *platform_dev)
{
	struct eswin_pinctrl *eswin_pinctrl_ptr = platform_get_drvdata(platform_dev);
	pinctrl_unregister(eswin_pinctrl_ptr->pctrl);
}

static const struct of_device_id eswin_pinctrl_of_match[] = {
	{ .compatible = "eswin,eic7700-pinctrl" },
	{ }
};

static struct platform_driver eswin_pinctrl_driver = {
	.driver = {
		.name = "eswin-pinctrl",
		.of_match_table = eswin_pinctrl_of_match,
	},
	.probe = eswin_pinctrl_probe,
	.remove = eswin_pinctrl_remove,
};

static int __init eswin_pinctrl_init(void)
{
	return platform_driver_register(&eswin_pinctrl_driver);
}
postcore_initcall(eswin_pinctrl_init);

static void __exit eswin_pinctrl_exit(void)
{
	platform_driver_unregister(&eswin_pinctrl_driver);
}
module_exit(eswin_pinctrl_exit);

MODULE_DESCRIPTION("ESWIN Pinctrl Controller Platform Device Drivers");
MODULE_AUTHOR("luyulin@eswincomputing.com");
MODULE_LICENSE("GPL");
