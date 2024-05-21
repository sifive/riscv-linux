/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Eswin Holdings Co., Ltd.
 */

#ifndef __ES_DC_H__
#define __ES_DC_H__

#include <linux/mm_types.h>
#include <drm/drm_modes.h>

#include "es_plane.h"
#include "es_crtc.h"
#include "es_dc_hw.h"
#ifdef CONFIG_ESWIN_MMU
#include "es_dc_mmu.h"
#endif

struct es_dc_funcs {
	void (*dump_enable)(struct device *dev, dma_addr_t addr,
			    unsigned int pitch);
	void (*dump_disable)(struct device *dev);
};

struct es_dc {
	struct es_crtc *crtc;
	struct dc_hw hw;

	struct clk *vo_mux;
	struct clk *spll0_fout1;
	struct clk *cfg_clk;
	struct clk *pix_clk;
	struct clk *axi_clk;
	unsigned int pix_clk_rate; /* in KHz */

	struct reset_control *vo_arst;
	struct reset_control *vo_prst;
	struct reset_control *dc_arst;
	struct reset_control *dc_prst;

	bool first_frame;
	bool dc_initialized;

	const struct es_dc_funcs *funcs;
};

extern struct platform_driver dc_platform_driver;
#endif /* __ES_DC_H__ */
