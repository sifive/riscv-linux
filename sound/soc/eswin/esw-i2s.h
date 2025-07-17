/*
 * Copyright (ST) 2012 Rajeev Kumar (rajeevkumar.linux@gmail.com)
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

/*
 *
 * Copyright (C) 2021 ESWIN, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef __I2S_H
#define __I2S_H

#include <linux/device.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/clk.h>
#include <linux/types.h>
#include <sound/dmaengine_pcm.h>
#include <sound/designware_i2s.h>


/* common register for all channel */
#define IER		0x000
#define IRER		0x004
#define ITER		0x008
#define CER		0x00C
#define CCR		0x010
#define RXFFR		0x014
#define TXFFR		0x018

/* DMA Control Register Offset */
#define DMACR       	0x200
/* DMA Control Register fields */
#define DMAEN_TXBLOCK	BIT(17)
#define DMAEN_RXBLOCK   BIT(16)
#define DMAEN_TXCH_3    BIT(11)
#define DMAEN_TXCH_2    BIT(10)
#define DMAEN_TXCH_1	BIT(9)
#define DMAEN_TXCH_0    BIT(8)
#define DMAEN_RXCH_3	BIT(3)
#define DMAEN_RXCH_2    BIT(2)
#define DMAEN_RXCH_1	BIT(1)
#define DMAEN_RXCH_0    BIT(0)

/* Interrupt status register fields */
#define ISR_TXFO	BIT(5)
#define ISR_TXFE	BIT(4)
#define ISR_RXFO	BIT(1)
#define ISR_RXDA	BIT(0)

/* I2STxRxRegisters for all channels */
#define LRBR_LTHR(x)	(0x40 * x + 0x020)
#define RRBR_RTHR(x)	(0x40 * x + 0x024)
#define RER(x)		(0x40 * x + 0x028)
#define TER(x)		(0x40 * x + 0x02C)
#define RCR(x)		(0x40 * x + 0x030)
#define TCR(x)		(0x40 * x + 0x034)
#define ISR(x)		(0x40 * x + 0x038)
#define IMR(x)		(0x40 * x + 0x03C)
#define ROR(x)		(0x40 * x + 0x040)
#define TOR(x)		(0x40 * x + 0x044)
#define RFCR(x)		(0x40 * x + 0x048)
#define TFCR(x)		(0x40 * x + 0x04C)
#define RFF(x)		(0x40 * x + 0x050)
#define TFF(x)		(0x40 * x + 0x054)

/* I2SCOMPRegisters */
#define I2S_COMP_PARAM_2	0x01F0
#define I2S_COMP_PARAM_1	0x01F4
#define I2S_COMP_VERSION	0x01F8
#define I2S_COMP_TYPE		0x01FC

/* I2S DMA registers */
#define RXDMA_CH(x)		(0x4 * x + 0x204)
#define TXDMA_CH(x)		(0x4 * x + 0x214)

/*
 * Component parameter register fields - define the I2S block's
 * configuration.
 */
#define	COMP1_TX_WORDSIZE_3(r)	(((r) & GENMASK(27, 25)) >> 25)
#define	COMP1_TX_WORDSIZE_2(r)	(((r) & GENMASK(24, 22)) >> 22)
#define	COMP1_TX_WORDSIZE_1(r)	(((r) & GENMASK(21, 19)) >> 19)
#define	COMP1_TX_WORDSIZE_0(r)	(((r) & GENMASK(18, 16)) >> 16)
#define	COMP1_TX_CHANNELS(r)	(((r) & GENMASK(10, 9)) >> 9)
#define	COMP1_RX_CHANNELS(r)	(((r) & GENMASK(8, 7)) >> 7)
#define	COMP1_RX_ENABLED(r)	(((r) & BIT(6)) >> 6)
#define	COMP1_TX_ENABLED(r)	(((r) & BIT(5)) >> 5)
#define	COMP1_MODE_EN(r)	(((r) & BIT(4)) >> 4)
#define	COMP1_FIFO_DEPTH_GLOBAL(r)	(((r) & GENMASK(3, 2)) >> 2)
#define	COMP1_APB_DATA_WIDTH(r)	(((r) & GENMASK(1, 0)) >> 0)
#define	COMP2_RX_WORDSIZE_3(r)	(((r) & GENMASK(12, 10)) >> 10)
#define	COMP2_RX_WORDSIZE_2(r)	(((r) & GENMASK(9, 7)) >> 7)
#define	COMP2_RX_WORDSIZE_1(r)	(((r) & GENMASK(5, 3)) >> 3)
#define	COMP2_RX_WORDSIZE_0(r)	(((r) & GENMASK(2, 0)) >> 0)

/* Number of entries in WORDSIZE and DATA_WIDTH parameter registers */
#define	COMP_MAX_WORDSIZE	(1 << 3)
#define	COMP_MAX_DATA_WIDTH	(1 << 2)
#define MAX_CHANNEL_NUM		2
#define MIN_CHANNEL_NUM		2
#define STEREO		0
#define TDM		1

#define CCR_SCLKG_POS 0
#define CCR_WSS_POS   3

enum {
	CLOCK_CYCLES_16,
	CLOCK_CYCLES_24,
	CLOCK_CYCLES_32
};

enum {
	NO_CLOCK_GATING,
	GATE_CLOCK_CYCLES_12,
	GATE_CLOCK_CYCLES_16,
	GATE_CLOCK_CYCLES_20,
	GATE_CLOCK_CYCLES_24
};

enum {
	IGNORE_WORD_LENGTH,
	RESOLUTION_12_BIT,
	RESOLUTION_16_BIT,
	RESOLUTION_20_BIT,
	RESOLUTION_24_BIT,
	RESOLUTION_32_BIT
};

struct i2s_dev {
	void __iomem *i2s_base;
	struct clk *clk;
	struct device *dev;
	unsigned int i2s_reg_comp1;
	unsigned int i2s_reg_comp2;
	unsigned int capability;
	u32 fifo_th;
	bool use_pio;
	/* data related to DMA transfers b/w i2s and DMAC */
	struct snd_dmaengine_dai_dma_data play_dma_data;
	struct snd_dmaengine_dai_dma_data capture_dma_data;
	struct i2s_clk_config_data config;
	struct snd_pcm_substream __rcu *tx_substream;
	struct snd_pcm_substream __rcu *rx_substream;
	unsigned int (*tx_fn)(struct i2s_dev *dev,
			struct snd_pcm_runtime *runtime, unsigned int tx_ptr,
			bool *period_elapsed,int type);
	unsigned int (*rx_fn)(struct i2s_dev *dev,
			struct snd_pcm_runtime *runtime, unsigned int rx_ptr,
			bool *period_elapsed,int type);
	unsigned int tx_ptr;
	unsigned int rx_ptr;
	u32 xfer_resolution;
	int active;
	u32 ccr;
	void __iomem *i2s_div_base;
	u32 i2s_div_num;
	bool playback_active;
	bool capture_active;
};

#endif /* __I2S_H */
