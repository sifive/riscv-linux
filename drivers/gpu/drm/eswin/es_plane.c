// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Eswin Holdings Co., Ltd.
 */

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_blend.h>

#include "es_drm.h"
#include "es_type.h"
#include "es_crtc.h"
#include "es_plane.h"
#include "es_gem.h"
#include "es_fb.h"

void es_plane_destory(struct drm_plane *plane)
{
	struct es_plane *es_plane = to_es_plane(plane);

	drm_plane_cleanup(plane);
	kfree(es_plane);
}

static void es_plane_reset(struct drm_plane *plane)
{
	struct es_plane_state *state;

	if (plane->state) {
		__drm_atomic_helper_plane_destroy_state(plane->state);

		state = to_es_plane_state(plane->state);
		kfree(state);
		plane->state = NULL;
	}

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (state == NULL)
		return;

	__drm_atomic_helper_plane_reset(plane, &state->base);

	state->degamma = ES_DEGAMMA_DISABLE;
	state->degamma_changed = false;
	memset(&state->status, 0, sizeof(state->status));
}

static void _es_plane_duplicate_blob(struct es_plane_state *state,
				     struct es_plane_state *ori_state)
{
	state->roi = ori_state->roi;
	state->color_mgmt = ori_state->color_mgmt;
	if (state->roi)
		drm_property_blob_get(state->roi);
	if (state->color_mgmt)
		drm_property_blob_get(state->color_mgmt);
}

static struct drm_plane_state *
es_plane_atomic_duplicate_state(struct drm_plane *plane)
{
	struct es_plane_state *ori_state;
	struct es_plane_state *state;

	if (WARN_ON(!plane->state))
		return NULL;

	ori_state = to_es_plane_state(plane->state);
	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &state->base);

	state->degamma = ori_state->degamma;
	state->degamma_changed = ori_state->degamma_changed;

	_es_plane_duplicate_blob(state, ori_state);
	memcpy(&state->status, &ori_state->status, sizeof(ori_state->status));

	return &state->base;
}

static void es_plane_atomic_destroy_state(struct drm_plane *plane,
					  struct drm_plane_state *state)
{
	struct es_plane_state *es_plane_state = to_es_plane_state(state);

	__drm_atomic_helper_plane_destroy_state(state);
	drm_property_blob_put(es_plane_state->roi);
	drm_property_blob_put(es_plane_state->color_mgmt);
	kfree(es_plane_state);
}

static int _es_plane_set_property_blob_from_id(struct drm_device *dev,
					       struct drm_property_blob **blob,
					       u64 blob_id,
					       size_t expected_size)
{
	struct drm_property_blob *new_blob = NULL;

	if (blob_id) {
		new_blob = drm_property_lookup_blob(dev, blob_id);
		if (!new_blob) {
			return -EINVAL;
		}

		if (new_blob->length != expected_size) {
			drm_property_blob_put(new_blob);
			return -EINVAL;
		}
	}
	drm_property_replace_blob(blob, new_blob);
	drm_property_blob_put(new_blob);
	return 0;
}

static int es_plane_atomic_set_property(struct drm_plane *plane,
					struct drm_plane_state *state,
					struct drm_property *property,
					uint64_t val)
{
	struct drm_device *dev = plane->dev;
	struct es_plane *es_plane = to_es_plane(plane);
	struct es_plane_state *es_plane_state = to_es_plane_state(state);
	int ret = 0;

	if (property == es_plane->degamma_mode) {
		if (es_plane_state->degamma != val) {
			es_plane_state->degamma = val;
			es_plane_state->degamma_changed = true;
		} else {
			es_plane_state->degamma_changed = false;
		}
	} else if (property == es_plane->roi_prop) {
		ret = _es_plane_set_property_blob_from_id(
			dev, &es_plane_state->roi, val,
			sizeof(struct drm_es_roi));
		return ret;
	} else if (property == es_plane->color_mgmt_prop) {
		ret = _es_plane_set_property_blob_from_id(
			dev, &es_plane_state->color_mgmt, val,
			sizeof(struct drm_es_color_mgmt));
		return ret;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int es_plane_atomic_get_property(struct drm_plane *plane,
					const struct drm_plane_state *state,
					struct drm_property *property,
					uint64_t *val)
{
	struct es_plane *es_plane = to_es_plane(plane);
	const struct es_plane_state *es_plane_state =
		container_of(state, const struct es_plane_state, base);

	if (property == es_plane->degamma_mode)
		*val = es_plane_state->degamma;
	else if (property == es_plane->roi_prop)
		*val = (es_plane_state->roi) ? es_plane_state->roi->base.id : 0;
	else if (property == es_plane->color_mgmt_prop)
		*val = (es_plane_state->color_mgmt) ?
			       es_plane_state->color_mgmt->base.id :
			       0;
	else
		return -EINVAL;

	return 0;
}

static bool es_format_mod_supported(struct drm_plane *plane, u32 format,
				    u64 modifier)
{
	int i;

	/* We always have to allow these modifiers:
     * 1. Core DRM checks for LINEAR support if userspace does not provide
 modifiers.
     * 2. Not passing any modifiers is the same as explicitly passing
 INVALID.
     */
	if (modifier == DRM_FORMAT_MOD_LINEAR)
		return true;

	/* Check that the modifier is on the list of the plane's supported
 modifiers. */
	for (i = 0; i < plane->modifier_count; i++) {
		if (modifier == plane->modifiers[i])
			break;
	}

	if (i == plane->modifier_count)
		return false;

	return true;
}

const struct drm_plane_funcs es_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = es_plane_destory,
	.reset = es_plane_reset,
	.atomic_duplicate_state = es_plane_atomic_duplicate_state,
	.atomic_destroy_state = es_plane_atomic_destroy_state,
	.atomic_set_property = es_plane_atomic_set_property,
	.atomic_get_property = es_plane_atomic_get_property,
	.format_mod_supported = es_format_mod_supported,
};

static unsigned char es_get_plane_number(struct drm_framebuffer *fb)
{
	const struct drm_format_info *info;

	if (!fb)
		return 0;

	info = drm_format_info(fb->format->format);
	if (!info || info->num_planes > MAX_NUM_PLANES)
		return 0;

	return info->num_planes;
}

static int es_plane_atomic_check(struct drm_plane *plane,
				 struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state =
		drm_atomic_get_new_plane_state(state, plane);
	struct es_plane *es_plane = to_es_plane(plane);
	struct drm_framebuffer *fb = new_plane_state->fb;
	struct drm_crtc *crtc = new_plane_state->crtc;
	struct es_crtc *es_crtc = to_es_crtc(crtc);

	if (!crtc || !fb)
		return 0;

	return es_plane->funcs->check(es_crtc->dev, es_plane, new_plane_state);
}

static void es_plane_atomic_update(struct drm_plane *plane,
				   struct drm_atomic_state *old_state)
{
	unsigned char i, num_planes;
	struct drm_framebuffer *fb;
	struct es_plane *es_plane = to_es_plane(plane);
	struct drm_plane_state *state = plane->state;
	struct es_crtc *es_crtc = to_es_crtc(state->crtc);
	struct es_plane_state *plane_state = to_es_plane_state(state);
	// struct drm_format_name_buf *name = &plane_state->status.format_name;

	if (!state->fb || !state->crtc)
		return;

	fb = state->fb;

	num_planes = es_get_plane_number(fb);

	for (i = 0; i < num_planes; i++) {
		struct es_gem_object *es_obj;

		es_obj = es_fb_get_gem_obj(fb, i);
		es_plane->dma_addr[i] = es_obj->iova + fb->offsets[i];
	}

	plane_state->status.tile_mode = 0; /* to be updated */
	plane_state->status.src = drm_plane_state_src(state);
	plane_state->status.dest = drm_plane_state_dest(state);
	// drm_get_format_name(fb->format->format, name);

	es_plane->funcs->update(es_crtc->dev, es_plane);
}

static void es_plane_atomic_disable(struct drm_plane *plane,
				    struct drm_atomic_state *state)
{
	struct drm_plane_state *old_plane_state =
		drm_atomic_get_old_plane_state(state, plane);
	struct es_plane *es_plane = to_es_plane(plane);
	struct es_crtc *es_crtc = to_es_crtc(old_plane_state->crtc);

	if (!old_plane_state->crtc)
		return;

	if (es_plane->funcs && es_plane->funcs->disable)
		es_plane->funcs->disable(es_crtc->dev, es_plane);
}

const struct drm_plane_helper_funcs es_plane_helper_funcs = {
	.atomic_check = es_plane_atomic_check,
	.atomic_update = es_plane_atomic_update,
	.atomic_disable = es_plane_atomic_disable,
};

static const struct drm_prop_enum_list es_degamma_mode_enum_list[] = {
	{ ES_DEGAMMA_DISABLE, "disabled" },
	{ ES_DEGAMMA_BT709, "preset degamma for BT709" },
	{ ES_DEGAMMA_BT2020, "preset degamma for BT2020" },
};

struct es_plane *es_plane_create(struct drm_device *drm_dev,
				 struct es_plane_info *info,
				 unsigned int possible_crtcs)
{
	struct es_plane *plane;
	int ret;

	if (!info)
		return NULL;

	plane = kzalloc(sizeof(struct es_plane), GFP_KERNEL);
	if (!plane)
		return NULL;

	plane->id = info->id;

	ret = drm_universal_plane_init(drm_dev, &plane->base, possible_crtcs,
				       &es_plane_funcs, info->formats,
				       info->num_formats, info->modifiers,
				       info->type,
				       info->name ? info->name : NULL);
	if (ret)
		goto err_free_plane;

	drm_plane_helper_add(&plane->base, &es_plane_helper_funcs);

	/* Set up the plane properties */
	if (info->degamma_size) {
		plane->degamma_mode = drm_property_create_enum(
			drm_dev, 0, "DEGAMMA_MODE", es_degamma_mode_enum_list,
			ARRAY_SIZE(es_degamma_mode_enum_list));

		if (!plane->degamma_mode)
			goto error_cleanup_plane;

		drm_object_attach_property(&plane->base.base,
					   plane->degamma_mode,
					   ES_DEGAMMA_DISABLE);
	}

	if (info->rotation) {
		ret = drm_plane_create_rotation_property(
			&plane->base, DRM_MODE_ROTATE_0, info->rotation);
		if (ret)
			goto error_cleanup_plane;
	}

	if (info->blend_mode) {
		ret = drm_plane_create_blend_mode_property(&plane->base,
							   info->blend_mode);
		if (ret)
			goto error_cleanup_plane;
		ret = drm_plane_create_alpha_property(&plane->base);
		if (ret)
			goto error_cleanup_plane;
	}

	if (info->color_encoding) {
		ret = drm_plane_create_color_properties(
			&plane->base, info->color_encoding,
			BIT(DRM_COLOR_YCBCR_LIMITED_RANGE),
			DRM_COLOR_YCBCR_BT709, DRM_COLOR_YCBCR_LIMITED_RANGE);
		if (ret)
			goto error_cleanup_plane;
	}

	if (info->roi) {
		plane->roi_prop = drm_property_create(
			drm_dev, DRM_MODE_PROP_BLOB, "ROI", 0);
		if (!plane->roi_prop)
			goto error_cleanup_plane;

		drm_object_attach_property(&plane->base.base, plane->roi_prop,
					   0);
	}

	if (info->color_mgmt) {
		plane->color_mgmt_prop = drm_property_create(
			drm_dev, DRM_MODE_PROP_BLOB, "COLOR_CONFIG", 0);
		if (!plane->color_mgmt_prop)
			return NULL;

		drm_object_attach_property(&plane->base.base,
					   plane->color_mgmt_prop, 0);
	}

	return plane;

error_cleanup_plane:
	drm_plane_cleanup(&plane->base);
err_free_plane:
	kfree(plane);
	return NULL;
}
