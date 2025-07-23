/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Eswin Holdings Co., Ltd.
 */

#ifndef __ES_CRTC_H__
#define __ES_CRTC_H__

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

#include "es_type.h"

struct es_crtc_funcs {
	void (*enable)(struct device *dev, struct drm_crtc *crtc);
	void (*disable)(struct device *dev);
	bool (*mode_fixup)(struct device *dev,
			   const struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);
	void (*set_gamma)(struct device *dev, struct drm_color_lut *lut,
			  unsigned int size);
	void (*enable_gamma)(struct device *dev, bool enable);
	void (*enable_vblank)(struct device *dev, bool enable);
	void (*commit)(struct device *dev);
};

struct es_crtc_state {
	struct drm_crtc_state base;

	u32 sync_mode;
	u32 output_fmt;
	u32 bg_color;
	u8 encoder_type;
	u8 mmu_prefetch;
	u8 bpp;
	bool dither_enable;
	bool underflow;
};

struct es_crtc {
	struct drm_crtc base;
	struct device *dev;
	struct drm_pending_vblank_event *event;
	unsigned int max_bpc;
	unsigned int color_formats; /* supported color format */

	struct drm_property *sync_mode;
	struct drm_property *mmu_prefetch;
	struct drm_property *dither;
	struct drm_property *bg_color;

	const struct es_crtc_funcs *funcs;
};

void es_crtc_destroy(struct drm_crtc *crtc);

struct es_crtc *es_crtc_create(struct drm_device *drm_dev,
			       struct es_dc_info *info);
void es_crtc_handle_vblank(struct drm_crtc *crtc, bool underflow);

static inline struct es_crtc *to_es_crtc(struct drm_crtc *crtc)
{
	return container_of(crtc, struct es_crtc, base);
}

static inline struct es_crtc_state *
to_es_crtc_state(struct drm_crtc_state *state)
{
	return container_of(state, struct es_crtc_state, base);
}
#endif /* __ES_CRTC_H__ */
