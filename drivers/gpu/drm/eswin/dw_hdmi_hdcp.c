/*
 * Copyright (C) ESWIN Electronics Co.Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/hdmi.h>
#include <linux/iopoll.h>
#include <linux/irq.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/spinlock.h>
#include <linux/soc/eswin/eswin_vendor_storage.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/module.h>
#include "dw_hdmi.h"
#include "dw_hdmi_hdcp.h"

#define HDCP_KEY_PATH "/usr/hdcp1.4_key/Tx_A2_TestDPK_encrypted"

#define HDCP_KEY_SIZE 308
#define HDCP_KEY_SEED_SIZE 2

#define KSV_LEN 5
#define HEADER 10
#define SHAMAX 20

#define MAX_DOWNSTREAM_DEVICE_NUM 5
#define DPK_WR_OK_TIMEOUT_US 30000
#define HDMI_HDCP1X_ID 5

/* HDCP DCP KEY & SEED */
const u8 hdcp_const_data[320] = {
	/* 0     1     2     3     4     5     6    */
	0x00,
	0x00,
	0xf0,
	0xff,
	0xff,
	0x00,
	0x00,
	0x00, //KSV
	0x91,
	0x71,
	0x7,
	0x42,
	0x86,
	0xC1,
	0xD1,
	0x89,
	0x0E,
	0x2D,
	0xFF,
	0x92,
	0x95,
	0x28,
	0xF4,
	0x7D,
	0x7B,
	0x1F,
	0x2A,
	0xD9,
	0xBB,
	0xE4,
	0xFD,
	0x10,
	0x18,
	0xAA,
	0xFB,
	0x99,
	0x5A,
	0x83,
	0x97,
	0xD5,
	0xDA,
	0x85,
	0x2D,
	0x52,
	0x8B,
	0xB5,
	0xB2,
	0x49,
	0xDC,
	0x64,
	0xC6,
	0x62,
	0xF0,
	0xDB,
	0xAA,
	0x48,
	0x2E,
	0x84,
	0xAD,
	0x21,
	0xCD,
	0xB9,
	0xD6,
	0x47,
	0xC7,
	0xD7,
	0xD1,
	0x9F,
	0xD4,
	0xB1,
	0x29,
	0x4E,
	0x98,
	0xC6,
	0xAE,
	0xA4,
	0xF5,
	0xA6,
	0xFE,
	0x68,
	0x3D,
	0x43,
	0x97,
	0x7B,
	0x52,
	0xC7,
	0xA1,
	0x65,
	0x7B,
	0xF9,
	0x8C,
	0xCC,
	0x20,
	0x8C,
	0xCB,
	0x2F,
	0x7D,
	0xFA,
	0xC5,
	0x80,
	0xD8,
	0xDB,
	0x5A,
	0x72,
	0x2D,
	0xE1,
	0xA6,
	0x79,
	0xF4,
	0xAE,
	0x96,
	0x1D,
	0xE8,
	0x28,
	0x85,
	0x5F,
	0xBD,
	0x64,
	0xF8,
	0xBF,
	0x7A,
	0xE7,
	0xFF,
	0xBC,
	0x1F,
	0xC6,
	0x75,
	0x56,
	0xB9,
	0xF9,
	0x0F,
	0x36,
	0x29,
	0x5A,
	0x3B,
	0xF3,
	0x76,
	0x7B,
	0x8B,
	0xF8,
	0xFD,
	0x13,
	0x80,
	0x49,
	0xAB,
	0x5C,
	0x12,
	0x63,
	0xB9,
	0xE7,
	0x91,
	0x2A,
	0xBA,
	0x82,
	0xF3,
	0xCD,
	0xFA,
	0xFB,
	0x4E,
	0xA7,
	0xE1,
	0xBD,
	0x8B,
	0xC3,
	0x24,
	0xEC,
	0x31,
	0xBC,
	0x1,
	0xB1,
	0xCE,
	0x9A,
	0x4,
	0x9C,
	0x69,
	0x5D,
	0xBA,
	0x3C,
	0xF7,
	0x97,
	0x50,
	0x88,
	0xE2,
	0xA2,
	0xE1,
	0x3,
	0xDB,
	0x39,
	0xDD,
	0x93,
	0x0A,
	0x24,
	0x5C,
	0x6E,
	0x17,
	0xE9,
	0x1,
	0x4C,
	0x25,
	0xF5,
	0x9,
	0x24,
	0xC6,
	0x91,
	0xC6,
	0x6A,
	0x7A,
	0x40,
	0x89,
	0x62,
	0x7F,
	0xED,
	0x6B,
	0x8E,
	0x5F,
	0x79,
	0xAD,
	0xF2,
	0x50,
	0x59,
	0xC4,
	0x11,
	0x2E,
	0x1,
	0xC2,
	0xDC,
	0x8,
	0xCE,
	0xDC,
	0x51,
	0x14,
	0xF4,
	0x8C,
	0x3D,
	0x9E,
	0xB7,
	0x16,
	0xB3,
	0x9C,
	0xF3,
	0x55,
	0xC0,
	0xCE,
	0x74,
	0x5B,
	0x19,
	0x4E,
	0xF5,
	0x39,
	0x37,
	0xA6,
	0xEA,
	0xB5,
	0x20,
	0xBF,
	0xD7,
	0x79,
	0x24,
	0xE2,
	0x8D,
	0x13,
	0xBC,
	0x38,
	0x10,
	0x60,
	0x93,
	0xAE,
	0x70,
	0xA9,
	0x66,
	0x81,
	0xF3,
	0x19,
	0xEC,
	0x45,
	0xEC,
	0xE5,
	0x5,
	0x47,
	0xE4,
	0x67,
	0x65,
	0x4C,
	0x62,
	0x1,
	0x98,
	0xA3,
	0x52,
	//SHA1
	0x18,
	0xb4,
	0x70,
	0x59,
	0xfe,
	0x13,
	0x38,
	0xc4,
	0x15,
	0xae,
	0xf0,
	0x81,
	0xcb,
	0x96,
	0x27,
	0xe7,
	0xd9,
	0x7b,
	0xc5,
	0x27,
	0x20, //seed 0x2020
	0x20,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
	0x00,
};

/* HDCP Registers */
#define HDMI_A_KSVMEMCTRL 0x5016
#define HDMI_HDCPREG_ANCONF 0x7805
#define HDMI_HDCPREG_AN0 0x7806
#define HDMI_HDCPREG_AN1 0x7807
#define HDMI_HDCPREG_AN2 0x7808
#define HDMI_HDCPREG_AN3 0x7809
#define HDMI_HDCPREG_AN4 0x780a
#define HDMI_HDCPREG_AN5 0x780b
#define HDMI_HDCPREG_AN6 0x780c
#define HDMI_HDCPREG_AN7 0x780d
#define HDMI_HDCPREG_RMCTL 0x780e
#define HDMI_HDCPREG_RMSTS 0x780f
#define HDMI_HDCPREG_SEED0 0x7810
#define HDMI_HDCPREG_SEED1 0x7811
#define HDMI_HDCPREG_DPK0 0x7812
#define HDMI_HDCPREG_DPK1 0x7813
#define HDMI_HDCPREG_DPK2 0x7814
#define HDMI_HDCPREG_DPK3 0x7815
#define HDMI_HDCPREG_DPK4 0x7816
#define HDMI_HDCPREG_DPK5 0x7817
#define HDMI_HDCPREG_DPK6 0x7818
#define HDMI_HDCP2REG_CTRL 0x7904
#define HDMI_HDCP2REG_MASK 0x790c
#define HDMI_HDCP2REG_MUTE 0x7912

enum dw_hdmi_hdcp_state {
	DW_HDCP_DISABLED,
	DW_HDCP_AUTH_START,
	DW_HDCP_AUTH_SUCCESS,
	DW_HDCP_AUTH_FAIL,
};

enum {
	DW_HDMI_HDCP_KSV_LEN = 8,
	DW_HDMI_HDCP_SHA_LEN = 20,
	DW_HDMI_HDCP_DPK_LEN = 280,
	DW_HDMI_HDCP_KEY_LEN = 308,
	DW_HDMI_HDCP_SEED_LEN = 2,
};

enum {
	HDMI_MC_CLKDIS_HDCPCLK_MASK = 0x40,
	HDMI_MC_CLKDIS_HDCPCLK_ENABLE = 0x00,

	HDMI_A_SRMCTRL_SHA1_FAIL_MASK = 0X08,
	HDMI_A_SRMCTRL_SHA1_FAIL_DISABLE = 0X00,
	HDMI_A_SRMCTRL_SHA1_FAIL_ENABLE = 0X08,

	HDMI_A_SRMCTRL_KSV_UPDATE_MASK = 0X04,
	HDMI_A_SRMCTRL_KSV_UPDATE_DISABLE = 0X00,
	HDMI_A_SRMCTRL_KSV_UPDATE_ENABLE = 0X04,

	HDMI_A_SRMCTRL_KSV_MEM_REQ_MASK = 0X01,
	HDMI_A_SRMCTRL_KSV_MEM_REQ_DISABLE = 0X00,
	HDMI_A_SRMCTRL_KSV_MEM_REQ_ENABLE = 0X01,

	HDMI_A_SRMCTRL_KSV_MEM_ACCESS_MASK = 0X02,
	HDMI_A_SRMCTRL_KSV_MEM_ACCESS_DISABLE = 0X00,
	HDMI_A_SRMCTRL_KSV_MEM_ACCESS_ENABLE = 0X02,

	HDMI_A_SRM_BASE_MAX_DEVS_EXCEEDED = 0x80,
	HDMI_A_SRM_BASE_DEVICE_COUNT = 0x7f,

	HDMI_A_SRM_BASE_MAX_CASCADE_EXCEEDED = 0x08,

	HDMI_A_APIINTSTAT_KSVSHA1_CALC_INT = 0x02,
	HDMI_A_APIINTSTAT_KSVSHA1_CALC_DONE_INT = 0x20,
	/* HDCPREG_RMSTS field values */
	DPK_WR_OK_STS = 0x40,

	HDMI_A_HDCP22_MASK = 0x40,

	HDMI_HDCP2_OVR_EN_MASK = 0x02,
	HDMI_HDCP2_OVR_ENABLE = 0x02,
	HDMI_HDCP2_OVR_DISABLE = 0x00,

	HDMI_HDCP2_FORCE_MASK = 0x04,
	HDMI_HDCP2_FORCE_ENABLE = 0x04,
	HDMI_HDCP2_FORCE_DISABLE = 0x00,
	HDMI_A_KSVMEMCTRL_KSV_SHA1_STATUS = 0x08,
};

static struct dw_hdcp *g_hdcp;
static int trytimes = 0;

static void hdcp_modb(struct dw_hdcp *hdcp, u8 data, u8 mask, unsigned int reg)
{
	struct dw_hdmi *hdmi = hdcp->hdmi;
	u8 val = hdcp->read(hdmi, reg) & ~mask;

	val |= data & mask;
	hdcp->write(hdmi, val, reg);
}

static int hdcp_load_keys_cb(struct dw_hdcp *hdcp)
{
	u32 size;
	u8 hdcp_vendor_data[320];
	int i;
#if 0
    int j;
    struct file *fp;
    loff_t pos = 0;
    ssize_t nread;
#endif
	hdcp->keys = kmalloc(HDCP_KEY_SIZE, GFP_KERNEL);
	if (!hdcp->keys)
		return -ENOMEM;

	hdcp->seeds = kmalloc(HDCP_KEY_SEED_SIZE, GFP_KERNEL);
	if (!hdcp->seeds) {
		kfree(hdcp->keys);
		return -ENOMEM;
	}
#if 1
	size = eswin_vendor_read(HDMI_HDCP1X_ID, hdcp_vendor_data, 314);

	for (i = 0; i < sizeof(hdcp_vendor_data); i++)
		hdcp_vendor_data[i] = hdcp_const_data[i];
	size = 320;

	if (size < (HDCP_KEY_SIZE + HDCP_KEY_SEED_SIZE)) {
		dev_dbg(hdcp->dev, "HDCP: read size %d\n", size);
		memset(hdcp->keys, 0, HDCP_KEY_SIZE);
		memset(hdcp->seeds, 0, HDCP_KEY_SEED_SIZE);
	} else {
		memcpy(hdcp->keys, hdcp_vendor_data, HDCP_KEY_SIZE);
		memcpy(hdcp->seeds, hdcp_vendor_data + HDCP_KEY_SIZE,
		       HDCP_KEY_SEED_SIZE);
	}
#else
	fp = filp_open(HDCP_KEY_PATH, O_RDONLY, 0644);
	if (IS_ERR(fp)) {
		printk("Error, Tx_A2_TestDPK_encrypted.txt doesn't exist.\n");
		return 0;
	}

	nread = kernel_read(fp, hdcp_vendor_data, sizeof(hdcp_vendor_data),
			    &pos);

	if (nread != sizeof(hdcp_vendor_data)) {
		printk("Error, failed to read %ld bytes to non volatile memory area,ret %ld\n",
		       sizeof(hdcp_vendor_data), nread);
		return -EIO;
	}

	memcpy(hdcp->keys, hdcp_vendor_data, HDCP_KEY_SIZE);
	memcpy(hdcp->seeds, hdcp_vendor_data + HDCP_KEY_SIZE,
	       HDCP_KEY_SEED_SIZE);

	filp_close(fp, NULL);
#endif

	return 0;
}

static int dw_hdmi_hdcp_load_key(struct dw_hdcp *hdcp)
{
	int i, j;
	int ret, val;
	void __iomem *reg_rmsts_addr;
	struct hdcp_keys *hdcp_keys;
	struct dw_hdmi *hdmi = hdcp->hdmi;

	if (!hdcp->keys) {
		ret = hdcp_load_keys_cb(hdcp);
		if (ret)
			return ret;
	}
	hdcp_keys = hdcp->keys;

	if (hdcp->reg_io_width == 4)
		reg_rmsts_addr = hdcp->regs + (HDMI_HDCPREG_RMSTS << 2);
	else if (hdcp->reg_io_width == 1)
		reg_rmsts_addr = hdcp->regs + HDMI_HDCPREG_RMSTS;
	else
		return -EPERM;

	/* Disable decryption logic */
	hdcp->write(hdmi, 0, HDMI_HDCPREG_RMCTL);
	ret = readx_poll_timeout(readl, reg_rmsts_addr, val,
				 val & DPK_WR_OK_STS, 1000,
				 DPK_WR_OK_TIMEOUT_US);
	if (ret)
		return ret;

	hdcp->write(hdmi, 0, HDMI_HDCPREG_DPK6);
	hdcp->write(hdmi, 0, HDMI_HDCPREG_DPK5);

	/* The useful data in ksv should be 5 byte */
	for (i = 4; i >= 0; i--)
		hdcp->write(hdmi, hdcp_keys->KSV[i], HDMI_HDCPREG_DPK0 + i);
	ret = readx_poll_timeout(readl, reg_rmsts_addr, val,
				 val & DPK_WR_OK_STS, 1000,
				 DPK_WR_OK_TIMEOUT_US);

	if (ret)
		return ret;

	/* Enable decryption logic */
	if (hdcp->seeds) {
		hdcp->write(hdmi, 1, HDMI_HDCPREG_RMCTL);
		hdcp->write(hdmi, hdcp->seeds[0], HDMI_HDCPREG_SEED1);
		hdcp->write(hdmi, hdcp->seeds[1], HDMI_HDCPREG_SEED0);
	} else {
		hdcp->write(hdmi, 0, HDMI_HDCPREG_RMCTL);
	}

	/* Write encrypt device private key */
	for (i = 0; i < DW_HDMI_HDCP_DPK_LEN - 6; i += 7) {
		for (j = 6; j >= 0; j--)
			hdcp->write(hdmi, hdcp_keys->devicekey[i + j],
				    HDMI_HDCPREG_DPK0 + j);
		ret = readx_poll_timeout(readl, reg_rmsts_addr, val,
					 val & DPK_WR_OK_STS, 1000,
					 DPK_WR_OK_TIMEOUT_US);

		if (ret)
			return ret;
	}
	return 0;
}

static int dw_hdmi_hdcp1x_start(struct dw_hdcp *hdcp)
{
	struct dw_hdmi *hdmi = hdcp->hdmi;
	int i;
	int val;
	u8 An[8];

	if (!hdcp->enable)
		return -EPERM;

	if (hdcp->status == DW_HDCP_AUTH_START ||
	    hdcp->status == DW_HDCP_AUTH_SUCCESS)
		return 0;

	/* disable the pixel clock*/
	dev_dbg(hdcp->dev, "start hdcp with disable hdmi pixel clock\n");
	hdcp_modb(hdcp, HDMI_MC_CLKDIS_PIXELCLK_DISABLE,
		  HDMI_MC_CLKDIS_PIXELCLK_MASK, HDMI_MC_CLKDIS);

	/* Update An */
	get_random_bytes(&An, sizeof(An));
	for (i = 0; i < 8; i++)
		hdcp->write(hdmi, An[i], HDMI_HDCPREG_AN0 + i);

	hdcp->write(hdmi, 0x01, HDMI_HDCPREG_ANCONF);

	if (!(hdcp->read(hdmi, HDMI_HDCPREG_RMSTS) & 0x3f))
		dw_hdmi_hdcp_load_key(hdcp);

	if (hdcp->hdcp2) {
		for (i = 0; i < 100; i++) {
			if (hdcp->hdcp2->wait_hdcp2_reset) {
				msleep(80);
			} else {
				break;
			}
		}
		printk("wait_hdcp2_reset i = %d\n", i);
	}
	val = hdcp->read(hdmi, HDMI_HDCP2REG_CTRL);
	dev_dbg(hdcp->dev, "before set HDMI_HDCP2REG_CTRL val = %d\n", val);

	hdcp_modb(hdcp, HDMI_FC_INVIDCONF_HDCP_KEEPOUT_ACTIVE,
		  HDMI_FC_INVIDCONF_HDCP_KEEPOUT_MASK, HDMI_FC_INVIDCONF);

	hdcp->remaining_times = hdcp->retry_times;
	if (hdcp->read(hdmi, HDMI_CONFIG1_ID) & HDMI_A_HDCP22_MASK) {
		hdcp_modb(hdcp,
			  HDMI_HDCP2_OVR_ENABLE | HDMI_HDCP2_FORCE_DISABLE,
			  HDMI_HDCP2_OVR_EN_MASK | HDMI_HDCP2_FORCE_MASK,
			  HDMI_HDCP2REG_CTRL);
		hdcp->write(hdmi, 0xff, HDMI_HDCP2REG_MASK);
		hdcp->write(hdmi, 0xff, HDMI_HDCP2REG_MUTE);
	}

	hdcp->write(hdmi, 0x40, HDMI_A_OESSWCFG);
	hdcp_modb(hdcp,
		  HDMI_A_HDCPCFG0_BYPENCRYPTION_DISABLE |
			  HDMI_A_HDCPCFG0_EN11FEATURE_DISABLE |
			  HDMI_A_HDCPCFG0_SYNCRICHECK_ENABLE,
		  HDMI_A_HDCPCFG0_BYPENCRYPTION_MASK |
			  HDMI_A_HDCPCFG0_EN11FEATURE_MASK |
			  HDMI_A_HDCPCFG0_SYNCRICHECK_MASK,
		  HDMI_A_HDCPCFG0);

	hdcp_modb(hdcp,
		  HDMI_A_HDCPCFG1_ENCRYPTIONDISABLE_ENABLE |
			  HDMI_A_HDCPCFG1_PH2UPSHFTENC_ENABLE,
		  HDMI_A_HDCPCFG1_ENCRYPTIONDISABLE_MASK |
			  HDMI_A_HDCPCFG1_PH2UPSHFTENC_MASK,
		  HDMI_A_HDCPCFG1);

	/* Reset HDCP Engine */
	if (hdcp->read(hdmi, HDMI_MC_CLKDIS) & HDMI_MC_CLKDIS_HDCPCLK_MASK) {
		hdcp_modb(hdcp, HDMI_A_HDCPCFG1_SWRESET_ASSERT,
			  HDMI_A_HDCPCFG1_SWRESET_MASK, HDMI_A_HDCPCFG1);
	}

	hdcp->write(hdmi, 0x00, HDMI_A_APIINTMSK);
	hdcp_modb(hdcp, HDMI_A_HDCPCFG0_RXDETECT_ENABLE,
		  HDMI_A_HDCPCFG0_RXDETECT_MASK, HDMI_A_HDCPCFG0);

	/*
     * XXX: to sleep 500ms here between output hdmi and enable hdcpclk,
     * otherwise hdcp auth fail when Connect to repeater
     */
	msleep(500);
	hdcp_modb(hdcp, HDMI_MC_CLKDIS_HDCPCLK_ENABLE,
		  HDMI_MC_CLKDIS_HDCPCLK_MASK, HDMI_MC_CLKDIS);

	hdcp->status = DW_HDCP_AUTH_START;
	dev_info(hdcp->dev, "%s success\n", __func__);

	/* enable the pixel clock*/
	dev_dbg(hdcp->dev, "start hdcp with enable hdmi pixel clock\n");
	hdcp_modb(hdcp, HDMI_MC_CLKDIS_PIXELCLK_ENABLE,
		  HDMI_MC_CLKDIS_PIXELCLK_MASK, HDMI_MC_CLKDIS);

	return 0;
}

static int dw_hdmi_hdcp1x_stop(struct dw_hdcp *hdcp)
{
	struct dw_hdmi *hdmi = hdcp->hdmi;
	u8 val;
	bool phy_enable = false;

	if (!hdcp->enable)
		return -EPERM;

	val = hdcp->read(hdmi, HDMI_PHY_CONF0);
	if (val & HDMI_PHY_CONF0_GEN2_TXPWRON_MASK) {
		phy_enable = true;
	}

	dev_dbg(hdcp->dev, "dw_hdmi_hdcp1x_stop\n");
	if (phy_enable) {
		dev_dbg(hdcp->dev, "stop hdcp with disable hdmi pixel clock\n");
		hdcp_modb(hdcp, HDMI_MC_CLKDIS_PIXELCLK_DISABLE,
			  HDMI_MC_CLKDIS_PIXELCLK_MASK, HDMI_MC_CLKDIS);
	}

	hdcp_modb(hdcp, HDMI_MC_CLKDIS_HDCPCLK_DISABLE,
		  HDMI_MC_CLKDIS_HDCPCLK_MASK, HDMI_MC_CLKDIS);
	hdcp->write(hdmi, 0xff, HDMI_A_APIINTMSK);

	hdcp_modb(hdcp, HDMI_A_HDCPCFG0_RXDETECT_DISABLE,
		  HDMI_A_HDCPCFG0_RXDETECT_MASK, HDMI_A_HDCPCFG0);

	hdcp_modb(hdcp,
		  HDMI_A_SRMCTRL_SHA1_FAIL_DISABLE |
			  HDMI_A_SRMCTRL_KSV_UPDATE_DISABLE,
		  HDMI_A_SRMCTRL_SHA1_FAIL_MASK |
			  HDMI_A_SRMCTRL_KSV_UPDATE_MASK,
		  HDMI_A_SRMCTRL);

	hdcp->status = DW_HDCP_DISABLED;

	if (phy_enable) {
		dev_dbg(hdcp->dev, "stop hdcp with enable hdmi pixel clock\n");
		hdcp_modb(hdcp, HDMI_MC_CLKDIS_PIXELCLK_ENABLE,
			  HDMI_MC_CLKDIS_PIXELCLK_MASK, HDMI_MC_CLKDIS);
	}

	return 0;
}

void dw_hdmi_hdcp2_init(struct dw_hdcp2 *hdcp2)
{
	if (g_hdcp)
		g_hdcp->hdcp2 = hdcp2;
}
EXPORT_SYMBOL_GPL(dw_hdmi_hdcp2_init);

void dw_hdmi_hdcp2_remove(void)
{
	printk("func: %s; line: %d\n", __func__, __LINE__);
	if (g_hdcp->hdcp2)
		g_hdcp->hdcp2->stop();
	g_hdcp->hdcp2 = NULL;
}
EXPORT_SYMBOL_GPL(dw_hdmi_hdcp2_remove);

void dw_hdmi_hdcp2_start(int enable)
{
	int val;

	if (!(g_hdcp->hdcp2))
		return;

	dev_dbg(g_hdcp->dev, "%s enable = %d\n", __func__, enable);
	if (enable == 0) {
		hdcp_modb(g_hdcp,
			  HDMI_HDCP2_OVR_ENABLE | HDMI_HDCP2_FORCE_DISABLE,
			  HDMI_HDCP2_OVR_EN_MASK | HDMI_HDCP2_FORCE_MASK,
			  HDMI_HDCP2REG_CTRL);
		hdcp_modb(g_hdcp, HDMI_MC_CLKDIS_HDCPCLK_DISABLE,
			  HDMI_MC_CLKDIS_HDCPCLK_MASK, HDMI_MC_CLKDIS);
	} else if (enable == 1) {
		hdcp_modb(g_hdcp, HDMI_MC_CLKDIS_HDCPCLK_ENABLE,
			  HDMI_MC_CLKDIS_HDCPCLK_MASK, HDMI_MC_CLKDIS);
		hdcp_modb(g_hdcp,
			  HDMI_HDCP2_OVR_ENABLE | HDMI_HDCP2_FORCE_ENABLE,
			  HDMI_HDCP2_OVR_EN_MASK | HDMI_HDCP2_FORCE_MASK,
			  HDMI_HDCP2REG_CTRL);
		hdcp_modb(g_hdcp, HDMI_FC_INVIDCONF_HDCP_KEEPOUT_ACTIVE,
			  HDMI_FC_INVIDCONF_HDCP_KEEPOUT_MASK,
			  HDMI_FC_INVIDCONF);
	} else if (enable == 2) {
		val = g_hdcp->read(g_hdcp->hdmi, HDMI_PHY_STAT0);
		if (val & HDMI_PHY_HPD)
			dw_hdmi_hdcp1x_start(g_hdcp);
	} else if (enable == 3) {
		if (g_hdcp->hdcp2 && g_hdcp->hdcp2->enable &&
		    (tv_hdmi_hdcp2_support(g_hdcp->hdmi) == 1)) {
			if (g_hdcp->status != DW_HDCP_DISABLED)
				dw_hdmi_hdcp1x_stop(g_hdcp);
			g_hdcp->hdcp2->start();
		}
	}
}
EXPORT_SYMBOL_GPL(dw_hdmi_hdcp2_start);

static int dw_hdmi_hdcp_start(struct dw_hdcp *hdcp)
{
	if (hdcp->hdcp2 && hdcp->hdcp2->enable &&
	    (tv_hdmi_hdcp2_support(hdcp->hdmi) == 1)) {
		// hdcp->hdcp2->start();
		return 0;
	}
	return dw_hdmi_hdcp1x_start(hdcp);
}

static int dw_hdmi_hdcp_stop(struct dw_hdcp *hdcp)
{
	if (hdcp->hdcp2 && hdcp->hdcp2->hot_plug) {
		// g_hdcp->hdcp2->stop();
		printk("func: %s; line: %d\n", __func__, __LINE__);
	}

	return dw_hdmi_hdcp1x_stop(hdcp);
}

static void dw_hdmi_hdcp_isr(struct dw_hdcp *hdcp, int hdcp_int)
{
	struct dw_hdmi *hdmi = hdcp->hdmi;
	int val;

	dev_info(hdcp->dev, "hdcp_int is 0x%02x\n", hdcp_int);

	if (hdcp_int & HDMI_A_APIINTSTAT_KSVSHA1_CALC_DONE_INT) {
		dev_dbg(hdcp->dev, "hdcp sink is a repeater\n");
		val = hdcp->read(hdmi, HDMI_A_KSVMEMCTRL);
		if (val | HDMI_A_KSVMEMCTRL_KSV_SHA1_STATUS) {
			dev_dbg(hdcp->dev,
				"hdcp verifivation failed, waiting hdmi controller re-authentication!\n");
		} else {
			dev_dbg(hdcp->dev, "hdcp verifivation succeeded!\n");
			/* reset HDCP */
			hdcp_modb(hdcp, HDMI_A_HDCPCFG1_SWRESET_ASSERT,
				  HDMI_A_HDCPCFG1_SWRESET_MASK,
				  HDMI_A_HDCPCFG1);
		}
	}

	if (hdcp_int & 0x40) {
		hdcp->status = DW_HDCP_AUTH_FAIL;
		dev_info(hdcp->dev, "hdcp auth fail\n");
		if (hdcp->remaining_times > 1)
			hdcp->remaining_times--;
		else if (hdcp->remaining_times == 1)
			hdcp_modb(hdcp,
				  HDMI_A_HDCPCFG1_ENCRYPTIONDISABLE_DISABLE,
				  HDMI_A_HDCPCFG1_ENCRYPTIONDISABLE_MASK,
				  HDMI_A_HDCPCFG1);
	} else if (hdcp_int & 0x80) {
		dev_info(hdcp->dev, "hdcp auth success\n");
		hdcp->status = DW_HDCP_AUTH_SUCCESS;
	} else if (hdcp_int & 0x10) {
		dev_info(hdcp->dev, "i2c nack\n");
		trytimes++;
		if (trytimes == 20) {
			trytimes = 0;
			dw_hdmi_hdcp1x_stop(hdcp);
		}
	}
}

static ssize_t hdcp_enable_read(struct device *device,
				struct device_attribute *attr, char *buf)
{
	bool enable = 0;
	struct dw_hdcp *hdcp = g_hdcp;

	if (hdcp)
		enable = hdcp->enable;

	return snprintf(buf, PAGE_SIZE, "%d\n", enable);
}

static ssize_t hdcp_enable_write(struct device *device,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	bool enable;
	struct dw_hdcp *hdcp = g_hdcp;

	if (!hdcp)
		return -EINVAL;

	if (kstrtobool(buf, &enable))
		return -EINVAL;

	if (hdcp->enable != enable) {
		if (enable) {
			hdcp->enable = enable;
			if (hdcp->hdcp2 && hdcp->hdcp2->hot_plug) {
				return count;
			}

			if (hdcp->read(hdcp->hdmi, HDMI_PHY_STAT0) &
			    HDMI_PHY_HPD) {
				dw_hdmi_hdcp1x_start(hdcp);
			}
		} else {
			if (hdcp->status != DW_HDCP_DISABLED) {
				dw_hdmi_hdcp1x_stop(hdcp);
			}
			hdcp->enable = enable;
		}
	}

	return count;
}

static DEVICE_ATTR(enable, 0644, hdcp_enable_read, hdcp_enable_write);

static ssize_t hdcp_trytimes_read(struct device *device,
				  struct device_attribute *attr, char *buf)
{
	int trytimes = 0;
	struct dw_hdcp *hdcp = g_hdcp;

	if (hdcp)
		trytimes = hdcp->retry_times;

	return snprintf(buf, PAGE_SIZE, "%d\n", trytimes);
}

static ssize_t hdcp_trytimes_write(struct device *device,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int trytimes;
	struct dw_hdcp *hdcp = g_hdcp;

	if (!hdcp)
		return -EINVAL;

	if (kstrtoint(buf, 0, &trytimes))
		return -EINVAL;

	if (hdcp->retry_times != trytimes) {
		hdcp->retry_times = trytimes;
		hdcp->remaining_times = hdcp->retry_times;
	}

	return count;
}

static DEVICE_ATTR(trytimes, 0644, hdcp_trytimes_read, hdcp_trytimes_write);

static ssize_t hdcp_status_read(struct device *device,
				struct device_attribute *attr, char *buf)
{
	int status = DW_HDCP_DISABLED;
	struct dw_hdcp *hdcp = g_hdcp;

	if (hdcp)
		status = hdcp->status;

	if (status == DW_HDCP_DISABLED)
		return snprintf(buf, PAGE_SIZE, "hdcp disable\n");
	else if (status == DW_HDCP_AUTH_START)
		return snprintf(buf, PAGE_SIZE, "hdcp_auth_start\n");
	else if (status == DW_HDCP_AUTH_SUCCESS)
		return snprintf(buf, PAGE_SIZE, "hdcp_auth_success\n");
	else if (status == DW_HDCP_AUTH_FAIL)
		return snprintf(buf, PAGE_SIZE, "hdcp_auth_fail\n");
	else
		return snprintf(buf, PAGE_SIZE, "unknown status\n");
}

static DEVICE_ATTR(status, 0444, hdcp_status_read, NULL);

static int dw_hdmi_hdcp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct dw_hdcp *hdcp = pdev->dev.platform_data;

	dev_info(&pdev->dev, "%s...\n", __func__);
	g_hdcp = hdcp;
	hdcp->mdev.minor = MISC_DYNAMIC_MINOR;
	hdcp->mdev.name = "hdmi_hdcp1x";
	hdcp->mdev.mode = 0666;

	if (misc_register(&hdcp->mdev)) {
		dev_err(&pdev->dev, "HDCP: Could not add character driver\n");
		return -EINVAL;
	}

	ret = device_create_file(hdcp->mdev.this_device, &dev_attr_enable);
	if (ret) {
		dev_err(&pdev->dev, "HDCP: Could not add sys file enable\n");
		ret = -EINVAL;
		goto error0;
	}

	ret = device_create_file(hdcp->mdev.this_device, &dev_attr_trytimes);
	if (ret) {
		dev_err(&pdev->dev, "HDCP: Could not add sys file trytimes\n");
		ret = -EINVAL;
		goto error1;
	}

	ret = device_create_file(hdcp->mdev.this_device, &dev_attr_status);
	if (ret) {
		dev_err(&pdev->dev, "HDCP: Could not add sys file status\n");
		ret = -EINVAL;
		goto error2;
	}

	/* retry time if hdcp auth fail. unlimited time if set 0 */
	hdcp->retry_times = 0;
	hdcp->dev = &pdev->dev;
	hdcp->hdcp_start = dw_hdmi_hdcp_start;
	hdcp->hdcp_stop = dw_hdmi_hdcp_stop;
	hdcp->hdcp_isr = dw_hdmi_hdcp_isr;

#ifdef CONFIG_DW_HDMI_HDCP1X_ENABLED
	hdcp_enable_write(NULL, NULL, "1", 1);
#endif

	dev_dbg(hdcp->dev, "%s success\n", __func__);
	return 0;

error2:
	device_remove_file(hdcp->mdev.this_device, &dev_attr_trytimes);
error1:
	device_remove_file(hdcp->mdev.this_device, &dev_attr_enable);
error0:
	misc_deregister(&hdcp->mdev);
	return ret;
}

static int dw_hdmi_hdcp_remove(struct platform_device *pdev)
{
	struct dw_hdcp *hdcp = pdev->dev.platform_data;

	device_remove_file(hdcp->mdev.this_device, &dev_attr_trytimes);
	device_remove_file(hdcp->mdev.this_device, &dev_attr_enable);
	device_remove_file(hdcp->mdev.this_device, &dev_attr_status);
	misc_deregister(&hdcp->mdev);

	kfree(hdcp->keys);
	kfree(hdcp->seeds);

	return 0;
}

struct platform_driver dw_hdmi_hdcp_driver = {
    .probe  = dw_hdmi_hdcp_probe,
    .remove = dw_hdmi_hdcp_remove,
    .driver = {
        .name = DW_HDCP_DRIVER_NAME,
    },
};

//module_platform_driver(dw_hdmi_hdcp_driver);
MODULE_DESCRIPTION("DW HDMI transmitter HDCP driver");
MODULE_LICENSE("GPL");
