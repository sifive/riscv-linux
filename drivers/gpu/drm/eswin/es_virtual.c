// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Eswin Holdings Co., Ltd.
 */

#include <linux/component.h>
#include <linux/of_platform.h>
#include <linux/media-bus-format.h>
#include <linux/debugfs.h>

#include <drm/drm_probe_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_encoder.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_atomic_helper.h>

#include "es_virtual.h"
#include "es_dc.h"
#include "es_gem.h"

static unsigned char __get_bpp(struct es_virtual_display *vd)
{
	if (vd->bus_format == MEDIA_BUS_FMT_RGB101010_1X30)
		return 10;
	return 8;
}

static void vd_dump_destroy(struct es_virtual_display *vd)
{
	struct drm_device *drm_dev = vd->encoder.dev;

	if (vd->dump_blob.data) {
		vunmap(vd->dump_blob.data);
		vd->dump_blob.data = NULL;
	}
	vd->dump_blob.size = 0;

	debugfs_remove(vd->dump_debugfs);
	vd->dump_debugfs = NULL;

	if (vd->dump_obj) {
		mutex_lock(&drm_dev->struct_mutex);
		drm_gem_object_put(&vd->dump_obj->base);
		mutex_unlock(&drm_dev->struct_mutex);
		vd->dump_obj = NULL;
	}
}

static void vd_dump_create(struct es_virtual_display *vd,
			   struct drm_display_mode *mode)
{
	struct drm_device *drm_dev = vd->encoder.dev;
	struct es_dc *dc = dev_get_drvdata(vd->dc);
	struct es_gem_object *obj;
	unsigned int pitch, size;
	void *kvaddr;
	char *name;

	if (!dc->funcs)
		return;

	vd_dump_destroy(vd);

	/* dump in 4bytes XRGB format */
	pitch = mode->hdisplay * 4;
	pitch = ALIGN(pitch, dc->hw.info->pitch_alignment);
	size = PAGE_ALIGN(pitch * mode->vdisplay);

	obj = es_gem_create_object(drm_dev, size);
	if (IS_ERR(obj)) {
		return;
	}

	vd->dump_obj = obj;
	vd->pitch = pitch;

	kvaddr = vmap(obj->pages, obj->size >> PAGE_SHIFT, VM_MAP,
		      pgprot_writecombine(PAGE_KERNEL));
	if (!kvaddr)
		goto err;

	vd->dump_blob.data = kvaddr;
	vd->dump_blob.size = obj->size;

	name = kasprintf(GFP_KERNEL, "%dx%d-XRGB-%d.raw", mode->hdisplay,
			 mode->vdisplay, __get_bpp(vd));
	if (!name)
		goto err;

	vd->dump_debugfs = debugfs_create_blob(
		name, 0444, vd->connector.debugfs_entry, &vd->dump_blob);
	kfree(name);

	return;

err:
	vd_dump_destroy(vd);
}

static void vd_encoder_destroy(struct drm_encoder *encoder)
{
	struct es_virtual_display *vd;

	drm_encoder_cleanup(encoder);
	vd = to_virtual_display_with_encoder(encoder);
	vd_dump_destroy(vd);
}

static const struct drm_encoder_funcs vd_encoder_funcs = {
	.destroy = vd_encoder_destroy
};

static void vd_mode_set(struct drm_encoder *encoder,
			struct drm_display_mode *mode,
			struct drm_display_mode *adjusted_mode)
{
	struct es_virtual_display *vd;

	vd = to_virtual_display_with_encoder(encoder);
	vd_dump_create(vd, adjusted_mode);
}

static void vd_encoder_disable(struct drm_encoder *encoder)
{
	struct es_virtual_display *vd;
	struct es_dc *dc;

	vd = to_virtual_display_with_encoder(encoder);
	dc = dev_get_drvdata(vd->dc);
	if (dc->funcs && dc->funcs->dump_disable)
		dc->funcs->dump_disable(vd->dc);
}

static void vd_encoder_enable(struct drm_encoder *encoder)
{
	struct es_virtual_display *vd;
	struct es_dc *dc;

	vd = to_virtual_display_with_encoder(encoder);
	dc = dev_get_drvdata(vd->dc);
	if (dc->funcs && dc->funcs->dump_enable && vd->dump_obj)
		dc->funcs->dump_enable(vd->dc, vd->dump_obj->iova, vd->pitch);
}

int vd_encoder_atomic_check(struct drm_encoder *encoder,
			    struct drm_crtc_state *crtc_state,
			    struct drm_connector_state *conn_state)
{
	struct es_crtc_state *es_crtc_state = to_es_crtc_state(crtc_state);

	es_crtc_state->encoder_type = encoder->encoder_type;

	return 0;
}

static const struct drm_encoder_helper_funcs vd_encoder_helper_funcs = {
	.mode_set = vd_mode_set,
	.enable = vd_encoder_enable,
	.disable = vd_encoder_disable,
	.atomic_check = vd_encoder_atomic_check,
};

static const struct drm_display_mode edid_cea_modes_1[] = {
	/* 1 - 640x480@60Hz 4:3 */
	{
		DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 25175, 640, 656, 752,
			 800, 0, 480, 490, 492, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 2 - 720x480@60Hz 4:3 */
	{
		DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 27000, 720, 736, 798,
			 858, 0, 480, 489, 495, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 3 - 720x480@60Hz 16:9 */
	{
		DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 27000, 720, 736, 798,
			 858, 0, 480, 489, 495, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 4 - 1280x720@60Hz 16:9 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1390,
			 1430, 1650, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 5 - 1920x1080i@60Hz 16:9 */
	{
		DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2008,
			 2052, 2200, 0, 1080, 1084, 1094, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
				 DRM_MODE_FLAG_INTERLACE),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 6 - 720(1440)x480i@60Hz 4:3 */
	{
		DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 13500, 720, 739, 801,
			 858, 0, 480, 488, 494, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 7 - 720(1440)x480i@60Hz 16:9 */
	{
		DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 13500, 720, 739, 801,
			 858, 0, 480, 488, 494, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 8 - 720(1440)x240@60Hz 4:3 */
	{
		DRM_MODE("720x240", DRM_MODE_TYPE_DRIVER, 13500, 720, 739, 801,
			 858, 0, 240, 244, 247, 262, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 9 - 720(1440)x240@60Hz 16:9 */
	{
		DRM_MODE("720x240", DRM_MODE_TYPE_DRIVER, 13500, 720, 739, 801,
			 858, 0, 240, 244, 247, 262, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 10 - 2880x480i@60Hz 4:3 */
	{
		DRM_MODE("2880x480i", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2956,
			 3204, 3432, 0, 480, 488, 494, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 11 - 2880x480i@60Hz 16:9 */
	{
		DRM_MODE("2880x480i", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2956,
			 3204, 3432, 0, 480, 488, 494, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 12 - 2880x240@60Hz 4:3 */
	{
		DRM_MODE("2880x240", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2956,
			 3204, 3432, 0, 240, 244, 247, 262, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 13 - 2880x240@60Hz 16:9 */
	{
		DRM_MODE("2880x240", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2956,
			 3204, 3432, 0, 240, 244, 247, 262, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 14 - 1440x480@60Hz 4:3 */
	{
		DRM_MODE("1440x480", DRM_MODE_TYPE_DRIVER, 54000, 1440, 1472,
			 1596, 1716, 0, 480, 489, 495, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 15 - 1440x480@60Hz 16:9 */
	{
		DRM_MODE("1440x480", DRM_MODE_TYPE_DRIVER, 54000, 1440, 1472,
			 1596, 1716, 0, 480, 489, 495, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 16 - 1920x1080@60Hz 16:9 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
			 2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 17 - 720x576@50Hz 4:3 */
	{
		DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 27000, 720, 732, 796,
			 864, 0, 576, 581, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 18 - 720x576@50Hz 16:9 */
	{
		DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 27000, 720, 732, 796,
			 864, 0, 576, 581, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 19 - 1280x720@50Hz 16:9 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1720,
			 1760, 1980, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 20 - 1920x1080i@50Hz 16:9 */
	{
		DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2448,
			 2492, 2640, 0, 1080, 1084, 1094, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
				 DRM_MODE_FLAG_INTERLACE),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 21 - 720(1440)x576i@50Hz 4:3 */
	{
		DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 13500, 720, 732, 795,
			 864, 0, 576, 580, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 22 - 720(1440)x576i@50Hz 16:9 */
	{
		DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 13500, 720, 732, 795,
			 864, 0, 576, 580, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 23 - 720(1440)x288@50Hz 4:3 */
	{
		DRM_MODE("720x288", DRM_MODE_TYPE_DRIVER, 13500, 720, 732, 795,
			 864, 0, 288, 290, 293, 312, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 24 - 720(1440)x288@50Hz 16:9 */
	{
		DRM_MODE("720x288", DRM_MODE_TYPE_DRIVER, 13500, 720, 732, 795,
			 864, 0, 288, 290, 293, 312, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 25 - 2880x576i@50Hz 4:3 */
	{
		DRM_MODE("2880x576i", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2928,
			 3180, 3456, 0, 576, 580, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 26 - 2880x576i@50Hz 16:9 */
	{
		DRM_MODE("2880x576i", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2928,
			 3180, 3456, 0, 576, 580, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 27 - 2880x288@50Hz 4:3 */
	{
		DRM_MODE("2880x288", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2928,
			 3180, 3456, 0, 288, 290, 293, 312, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 28 - 2880x288@50Hz 16:9 */
	{
		DRM_MODE("2880x288", DRM_MODE_TYPE_DRIVER, 54000, 2880, 2928,
			 3180, 3456, 0, 288, 290, 293, 312, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 29 - 1440x576@50Hz 4:3 */
	{
		DRM_MODE("1440x576", DRM_MODE_TYPE_DRIVER, 54000, 1440, 1464,
			 1592, 1728, 0, 576, 581, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 30 - 1440x576@50Hz 16:9 */
	{
		DRM_MODE("1440x576", DRM_MODE_TYPE_DRIVER, 54000, 1440, 1464,
			 1592, 1728, 0, 576, 581, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 31 - 1920x1080@50Hz 16:9 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2448,
			 2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 32 - 1920x1080@24Hz 16:9 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2558,
			 2602, 2750, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 33 - 1920x1080@25Hz 16:9 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2448,
			 2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 34 - 1920x1080@30Hz 16:9 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2008,
			 2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 35 - 2880x480@60Hz 4:3 */
	{
		DRM_MODE("2880x480", DRM_MODE_TYPE_DRIVER, 108000, 2880, 2944,
			 3192, 3432, 0, 480, 489, 495, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 36 - 2880x480@60Hz 16:9 */
	{
		DRM_MODE("2880x480", DRM_MODE_TYPE_DRIVER, 108000, 2880, 2944,
			 3192, 3432, 0, 480, 489, 495, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 37 - 2880x576@50Hz 4:3 */
	{
		DRM_MODE("2880x576", DRM_MODE_TYPE_DRIVER, 108000, 2880, 2928,
			 3184, 3456, 0, 576, 581, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 38 - 2880x576@50Hz 16:9 */
	{
		DRM_MODE("2880x576", DRM_MODE_TYPE_DRIVER, 108000, 2880, 2928,
			 3184, 3456, 0, 576, 581, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 39 - 1920x1080i@50Hz 16:9 */
	{
		DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 72000, 1920, 1952,
			 2120, 2304, 0, 1080, 1126, 1136, 1250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 40 - 1920x1080i@100Hz 16:9 */
	{
		DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2448,
			 2492, 2640, 0, 1080, 1084, 1094, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
				 DRM_MODE_FLAG_INTERLACE),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 41 - 1280x720@100Hz 16:9 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 148500, 1280, 1720,
			 1760, 1980, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 42 - 720x576@100Hz 4:3 */
	{
		DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 54000, 720, 732, 796,
			 864, 0, 576, 581, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 43 - 720x576@100Hz 16:9 */
	{
		DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 54000, 720, 732, 796,
			 864, 0, 576, 581, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 44 - 720(1440)x576i@100Hz 4:3 */
	{
		DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 27000, 720, 732, 795,
			 864, 0, 576, 580, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 45 - 720(1440)x576i@100Hz 16:9 */
	{
		DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 27000, 720, 732, 795,
			 864, 0, 576, 580, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 46 - 1920x1080i@120Hz 16:9 */
	{
		DRM_MODE("1920x1080i", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
			 2052, 2200, 0, 1080, 1084, 1094, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC |
				 DRM_MODE_FLAG_INTERLACE),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 47 - 1280x720@120Hz 16:9 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 148500, 1280, 1390,
			 1430, 1650, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 48 - 720x480@120Hz 4:3 */
	{
		DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 54000, 720, 736, 798,
			 858, 0, 480, 489, 495, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 49 - 720x480@120Hz 16:9 */
	{
		DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 54000, 720, 736, 798,
			 858, 0, 480, 489, 495, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 50 - 720(1440)x480i@120Hz 4:3 */
	{
		DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 27000, 720, 739, 801,
			 858, 0, 480, 488, 494, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 51 - 720(1440)x480i@120Hz 16:9 */
	{
		DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 27000, 720, 739, 801,
			 858, 0, 480, 488, 494, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 52 - 720x576@200Hz 4:3 */
	{
		DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 108000, 720, 732, 796,
			 864, 0, 576, 581, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 53 - 720x576@200Hz 16:9 */
	{
		DRM_MODE("720x576", DRM_MODE_TYPE_DRIVER, 108000, 720, 732, 796,
			 864, 0, 576, 581, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 54 - 720(1440)x576i@200Hz 4:3 */
	{
		DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 54000, 720, 732, 795,
			 864, 0, 576, 580, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 55 - 720(1440)x576i@200Hz 16:9 */
	{
		DRM_MODE("720x576i", DRM_MODE_TYPE_DRIVER, 54000, 720, 732, 795,
			 864, 0, 576, 580, 586, 625, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 56 - 720x480@240Hz 4:3 */
	{
		DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 108000, 720, 736, 798,
			 858, 0, 480, 489, 495, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 57 - 720x480@240Hz 16:9 */
	{
		DRM_MODE("720x480", DRM_MODE_TYPE_DRIVER, 108000, 720, 736, 798,
			 858, 0, 480, 489, 495, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 58 - 720(1440)x480i@240Hz 4:3 */
	{
		DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 54000, 720, 739, 801,
			 858, 0, 480, 488, 494, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_4_3,
	},
	/* 59 - 720(1440)x480i@240Hz 16:9 */
	{
		DRM_MODE("720x480i", DRM_MODE_TYPE_DRIVER, 54000, 720, 739, 801,
			 858, 0, 480, 488, 494, 525, 0,
			 DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC |
				 DRM_MODE_FLAG_INTERLACE |
				 DRM_MODE_FLAG_DBLCLK),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 60 - 1280x720@24Hz 16:9 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 59400, 1280, 3040,
			 3080, 3300, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 61 - 1280x720@25Hz 16:9 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 3700,
			 3740, 3960, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 62 - 1280x720@30Hz 16:9 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 3040,
			 3080, 3300, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 63 - 1920x1080@120Hz 16:9 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 297000, 1920, 2008,
			 2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 64 - 1920x1080@100Hz 16:9 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 297000, 1920, 2448,
			 2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 65 - 1280x720@24Hz 64:27 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 59400, 1280, 3040,
			 3080, 3300, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 66 - 1280x720@25Hz 64:27 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 3700,
			 3740, 3960, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 67 - 1280x720@30Hz 64:27 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 3040,
			 3080, 3300, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 68 - 1280x720@50Hz 64:27 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1720,
			 1760, 1980, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 69 - 1280x720@60Hz 64:27 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 74250, 1280, 1390,
			 1430, 1650, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 70 - 1280x720@100Hz 64:27 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 148500, 1280, 1720,
			 1760, 1980, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 71 - 1280x720@120Hz 64:27 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 148500, 1280, 1390,
			 1430, 1650, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 72 - 1920x1080@24Hz 64:27 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2558,
			 2602, 2750, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 73 - 1920x1080@25Hz 64:27 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2448,
			 2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 74 - 1920x1080@30Hz 64:27 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 74250, 1920, 2008,
			 2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 75 - 1920x1080@50Hz 64:27 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2448,
			 2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 76 - 1920x1080@60Hz 64:27 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2008,
			 2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 77 - 1920x1080@100Hz 64:27 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 297000, 1920, 2448,
			 2492, 2640, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 78 - 1920x1080@120Hz 64:27 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 297000, 1920, 2008,
			 2052, 2200, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 79 - 1680x720@24Hz 64:27 */
	{
		DRM_MODE("1680x720", DRM_MODE_TYPE_DRIVER, 59400, 1680, 3040,
			 3080, 3300, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 80 - 1680x720@25Hz 64:27 */
	{
		DRM_MODE("1680x720", DRM_MODE_TYPE_DRIVER, 59400, 1680, 2908,
			 2948, 3168, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 81 - 1680x720@30Hz 64:27 */
	{
		DRM_MODE("1680x720", DRM_MODE_TYPE_DRIVER, 59400, 1680, 2380,
			 2420, 2640, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 82 - 1680x720@50Hz 64:27 */
	{
		DRM_MODE("1680x720", DRM_MODE_TYPE_DRIVER, 82500, 1680, 1940,
			 1980, 2200, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 83 - 1680x720@60Hz 64:27 */
	{
		DRM_MODE("1680x720", DRM_MODE_TYPE_DRIVER, 99000, 1680, 1940,
			 1980, 2200, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 84 - 1680x720@100Hz 64:27 */
	{
		DRM_MODE("1680x720", DRM_MODE_TYPE_DRIVER, 165000, 1680, 1740,
			 1780, 2000, 0, 720, 725, 730, 825, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 85 - 1680x720@120Hz 64:27 */
	{
		DRM_MODE("1680x720", DRM_MODE_TYPE_DRIVER, 198000, 1680, 1740,
			 1780, 2000, 0, 720, 725, 730, 825, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 86 - 2560x1080@24Hz 64:27 */
	{
		DRM_MODE("2560x1080", DRM_MODE_TYPE_DRIVER, 99000, 2560, 3558,
			 3602, 3750, 0, 1080, 1084, 1089, 1100, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 87 - 2560x1080@25Hz 64:27 */
	{
		DRM_MODE("2560x1080", DRM_MODE_TYPE_DRIVER, 90000, 2560, 3008,
			 3052, 3200, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 88 - 2560x1080@30Hz 64:27 */
	{
		DRM_MODE("2560x1080", DRM_MODE_TYPE_DRIVER, 118800, 2560, 3328,
			 3372, 3520, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 89 - 2560x1080@50Hz 64:27 */
	{
		DRM_MODE("2560x1080", DRM_MODE_TYPE_DRIVER, 185625, 2560, 3108,
			 3152, 3300, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 90 - 2560x1080@60Hz 64:27 */
	{
		DRM_MODE("2560x1080", DRM_MODE_TYPE_DRIVER, 198000, 2560, 2808,
			 2852, 3000, 0, 1080, 1084, 1089, 1100, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 91 - 2560x1080@100Hz 64:27 */
	{
		DRM_MODE("2560x1080", DRM_MODE_TYPE_DRIVER, 371250, 2560, 2778,
			 2822, 2970, 0, 1080, 1084, 1089, 1250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 92 - 2560x1080@120Hz 64:27 */
	{
		DRM_MODE("2560x1080", DRM_MODE_TYPE_DRIVER, 495000, 2560, 3108,
			 3152, 3300, 0, 1080, 1084, 1089, 1250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 93 - 3840x2160@24Hz 16:9 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 297000, 3840, 5116,
			 5204, 5500, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 94 - 3840x2160@25Hz 16:9 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 297000, 3840, 4896,
			 4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 95 - 3840x2160@30Hz 16:9 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 297000, 3840, 4016,
			 4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 96 - 3840x2160@50Hz 16:9 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 594000, 3840, 4896,
			 4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 97 - 3840x2160@60Hz 16:9 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 594000, 3840, 4016,
			 4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 98 - 4096x2160@24Hz 256:135 */
	{
		DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 297000, 4096, 5116,
			 5204, 5500, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135,
	},
	/* 99 - 4096x2160@25Hz 256:135 */
	{
		DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 297000, 4096, 5064,
			 5152, 5280, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135,
	},
	/* 100 - 4096x2160@30Hz 256:135 */
	{
		DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 297000, 4096, 4184,
			 4272, 4400, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135,
	},
	/* 101 - 4096x2160@50Hz 256:135 */
	{
		DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 594000, 4096, 5064,
			 5152, 5280, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135,
	},
	/* 102 - 4096x2160@60Hz 256:135 */
	{
		DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 594000, 4096, 4184,
			 4272, 4400, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135,
	},
	/* 103 - 3840x2160@24Hz 64:27 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 297000, 3840, 5116,
			 5204, 5500, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 104 - 3840x2160@25Hz 64:27 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 297000, 3840, 4896,
			 4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 105 - 3840x2160@30Hz 64:27 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 297000, 3840, 4016,
			 4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 106 - 3840x2160@50Hz 64:27 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 594000, 3840, 4896,
			 4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 107 - 3840x2160@60Hz 64:27 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 594000, 3840, 4016,
			 4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 108 - 1280x720@48Hz 16:9 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 90000, 1280, 2240,
			 2280, 2500, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 109 - 1280x720@48Hz 64:27 */
	{
		DRM_MODE("1280x720", DRM_MODE_TYPE_DRIVER, 90000, 1280, 2240,
			 2280, 2500, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 110 - 1680x720@48Hz 64:27 */
	{
		DRM_MODE("1680x720", DRM_MODE_TYPE_DRIVER, 99000, 1680, 2490,
			 2530, 2750, 0, 720, 725, 730, 750, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 111 - 1920x1080@48Hz 16:9 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2558,
			 2602, 2750, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 112 - 1920x1080@48Hz 64:27 */
	{
		DRM_MODE("1920x1080", DRM_MODE_TYPE_DRIVER, 148500, 1920, 2558,
			 2602, 2750, 0, 1080, 1084, 1089, 1125, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 113 - 2560x1080@48Hz 64:27 */
	{
		DRM_MODE("2560x1080", DRM_MODE_TYPE_DRIVER, 198000, 2560, 3558,
			 3602, 3750, 0, 1080, 1084, 1089, 1100, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 114 - 3840x2160@48Hz 16:9 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 594000, 3840, 5116,
			 5204, 5500, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 115 - 4096x2160@48Hz 256:135 */
	{
		DRM_MODE("4096x2160", DRM_MODE_TYPE_DRIVER, 594000, 4096, 5116,
			 5204, 5500, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_256_135,
	},
	/* 116 - 3840x2160@48Hz 64:27 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 594000, 3840, 5116,
			 5204, 5500, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 117 - 3840x2160@100Hz 16:9 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 1188000, 3840, 4896,
			 4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 118 - 3840x2160@120Hz 16:9 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 1188000, 3840, 4016,
			 4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_16_9,
	},
	/* 119 - 3840x2160@100Hz 64:27 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 1188000, 3840, 4896,
			 4984, 5280, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 120 - 3840x2160@120Hz 64:27 */
	{
		DRM_MODE("3840x2160", DRM_MODE_TYPE_DRIVER, 1188000, 3840, 4016,
			 4104, 4400, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 121 - 5120x2160@24Hz 64:27 */
	{
		DRM_MODE("5120x2160", DRM_MODE_TYPE_DRIVER, 396000, 5120, 7116,
			 7204, 7500, 0, 2160, 2168, 2178, 2200, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 122 - 5120x2160@25Hz 64:27 */
	{
		DRM_MODE("5120x2160", DRM_MODE_TYPE_DRIVER, 396000, 5120, 6816,
			 6904, 7200, 0, 2160, 2168, 2178, 2200, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 123 - 5120x2160@30Hz 64:27 */
	{
		DRM_MODE("5120x2160", DRM_MODE_TYPE_DRIVER, 396000, 5120, 5784,
			 5872, 6000, 0, 2160, 2168, 2178, 2200, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 124 - 5120x2160@48Hz 64:27 */
	{
		DRM_MODE("5120x2160", DRM_MODE_TYPE_DRIVER, 742500, 5120, 5866,
			 5954, 6250, 0, 2160, 2168, 2178, 2475, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 125 - 5120x2160@50Hz 64:27 */
	{
		DRM_MODE("5120x2160", DRM_MODE_TYPE_DRIVER, 742500, 5120, 6216,
			 6304, 6600, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 126 - 5120x2160@60Hz 64:27 */
	{
		DRM_MODE("5120x2160", DRM_MODE_TYPE_DRIVER, 742500, 5120, 5284,
			 5372, 5500, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
	/* 127 - 5120x2160@100Hz 64:27 */
	{
		DRM_MODE("5120x2160", DRM_MODE_TYPE_DRIVER, 1485000, 5120, 6216,
			 6304, 6600, 0, 2160, 2168, 2178, 2250, 0,
			 DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
		.picture_aspect_ratio = HDMI_PICTURE_ASPECT_64_27,
	},
};

static int vd_get_modes(struct drm_connector *connector)
{
	struct drm_device *dev = connector->dev;
	struct drm_display_mode *mode = NULL;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(edid_cea_modes_1); i++) {
		if (edid_cea_modes_1[i].clock == 594000 ||
		    edid_cea_modes_1[i].clock == 297000 ||
		    edid_cea_modes_1[i].clock == 148500 ||
		    edid_cea_modes_1[i].clock == 108000 ||
		    edid_cea_modes_1[i].clock == 74250 ||
		    edid_cea_modes_1[i].clock == 54000 ||
		    edid_cea_modes_1[i].clock == 27000) {
			mode = drm_mode_duplicate(dev, &edid_cea_modes_1[i]);
			drm_mode_probed_add(connector, mode);
		}
	}

	return 0;
}

static struct drm_encoder *vd_best_encoder(struct drm_connector *connector)
{
	struct es_virtual_display *vd;

	vd = to_virtual_display_with_connector(connector);
	return &vd->encoder;
}

static enum drm_mode_status vd_mode_valid(struct drm_connector *connector,
					  struct drm_display_mode *mode)
{
	if (mode->clock != 594000 && mode->clock != 297000 &&
	    mode->clock != 148500 && mode->clock != 108000 &&
	    mode->clock != 74250 && mode->clock != 54000 &&
	    mode->clock != 27000) {
		return MODE_NOCLOCK;
	}

	return MODE_OK;
}

static const struct drm_connector_helper_funcs vd_connector_helper_funcs = {
	.get_modes = vd_get_modes,
	.mode_valid = vd_mode_valid,
	.best_encoder = vd_best_encoder,
};

static void vd_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static enum drm_connector_status
vd_connector_detect(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static const struct drm_connector_funcs vd_connector_funcs = {
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = vd_connector_destroy,
	.detect = vd_connector_detect,
	.atomic_duplicate_state = drm_atomic_helper_connector_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.reset = drm_atomic_helper_connector_reset,
};

static int vd_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm_dev = data;
	struct es_virtual_display *vd = dev_get_drvdata(dev);
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct device_node *ep, *np;
	struct platform_device *pdev;
	int ret;

	/* Encoder */
	encoder = &vd->encoder;
	ret = drm_encoder_init(drm_dev, encoder, &vd_encoder_funcs,
			       DRM_MODE_ENCODER_VIRTUAL, NULL);
	if (ret) {
		return ret;
	}

	encoder->encoder_type = DRM_MODE_ENCODER_VIRTUAL;
	drm_encoder_helper_add(encoder, &vd_encoder_helper_funcs);

	encoder->possible_crtcs =
		drm_of_find_possible_crtcs(drm_dev, dev->of_node);

	/* Connector */
	connector = &vd->connector;
	ret = drm_connector_init(drm_dev, connector, &vd_connector_funcs,
				 DRM_MODE_CONNECTOR_VIRTUAL);
	if (ret)
		goto connector_init_err;
	drm_connector_helper_add(connector, &vd_connector_helper_funcs);
	connector->interlace_allowed = false;
	connector->doublescan_allowed = false;
	connector->dpms = DRM_MODE_DPMS_OFF;
	connector->polled =
		DRM_CONNECTOR_POLL_CONNECT | DRM_CONNECTOR_POLL_DISCONNECT;
	ret = drm_connector_register(connector);
	if (ret)
		goto connector_reg_err;

	drm_display_info_set_bus_formats(&connector->display_info,
					 &vd->bus_format, 1);

	/* attach */
	ret = drm_connector_attach_encoder(connector, encoder);
	if (ret)
		goto attach_err;

	ep = of_graph_get_endpoint_by_regs(dev->of_node, 0, -1);
	if (!ep) {
		ret = -EINVAL;
		goto attach_err;
	}

	np = of_graph_get_remote_port_parent(ep);
	of_node_put(ep);
	if (!np) {
		ret = -EINVAL;
		goto attach_err;
	}

	pdev = of_find_device_by_node(np);
	of_node_put(np);
	if (!pdev) {
		ret = -EPROBE_DEFER;
		goto attach_err;
	}
	get_device(&pdev->dev);
	vd->dc = &pdev->dev;

	return 0;

attach_err:
	drm_connector_unregister(connector);
connector_reg_err:
	drm_connector_cleanup(connector);
connector_init_err:
	drm_encoder_cleanup(encoder);
	return ret;
}

static void vd_unbind(struct device *dev, struct device *master, void *data)
{
	struct es_virtual_display *vd = dev_get_drvdata(dev);

	drm_connector_unregister(&vd->connector);
	drm_connector_cleanup(&vd->connector);
	drm_encoder_cleanup(&vd->encoder);
	if (vd->dump_obj) {
		drm_gem_object_put(&vd->dump_obj->base);
		vd->dump_obj = NULL;
	}
}

const struct component_ops vd_component_ops = {
	.bind = vd_bind,
	.unbind = vd_unbind,
};

static const struct of_device_id vd_driver_dt_match[] = {
	{
		.compatible = "eswin,virtual_display",
	},
	{},
};
MODULE_DEVICE_TABLE(of, vd_driver_dt_match);

static int vd_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct es_virtual_display *vd;
	unsigned char bpp;

	vd = devm_kzalloc(dev, sizeof(*vd), GFP_KERNEL);
	if (!vd)
		return -ENOMEM;

	vd->bus_format = MEDIA_BUS_FMT_RGB101010_1X30;
	of_property_read_u8(dev->of_node, "bpp", &bpp);
	if (bpp == 8)
		vd->bus_format = MEDIA_BUS_FMT_RBG888_1X24;

	dev_set_drvdata(dev, vd);

	return component_add(dev, &vd_component_ops);
}

static int vd_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	component_del(dev, &vd_component_ops);

	dev_set_drvdata(dev, NULL);

	return 0;
}

struct platform_driver virtual_display_platform_driver = {
    .probe = vd_probe,
    .remove = vd_remove,
    .driver = {
        .name = "es-virtual-display",
        .of_match_table = of_match_ptr(vd_driver_dt_match),
    },
};

MODULE_DESCRIPTION("Eswin Virtual Display Driver");
MODULE_LICENSE("GPL v2");
