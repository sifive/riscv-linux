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

#include <sound/pcm.h>
#include <linux/device.h>
#include <sound/soc.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <sound/pcm_params.h>
#include <asm/io.h>
#include <sound/asound.h>
#include <sound/designware_i2s.h>
#include <linux/irqreturn.h>
#include <linux/clk.h>
#include <sound/soc-dai.h>
#include <linux/ioport.h>
#include <linux/err.h>
#include <linux/pm.h>
#include <linux/mfd/syscon.h>
#include <linux/reset.h>
#include <linux/dma-map-ops.h>
#include "i2s.h"
#include "es-audio-proc.h"

#define VO_MCLK_DIVSOR_MASK    0xff0
#define VO_MCLK_DIVSOR_OFFSET  4

#define MAX_SAMPLE_RATE_SUPPORT (192000UL)
#define MAX_SAMPLE_RATE_CLK (MAX_SAMPLE_RATE_SUPPORT * 32 * 2) // 32 bits, 2channels

#define VO_TOP_CSR             0x50280000UL
#define VO_I2S0_DIV_NUM        0x2000
#define VO_I2S1_DIV_NUM        0x2004
#define VO_I2S2_DIV_NUM        0x2008
#define DIV_NUM_MASK           0x1f

#define ESW_I2S_RATES (SNDRV_PCM_RATE_192000 | \
			SNDRV_PCM_RATE_96000 | \
			SNDRV_PCM_RATE_48000 | \
			SNDRV_PCM_RATE_32000 | \
			SNDRV_PCM_RATE_16000 | \
			SNDRV_PCM_RATE_8000)
#define ESW_I2S_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE)

static u32 dmaen_txch[] = {
	DMAEN_TXCH_0,
	DMAEN_TXCH_1,
	DMAEN_TXCH_2,
	DMAEN_TXCH_3
};

static u32 dmaen_rxch[] = {
	DMAEN_RXCH_0,
	DMAEN_RXCH_1,
	DMAEN_RXCH_2,
	DMAEN_RXCH_3
};

/* Maximum bit resolution of a channel - not uniformly spaced */
static const u32 fifo_width[COMP_MAX_WORDSIZE] = {
	12, 16, 20, 24, 32, 0, 0, 0
};

/* Width of (DMA) bus */
static const u32 bus_widths[COMP_MAX_DATA_WIDTH] = {
	DMA_SLAVE_BUSWIDTH_1_BYTE,
	DMA_SLAVE_BUSWIDTH_2_BYTES,
	DMA_SLAVE_BUSWIDTH_4_BYTES,
	DMA_SLAVE_BUSWIDTH_UNDEFINED
};

static inline u32 i2s_read_reg(void *io_base, int reg)
{
	return readl((char *)io_base + reg);
}

static inline void i2s_write_reg(void *io_base, int reg, u32 val)
{
	writel(val, (char *)io_base + reg);
}

static inline void i2s_disable_channels(struct i2s_dev *i2s_drvdata, u32 stream)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		i2s_write_reg(i2s_drvdata->i2s_base, TER(0), 0);
	} else {
		i2s_write_reg(i2s_drvdata->i2s_base, RER(0), 0);
	}
}

static void i2s_config(struct i2s_dev *i2s_drvdata, int stream)
{
	u32 ch_reg;
	struct i2s_clk_config_data *config = &i2s_drvdata->config;
	i2s_disable_channels(i2s_drvdata, stream);
	for (ch_reg = 0; ch_reg < (config->chan_nr / 2); ch_reg++) {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			i2s_write_reg(i2s_drvdata->i2s_base, TCR(ch_reg),
				      i2s_drvdata->xfer_resolution);
			i2s_write_reg(i2s_drvdata->i2s_base, TFCR(ch_reg),
				      i2s_drvdata->fifo_th - 1);
			i2s_write_reg(i2s_drvdata->i2s_base, TER(ch_reg), 1);
		} else {
			i2s_write_reg(i2s_drvdata->i2s_base, RCR(ch_reg),
				      i2s_drvdata->xfer_resolution);
			i2s_write_reg(i2s_drvdata->i2s_base, RFCR(ch_reg),
				      i2s_drvdata->fifo_th - 1);
			i2s_write_reg(i2s_drvdata->i2s_base, RER(ch_reg), 1);
		}
    }
}

static inline void i2s_enable_irqs(struct i2s_dev *i2s_drvdata, u32 stream,
				   int chan_nr)
{
	u32 i, irq;
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < (chan_nr / 2); i++) {
			irq = i2s_read_reg(i2s_drvdata->i2s_base, IMR(i));
			i2s_write_reg(i2s_drvdata->i2s_base, IMR(i), irq & ~0x30);
		}
	} else {
		for (i = 0; i < (chan_nr / 2); i++) {
			irq = i2s_read_reg(i2s_drvdata->i2s_base, IMR(i));
			i2s_write_reg(i2s_drvdata->i2s_base, IMR(i), irq & ~0x03);
		}
    }
}

static inline void i2s_enable_dedicated_dma(struct i2s_dev *i2s_drvdata, u32 stream,
				   int chan_nr)
{
	u32 i, dmacr;
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < (chan_nr / 2); i++) {
			dmacr = i2s_read_reg(i2s_drvdata->i2s_base, DMACR);
			i2s_write_reg(i2s_drvdata->i2s_base, DMACR, dmacr | dmaen_txch[i]);
		}
	} else {
		for (i = 0; i < (chan_nr / 2); i++) {
			dmacr = i2s_read_reg(i2s_drvdata->i2s_base, DMACR);
			i2s_write_reg(i2s_drvdata->i2s_base, DMACR, dmacr | dmaen_rxch[i]);
		}
    }
}

static inline void i2s_disable_dedicated_dma(struct i2s_dev *i2s_drvdata, u32 stream,
				   int chan_nr)
{
	u32 i, dmacr;
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < (chan_nr / 2); i++) {
			dmacr = i2s_read_reg(i2s_drvdata->i2s_base, DMACR);
			i2s_write_reg(i2s_drvdata->i2s_base, DMACR, dmacr & ~dmaen_txch[i]);
		}
	} else {
		for (i = 0; i < (chan_nr / 2); i++) {
			dmacr = i2s_read_reg(i2s_drvdata->i2s_base, DMACR);
			i2s_write_reg(i2s_drvdata->i2s_base, DMACR, dmacr & ~dmaen_rxch[i]);
		}
    }
}

static inline void i2s_enable_combined_dma(struct i2s_dev *i2s_drvdata, u32 stream)
{
	u32 dmacr;
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dmacr = i2s_read_reg(i2s_drvdata->i2s_base, DMACR);
		i2s_write_reg(i2s_drvdata->i2s_base, DMACR, dmacr | DMAEN_TXBLOCK);

	} else {
		dmacr = i2s_read_reg(i2s_drvdata->i2s_base, DMACR);
		i2s_write_reg(i2s_drvdata->i2s_base, DMACR, dmacr | DMAEN_RXBLOCK);
    }
}

static inline void i2s_disable_combined_dma(struct i2s_dev *i2s_drvdata, u32 stream)
{
	u32 dmacr;
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dmacr = i2s_read_reg(i2s_drvdata->i2s_base, DMACR);
		i2s_write_reg(i2s_drvdata->i2s_base, DMACR, dmacr & ~DMAEN_TXBLOCK);

	} else {
		dmacr = i2s_read_reg(i2s_drvdata->i2s_base, DMACR);
		i2s_write_reg(i2s_drvdata->i2s_base, DMACR, dmacr & ~DMAEN_RXBLOCK);
    }
}

static void i2s_start(struct i2s_dev *i2s_drvdata,
		      struct snd_pcm_substream *substream)
{
	struct i2s_clk_config_data *config = &i2s_drvdata->config;
	i2s_write_reg(i2s_drvdata->i2s_base, IER, 1);
	if (i2s_drvdata->use_pio) {
		i2s_enable_irqs(i2s_drvdata, substream->stream, config->chan_nr);
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		i2s_write_reg(i2s_drvdata->i2s_base, ITER, 1);
	} else {
		i2s_write_reg(i2s_drvdata->i2s_base, IRER, 1);
	}
	if (!i2s_drvdata->use_pio) {
		i2s_enable_dedicated_dma(i2s_drvdata, substream->stream, config->chan_nr);
	}
	i2s_write_reg(i2s_drvdata->i2s_base, CER, 1);
}

static inline void i2s_clear_irqs(struct i2s_dev *i2s_drvdata, u32 stream)
{
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		i2s_read_reg(i2s_drvdata->i2s_base, TOR(0));
	} else {
		i2s_read_reg(i2s_drvdata->i2s_base, ROR(0));
	}
}

static inline void i2s_disable_irqs(struct i2s_dev *i2s_drvdata, u32 stream,
				    int chan_nr)
{
	u32 i, irq;
	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < (chan_nr / 2); i++) {
			irq = i2s_read_reg(i2s_drvdata->i2s_base, IMR(i));
			i2s_write_reg(i2s_drvdata->i2s_base, IMR(i), irq | 0x30);
		}
	} else {
		for (i = 0; i < (chan_nr / 2); i++) {
			irq = i2s_read_reg(i2s_drvdata->i2s_base, IMR(i));
			i2s_write_reg(i2s_drvdata->i2s_base, IMR(i), irq | 0x03);
		}
	}
}

static void i2s_stop(struct i2s_dev *i2s_drvdata,
		struct snd_pcm_substream *substream)
{
	if (i2s_drvdata->use_pio) {
		i2s_clear_irqs(i2s_drvdata, substream->stream);
	}
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		i2s_write_reg(i2s_drvdata->i2s_base, ITER, 0);
	} else {
		i2s_write_reg(i2s_drvdata->i2s_base, IRER, 0);
	}
	if (i2s_drvdata->use_pio) {
		i2s_disable_irqs(i2s_drvdata, substream->stream, 2);
	} else {
		i2s_disable_dedicated_dma(i2s_drvdata, substream->stream, 2);
	}
	if (!i2s_drvdata->active) {
		i2s_write_reg(i2s_drvdata->i2s_base, CER, 0);
		i2s_write_reg(i2s_drvdata->i2s_base, IER, 0);
	}
}

static irqreturn_t i2s_irq_handler(int irq, void *dev_id)
{
	struct i2s_dev *i2s_drvdata = dev_id;
	bool irq_valid = false;
	u32 isr;

	isr = i2s_read_reg(i2s_drvdata->i2s_base, ISR(0));
	i2s_clear_irqs(i2s_drvdata, SNDRV_PCM_STREAM_PLAYBACK);
	i2s_clear_irqs(i2s_drvdata, SNDRV_PCM_STREAM_CAPTURE);
	if ((isr & ISR_TXFE)&& i2s_drvdata->use_pio) {
		i2s_pcm_push_tx(i2s_drvdata, STEREO);
		irq_valid = true;
	}
	if ((isr & ISR_RXDA)&& i2s_drvdata->use_pio) {
		i2s_pcm_pop_rx(i2s_drvdata, STEREO);
		irq_valid = true;
	}
	if (isr & ISR_TXFO) {
		dev_err(i2s_drvdata->dev, "TX overrun (ch_id=%d)\n", 0);
		irq_valid = true;
	}
	if (isr & ISR_RXFO) {
		dev_err(i2s_drvdata->dev, "RX overrun (ch_id=%d)\n", 0);
		irq_valid = true;
	}

	if (irq_valid) {
		return IRQ_HANDLED;
	}
	else {
		return IRQ_NONE;
	}
}

#define	COMP1_MAX_WORDSIZE	5
static const u32 i2s_formats[COMP1_MAX_WORDSIZE] = {
	SNDRV_PCM_FMTBIT_S16_LE,
	SNDRV_PCM_FMTBIT_S16_LE,
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S16_LE,
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S16_LE,
	SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S16_LE
};

static int i2s_configure_dai(struct i2s_dev *i2s_drvdata,
				   struct snd_soc_dai_driver *i2s_dai,
				   unsigned int rates)
{
	u32 idx;
	u32 fifo_depth;
	u32 comp1, comp2;

	comp1 = i2s_read_reg(i2s_drvdata->i2s_base, i2s_drvdata->i2s_reg_comp1);
	comp2 = i2s_read_reg(i2s_drvdata->i2s_base, i2s_drvdata->i2s_reg_comp2);
	fifo_depth = 1 << (1 + COMP1_FIFO_DEPTH_GLOBAL(comp1));

	if (COMP1_TX_ENABLED(comp1)) {
		dev_dbg(i2s_drvdata->dev, " i2s: play supported\n");
		idx = COMP1_TX_WORDSIZE_0(comp1);
		if (WARN_ON(idx >= ARRAY_SIZE(i2s_formats)))
			return -EINVAL;
		i2s_dai->playback.formats = i2s_formats[idx];
		i2s_dai->playback.channels_min = MIN_CHANNEL_NUM;
	    i2s_dai->playback.channels_max =
				 (COMP1_TX_CHANNELS(comp1) + 1) << 1;
		i2s_dai->playback.rates = rates;
	}

	if (COMP1_RX_ENABLED(comp1)){
		dev_dbg(i2s_drvdata->dev, "i2s: record supported\n");
		idx = COMP2_RX_WORDSIZE_0(comp2);
		if (WARN_ON(idx >= ARRAY_SIZE(i2s_formats)))
			return -EINVAL;
		i2s_dai->capture.formats = i2s_formats[idx];
		i2s_dai->capture.channels_min = MIN_CHANNEL_NUM;
		i2s_dai->capture.channels_max =
				 (COMP1_RX_CHANNELS(comp1) + 1) << 1;
		i2s_dai->capture.rates = rates;
	}

	if (COMP1_MODE_EN(comp1)) {
		dev_dbg(i2s_drvdata->dev, "eswin: i2s master mode supported\n");
		i2s_drvdata->capability |= DW_I2S_MASTER;
	} else {
		dev_dbg(i2s_drvdata->dev, "eswin: i2s slave mode supported\n");
		i2s_drvdata->capability |= DW_I2S_SLAVE;
	}
	i2s_drvdata->fifo_th = fifo_depth / 2;
	return 0;
}

static int i2s_configure_dai_by_dt(struct i2s_dev *dev,
				   struct snd_soc_dai_driver *i2s_dai,
				   struct resource *res)
{
	struct snd_soc_component *component;
	struct dmaengine_pcm *pcm;
	u32 comp1 = i2s_read_reg(dev->i2s_base, I2S_COMP_PARAM_1);
	u32 comp2 = i2s_read_reg(dev->i2s_base, I2S_COMP_PARAM_2);
	u32 fifo_depth;
	u32 idx;
	u32 idx2;
	int ret;

	dev_info(dev->dev, "comp1:0x%x, comp2:0x%x\n", comp1, comp2);
	fifo_depth = 1 << (1 + COMP1_FIFO_DEPTH_GLOBAL(comp1));
	idx = COMP1_APB_DATA_WIDTH(comp1);

	if (WARN_ON(idx >= ARRAY_SIZE(bus_widths))) {
		dev_err(dev->dev, "idx:%d inval\n", idx);
		return -EINVAL;
	}
	ret = i2s_configure_dai(dev, i2s_dai, SNDRV_PCM_RATE_8000_192000);
	if (ret < 0) {
		dev_err(dev->dev, "i2s_configure_dai failed: %d\n", ret);
		return ret;
	}
	component = snd_soc_lookup_component(dev->dev, SND_DMAENGINE_PCM_DRV_NAME);
	if (!component) {
		dev_err(dev->dev, "Can not find snd_soc_component\n");
		return -1;
	}

	pcm = soc_component_to_pcm(component);
	if (COMP1_TX_ENABLED(comp1)) {
		idx2 = COMP1_TX_WORDSIZE_0(comp1);
		dev->capability |= DWC_I2S_PLAY;
		/* only  configure Combined DMA addr, Our scenario is not Dedicated DMA case */
		dev->play_dma_data.addr_width = bus_widths[idx];
		dev->play_dma_data.fifo_size = fifo_depth *
			(fifo_width[idx2]) >> 3;
		if (of_node_name_prefix(pcm->chan[SNDRV_PCM_STREAM_PLAYBACK]->device->dev->of_node,
								"dma-controller-hsp")) {
			dev->play_dma_data.addr = dma_map_resource(
						pcm->chan[SNDRV_PCM_STREAM_PLAYBACK]->device->dev,
						res->start + TXDMA_CH(0),
						dev->play_dma_data.fifo_size,
						DMA_BIDIRECTIONAL,
						DMA_ATTR_SKIP_CPU_SYNC);
		} else {
			dev->play_dma_data.addr = res->start + TXDMA_CH(0);
		}
		dev->play_dma_data.maxburst = 16;
	}
	if (COMP1_RX_ENABLED(comp1)) {
		idx2 = COMP2_RX_WORDSIZE_0(comp2);
		dev->capability |= DWC_I2S_RECORD;
		/* only  configure Combined DMA addr, Our scenario is not Dedicated DMA case */
		dev->capture_dma_data.addr_width = bus_widths[idx];
		dev->capture_dma_data.fifo_size = fifo_depth *
			(fifo_width[idx2]) >> 3;
		if (of_node_name_prefix(pcm->chan[SNDRV_PCM_STREAM_CAPTURE]->device->dev->of_node,
								"dma-controller-hsp")) {
			dev->capture_dma_data.addr = dma_map_resource(
						pcm->chan[SNDRV_PCM_STREAM_CAPTURE]->device->dev,
						res->start + RXDMA_CH(0),
						dev->capture_dma_data.fifo_size,
						DMA_BIDIRECTIONAL,
						DMA_ATTR_SKIP_CPU_SYNC);
		} else {
			dev->capture_dma_data.addr = res->start + RXDMA_CH(0);
		}
		dev->capture_dma_data.maxburst = 16;
	}
	return 0;
}

static int i2s_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *cpu_dai)
{
	struct i2s_dev *dev = snd_soc_dai_get_drvdata(cpu_dai);
	struct snd_dmaengine_dai_dma_data *dma_data = NULL;

	if (!(dev->capability & DWC_I2S_RECORD) &&
			(substream->stream == SNDRV_PCM_STREAM_CAPTURE))
		return -EINVAL;

	if (!(dev->capability & DWC_I2S_PLAY) &&
			(substream->stream == SNDRV_PCM_STREAM_PLAYBACK))
		return -EINVAL;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dma_data = &dev->play_dma_data;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		dma_data = &dev->capture_dma_data;

	snd_soc_dai_set_dma_data(cpu_dai, substream, (void *)dma_data);
	return 0;
}


static int i2s_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct i2s_dev *i2s_drvdata = snd_soc_dai_get_drvdata(dai);
	struct i2s_clk_config_data *config = &i2s_drvdata->config;
	struct device_node *node = i2s_drvdata->dev->of_node;
	struct regmap *vo_mclk_sel_regmap;
	uint32_t vo_mclk_sel_reg;
	uint32_t vo_mclk_sel;
	int ret;
	uint32_t div_num = 0;
	uint32_t div_num_reg;

	dev_info(i2s_drvdata->dev, "sample rate:%d, chan:%d, width:%d\n",
			 params_rate(params), params_channels(params), params_width(params));
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		config->data_width = 16;
		i2s_drvdata->ccr = CLOCK_CYCLES_32 << CCR_WSS_POS |
					NO_CLOCK_GATING;
		i2s_drvdata->xfer_resolution = RESOLUTION_16_BIT;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		config->data_width = 24;
		i2s_drvdata->ccr = CLOCK_CYCLES_32 << CCR_WSS_POS |
					NO_CLOCK_GATING;
		i2s_drvdata->xfer_resolution = RESOLUTION_24_BIT;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		config->data_width = 32;
		i2s_drvdata->ccr = CLOCK_CYCLES_32 << CCR_WSS_POS |
					NO_CLOCK_GATING;
		i2s_drvdata->xfer_resolution = RESOLUTION_32_BIT;
		break;
	default:
		dev_err(i2s_drvdata->dev, "eswin-i2s: unsupported PCM fmt");
		return -EINVAL;
	}
	config->chan_nr = params_channels(params);
	switch (config->chan_nr) {
	case TWO_CHANNEL_SUPPORT:
		break;
	default:
		dev_err(i2s_drvdata->dev, "channel not supported\n");
		return -EINVAL;
	}
	i2s_config(i2s_drvdata, substream->stream);
	i2s_write_reg(i2s_drvdata->i2s_base, CCR, i2s_drvdata->ccr);
	config->sample_rate = params_rate(params);
	if (i2s_drvdata->capability & DW_I2S_MASTER) {
		if (!i2s_drvdata->eswin_plat) {
			vo_mclk_sel_regmap =
				syscon_regmap_lookup_by_phandle(node, "vo_mclk_sel,syscrg");
			if (IS_ERR(vo_mclk_sel_regmap)) {
				dev_err(i2s_drvdata->dev, "No vo_mclk_sel,syscrg phandle specified\n");
				return PTR_ERR(vo_mclk_sel_regmap);
			}
			ret = of_property_read_u32_index(node, "vo_mclk_sel,syscrg", 1,
							&vo_mclk_sel_reg);
			if (ret) {
				dev_err(i2s_drvdata->dev, "can't get vo_mclk_sel_reg offset (%d)\n", ret);
				return ret;
			}
			regmap_read(vo_mclk_sel_regmap, vo_mclk_sel_reg, &vo_mclk_sel);
			vo_mclk_sel &= ~VO_MCLK_DIVSOR_MASK;

			switch (config->sample_rate) {
			case 96000:
				vo_mclk_sel |= (0x10 << VO_MCLK_DIVSOR_OFFSET);
				break;
			case 48000:
				vo_mclk_sel |= (0x12 << VO_MCLK_DIVSOR_OFFSET);
				break;
			case 44100:
				vo_mclk_sel |= (0x11 << VO_MCLK_DIVSOR_OFFSET);
				break;
			default:
				dev_err(i2s_drvdata->dev, "Can't support sample rate: %d\n",
						config->sample_rate);
				return -EINVAL;
			}
			regmap_write(vo_mclk_sel_regmap, vo_mclk_sel_reg, vo_mclk_sel);
		} else {
			if (MAX_SAMPLE_RATE_SUPPORT % config->sample_rate != 0) {
				dev_err(i2s_drvdata->dev, "Not support sample rate: %d\n", config->sample_rate);
				return -EINVAL;
			}

			div_num = MAX_SAMPLE_RATE_SUPPORT / config->sample_rate - 1;

			if (i2s_drvdata->active) {
				if (i2s_drvdata->i2s_div_num != div_num) {
					dev_err(i2s_drvdata->dev, "Not support the playback and capture clocks are different\n");
					return -EINVAL;
				}
			} else {
				div_num_reg = i2s_read_reg(i2s_drvdata->i2s_div_base, 0) & ~DIV_NUM_MASK;
				div_num_reg |= div_num;

				dev_info(i2s_drvdata->dev, "div num:0x%x\n", div_num);
				i2s_drvdata->i2s_div_num = div_num;
				i2s_write_reg(i2s_drvdata->i2s_div_base, 0, div_num_reg);
			}
		}
	}

	return 0;
}

static int i2s_prepare(struct snd_pcm_substream *substream,
			  struct snd_soc_dai *dai)
{
	struct i2s_dev *i2s_drvdata = snd_soc_dai_get_drvdata(dai);
	 if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		i2s_write_reg(i2s_drvdata->i2s_base, TXFFR, 1);
	else
		i2s_write_reg(i2s_drvdata->i2s_base, RXFFR, 1);

	return 0;
}

static int i2s_trigger(struct snd_pcm_substream *substream,
		int cmd, struct snd_soc_dai *dai)
{
	struct i2s_dev *i2s_drvdata = snd_soc_dai_get_drvdata(dai);
	int ret = 0;
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		i2s_drvdata->active++;
		i2s_start(i2s_drvdata, substream);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			i2s_drvdata->playback_active = true;
		} else {
			i2s_drvdata->capture_active = true;
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		i2s_drvdata->active--;
		i2s_stop(i2s_drvdata, substream);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			i2s_drvdata->playback_active = false;
		} else {
			i2s_drvdata->capture_active = false;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int i2s_set_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	struct i2s_dev *i2s_drvdata = snd_soc_dai_get_drvdata(cpu_dai);
	int ret = 0;
	switch (fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK) {
	case SND_SOC_DAIFMT_BC_FC:
		if (i2s_drvdata->capability & DW_I2S_SLAVE)
			ret = 0;
		else
			ret = -EINVAL;
		break;
	case SND_SOC_DAIFMT_BP_FP:
		if (i2s_drvdata->capability & DW_I2S_MASTER)
			ret = 0;
		else
			ret = -EINVAL;
		break;
	case SND_SOC_DAIFMT_BC_FP:
	case SND_SOC_DAIFMT_BP_FC:
		ret = -EINVAL;
		break;
	default:
		dev_dbg(i2s_drvdata->dev, "dwc : Invalid master/slave format\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int i2s_pcm_dai_probe(struct snd_soc_dai *dai)
{
       struct i2s_dev *i2s_drvdata = snd_soc_dai_get_drvdata(dai);
       snd_soc_dai_init_dma_data(dai, &i2s_drvdata->play_dma_data, &i2s_drvdata->capture_dma_data);

       return 0;
}

static const struct snd_soc_dai_ops i2s_dai_ops = {
	.startup	= i2s_startup,
	.hw_params	= i2s_hw_params,
	.prepare	= i2s_prepare,
	.trigger	= i2s_trigger,
	.set_fmt	= i2s_set_fmt,
	.probe          = i2s_pcm_dai_probe,
};

#ifdef CONFIG_PM
static int i2s_runtime_suspend(struct device *dev)
{
	struct i2s_dev *i2s_drvdata = dev_get_drvdata(dev);
	if (i2s_drvdata->capability & DW_I2S_MASTER)
		clk_disable(i2s_drvdata->clk);

	return 0;
}

static int i2s_runtime_resume(struct device *dev)
{
	struct i2s_dev *i2s_drvdata = dev_get_drvdata(dev);
	if (i2s_drvdata->capability & DW_I2S_MASTER)
		clk_enable(i2s_drvdata->clk);

	return 0;
}

static int i2s_suspend(struct snd_soc_component *component)
{
	struct i2s_dev *i2s_drvdata = snd_soc_component_get_drvdata(component);
	if (i2s_drvdata->capability & DW_I2S_MASTER) {
		clk_disable(i2s_drvdata->clk);
	}
	return 0;
}

static int i2s_resume(struct snd_soc_component *component)
{
	struct i2s_dev *i2s_drvdata = snd_soc_component_get_drvdata(component);
	struct snd_soc_dai *dai = NULL;
	int stream;

	if (i2s_drvdata->capability & DW_I2S_MASTER)
		clk_enable(i2s_drvdata->clk);

	for_each_component_dais(component, dai) {
		for_each_pcm_streams(stream)
			if (snd_soc_dai_stream_active(dai, stream))
				i2s_config(i2s_drvdata, stream);
	}
	return 0;
}

#else
#define i2s_suspend	NULL
#define i2s_resume	NULL
#endif

static int i2s_reset(struct platform_device *pdev, struct i2s_dev *i2s)
{
	struct reset_control *rst;
	struct reset_control *prst;
	struct reset_control *voprst;
	int ret;

	rst = devm_reset_control_get_optional_exclusive(&pdev->dev, "i2srst");
	if (IS_ERR(rst)) {
		return PTR_ERR(rst);
	}

	prst = devm_reset_control_get_optional_exclusive(&pdev->dev, "i2sprst");
	if (IS_ERR(prst)) {
		return PTR_ERR(prst);
	}

	voprst = devm_reset_control_get_optional_exclusive(&pdev->dev, "voprst");
	if (IS_ERR(prst)) {
		return PTR_ERR(prst);
	}

	ret = reset_control_assert(rst);
	WARN_ON(0 != ret);
	ret = reset_control_assert(prst);
	WARN_ON(0 != ret);
	ret = reset_control_deassert(rst);
	WARN_ON(0 != ret);
	ret = reset_control_deassert(prst);
	WARN_ON(0 != ret);
	ret = reset_control_deassert(voprst);
	WARN_ON(0 != ret);

	return 0;
}
struct snd_kcontrol_new snd_dump_controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_CARD,
		.name = "Audio Dump Control",
		.index = 0,
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	}
};

static int i2s_open(struct snd_soc_component *component,
			      struct snd_pcm_substream *substream)
{
	struct i2s_dev *i2s_drvdata = snd_soc_component_get_drvdata(component);

	if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK && i2s_drvdata->playback_active == true) ||
		(substream->stream == SNDRV_PCM_STREAM_CAPTURE && i2s_drvdata->capture_active == true)) {
		dev_err(i2s_drvdata->dev, "i2s is busying\n");
		return -EBUSY;
	}
	return 0;
}

static const struct snd_soc_component_driver i2s0_component = {
	.name         = "i2s0",
	.open         = i2s_open,
	.suspend      = i2s_suspend,
	.resume       = i2s_resume,
	.controls     = snd_dump_controls,
	.num_controls = ARRAY_SIZE(snd_dump_controls),
};

static const struct snd_soc_component_driver i2s_component = {
	.name         = "i2s",
	.suspend      = i2s_suspend,
	.resume       = i2s_resume,
};

static struct snd_soc_dai_driver i2s_dai[4] = {
	{
		.name = "i2s0-hdmi",
		.id = 0,
		.ops = &i2s_dai_ops,
		.playback = {
			.stream_name = "Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ESW_I2S_RATES,
			.formats = ESW_I2S_FORMATS,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ESW_I2S_RATES,
			.formats = ESW_I2S_FORMATS,
		},
	},
	{
		.name = "i2s0",
		.id = 1,
		.ops = &i2s_dai_ops,
		.playback = {
			.stream_name = "Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ESW_I2S_RATES,
			.formats = ESW_I2S_FORMATS,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ESW_I2S_RATES,
			.formats = ESW_I2S_FORMATS,
		},
	},
	{
		.name = "i2s1",
		.id = 0,
		.ops = &i2s_dai_ops,
		.playback = {
			.stream_name = "Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ESW_I2S_RATES,
			.formats = ESW_I2S_FORMATS,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ESW_I2S_RATES,
			.formats = ESW_I2S_FORMATS,
		},
	},
	{
		.name = "i2s2",
		.id = 0,
		.ops = &i2s_dai_ops,
		.playback = {
			.stream_name = "Playback",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ESW_I2S_RATES,
			.formats = ESW_I2S_FORMATS,
		},
		.capture = {
			.stream_name = "Capture",
			.channels_min = 2,
			.channels_max = 2,
			.rates = ESW_I2S_RATES,
			.formats = ESW_I2S_FORMATS,
		},
	},
};

static int i2s_probe(struct platform_device *pdev)
{
	struct i2s_dev *i2s_drvdata;
	struct resource *res;
	int ret, irq;
	const char *clk_id;
	struct snd_dmaengine_pcm_config *config;

	dev_info(&pdev->dev, "dev name:%s\n", pdev->dev.of_node->name);
	i2s_drvdata = devm_kzalloc(&pdev->dev, sizeof(*i2s_drvdata), GFP_KERNEL);
	if (!i2s_drvdata)
		return -ENOMEM;

	config = devm_kzalloc(&pdev->dev,
			sizeof(struct snd_dmaengine_pcm_config), GFP_KERNEL);
	if (!config)
		return -ENOMEM;
	config->chan_names[SNDRV_PCM_STREAM_PLAYBACK] = "tx";
	config->chan_names[SNDRV_PCM_STREAM_CAPTURE] = "rx";
	config->prepare_slave_config = snd_dmaengine_pcm_prepare_slave_config;

	res = platform_get_resource(pdev,IORESOURCE_MEM, 0);
	i2s_drvdata->i2s_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(i2s_drvdata->i2s_base)) {
		dev_err(&pdev->dev, "devm_ioremap_resource failed\n");
		return PTR_ERR(i2s_drvdata->i2s_base);
	}
	i2s_drvdata->dev = &pdev->dev;

	clk_id = "mclk";
	if (of_node_name_prefix(pdev->dev.of_node, "i2s0")) {
		i2s_drvdata->clk = devm_clk_get(&pdev->dev, clk_id);
		if (IS_ERR(i2s_drvdata->clk))
			return PTR_ERR(i2s_drvdata->clk);
		ret = clk_prepare_enable(i2s_drvdata->clk);
		if (ret < 0)
			return ret;
		ret = clk_set_rate(i2s_drvdata->clk, MAX_SAMPLE_RATE_CLK);
		if (ret) {
			dev_err(i2s_drvdata->dev, "Can't set I2S clock rate: %d\n", ret);
		}

		ret = i2s_reset(pdev, i2s_drvdata);
		if (ret != 0) {
			dev_err(&pdev->dev, "i2s_reset failed\n");
			goto err_probe;
		}
	}

	dev_set_drvdata(&pdev->dev, i2s_drvdata);
	irq = platform_get_irq(pdev,0);
	if (irq >= 0) {
		ret = devm_request_irq(&pdev->dev, irq, i2s_irq_handler, 0, pdev->name, i2s_drvdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to request irq\n");
			return ret;
		}
	}

	if (of_node_name_prefix(pdev->dev.of_node, "i2s0")) {
		i2s_drvdata->i2s_div_base = devm_ioremap(i2s_drvdata->dev, VO_TOP_CSR + VO_I2S0_DIV_NUM, 4);
		if (!i2s_drvdata->i2s_div_base) {
			dev_err(&pdev->dev, "failed to remap i2s0 div config\n");
			return -ENOMEM;
		}
		ret = devm_snd_soc_register_component(&pdev->dev, &i2s0_component,
					&i2s_dai[0], 2);
	} else if (of_node_name_prefix(pdev->dev.of_node, "i2s1")) {
		i2s_drvdata->i2s_div_base = devm_ioremap(i2s_drvdata->dev, VO_TOP_CSR + VO_I2S1_DIV_NUM, 4);
		if (!i2s_drvdata->i2s_div_base) {
			dev_err(&pdev->dev, "failed to remap i2s1 div config\n");
			return -ENOMEM;
		}
		ret = devm_snd_soc_register_component(&pdev->dev, &i2s_component,
					&i2s_dai[2], 1);
	} else {
		i2s_drvdata->i2s_div_base = devm_ioremap(i2s_drvdata->dev, VO_TOP_CSR + VO_I2S2_DIV_NUM, 4);
		if (!i2s_drvdata->i2s_div_base) {
			dev_err(&pdev->dev, "failed to remap i2s2 div config\n");
			return -ENOMEM;
		}
		ret = devm_snd_soc_register_component(&pdev->dev, &i2s_component,
					&i2s_dai[3], 1);
	}
	if (ret != 0) {
		dev_err(&pdev->dev, "not able to register dai\n");
		goto err_probe;
	}

	if (irq >= 0) {
		ret = i2s_pcm_register(pdev);
		i2s_drvdata->use_pio = true;
	} else {
		ret = devm_snd_dmaengine_pcm_register(&pdev->dev, config, 0);
		i2s_drvdata->use_pio = false;
	}
	if (ret) {
		dev_err(&pdev->dev, "could not register pcm: %d\n", ret);
		goto err_probe;
	}

	i2s_drvdata->i2s_reg_comp1 = I2S_COMP_PARAM_1;
	i2s_drvdata->i2s_reg_comp2 = I2S_COMP_PARAM_2;
	ret = i2s_configure_dai_by_dt(i2s_drvdata, &i2s_dai[0], res);
	if (ret < 0) {
		dev_err(&pdev->dev, "i2s_configure_dai_by_dt failed\n");
		return ret;
	}

	ret = device_property_read_u32(&pdev->dev, "eswin-plat", &i2s_drvdata->eswin_plat);
    if (0 != ret) {
        dev_warn(&pdev->dev, "Failed to get eswin platform\n");
        i2s_drvdata->eswin_plat = 0;
    }
    dev_info(&pdev->dev, "eswin platform:%d\n", i2s_drvdata->eswin_plat);

	pm_runtime_enable(&pdev->dev);

	audio_proc_module_init();

	return 0;
err_probe:
	if (i2s_drvdata->capability & DW_I2S_MASTER)
		clk_disable_unprepare(i2s_drvdata->clk);
	return ret;
}

static int i2s_remove(struct platform_device *pdev)
{
	struct i2s_dev *i2s_drvdata = dev_get_drvdata(&pdev->dev);
	if (i2s_drvdata->capability & DW_I2S_MASTER)
		clk_disable_unprepare(i2s_drvdata->clk);

	pm_runtime_disable(&pdev->dev);

	audio_proc_module_exit();
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id i2s_of_match[] = {
	{ .compatible = "snps,i2s", },
	{},
};

MODULE_DEVICE_TABLE(of, i2s_of_match);
#endif

static const struct dev_pm_ops i2s_pm_ops = {
	SET_RUNTIME_PM_OPS(i2s_runtime_suspend, i2s_runtime_resume, NULL)
};

static struct platform_driver i2s_driver = {
	.probe		= i2s_probe,
	.remove		= i2s_remove,
	.driver		= {
		.name	= "i2s",
		.of_match_table = of_match_ptr(i2s_of_match),
		.pm = &i2s_pm_ops,
	},
};

module_platform_driver(i2s_driver);

MODULE_AUTHOR("ESWIN, INC.");
MODULE_DESCRIPTION("I2S driver");
MODULE_LICENSE("GPL");
