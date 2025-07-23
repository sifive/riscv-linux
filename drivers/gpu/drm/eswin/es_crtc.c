// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Eswin Holdings Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/media-bus-format.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_vblank.h>

#include "es_drm.h"
#include "es_crtc.h"

void es_crtc_destroy(struct drm_crtc *crtc)
{
	struct es_crtc *es_crtc = to_es_crtc(crtc);

	drm_crtc_cleanup(crtc);
	kfree(es_crtc);
}

static void es_crtc_reset(struct drm_crtc *crtc)
{
	struct es_crtc_state *state;

	if (crtc->state) {
		__drm_atomic_helper_crtc_destroy_state(crtc->state);

		state = to_es_crtc_state(crtc->state);
		kfree(state);
		crtc->state = NULL;
	}

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (state == NULL)
		return;

	__drm_atomic_helper_crtc_reset(crtc, &state->base);

	state->sync_mode = ES_SINGLE_DC;
	state->output_fmt = MEDIA_BUS_FMT_RBG888_1X24;
	state->encoder_type = DRM_MODE_ENCODER_NONE;
#ifdef CONFIG_ESWIN_MMU
	state->mmu_prefetch = ES_MMU_PREFETCH_DISABLE;
#endif
}

static struct drm_crtc_state *
es_crtc_atomic_duplicate_state(struct drm_crtc *crtc)
{
	struct es_crtc_state *ori_state;
	struct es_crtc_state *state;

	if (WARN_ON(!crtc->state))
		return NULL;

	ori_state = to_es_crtc_state(crtc->state);
	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &state->base);

	state->sync_mode = ori_state->sync_mode;
	state->output_fmt = ori_state->output_fmt;
	state->encoder_type = ori_state->encoder_type;
	state->bpp = ori_state->bpp;
	state->dither_enable = ori_state->dither_enable;
	state->underflow = ori_state->underflow;
	state->bg_color = ori_state->bg_color;
#ifdef CONFIG_ESWIN_MMU
	state->mmu_prefetch = ori_state->mmu_prefetch;
#endif

	return &state->base;
}

static void es_crtc_atomic_destroy_state(struct drm_crtc *crtc,
					 struct drm_crtc_state *state)
{
	__drm_atomic_helper_crtc_destroy_state(state);
	kfree(to_es_crtc_state(state));
}

static int es_crtc_atomic_set_property(struct drm_crtc *crtc,
				       struct drm_crtc_state *state,
				       struct drm_property *property,
				       uint64_t val)
{
	struct es_crtc *es_crtc = to_es_crtc(crtc);
	struct es_crtc_state *es_crtc_state = to_es_crtc_state(state);

	if (property == es_crtc->sync_mode)
		es_crtc_state->sync_mode = val;
	else if (property == es_crtc->mmu_prefetch)
		es_crtc_state->mmu_prefetch = val;
	else if (property == es_crtc->dither)
		es_crtc_state->dither_enable = val;
	else if (property == es_crtc->bg_color)
		es_crtc_state->bg_color = val;
	else
		return -EINVAL;

	return 0;
}

static int es_crtc_atomic_get_property(struct drm_crtc *crtc,
				       const struct drm_crtc_state *state,
				       struct drm_property *property,
				       uint64_t *val)
{
	struct es_crtc *es_crtc = to_es_crtc(crtc);
	const struct es_crtc_state *es_crtc_state =
		container_of(state, const struct es_crtc_state, base);

	if (property == es_crtc->sync_mode)
		*val = es_crtc_state->sync_mode;
	else if (property == es_crtc->mmu_prefetch)
		*val = es_crtc_state->mmu_prefetch;
	else if (property == es_crtc->dither)
		*val = es_crtc_state->dither_enable;
	else if (property == es_crtc->bg_color)
		*val = es_crtc_state->bg_color;
	else
		return -EINVAL;

	return 0;
}

#ifdef CONFIG_DEBUG_FS
static int es_crtc_debugfs_show(struct seq_file *s, void *data)
{
	struct drm_crtc *crtc = s->private;
	struct es_crtc_state *crtc_state = to_es_crtc_state(crtc->state);
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;

	seq_printf(s, "crtc[%u]: %s\n", crtc->base.id, crtc->name);
	seq_printf(s, "\tactive = %d\n", crtc->state->active);
	seq_printf(s, "\tsize = %dx%d\n", mode->hdisplay, mode->vdisplay);
	seq_printf(s, "\tbpp = %u\n", crtc_state->bpp);
	seq_printf(s, "\tunderflow = %d\n", crtc_state->underflow);

	return 0;
}

static int es_crtc_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, es_crtc_debugfs_show, inode->i_private);
}

static const struct file_operations es_crtc_debugfs_fops = {
	.open = es_crtc_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int es_crtc_debugfs_init(struct drm_crtc *crtc)
{
	debugfs_create_file("status", 0444, crtc->debugfs_entry, crtc,
			    &es_crtc_debugfs_fops);

	return 0;
}
#else
static int es_crtc_debugfs_init(struct drm_crtc *crtc)
{
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

static int es_crtc_late_register(struct drm_crtc *crtc)
{
	return es_crtc_debugfs_init(crtc);
}

static int es_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct es_crtc *es_crtc = to_es_crtc(crtc);

	es_crtc->funcs->enable_vblank(es_crtc->dev, true);

	return 0;
}

static void es_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct es_crtc *es_crtc = to_es_crtc(crtc);

	es_crtc->funcs->enable_vblank(es_crtc->dev, false);
}

static const struct drm_crtc_funcs es_crtc_funcs = {
	.set_config = drm_atomic_helper_set_config,
	.destroy = es_crtc_destroy,
	.page_flip = drm_atomic_helper_page_flip,
	.reset = es_crtc_reset,
	.atomic_duplicate_state = es_crtc_atomic_duplicate_state,
	.atomic_destroy_state = es_crtc_atomic_destroy_state,
	.atomic_set_property = es_crtc_atomic_set_property,
	.atomic_get_property = es_crtc_atomic_get_property,
	// .gamma_set      = drm_atomic_helper_legacy_gamma_set, /* TODO: */
	.late_register = es_crtc_late_register,
	.enable_vblank = es_crtc_enable_vblank,
	.disable_vblank = es_crtc_disable_vblank,
};

static u8 cal_pixel_bits(u32 bus_format)
{
	u8 bpp;

	switch (bus_format) {
	case MEDIA_BUS_FMT_RGB565_1X16:
	case MEDIA_BUS_FMT_UYVY8_1X16:
		bpp = 16;
		break;
	case MEDIA_BUS_FMT_RGB666_1X18:
	case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
		bpp = 18;
		break;
	case MEDIA_BUS_FMT_UYVY10_1X20:
		bpp = 20;
		break;
	case MEDIA_BUS_FMT_BGR888_1X24:
	case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
	case MEDIA_BUS_FMT_YUV8_1X24:
		bpp = 24;
		break;
	case MEDIA_BUS_FMT_RGB101010_1X30:
	case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
	case MEDIA_BUS_FMT_YUV10_1X30:
		bpp = 30;
		break;
	default:
		bpp = 24;
		break;
	}

	return bpp;
}

static bool es_crtc_mode_fixup(struct drm_crtc *crtc,
			       const struct drm_display_mode *mode,
			       struct drm_display_mode *adjusted_mode)
{
	struct es_crtc *es_crtc = to_es_crtc(crtc);

	return es_crtc->funcs->mode_fixup(es_crtc->dev, mode, adjusted_mode);
}

static void es_crtc_atomic_enable(struct drm_crtc *crtc,
				  struct drm_atomic_state *old_state)
{
	struct es_crtc *es_crtc = to_es_crtc(crtc);
	struct es_crtc_state *es_crtc_state = to_es_crtc_state(crtc->state);

	es_crtc_state->bpp = cal_pixel_bits(es_crtc_state->output_fmt);

	es_crtc->funcs->enable(es_crtc->dev, crtc);

	drm_crtc_vblank_on(crtc);
}

static void es_crtc_atomic_disable(struct drm_crtc *crtc,
				   struct drm_atomic_state *old_state)
{
	struct es_crtc *es_crtc = to_es_crtc(crtc);

	drm_crtc_vblank_off(crtc);

	es_crtc->funcs->disable(es_crtc->dev);

	if (crtc->state->event && !crtc->state->active) {
		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irq(&crtc->dev->event_lock);

		crtc->state->event = NULL;
	}
}

static void es_crtc_atomic_begin(struct drm_crtc *crtc,
				 struct drm_atomic_state *old_crtc_state)
{
	struct es_crtc *es_crtc = to_es_crtc(crtc);
	struct device *dev = es_crtc->dev;
	struct drm_property_blob *blob = crtc->state->gamma_lut;
	struct drm_color_lut *lut;

	if (crtc->state->color_mgmt_changed) {
		if ((blob) && (blob->length)) {
			lut = blob->data;
			es_crtc->funcs->set_gamma(dev, lut,
						  blob->length / sizeof(*lut));
			es_crtc->funcs->enable_gamma(dev, true);
		} else {
			es_crtc->funcs->enable_gamma(dev, false);
		}
	}
}

static void es_crtc_atomic_flush(struct drm_crtc *crtc,
				 struct drm_atomic_state *old_crtc_state)
{
	struct es_crtc *es_crtc = to_es_crtc(crtc);
	struct drm_pending_vblank_event *event = crtc->state->event;

	es_crtc->funcs->commit(es_crtc->dev);

	if (event) {
		WARN_ON(drm_crtc_vblank_get(crtc) != 0);

		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_arm_vblank_event(crtc, event);
		spin_unlock_irq(&crtc->dev->event_lock);
		crtc->state->event = NULL;
	}
}

static const struct drm_crtc_helper_funcs es_crtc_helper_funcs = {
	.mode_fixup = es_crtc_mode_fixup,
	.atomic_enable = es_crtc_atomic_enable,
	.atomic_disable = es_crtc_atomic_disable,
	.atomic_begin = es_crtc_atomic_begin,
	.atomic_flush = es_crtc_atomic_flush,
};

static const struct drm_prop_enum_list es_sync_mode_enum_list[] = {
	{ ES_SINGLE_DC, "single dc mode" },
	{ ES_MULTI_DC_PRIMARY, "primary dc for multi dc mode" },
	{ ES_MULTI_DC_SECONDARY, "secondary dc for multi dc mode" },
};

#ifdef CONFIG_ESWIN_MMU
static const struct drm_prop_enum_list es_mmu_prefetch_enum_list[] = {
	{ ES_MMU_PREFETCH_DISABLE, "disable mmu prefetch" },
	{ ES_MMU_PREFETCH_ENABLE, "enable mmu prefetch" },
};
#endif

struct es_crtc *es_crtc_create(struct drm_device *drm_dev,
			       struct es_dc_info *info)
{
	struct es_crtc *crtc;
	int ret;

	if (!info)
		return NULL;

	crtc = kzalloc(sizeof(struct es_crtc), GFP_KERNEL);
	if (!crtc)
		return NULL;

	ret = drm_crtc_init_with_planes(drm_dev, &crtc->base, NULL, NULL,
					&es_crtc_funcs,
					info->name ? info->name : NULL);
	if (ret)
		goto err_free_crtc;

	drm_crtc_helper_add(&crtc->base, &es_crtc_helper_funcs);

	/* Set up the crtc properties */
	if (info->pipe_sync) {
		crtc->sync_mode = drm_property_create_enum(
			drm_dev, 0, "SYNC_MODE", es_sync_mode_enum_list,
			ARRAY_SIZE(es_sync_mode_enum_list));

		if (!crtc->sync_mode)
			goto err_cleanup_crts;

		drm_object_attach_property(&crtc->base.base, crtc->sync_mode,
					   ES_SINGLE_DC);
	}

	if (info->gamma_size) {
		ret = drm_mode_crtc_set_gamma_size(&crtc->base,
						   info->gamma_size);
		if (ret)
			goto err_cleanup_crts;

		drm_crtc_enable_color_mgmt(&crtc->base, 0, false,
					   info->gamma_size);
	}

	if (info->background) {
		crtc->bg_color = drm_property_create_range(
			drm_dev, 0, "BG_COLOR", 0, 0xffffffff);

		if (!crtc->bg_color)
			goto err_cleanup_crts;

		drm_object_attach_property(&crtc->base.base, crtc->bg_color, 0);
	}

	crtc->dither = drm_property_create_bool(drm_dev, 0, "DITHER_ENABLED");
	if (!crtc->dither)
		goto err_cleanup_crts;

	drm_object_attach_property(&crtc->base.base, crtc->dither, 0);

#ifdef CONFIG_ESWIN_MMU
	if (info->mmu_prefetch) {
		crtc->mmu_prefetch = drm_property_create_enum(
			drm_dev, 0, "MMU_PREFETCH", es_mmu_prefetch_enum_list,
			ARRAY_SIZE(es_mmu_prefetch_enum_list));
		if (!crtc->mmu_prefetch)
			goto err_cleanup_crts;

		drm_object_attach_property(&crtc->base.base, crtc->mmu_prefetch,
					   ES_MMU_PREFETCH_DISABLE);
	}
#endif

	crtc->max_bpc = info->max_bpc;
	crtc->color_formats = info->color_formats;
	return crtc;

err_cleanup_crts:
	drm_crtc_cleanup(&crtc->base);

err_free_crtc:
	kfree(crtc);
	return NULL;
}

void es_crtc_handle_vblank(struct drm_crtc *crtc, bool underflow)
{
	struct es_crtc_state *es_crtc_state = to_es_crtc_state(crtc->state);

	drm_crtc_handle_vblank(crtc);

	es_crtc_state->underflow = underflow;
}
