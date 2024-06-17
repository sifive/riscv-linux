// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Eswin Holdings Co., Ltd.
 */

#include <linux/component.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include <linux/media-bus-format.h>

#include <drm/drm_framebuffer.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_blend.h>

#include "es_drm.h"
#include "es_type.h"
#include "es_dc_hw.h"
#include "es_dc.h"
#include "es_crtc.h"
#include "es_drv.h"

#define VO_ACLK_HIGHEST 800000000

static inline void update_format(u32 format, struct dc_hw_fb *fb)
{
	u8 f;

	switch (format) {
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_BGRX4444:
		f = FORMAT_X4R4G4B4;
		break;
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_BGRA4444:
		f = FORMAT_A4R4G4B4;
		break;
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_BGRX5551:
		f = FORMAT_X1R5G5B5;
		break;
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_BGRA5551:
		f = FORMAT_A1R5G5B5;
		break;
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_RGB565:
		f = FORMAT_R5G6B5;
		break;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRX8888:
		f = FORMAT_X8R8G8B8;
		break;
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_BGRA8888:
		f = FORMAT_A8R8G8B8;
		break;
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
		f = FORMAT_YUY2;
		break;
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		f = FORMAT_UYVY;
		break;
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV420:
		f = FORMAT_YV12;
		break;
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
		f = FORMAT_NV12;
		break;
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		f = FORMAT_NV16;
		break;
	case DRM_FORMAT_P010:
		f = FORMAT_P010;
		break;
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_BGRA1010102:
		f = FORMAT_A2R10G10B10;
		break;
	default:
		f = FORMAT_A8R8G8B8;
		break;
	}
	fb->format = f;
}

static inline void update_swizzle(u32 format, struct dc_hw_fb *fb)
{
	fb->swizzle = SWIZZLE_ARGB;
	fb->uv_swizzle = 0;

	switch (format) {
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBA1010102:
		fb->swizzle = SWIZZLE_RGBA;
		break;
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_ABGR2101010:
		fb->swizzle = SWIZZLE_ABGR;
		break;
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_BGRX4444:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_BGRA5551:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_BGRA1010102:
		fb->swizzle = SWIZZLE_BGRA;
		break;
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV61:
		fb->uv_swizzle = 1;
		break;
	default:
		break;
	}
}

static inline u8 to_es_rotation(unsigned int rotation)
{
	u8 rot;

	switch (rotation & DRM_MODE_REFLECT_MASK) {
	case DRM_MODE_REFLECT_X:
		rot = FLIP_X;
		return rot;
	case DRM_MODE_REFLECT_Y:
		rot = FLIP_Y;
		return rot;
	case DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y:
		rot = FLIP_XY;
		return rot;
	default:
		break;
	}

	switch (rotation & DRM_MODE_ROTATE_MASK) {
	case DRM_MODE_ROTATE_0:
		rot = ROT_0;
		break;
	case DRM_MODE_ROTATE_90:
		rot = ROT_90;
		break;
	case DRM_MODE_ROTATE_180:
		rot = ROT_180;
		break;
	case DRM_MODE_ROTATE_270:
		rot = ROT_270;
		break;
	default:
		rot = ROT_0;
		break;
	}

	return rot;
}

static inline u8 to_es_yuv_color_space(u32 color_space)
{
	u8 cs;

	switch (color_space) {
	case DRM_COLOR_YCBCR_BT601:
		cs = COLOR_SPACE_601;
		break;
	case DRM_COLOR_YCBCR_BT709:
		cs = COLOR_SPACE_709;
		break;
	case DRM_COLOR_YCBCR_BT2020:
		cs = COLOR_SPACE_2020;
		break;
	default:
		cs = COLOR_SPACE_601;
		break;
	}

	return cs;
}

static inline u8 to_es_tile_mode(u64 modifier)
{
	// DRM_FORMAT_MOD_ES_NORM_MODE_MASK 0x1F
	return (u8)(modifier & 0x1F);
}

static int es_dc_clk_configs(struct device *dev, bool enable)
{
	int ret = 0;
	struct es_dc *dc = dev_get_drvdata(dev);

	if (enable) {
		if (dc->dc_clkon) {
			return 0;
		}

		ret = clk_prepare_enable(dc->cfg_clk);
		if (ret < 0) {
			dev_err(dev, "failed to prepare/enable cfg_clk\n");
			return ret;
		}

		ret = clk_prepare_enable(dc->pix_clk);
		if (ret < 0) {
			dev_err(dev, "failed to prepare/enable pix_clk\n");
			return ret;
		}

		ret = clk_prepare_enable(dc->axi_clk);
		if (ret < 0) {
			dev_err(dev, "failed to prepare/enable axi_clk\n");
			return ret;
		}

		dc->pix_clk_rate = clk_get_rate(dc->pix_clk);
		dc->dc_clkon = true;
	} else {
		if (!dc->dc_clkon) {
			return 0;
		}
		clk_disable_unprepare(dc->pix_clk);
		clk_disable_unprepare(dc->axi_clk);
		clk_disable_unprepare(dc->cfg_clk);
		dc->dc_clkon = false;
	}

	return 0;
}

static void dc_deinit(struct device *dev)
{
	struct es_dc *dc = dev_get_drvdata(dev);

	dc_hw_enable_interrupt(&dc->hw, 0);
	dc_hw_deinit(&dc->hw);
	es_dc_clk_configs(dev, false);
}

static int dc_init(struct device *dev)
{
	struct es_dc *dc = dev_get_drvdata(dev);
	int ret;
	long rate;

	dc->first_frame = true;

	ret = clk_set_parent(dc->vo_mux, dc->spll0_fout1);
	if (ret < 0) {
		pr_err("DC: failed to set core clk parent: %d\n", ret);
		return ret;
	}

	rate = clk_round_rate(dc->axi_clk, VO_ACLK_HIGHEST);
	if (rate > 0) {
		ret = clk_set_rate(dc->axi_clk, rate);
		if (ret) {
			pr_err("DC: failed to set axi clk: %d\n", ret);
			return ret;
		}
	}

	ret = es_dc_clk_configs(dev, true);
	if (ret < 0) {
		dev_err(dev, "failed to prepare/enable clks\n");
		return ret;
	}

	ret = dc_hw_init(&dc->hw);
	if (ret) {
		dev_err(dev, "failed to init DC HW\n");
		return ret;
	}

	return 0;
}

static void es_dc_dump_enable(struct device *dev, dma_addr_t addr,
			      unsigned int pitch)
{
	struct es_dc *dc = dev_get_drvdata(dev);

	dc_hw_enable_dump(&dc->hw, addr, pitch);
}

static void es_dc_dump_disable(struct device *dev)
{
	struct es_dc *dc = dev_get_drvdata(dev);

	dc_hw_disable_dump(&dc->hw);
}

static void es_dc_enable(struct device *dev, struct drm_crtc *crtc)
{
	struct es_dc *dc = dev_get_drvdata(dev);
	struct es_crtc_state *crtc_state = to_es_crtc_state(crtc->state);
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;
	struct dc_hw_display display;

	display.bus_format = crtc_state->output_fmt;
	display.h_active = mode->hdisplay;
	display.h_total = mode->htotal;
	display.h_sync_start = mode->hsync_start;
	display.h_sync_end = mode->hsync_end;
	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		display.h_sync_polarity = true;
	else
		display.h_sync_polarity = false;

	display.v_active = mode->vdisplay;
	display.v_total = mode->vtotal;
	display.v_sync_start = mode->vsync_start;
	display.v_sync_end = mode->vsync_end;
	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		display.v_sync_polarity = true;
	else
		display.v_sync_polarity = false;

	display.sync_mode = crtc_state->sync_mode;
	display.bg_color = crtc_state->bg_color;
	display.dither_enable = crtc_state->dither_enable;

	display.enable = true;
	es_dc_clk_configs(dev, true);

	if (dc->pix_clk_rate != mode->clock) {
		clk_set_rate(dc->pix_clk, mode->clock * 1000);
		dc->pix_clk_rate = mode->clock;
	}

	if (crtc_state->encoder_type == DRM_MODE_ENCODER_DSI ||
	    crtc_state->encoder_type == DRM_MODE_ENCODER_VIRTUAL ||
	    crtc_state->encoder_type == DRM_MODE_ENCODER_NONE)
		dc_hw_set_out(&dc->hw, OUT_DPI);
	else
		dc_hw_set_out(&dc->hw, OUT_DP);

#ifdef CONFIG_ESWIN_MMU
	if (crtc_state->mmu_prefetch == ES_MMU_PREFETCH_ENABLE)
		dc_hw_enable_mmu_prefetch(&dc->hw, true);
	else
		dc_hw_enable_mmu_prefetch(&dc->hw, false);
#endif

	dc_hw_setup_display(&dc->hw, &display);
}

static void es_dc_disable(struct device *dev)
{
	struct es_dc *dc = dev_get_drvdata(dev);
	struct dc_hw_display display;

	display.enable = false;

	dc_hw_setup_display(&dc->hw, &display);
	es_dc_clk_configs(dev, false);
}

static bool es_dc_mode_fixup(struct device *dev,
			     const struct drm_display_mode *mode,
			     struct drm_display_mode *adjusted_mode)
{
	struct es_dc *dc = dev_get_drvdata(dev);
	long clk_rate;

	if (dc->pix_clk) {
		clk_rate = clk_round_rate(dc->pix_clk,
					  adjusted_mode->clock * 1000);
		adjusted_mode->clock = DIV_ROUND_UP(clk_rate, 1000);
	}

	return true;
}

static void es_dc_set_gamma(struct device *dev, struct drm_color_lut *lut,
			    unsigned int size)
{
	struct es_dc *dc = dev_get_drvdata(dev);
	u16 i, r, g, b;
	u8 bits;

	if (size != dc->hw.info->gamma_size) {
		dev_err(dev, "gamma size does not match!\n");
		return;
	}

	bits = dc->hw.info->gamma_bits;
	for (i = 0; i < size; i++) {
		r = drm_color_lut_extract(lut[i].red, bits);
		g = drm_color_lut_extract(lut[i].green, bits);
		b = drm_color_lut_extract(lut[i].blue, bits);
		dc_hw_update_gamma(&dc->hw, i, r, g, b);
	}
}

static void es_dc_enable_gamma(struct device *dev, bool enable)
{
	struct es_dc *dc = dev_get_drvdata(dev);

	dc_hw_enable_gamma(&dc->hw, enable);
}

static void es_dc_enable_vblank(struct device *dev, bool enable)
{
	struct es_dc *dc = dev_get_drvdata(dev);

	dc_hw_enable_interrupt(&dc->hw, enable);
}

static u32 calc_factor(u32 src, u32 dest)
{
	u32 factor = 1 << 16;

	if ((src > 1) && (dest > 1))
		factor = ((src - 1) << 16) / (dest - 1);

	return factor;
}

static void update_scale(struct dc_hw_scale *scale, int width, int height,
			 int dst_w, int dst_h, unsigned int rotation,
			 struct dc_hw_roi *roi)
{
	int src_w, src_h, temp;

	scale->enable = false;
	if (roi->enable) {
		src_w = roi->width;
		src_h = roi->height;
	} else {
		src_w = width;
		src_h = height;
	}

	if (drm_rotation_90_or_270(rotation)) {
		temp = src_w;
		src_w = src_h;
		src_h = temp;
	}

	if (src_w != dst_w) {
		scale->scale_factor_x = calc_factor(src_w, dst_w);
		scale->enable = true;
	} else {
		scale->scale_factor_x = 1 << 16;
	}
	if (src_h != dst_h) {
		scale->scale_factor_y = calc_factor(src_h, dst_h);
		scale->enable = true;
	} else {
		scale->scale_factor_y = 1 << 16;
	}
}

static void update_fb(struct es_plane *plane, struct dc_hw_fb *fb)
{
	struct drm_plane_state *state = plane->base.state;
	struct drm_framebuffer *drm_fb = state->fb;
	struct drm_rect *src = &state->src;

	fb->y_address = plane->dma_addr[0];
	fb->y_stride = drm_fb->pitches[0];
	if (drm_fb->format->format == DRM_FORMAT_YVU420) {
		fb->u_address = plane->dma_addr[2];
		fb->v_address = plane->dma_addr[1];
		fb->u_stride = drm_fb->pitches[2];
		fb->v_stride = drm_fb->pitches[1];
	} else {
		fb->u_address = plane->dma_addr[1];
		fb->v_address = plane->dma_addr[2];
		fb->u_stride = drm_fb->pitches[1];
		fb->v_stride = drm_fb->pitches[2];
	}
	fb->width = drm_rect_width(src) >> 16;
	fb->height = drm_rect_height(src) >> 16;
	fb->tile_mode = to_es_tile_mode(drm_fb->modifier);
	fb->rotation = to_es_rotation(state->rotation);
	fb->yuv_color_space = to_es_yuv_color_space(state->color_encoding);
	fb->enable = state->visible;
	update_format(drm_fb->format->format, fb);
	update_swizzle(drm_fb->format->format, fb);
}

static void update_degamma(struct es_dc *dc, struct es_plane *plane,
			   struct es_plane_state *plane_state)
{
	dc_hw_update_degamma(&dc->hw, plane->id, plane_state->degamma);
	plane_state->degamma_changed = false;
}

void update_roi(struct es_dc *dc, enum dc_hw_plane_id id,
		struct es_plane_state *plane_state, struct dc_hw_roi *roi,
		struct dc_hw_fb *fb)
{
	struct drm_es_roi *data;
	u16 src_w = fb->width;
	u16 src_h = fb->height;

	if (plane_state->roi) {
		data = (struct drm_es_roi *)plane_state->roi->data;
		if (data->enable) {
			roi->x = data->roi_x;
			roi->y = data->roi_y;
			roi->width = (data->roi_w + data->roi_x > src_w) ?
					     (src_w - data->roi_x) :
					     data->roi_w;
			roi->height = (data->roi_h + data->roi_y > src_h) ?
					      (src_h - data->roi_y) :
					      data->roi_h;
			roi->enable = true;
		} else {
			roi->enable = false;
			roi->width = src_w;
			roi->height = src_h;
		}

		dc_hw_update_roi(&dc->hw, id, roi);
	} else {
		roi->enable = false;
	}
}

static void update_color_mgmt(struct es_dc *dc, u8 id, struct dc_hw_fb *fb,
			      struct es_plane_state *plane_state)
{
	struct drm_es_color_mgmt *data;
	struct dc_hw_colorkey colorkey;

	if (plane_state->color_mgmt) {
		data = plane_state->color_mgmt->data;

		fb->clear_enable = data->clear_enable;
		fb->clear_value = data->clear_value;

		if (data->colorkey > data->colorkey_high)
			data->colorkey = data->colorkey_high;

		colorkey.colorkey = data->colorkey;
		colorkey.colorkey_high = data->colorkey_high;
		colorkey.transparency = (data->transparency) ?
						DC_TRANSPARENCY_KEY :
						DC_TRANSPARENCY_OPAQUE;
		dc_hw_update_colorkey(&dc->hw, id, &colorkey);
	}
}

static void update_primary_plane(struct es_dc *dc, struct es_plane *plane)
{
	struct dc_hw_fb fb = { 0 };
	struct dc_hw_scale scale;
	struct drm_plane_state *state = plane->base.state;
	struct es_plane_state *plane_state = to_es_plane_state(state);
	struct drm_crtc *crtc = state->crtc;
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;
	struct dc_hw_roi roi;

	update_fb(plane, &fb);

	update_roi(dc, plane->id, plane_state, &roi, &fb);

	update_scale(&scale, fb.width, fb.height, mode->hdisplay,
		     mode->vdisplay, state->rotation, &roi);

	if (plane_state->degamma_changed)
		update_degamma(dc, plane, plane_state);

	update_color_mgmt(dc, plane->id, &fb, plane_state);

	dc_hw_update_plane(&dc->hw, plane->id, &fb, &scale);
}

static void update_overlay_plane(struct es_dc *dc, struct es_plane *plane)
{
	struct dc_hw_fb fb = { 0 };
	struct dc_hw_scale scale;
	struct dc_hw_position pos;
	struct dc_hw_blend blend;
	struct drm_plane_state *state = plane->base.state;
	struct es_plane_state *plane_state = to_es_plane_state(state);
	struct drm_rect *dest = &state->dst;
	struct dc_hw_roi roi;

	update_fb(plane, &fb);
	update_roi(dc, plane->id, plane_state, &roi, &fb);
	update_scale(&scale, fb.width, fb.height, drm_rect_width(dest),
		     drm_rect_height(dest), state->rotation, &roi);

	if (plane_state->degamma_changed)
		update_degamma(dc, plane, plane_state);

	update_color_mgmt(dc, plane->id, &fb, plane_state);

	dc_hw_update_plane(&dc->hw, plane->id, &fb, &scale);

	pos.start_x = dest->x1;
	pos.start_y = dest->y1;
	pos.end_x = dest->x2;
	pos.end_y = dest->y2;
	dc_hw_set_position(&dc->hw, &pos);

	blend.alpha = (u8)(state->alpha >> 8);
	blend.blend_mode = (u8)(state->pixel_blend_mode);
	dc_hw_set_blend(&dc->hw, &blend);
}

static void update_cursor_size(struct drm_plane_state *state, struct dc_hw_cursor *cursor)
{
	u8 size_type;

	switch (state->crtc_w) {
		case 32:
			size_type = CURSOR_SIZE_32X32;
			break;
		case 64:
			size_type = CURSOR_SIZE_64X64;
			break;
		case 128:
			size_type = CURSOR_SIZE_128X128;
			break;
		case 256:
			size_type = CURSOR_SIZE_256X256;
			break;
		default:
			size_type = CURSOR_SIZE_32X32;
			break;
	}

	cursor->size = size_type;
}

static void update_cursor_plane(struct es_dc *dc, struct es_plane *plane)
{
	struct drm_plane_state *state = plane->base.state;
	struct dc_hw_cursor cursor;

	cursor.address = plane->dma_addr[0];

	if (state->crtc_x > 0) {
		cursor.x = state->crtc_x;
		cursor.hot_x = 0;
	} else {
		cursor.hot_x = -state->crtc_x;
		cursor.x = 0;
	}
	if (state->crtc_y > 0) {
		cursor.y = state->crtc_y;
		cursor.hot_y = 0;
	} else {
		cursor.hot_y = -state->crtc_y;
		cursor.y = 0;
	}

	update_cursor_size(state, &cursor);
	cursor.enable = true;

	dc_hw_update_cursor(&dc->hw, &cursor);
}

static void es_dc_update_plane(struct device *dev, struct es_plane *plane)
{
	struct es_dc *dc = dev_get_drvdata(dev);
	enum drm_plane_type type = plane->base.type;

	switch (type) {
	case DRM_PLANE_TYPE_PRIMARY:
		update_primary_plane(dc, plane);
		break;
	case DRM_PLANE_TYPE_OVERLAY:
		update_overlay_plane(dc, plane);
		break;
	case DRM_PLANE_TYPE_CURSOR:
		update_cursor_plane(dc, plane);
		break;
	default:
		break;
	}
}

static void es_dc_disable_plane(struct device *dev, struct es_plane *plane)
{
	struct es_dc *dc = dev_get_drvdata(dev);
	enum drm_plane_type type = plane->base.type;
	struct dc_hw_fb fb = { 0 };
	struct dc_hw_cursor cursor = { 0 };

	switch (type) {
	case DRM_PLANE_TYPE_PRIMARY:
	case DRM_PLANE_TYPE_OVERLAY:
		fb.enable = false;
		dc_hw_update_plane(&dc->hw, plane->id, &fb, NULL);
		break;
	case DRM_PLANE_TYPE_CURSOR:
		cursor.enable = false;
		dc_hw_update_cursor(&dc->hw, &cursor);
		break;
	default:
		break;
	}
}

static bool es_dc_mod_supported(const struct es_plane_info *plane_info,
				u64 modifier)
{
	const u64 *mods;

	if (plane_info->modifiers == NULL)
		return false;

	for (mods = plane_info->modifiers; *mods != DRM_FORMAT_MOD_INVALID;
	     mods++) {
		if (*mods == modifier)
			return true;
	}

	return false;
}

static int es_dc_check_plane(struct device *dev, struct es_plane *plane,
			     struct drm_plane_state *state)
{
	struct es_dc *dc = dev_get_drvdata(dev);
	struct drm_framebuffer *fb = state->fb;
	const struct es_plane_info *plane_info;
	struct drm_crtc *crtc = state->crtc;
	struct drm_crtc_state *crtc_state;

	plane_info = &dc->hw.info->planes[plane->id];
	if (plane_info == NULL)
		return -EINVAL;

	if (fb->width < plane_info->min_width ||
	    fb->width > plane_info->max_width ||
	    fb->height < plane_info->min_height ||
	    fb->height > plane_info->max_height)
		dev_err_once(dev, "buffer size may not support on plane%d.\n",
			     plane->id);

	if ((plane->base.type != DRM_PLANE_TYPE_CURSOR) &&
	    (!es_dc_mod_supported(plane_info, fb->modifier))) {
		dev_err(dev, "unsupported modifier on plane%d.\n", plane->id);
		return -EINVAL;
	}

	crtc_state = drm_atomic_get_existing_crtc_state(state->state, crtc);
	if (IS_ERR(crtc_state))
		return -EINVAL;

	return drm_atomic_helper_check_plane_state(state, crtc_state,
						   plane_info->min_scale,
						   plane_info->max_scale, true,
						   true);
}

static irqreturn_t dc_isr(int irq, void *data)
{
	struct es_dc *dc = data;

	dc_hw_get_interrupt(&dc->hw);

	if (!dc->dc_initialized) {
		return IRQ_HANDLED;
	}

	es_crtc_handle_vblank(&dc->crtc->base, dc_hw_check_underflow(&dc->hw));

	return IRQ_HANDLED;
}

static void es_dc_commit(struct device *dev)
{
	struct es_dc *dc = dev_get_drvdata(dev);

#ifdef CONFIG_ESWIN_MMU
	dc_mmu_flush(&dc->hw);
#endif

	if (!dc->first_frame) {
		if (dc_hw_flip_in_progress(&dc->hw))
			udelay(100);

		dc_hw_enable_shadow_register(&dc->hw, false);
	}

	dc_hw_commit(&dc->hw);

	if (dc->first_frame)
		dc->first_frame = false;

	if (!dc->dc_initialized)
		dc->dc_initialized = true;

	dc_hw_enable_shadow_register(&dc->hw, true);
}

static const struct es_crtc_funcs dc_crtc_funcs = {
	.enable = es_dc_enable,
	.disable = es_dc_disable,
	.mode_fixup = es_dc_mode_fixup,
	.set_gamma = es_dc_set_gamma,
	.enable_gamma = es_dc_enable_gamma,
	.enable_vblank = es_dc_enable_vblank,
	.commit = es_dc_commit,
};

static const struct es_plane_funcs dc_plane_funcs = {
	.update = es_dc_update_plane,
	.disable = es_dc_disable_plane,
	.check = es_dc_check_plane,
};

static const struct es_dc_funcs dc_funcs = {
	.dump_enable = es_dc_dump_enable,
	.dump_disable = es_dc_dump_disable,
};

static int dc_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm_dev = data;
#ifdef CONFIG_ESWIN_MMU
	struct es_drm_private *priv = drm_dev->dev_private;
#endif
	struct es_dc *dc = dev_get_drvdata(dev);
	struct device_node *port;
	struct es_crtc *crtc;
	struct es_dc_info *dc_info;
	struct es_plane *plane;
	struct drm_plane *drm_plane, *tmp;
	struct es_plane_info *plane_info;
	int i, ret;

	if (!drm_dev || !dc) {
		dev_err(dev, "devices are not created.\n");
		return -ENODEV;
	}

	ret = dc_init(dev);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize DC hardware.\n");
		return ret;
	}

#ifdef CONFIG_ESWIN_MMU
	ret = dc_mmu_construct(priv->dma_dev, &priv->mmu);
	if (ret) {
		dev_err(dev, "failed to construct DC MMU\n");
		goto err_clean_dc;
	}

	ret = dc_hw_mmu_init(&dc->hw, priv->mmu);
	if (ret) {
		dev_err(dev, "failed to init DC MMU\n");
		goto err_clean_dc;
	}
#endif

	ret = es_drm_iommu_attach_device(drm_dev, dev);
	if (ret < 0) {
		dev_err(dev, "Failed to attached iommu device.\n");
		goto err_clean_dc;
	}

	port = of_get_child_by_name(dev->of_node, "port");
	if (!port) {
		dev_err(dev, "no port node found\n");
		goto err_detach_dev;
	}
	of_node_put(port);

	dc_info = dc->hw.info;
	crtc = es_crtc_create(drm_dev, dc_info);
	if (!crtc) {
		dev_err(dev, "Failed to create CRTC.\n");
		ret = -ENOMEM;
		goto err_detach_dev;
	}

	crtc->base.port = port;
	crtc->dev = dev;
	crtc->funcs = &dc_crtc_funcs;

	for (i = 0; i < dc_info->plane_num; i++) {
		plane_info = (struct es_plane_info *)&dc_info->planes[i];

		plane = es_plane_create(drm_dev, plane_info,
					drm_crtc_mask(&crtc->base));
		if (!plane)
			goto err_cleanup_planes;

		plane->funcs = &dc_plane_funcs;

		if (plane_info->type == DRM_PLANE_TYPE_PRIMARY) {
			crtc->base.primary = &plane->base;
			drm_dev->mode_config.min_width = plane_info->min_width;
			drm_dev->mode_config.min_height =
				plane_info->min_height;
			drm_dev->mode_config.max_width = plane_info->max_width;
			drm_dev->mode_config.max_height =
				plane_info->max_height;
		}

		if (plane_info->type == DRM_PLANE_TYPE_CURSOR) {
			crtc->base.cursor = &plane->base;
			drm_dev->mode_config.cursor_width =
				plane_info->max_width;
			drm_dev->mode_config.cursor_height =
				plane_info->max_height;
		}
	}

	dc->crtc = crtc;
	dc->funcs = &dc_funcs;

	es_drm_update_pitch_alignment(drm_dev, dc_info->pitch_alignment);

	es_dc_clk_configs(dev, false);

	return 0;

err_cleanup_planes:
	list_for_each_entry_safe (drm_plane, tmp,
				  &drm_dev->mode_config.plane_list, head)
		if (drm_plane->possible_crtcs == drm_crtc_mask(&crtc->base))
			es_plane_destory(drm_plane);

	es_crtc_destroy(&crtc->base);
err_detach_dev:
	es_drm_iommu_detach_device(drm_dev, dev);
err_clean_dc:
	dc_deinit(dev);
	return ret;
}

static void dc_unbind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm_dev = data;

	dc_deinit(dev);

	es_drm_iommu_detach_device(drm_dev, dev);
}

const struct component_ops dc_component_ops = {
	.bind = dc_bind,
	.unbind = dc_unbind,
};

static void vo_qos_cfg(void)
{
	void __iomem *qos;

	#define VO_QOS_CSR	0x50281050UL
	qos = ioremap(VO_QOS_CSR, 8);
	if (!qos) {
		printk("qos ioremap fail---------------\n");
		return;
	}
	writel(0x9, qos);
	writel(0x9, (char *)qos + 4);

	iounmap(qos);
	return;
}

static const struct of_device_id dc_driver_dt_match[] = {
	{
		.compatible = "eswin,dc",
	},
	{},
};
MODULE_DEVICE_TABLE(of, dc_driver_dt_match);

static int dc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct es_dc *dc;
	int irq, ret;

	dc = devm_kzalloc(dev, sizeof(*dc), GFP_KERNEL);
	if (!dc)
		return -ENOMEM;

	dc->hw.hi_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dc->hw.hi_base))
		return PTR_ERR(dc->hw.hi_base);

#ifdef CONFIG_ESWIN_MMU
	dc->hw.mmu_base = devm_platform_ioremap_resource(pdev, 1);
	if (IS_ERR(dc->hw.mmu_base))
		return PTR_ERR(dc->hw.mmu_base);
#endif

	dc->hw.reg_base = devm_platform_ioremap_resource(pdev, 2);
	if (IS_ERR(dc->hw.reg_base))
		return PTR_ERR(dc->hw.reg_base);

	irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(dev, irq, dc_isr, 0, dev_name(dev), dc);
	if (ret < 0) {
		dev_err(dev, "Failed to install irq:%u.\n", irq);
		return ret;
	}

	dc->vo_mux = devm_clk_get(dev, "vo_mux");
	if (IS_ERR(dc->vo_mux)) {
		ret = PTR_ERR(dc->vo_mux);
		dev_err(dev, "failed to get vo_mux: %d\n", ret);
		return ret;
	}

	dc->spll0_fout1 = devm_clk_get_optional(dev, "spll0_fout1");
	if (IS_ERR(dc->spll0_fout1)) {
		dev_err(dev, "failed to get spll0_fout1 source\n");
		return PTR_ERR(dc->spll0_fout1);
	}

	dc->cfg_clk = devm_clk_get_optional(dev, "cfg_clk");
	if (IS_ERR(dc->cfg_clk)) {
		dev_err(dev, "failed to get cfg_clk source\n");
		return PTR_ERR(dc->cfg_clk);
	}

	dc->pix_clk = devm_clk_get_optional(dev, "pix_clk");
	if (IS_ERR(dc->pix_clk)) {
		dev_err(dev, "failed to get pix_clk source\n");
		return PTR_ERR(dc->pix_clk);
	}

	dc->axi_clk = devm_clk_get_optional(dev, "axi_clk");
	if (IS_ERR(dc->axi_clk)) {
		dev_err(dev, "failed to get axi_clk source\n");
		return PTR_ERR(dc->axi_clk);
	}

	dc->vo_arst = devm_reset_control_get_optional(dev, "vo_arst");
	if (IS_ERR_OR_NULL(dc->vo_arst)) {
		dev_err(dev, "Failed to vo_arst handle\n");
		return PTR_ERR(dc->vo_arst);
	}

	dc->vo_prst = devm_reset_control_get_optional(dev, "vo_prst");
	if (IS_ERR_OR_NULL(dc->vo_prst)) {
		dev_err(dev, "Failed to vo_prst handle\n");
		return PTR_ERR(dc->vo_prst);
	}

	dc->dc_arst = devm_reset_control_get_optional(dev, "dc_arst");
	if (IS_ERR_OR_NULL(dc->dc_arst)) {
		dev_err(dev, "Failed to dc_arst handle\n");
		return PTR_ERR(dc->dc_arst);
	}

	dc->dc_prst = devm_reset_control_get_optional(dev, "dc_prst");
	if (IS_ERR_OR_NULL(dc->dc_prst)) {
		dev_err(dev, "Failed to dc_prst handle\n");
		return PTR_ERR(dc->dc_prst);
	}

	/* reset dc first to ensure no data on axi bus */
	if (dc->dc_arst) {
		ret = reset_control_reset(dc->dc_arst);
		WARN_ON(0 != ret);
	}

	if (dc->dc_prst) {
		ret = reset_control_reset(dc->dc_prst);
		WARN_ON(0 != ret);
	}

	if (dc->vo_arst) {
		ret = reset_control_reset(dc->vo_arst);
		WARN_ON(0 != ret);
	}

	if (dc->vo_prst) {
		ret = reset_control_reset(dc->vo_prst);
		WARN_ON(0 != ret);
	}

	dev_set_drvdata(dev, dc);
	vo_qos_cfg();
	return component_add(dev, &dc_component_ops);
}

static int dc_remove(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct es_dc *dc = dev_get_drvdata(dev);

	component_del(dev, &dc_component_ops);

	if (dc->dc_arst) {
		ret = reset_control_assert(dc->dc_arst);
		WARN_ON(0 != ret);
	}

	if (dc->dc_prst) {
		ret = reset_control_assert(dc->dc_prst);
		WARN_ON(0 != ret);
	}

	if (dc->vo_arst) {
		ret = reset_control_assert(dc->vo_arst);
		WARN_ON(0 != ret);
	}

	if (dc->vo_prst) {
		ret = reset_control_assert(dc->vo_prst);
		WARN_ON(0 != ret);
	}

	dev_set_drvdata(dev, NULL);

	return 0;
}

struct platform_driver dc_platform_driver = {
    .probe = dc_probe,
    .remove = dc_remove,
    .driver = {
        .name = "es-dc",
        .of_match_table = of_match_ptr(dc_driver_dt_match),
    },
};

MODULE_DESCRIPTION("Eswin DC Driver");
MODULE_LICENSE("GPL v2");
