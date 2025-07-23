/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Eswin Holdings Co., Ltd.
 */

#ifndef __ES_PLANE_H__
#define __ES_PLANE_H__

#include <drm/drm_fourcc.h>
#include <drm/drm_plane_helper.h>

#include "es_type.h"
#include "es_fb.h"

#define MAX_NUM_PLANES 3 /* colour format plane */

struct es_plane;

struct es_plane_funcs {
	void (*update)(struct device *dev, struct es_plane *plane);
	void (*disable)(struct device *dev, struct es_plane *plane);
	int (*check)(struct device *dev, struct es_plane *plane,
		     struct drm_plane_state *state);
};

struct drm_es_roi {
	__u16 enable;
	__u16 roi_x;
	__u16 roi_y;
	__u16 roi_w;
	__u16 roi_h;
};

struct drm_es_color_mgmt {
	__u32 colorkey;
	__u32 colorkey_high;
	__u32 clear_value;
	__u16 clear_enable;
	__u16 transparency;
};

struct es_plane_status {
	u32 tile_mode;
	struct drm_rect src;
	struct drm_rect dest;
	// struct drm_format_name_buf format_name;
};

struct es_plane_state {
	struct drm_plane_state base;
	struct es_plane_status status; /* for debugfs */
	struct drm_property_blob *roi;
	struct drm_property_blob *color_mgmt;

	u32 degamma;
	bool degamma_changed;
};

struct es_plane {
	struct drm_plane base;
	u8 id;
	dma_addr_t dma_addr[MAX_NUM_PLANES];

	struct drm_property *degamma_mode;
	struct drm_property *roi_prop;
	struct drm_property *color_mgmt_prop;

	const struct es_plane_funcs *funcs;
};

void es_plane_destory(struct drm_plane *plane);

struct es_plane *es_plane_create(struct drm_device *drm_dev,
				 struct es_plane_info *info,
				 unsigned int possible_crtcs);

static inline struct es_plane *to_es_plane(struct drm_plane *plane)
{
	return container_of(plane, struct es_plane, base);
}

static inline struct es_plane_state *
to_es_plane_state(struct drm_plane_state *state)
{
	return container_of(state, struct es_plane_state, base);
}
#endif /* __ES_PLANE_H__ */
