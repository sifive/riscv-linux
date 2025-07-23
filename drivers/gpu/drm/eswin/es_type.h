/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Eswin Holdings Co., Ltd.
 */

#ifndef __ES_TYPE_H__
#define __ES_TYPE_H__

#include <drm/drm_plane.h>
#include <drm/drm_plane_helper.h>

struct es_plane_info {
	const char *name;
	u8 id;
	enum drm_plane_type type;
	unsigned int num_formats;
	const u32 *formats;
	const u64 *modifiers;
	unsigned int min_width;
	unsigned int min_height;
	unsigned int max_width;
	unsigned int max_height;
	unsigned int rotation;
	unsigned int blend_mode;
	unsigned int color_encoding;

	/* 0 means no de-gamma LUT */
	unsigned int degamma_size;

	int min_scale; /* 16.16 fixed point */
	int max_scale; /* 16.16 fixed point */
	bool roi;
	bool color_mgmt;
	bool background;
};

struct es_dc_info {
	const char *name;

	/* planes */
	unsigned char plane_num;
	const struct es_plane_info *planes;

	unsigned int max_bpc;
	unsigned int color_formats;

	/* 0 means no gamma LUT */
	u16 gamma_size;
	u8 gamma_bits;

	u16 pitch_alignment;

	bool pipe_sync;
	bool mmu_prefetch;
	bool background;
};

#endif /* __ES_TYPE_H__ */
