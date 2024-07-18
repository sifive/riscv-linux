// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021, ESWIN Electronics Co., Ltd
 */

#include <linux/component.h>
#include <linux/clk.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/regmap.h>

#include <drm/drm_mode_config.h>
#include <drm/drm_edid.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/bridge/dw_hdmi.h>

#include <drm/drm_mode.h>
#include <linux/videodev2.h>
#include <linux/media-bus-format.h>
#include <linux/pm_runtime.h>

#include "dw-hdmi.h"
#include "es_drv.h"
#include "es_crtc.h"

#define HIWORD_UPDATE(val, mask) (val | (mask) << 16)
#define ESWIN_HDMI_COLORIMETRY_BT2020                                          \
	(HDMI_COLORIMETRY_EXTENDED + HDMI_EXTENDED_COLORIMETRY_BT2020)

/* HDMI output pixel format */
enum drm_hdmi_output_type {
	DRM_HDMI_OUTPUT_DEFAULT_RGB, /* default RGB */
	DRM_HDMI_OUTPUT_YCBCR444, /* YCBCR 444 */
	DRM_HDMI_OUTPUT_YCBCR422, /* YCBCR 422 */
	DRM_HDMI_OUTPUT_YCBCR420, /* YCBCR 420 */
	DRM_HDMI_OUTPUT_YCBCR_HQ, /* Highest subsampled YUV */
	DRM_HDMI_OUTPUT_YCBCR_LQ, /* Lowest subsampled YUV */
	DRM_HDMI_OUTPUT_INVALID, /* Guess what ? */
};

enum dw_hdmi_eswin_color_depth {
	ESWIN_HDMI_DEPTH_8,
	ESWIN_HDMI_DEPTH_10,
	ESWIN_HDMI_DEPTH_12,
	ESWIN_HDMI_DEPTH_16,
	ESWIN_HDMI_DEPTH_420_10,
	ESWIN_HDMI_DEPTH_420_12,
	ESWIN_HDMI_DEPTH_420_16
};

struct eswin_hdmi {
	struct device *dev;
	struct regmap *regmap;
	struct dw_hdmi *hdmi;
	struct drm_encoder encoder;
	u8 id;
	unsigned long bus_format;
	unsigned long output_bus_format;
	unsigned long enc_out_encoding;

	struct drm_property *color_depth_property;
	struct drm_property *output_format_property;
	struct drm_property *colorimetry_property;
	struct drm_property *video_enable_property;

	struct drm_property *color_depth_capacity;
	struct drm_property *output_format_capacity;
	struct drm_property *is_hdmi_capacity;
	struct drm_property *width_heigth_capacity;
	struct drm_property *quant_range_select_capacity;
	struct drm_property *max_tmds_clock_capacity;

	unsigned int colordepth;
	unsigned int colorimetry;
	unsigned int phy_bus_width;
	unsigned int hdmi_quant_range;
	enum drm_hdmi_output_type hdmi_output;
	bool video_enable;
};

#define to_eswin_hdmi(x) container_of(x, struct eswin_hdmi, x)

static const struct dw_hdmi_mpll_config eswin_mpll_cfg[] = {
	{
		27000000,
		{
			{ 0x0003, 0x0628 },
			{ 0x1003, 0x0632 },
			{ 0x2003, 0x023c },
		},
	},
	{
		54000000,
		{
			{ 0x0002, 0x0614 },
			{ 0x1002, 0x0619 },
			{ 0x2002, 0x021e },
		},
	},
	{
		74250000,
		{
			{ 0x0002, 0x0214 },
			{ 0x1009, 0x0619 },
			{ 0x2001, 0x060f },
		},
	},
	{
		108000000,
		{
			{ 0x0001, 0x060a },
			{ 0x1009, 0x0619 },
			{ 0x2001, 0x020f },
		},
	},
	{
		148500000,
		{
			{ 0x0001, 0x020a },
			{ 0x1018, 0x0619 },
			{ 0x2008, 0x060f },
		},
	},
	{
		297000000,
		{
			{ 0x0000, 0x0205 },
			{ 0x1658, 0x0219 },
			{ 0x2648, 0x020f },
		},
	},
	{
		594000000,
		{
			{ 0x0640, 0x0005 },
			{ 0x1658, 0x0019 },
			{ 0x2648, 0x000f },
		},
	},
	{
		~0UL,
		{
			{ 0x0000, 0x0000 },
			{ 0x0000, 0x0000 },
			{ 0x0000, 0x0000 },
		},
	}
};

static const struct dw_hdmi_curr_ctrl eswin_cur_ctr[] = {
	/*      pixelclk    bpp8    bpp10   bpp12 */
	{
		27000000,
		{ 0x0283, 0x0281, 0x02c2 },
	},
	{
		54000000,
		{ 0x1183, 0x1203, 0x1202 },
	},
	{
		74250000,
		{ 0x1142, 0x2203, 0x2141 },
	},
	{
		108000000,
		{ 0x20c0, 0x2203, 0x2100 },
	},
	{
		148500000,
		{ 0x2080, 0x3203, 0x3141 },
	},
	{
		297000000,
		{ 0x3041, 0x3182, 0x3100 },
	},
	{
		594000000,
		{ 0x3080, 0x31c0, 0x3100 },
	},
	{
		~0UL,
		{ 0x0000, 0x0000, 0x0000 },
	}
};

static struct dw_hdmi_phy_config eswin_phy_config[] = {
	/*pixelclk   symbol   term   vlev*/
	{ 165000000, 0x8088, 0x0007, 0x0180 },
	{ 297000000, 0x80c8, 0x0004, 0x0180 },
	{ 594000000, 0x80f8, 0x0000, 0x0180 },
	{ ~0UL, 0x0000, 0x0000, 0x0000 }
};

static enum drm_mode_status
dw_hdmi_eswin_mode_valid(struct dw_hdmi *hdmi, void *data,
			 const struct drm_display_info *info,
			 const struct drm_display_mode *mode)
{
	const struct dw_hdmi_mpll_config *mpll_cfg = eswin_mpll_cfg;
	int pclk = mode->clock * 1000;
	bool valid = false;
	int i;

	for (i = 0; mpll_cfg[i].mpixelclock != (~0UL); i++) {
		if (pclk == mpll_cfg[i].mpixelclock) {
			valid = true;
			break;
		}
	}

	return (valid) ? MODE_OK : MODE_BAD;
}

static bool
dw_hdmi_eswin_encoder_mode_fixup(struct drm_encoder *encoder,
				 const struct drm_display_mode *mode,
				 struct drm_display_mode *adj_mode)
{
	return true;
}

static void dw_hdmi_eswin_select_output(struct drm_connector_state *conn_state,
					struct drm_crtc_state *crtc_state,
					struct eswin_hdmi *hdmi,
					unsigned int *color_format,
					unsigned int *color_depth,
					unsigned long *enc_out_encoding,
					unsigned int *eotf)
{
	struct drm_display_info *info = &conn_state->connector->display_info;
	struct drm_display_mode *mode = &crtc_state->mode;
	struct hdr_output_metadata *hdr_metadata;
	u32 vic = drm_match_cea_mode(mode);
	unsigned long tmdsclock, pixclock = mode->crtc_clock;
	bool support_dc = false;
	int max_tmds_clock = info->max_tmds_clock;
	int output_eotf;

	*color_format = DRM_HDMI_OUTPUT_DEFAULT_RGB;

	switch (hdmi->hdmi_output) {
	case DRM_HDMI_OUTPUT_YCBCR_HQ:
		if (info->color_formats & DRM_COLOR_FORMAT_YCBCR444)
			*color_format = DRM_HDMI_OUTPUT_YCBCR444;
		else if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422)
			*color_format = DRM_HDMI_OUTPUT_YCBCR422;
		else if (conn_state->connector->ycbcr_420_allowed &&
			 drm_mode_is_420(info, mode))
			*color_format = DRM_HDMI_OUTPUT_YCBCR420;
		break;
	case DRM_HDMI_OUTPUT_YCBCR_LQ:
		if (conn_state->connector->ycbcr_420_allowed &&
		    drm_mode_is_420(info, mode))
			*color_format = DRM_HDMI_OUTPUT_YCBCR420;
		else if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422)
			*color_format = DRM_HDMI_OUTPUT_YCBCR422;
		else if (info->color_formats & DRM_COLOR_FORMAT_YCBCR444)
			*color_format = DRM_HDMI_OUTPUT_YCBCR444;
		break;
	case DRM_HDMI_OUTPUT_YCBCR420:
		if (conn_state->connector->ycbcr_420_allowed &&
		    drm_mode_is_420(info, mode))
			*color_format = DRM_HDMI_OUTPUT_YCBCR420;
		break;
	case DRM_HDMI_OUTPUT_YCBCR422:
		if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422)
			*color_format = DRM_HDMI_OUTPUT_YCBCR422;
		break;
	case DRM_HDMI_OUTPUT_YCBCR444:
		if (info->color_formats & DRM_COLOR_FORMAT_YCBCR444)
			*color_format = DRM_HDMI_OUTPUT_YCBCR444;
		break;
	case DRM_HDMI_OUTPUT_DEFAULT_RGB:
	default:
		break;
	}

	if (*color_format == DRM_HDMI_OUTPUT_DEFAULT_RGB &&
	    info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_30)
		support_dc = true;
	if (*color_format == DRM_HDMI_OUTPUT_YCBCR444 &&
	    info->edid_hdmi_rgb444_dc_modes &
		    (DRM_EDID_HDMI_DC_Y444 | DRM_EDID_HDMI_DC_30))
		support_dc = true;
	if (*color_format == DRM_HDMI_OUTPUT_YCBCR422)
		support_dc = true;
	if (*color_format == DRM_HDMI_OUTPUT_YCBCR420 &&
	    info->hdmi.y420_dc_modes & DRM_EDID_YCBCR420_DC_30)
		support_dc = true;

	if (hdmi->colordepth > 8 && support_dc)
		*color_depth = 10;
	else
		*color_depth = 8;

	*eotf = TRADITIONAL_GAMMA_SDR;
	if (conn_state->hdr_output_metadata) {
		hdr_metadata = (struct hdr_output_metadata *)
				       conn_state->hdr_output_metadata->data;
		output_eotf = hdr_metadata->hdmi_metadata_type1.eotf;
		if (output_eotf > TRADITIONAL_GAMMA_HDR &&
		    output_eotf < FUTURE_EOTF)
			*eotf = output_eotf;
	}

	if ((*eotf > TRADITIONAL_GAMMA_HDR &&
	     conn_state->connector->hdr_sink_metadata.hdmi_type1.eotf &
		     BIT(*eotf)) ||
	    (hdmi->colorimetry == ESWIN_HDMI_COLORIMETRY_BT2020))
		*enc_out_encoding = V4L2_YCBCR_ENC_BT2020;
	else if ((vic == 6) || (vic == 7) || (vic == 21) || (vic == 22) ||
		 (vic == 2) || (vic == 3) || (vic == 17) || (vic == 18))
		*enc_out_encoding = V4L2_YCBCR_ENC_601;
	else
		*enc_out_encoding = V4L2_YCBCR_ENC_709;

	if (*enc_out_encoding == V4L2_YCBCR_ENC_BT2020) {
		/* BT2020 require color depth at lest 10bit */
		*color_depth = 10;
		/* We prefer use YCbCr422 to send 10bit */
		if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422)
			*color_format = DRM_HDMI_OUTPUT_YCBCR422;
	}

	if (mode->flags & DRM_MODE_FLAG_DBLCLK)
		pixclock *= 2;
	if ((mode->flags & DRM_MODE_FLAG_3D_MASK) ==
	    DRM_MODE_FLAG_3D_FRAME_PACKING)
		pixclock *= 2;

	if (*color_format == DRM_HDMI_OUTPUT_YCBCR422 || *color_depth == 8)
		tmdsclock = pixclock;
	else
		tmdsclock = pixclock * (*color_depth) / 8;

	if (*color_format == DRM_HDMI_OUTPUT_YCBCR420)
		tmdsclock /= 2;

	/* XXX: max_tmds_clock of some sink is 0, we think it is 340MHz. */
	if (!max_tmds_clock)
		max_tmds_clock = 340000;

	max_tmds_clock = min(max_tmds_clock, 594000);

	if (tmdsclock > max_tmds_clock) {
		if (max_tmds_clock >= 594000) {
			*color_depth = 8;
		} else if (max_tmds_clock > 340000) {
			if (drm_mode_is_420(info, mode) || tmdsclock >= 594000)
				*color_format = DRM_HDMI_OUTPUT_YCBCR420;
		} else {
			*color_depth = 8;
			if (drm_mode_is_420(info, mode) || tmdsclock >= 594000)
				*color_format = DRM_HDMI_OUTPUT_YCBCR420;
		}
	}
}

static int
dw_hdmi_eswin_encoder_atomic_check(struct drm_encoder *encoder,
				   struct drm_crtc_state *crtc_state,
				   struct drm_connector_state *conn_state)
{
	struct es_crtc_state *s = to_es_crtc_state(crtc_state);
	struct eswin_hdmi *hdmi = to_eswin_hdmi(encoder);
	unsigned int colordepth, colorformat, bus_width, eotf;

	dw_hdmi_eswin_select_output(conn_state, crtc_state, hdmi, &colorformat,
				    &colordepth, &hdmi->enc_out_encoding,
				    &eotf);
	if (colordepth > 8)
		hdmi->bus_format = MEDIA_BUS_FMT_RGB101010_1X30;
	else
		hdmi->bus_format = MEDIA_BUS_FMT_RGB888_1X24;

	/* DC does not support YUV output */
	s->output_fmt = hdmi->bus_format;

	if (colorformat == DRM_HDMI_OUTPUT_YCBCR420) {
		if (colordepth > 8)
			hdmi->output_bus_format = MEDIA_BUS_FMT_UYYVYY10_0_5X30;
		else
			hdmi->output_bus_format = MEDIA_BUS_FMT_UYYVYY8_0_5X24;

		bus_width = colordepth / 2;
	} else {
		if ((colordepth > 8) &&
		    (colorformat != DRM_HDMI_OUTPUT_YCBCR422)) {
			if (colorformat != DRM_HDMI_OUTPUT_DEFAULT_RGB)
				hdmi->output_bus_format =
					MEDIA_BUS_FMT_YUV10_1X30;
			else
				hdmi->output_bus_format =
					MEDIA_BUS_FMT_RGB101010_1X30;
		} else {
			if (colorformat != DRM_HDMI_OUTPUT_DEFAULT_RGB)
				hdmi->output_bus_format =
					MEDIA_BUS_FMT_YUV8_1X24;
			else
				hdmi->output_bus_format =
					MEDIA_BUS_FMT_RGB888_1X24;
		}
		if (colorformat == DRM_HDMI_OUTPUT_YCBCR422) {
			bus_width = 8;
			if (colordepth > 8)
				hdmi->output_bus_format =
					MEDIA_BUS_FMT_UYVY10_1X20;
			else
				hdmi->output_bus_format =
					MEDIA_BUS_FMT_UYVY8_1X16;
		} else {
			bus_width = colordepth;
		}
	}

	hdmi->phy_bus_width = bus_width;

	s->encoder_type = DRM_MODE_ENCODER_TMDS;

	return 0;
}

static unsigned long dw_hdmi_eswin_get_input_bus_format(void *data)
{
	struct eswin_hdmi *hdmi = (struct eswin_hdmi *)data;

	return hdmi->bus_format;
}

static unsigned long dw_hdmi_eswin_get_output_bus_format(void *data)
{
	struct eswin_hdmi *hdmi = (struct eswin_hdmi *)data;

	return hdmi->output_bus_format;
}

static unsigned long dw_hdmi_eswin_get_enc_in_encoding(void *data)
{
	struct eswin_hdmi *hdmi = (struct eswin_hdmi *)data;

	return hdmi->enc_out_encoding;
}

static unsigned long dw_hdmi_eswin_get_enc_out_encoding(void *data)
{
	struct eswin_hdmi *hdmi = (struct eswin_hdmi *)data;

	return hdmi->enc_out_encoding;
}

static const struct drm_prop_enum_list color_depth_enum_list[] = {
	{ 0, "Automatic" }, /* Same as 24bit */
	{ 8, "24bit" },
	{ 10, "30bit" },
};

static const struct drm_prop_enum_list drm_hdmi_output_enum_list[] = {
	{ DRM_HDMI_OUTPUT_DEFAULT_RGB, "output_rgb" },
	{ DRM_HDMI_OUTPUT_YCBCR444, "output_ycbcr444" },
	{ DRM_HDMI_OUTPUT_YCBCR422, "output_ycbcr422" },
	{ DRM_HDMI_OUTPUT_YCBCR420, "output_ycbcr420" },
	{ DRM_HDMI_OUTPUT_YCBCR_HQ, "output_ycbcr_high_subsampling" },
	{ DRM_HDMI_OUTPUT_YCBCR_LQ, "output_ycbcr_low_subsampling" },
	{ DRM_HDMI_OUTPUT_INVALID, "invalid_output" },
};

static const struct drm_prop_enum_list colorimetry_enum_list[] = {
	{ HDMI_COLORIMETRY_NONE, "None" },
	{ ESWIN_HDMI_COLORIMETRY_BT2020, "ITU_2020" },
};

static const struct drm_prop_enum_list quant_range_enum_list[] = {
	{ HDMI_QUANTIZATION_RANGE_DEFAULT, "default" },
	{ HDMI_QUANTIZATION_RANGE_LIMITED, "limit" },
	{ HDMI_QUANTIZATION_RANGE_FULL, "full" },
};

static const struct drm_prop_enum_list color_depth_capacity_list[] = {
	{ BIT(ESWIN_HDMI_DEPTH_8), "8bit" },
	{ BIT(ESWIN_HDMI_DEPTH_10), "10bit" },
	{ BIT(ESWIN_HDMI_DEPTH_12), "12bit" },
	{ BIT(ESWIN_HDMI_DEPTH_16), "16bit" },
	{ BIT(ESWIN_HDMI_DEPTH_420_10), "yuv420_10bit" },
	{ BIT(ESWIN_HDMI_DEPTH_420_12), "yuv420_12bit" },
	{ BIT(ESWIN_HDMI_DEPTH_420_16), "yuv420_16bit" },
};

static const struct drm_prop_enum_list output_format_capacity_list[] = {
	{ BIT(DRM_HDMI_OUTPUT_DEFAULT_RGB), "rgb" },
	{ BIT(DRM_HDMI_OUTPUT_YCBCR444), "yuv444" },
	{ BIT(DRM_HDMI_OUTPUT_YCBCR422), "yuv422" },
	{ BIT(DRM_HDMI_OUTPUT_YCBCR420), "yuv420" },
	{ BIT(DRM_HDMI_OUTPUT_YCBCR_HQ), "yuv_hq" },
	{ BIT(DRM_HDMI_OUTPUT_YCBCR_LQ), "yuv_lq" },
};

static void dw_hdmi_eswin_attatch_properties(struct drm_connector *connector,
					     unsigned int color, int version,
					     void *data)
{
	struct eswin_hdmi *hdmi = (struct eswin_hdmi *)data;
	struct drm_property *prop;
#ifdef CONFIG_ESWIN_DW_HDMI
	struct es_drm_private *private = connector->dev->dev_private;
#endif
	switch (color) {
	case MEDIA_BUS_FMT_RGB101010_1X30:
		hdmi->hdmi_output = DRM_HDMI_OUTPUT_DEFAULT_RGB;
		hdmi->colordepth = 10;
		break;
	case MEDIA_BUS_FMT_YUV8_1X24:
		hdmi->hdmi_output = DRM_HDMI_OUTPUT_YCBCR444;
		hdmi->colordepth = 8;
		break;
	case MEDIA_BUS_FMT_YUV10_1X30:
		hdmi->hdmi_output = DRM_HDMI_OUTPUT_YCBCR444;
		hdmi->colordepth = 10;
		break;
	case MEDIA_BUS_FMT_UYVY10_1X20:
		hdmi->hdmi_output = DRM_HDMI_OUTPUT_YCBCR422;
		hdmi->colordepth = 10;
		break;
	case MEDIA_BUS_FMT_UYVY8_1X16:
		hdmi->hdmi_output = DRM_HDMI_OUTPUT_YCBCR422;
		hdmi->colordepth = 8;
		break;
	case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
		hdmi->hdmi_output = DRM_HDMI_OUTPUT_YCBCR420;
		hdmi->colordepth = 8;
		break;
	case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
		hdmi->hdmi_output = DRM_HDMI_OUTPUT_YCBCR420;
		hdmi->colordepth = 10;
		break;
	default:
		hdmi->hdmi_output = DRM_HDMI_OUTPUT_DEFAULT_RGB;
		hdmi->colordepth = 8;
	}

	if (!hdmi->color_depth_property) {
		prop = drm_property_create_enum(
			connector->dev, 0, "hdmi_output_color_depth",
			color_depth_enum_list,
			ARRAY_SIZE(color_depth_enum_list));
		if (prop) {
			hdmi->color_depth_property = prop;
			drm_object_attach_property(&connector->base, prop, 0);
		}
	}

	prop = drm_property_create_enum(connector->dev, 0, "hdmi_output_format",
					drm_hdmi_output_enum_list,
					ARRAY_SIZE(drm_hdmi_output_enum_list));
	if (prop) {
		hdmi->output_format_property = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_enum(connector->dev, 0,
					"hdmi_output_colorimetry",
					colorimetry_enum_list,
					ARRAY_SIZE(colorimetry_enum_list));
	if (prop) {
		hdmi->colorimetry_property = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_bool(connector->dev, 0, "video_enable");
	if (prop) {
		hdmi->video_enable_property = prop;
		drm_object_attach_property(&connector->base, prop, 1);
		hdmi->video_enable = true;
	}

	prop = drm_property_create_enum(connector->dev, 0,
					"hdmi_color_depth_capacity",
					color_depth_capacity_list,
					ARRAY_SIZE(color_depth_capacity_list));
	if (prop) {
		hdmi->color_depth_capacity = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_enum(
		connector->dev, 0, "hdmi_output_format_capacity",
		output_format_capacity_list,
		ARRAY_SIZE(output_format_capacity_list));
	if (prop) {
		hdmi->output_format_capacity = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_bool(connector->dev, 0, "is_hdmi_capacity");
	if (prop) {
		hdmi->is_hdmi_capacity = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_range(
		connector->dev, 0, "hdmi_width_height_mm_capacity", 0, 0xff);
	if (prop) {
		hdmi->width_heigth_capacity = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_bool(connector->dev, 0,
					"hdmi_quant_range_sel_capacity");
	if (prop) {
		hdmi->quant_range_select_capacity = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = drm_property_create_range(
		connector->dev, 0, "hdmi_max_tmds_clock_capacity", 0, 340000);
	if (prop) {
		hdmi->max_tmds_clock_capacity = prop;
		drm_object_attach_property(&connector->base, prop, 0);
	}

	prop = connector->dev->mode_config.hdr_output_metadata_property;
	if (version >= 0x211a)
		drm_object_attach_property(&connector->base, prop, 0);

#ifdef CONFIG_ESWIN_DW_HDMI
	drm_object_attach_property(&connector->base, private->connector_id_prop,
				   0);
#endif
}

static void dw_hdmi_eswin_destroy_properties(struct drm_connector *connector,
					     void *data)
{
	struct eswin_hdmi *hdmi = (struct eswin_hdmi *)data;

	if (hdmi->color_depth_property) {
		drm_property_destroy(connector->dev,
				     hdmi->color_depth_property);
		hdmi->color_depth_property = NULL;
	}

	if (hdmi->output_format_property) {
		drm_property_destroy(connector->dev,
				     hdmi->output_format_property);
		hdmi->output_format_property = NULL;
	}

	if (hdmi->colorimetry_property) {
		drm_property_destroy(connector->dev,
				     hdmi->colorimetry_property);
		hdmi->colorimetry_property = NULL;
	}

	if (hdmi->video_enable_property) {
		drm_property_destroy(connector->dev,
				     hdmi->video_enable_property);
		hdmi->video_enable_property = NULL;
	}

	if (hdmi->color_depth_capacity) {
		drm_property_destroy(connector->dev,
				     hdmi->color_depth_capacity);
		hdmi->color_depth_capacity = NULL;
	}

	if (hdmi->output_format_capacity) {
		drm_property_destroy(connector->dev,
				     hdmi->output_format_capacity);
		hdmi->output_format_capacity = NULL;
	}

	if (hdmi->is_hdmi_capacity) {
		drm_property_destroy(connector->dev, hdmi->is_hdmi_capacity);
		hdmi->is_hdmi_capacity = NULL;
	}

	if (hdmi->width_heigth_capacity) {
		drm_property_destroy(connector->dev,
				     hdmi->width_heigth_capacity);
		hdmi->width_heigth_capacity = NULL;
	}

	if (hdmi->quant_range_select_capacity) {
		drm_property_destroy(connector->dev,
				     hdmi->quant_range_select_capacity);
		hdmi->quant_range_select_capacity = NULL;
	}

	if (hdmi->max_tmds_clock_capacity) {
		drm_property_destroy(connector->dev,
				     hdmi->max_tmds_clock_capacity);
		hdmi->max_tmds_clock_capacity = NULL;
	}
}

static int dw_hdmi_eswin_set_property(struct drm_connector *connector,
				      struct drm_connector_state *state,
				      struct drm_property *property,
				      uint64_t val, void *data)
{
	struct eswin_hdmi *hdmi = (struct eswin_hdmi *)data;

	if (property == hdmi->color_depth_property) {
		hdmi->colordepth = val;
	} else if (property == hdmi->output_format_property) {
		hdmi->hdmi_output = val;
	} else if (property == hdmi->colorimetry_property) {
		hdmi->colorimetry = val;
	} else if (property == hdmi->video_enable_property) {
		if (hdmi->video_enable != val) {
			if (val == true) {
				dw_hdmi_enable_video(hdmi->hdmi);
			} else {
				dw_hdmi_disable_video(hdmi->hdmi);
			}
			hdmi->video_enable = val;
		}
	} else {
		DRM_DEBUG("don't support set %s property\n", property->name);
		return 0;
	}
	return 0;
}

static int dw_hdmi_eswin_get_property(struct drm_connector *connector,
				      const struct drm_connector_state *state,
				      struct drm_property *property,
				      uint64_t *val, void *data)
{
	struct eswin_hdmi *hdmi = (struct eswin_hdmi *)data;
	struct drm_display_info *info = &connector->display_info;
	struct drm_mode_config *config = &connector->dev->mode_config;
#ifdef CONFIG_ESWIN_DW_HDMI
	struct es_drm_private *private = connector->dev->dev_private;
#endif
	if (property == hdmi->color_depth_property) {
		*val = hdmi->colordepth;
	} else if (property == hdmi->output_format_property) {
		*val = hdmi->hdmi_output;
	} else if (property == hdmi->color_depth_capacity) {
		*val = BIT(ESWIN_HDMI_DEPTH_8);
		if (info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_30)
			*val |= BIT(ESWIN_HDMI_DEPTH_10);
		if (info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_36)
			*val |= BIT(ESWIN_HDMI_DEPTH_12);
		if (info->edid_hdmi_rgb444_dc_modes & DRM_EDID_HDMI_DC_48)
			*val |= BIT(ESWIN_HDMI_DEPTH_16);
		if (info->hdmi.y420_dc_modes & DRM_EDID_YCBCR420_DC_30)
			*val |= BIT(ESWIN_HDMI_DEPTH_420_10);
		if (info->hdmi.y420_dc_modes & DRM_EDID_YCBCR420_DC_36)
			*val |= BIT(ESWIN_HDMI_DEPTH_420_12);
		if (info->hdmi.y420_dc_modes & DRM_EDID_YCBCR420_DC_48)
			*val |= BIT(ESWIN_HDMI_DEPTH_420_16);
	} else if (property == hdmi->output_format_capacity) {
		*val = BIT(DRM_HDMI_OUTPUT_DEFAULT_RGB);
		if (info->color_formats & DRM_COLOR_FORMAT_YCBCR444)
			*val |= BIT(DRM_HDMI_OUTPUT_YCBCR444);
		if (info->color_formats & DRM_COLOR_FORMAT_YCBCR422)
			*val |= BIT(DRM_HDMI_OUTPUT_YCBCR422);
		if (connector->ycbcr_420_allowed &&
		    info->color_formats & DRM_COLOR_FORMAT_YCBCR420)
			*val |= BIT(DRM_HDMI_OUTPUT_YCBCR420);
	} else if (property == config->hdr_output_metadata_property) {
		*val = state->hdr_output_metadata ?
			       state->hdr_output_metadata->base.id :
			       0;
	} else if (property == hdmi->colorimetry_property) {
		*val = hdmi->colorimetry;
	}
#ifdef CONFIG_ESWIN_DW_HDMI
	else if (property == private->connector_id_prop) {
		*val = hdmi->id;
	}
#endif
	else if (property == hdmi->is_hdmi_capacity) {
		*val = info->is_hdmi;
	} else if (property == hdmi->quant_range_select_capacity) {
		*val = info->rgb_quant_range_selectable;
	} else if (property == hdmi->width_heigth_capacity) {
		property->values[0] = info->width_mm;
		property->values[1] = info->height_mm;
		*val = 0;
	} else if (property == hdmi->max_tmds_clock_capacity) {
		*val = info->max_tmds_clock;
	} else if (property == hdmi->video_enable_property) {
		*val = hdmi->video_enable;
	} else {
		DRM_ERROR("failed to get eswin hdmi connector %s property\n",
			  property->name);
		return -EINVAL;
	}
	return 0;
}

static const struct dw_hdmi_property_ops dw_hdmi_eswin_property_ops = {
	.attatch_properties = dw_hdmi_eswin_attatch_properties,
	.destroy_properties = dw_hdmi_eswin_destroy_properties,
	.set_property = dw_hdmi_eswin_set_property,
	.get_property = dw_hdmi_eswin_get_property,
};

static const struct drm_encoder_helper_funcs
	dw_hdmi_eswin_encoder_helper_funcs = {
		.mode_fixup = dw_hdmi_eswin_encoder_mode_fixup,
		.atomic_check = dw_hdmi_eswin_encoder_atomic_check,
	};

static const struct dw_hdmi_plat_data win2030_hdmi_drv_data = {
	.mode_valid = dw_hdmi_eswin_mode_valid,
	.mpll_cfg = eswin_mpll_cfg,
	.cur_ctr = eswin_cur_ctr,
	.phy_config = eswin_phy_config,
	.use_drm_infoframe = true,
	.ycbcr_420_allowed = false,
};

static const struct of_device_id dw_hdmi_eswin_dt_ids[] = {
	{ .compatible = "eswin,eswin-dw-hdmi", .data = &win2030_hdmi_drv_data },
	{},
};
MODULE_DEVICE_TABLE(of, dw_hdmi_eswin_dt_ids);

static int dw_hdmi_eswin_bind(struct device *dev, struct device *master,
			      void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct dw_hdmi_plat_data *plat_data;
	const struct of_device_id *match;
	struct drm_device *drm = data;
	struct drm_encoder *encoder;
	struct eswin_hdmi *hdmi;
	int ret = 0;

	if (!pdev->dev.of_node)
		return -ENODEV;

	hdmi = devm_kzalloc(&pdev->dev, sizeof(*hdmi), GFP_KERNEL);
	if (!hdmi)
		return -ENOMEM;

	match = of_match_node(dw_hdmi_eswin_dt_ids, pdev->dev.of_node);
	plat_data = devm_kmemdup(&pdev->dev, match->data, sizeof(*plat_data),
				 GFP_KERNEL);
	if (!plat_data)
		return -ENOMEM;

	hdmi->dev = &pdev->dev;

	plat_data->phy_data = hdmi;
	encoder = &hdmi->encoder;

	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm, dev->of_node);
	/*
     * If we failed to find the CRTC(s) which this encoder is
     * supposed to be connected to, it's because the CRTC has
     * not been registered yet.  Defer probing, and hope that
     * the required CRTC is added later.
     */
	if (encoder->possible_crtcs == 0)
		return -EPROBE_DEFER;

	plat_data->phy_data = hdmi;
	plat_data->get_input_bus_format = dw_hdmi_eswin_get_input_bus_format;
	plat_data->get_output_bus_format = dw_hdmi_eswin_get_output_bus_format;
	plat_data->get_enc_in_encoding = dw_hdmi_eswin_get_enc_in_encoding;
	plat_data->get_enc_out_encoding = dw_hdmi_eswin_get_enc_out_encoding;
	plat_data->property_ops = &dw_hdmi_eswin_property_ops;

	drm_encoder_helper_add(encoder, &dw_hdmi_eswin_encoder_helper_funcs);
	drm_simple_encoder_init(drm, encoder, DRM_MODE_ENCODER_TMDS);

	platform_set_drvdata(pdev, hdmi);

	hdmi->hdmi = dw_hdmi_bind(pdev, encoder, plat_data);

	/*
     * If dw_hdmi_bind() fails we'll never call dw_hdmi_unbind(),
     * which would have called the encoder cleanup.  Do it manually.
     */
	if (IS_ERR(hdmi->hdmi)) {
		ret = PTR_ERR(hdmi->hdmi);
		drm_encoder_cleanup(encoder);
	}

	return ret;
}

static void dw_hdmi_eswin_unbind(struct device *dev, struct device *master,
				 void *data)
{
	struct eswin_hdmi *hdmi = dev_get_drvdata(dev);

	dw_hdmi_unbind(hdmi->hdmi);
}

static const struct component_ops dw_hdmi_eswin_ops = {
	.bind = dw_hdmi_eswin_bind,
	.unbind = dw_hdmi_eswin_unbind,
};

static int dw_hdmi_eswin_probe(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &dw_hdmi_eswin_ops);
}

static void dw_hdmi_eswin_shutdown(struct platform_device *pdev)
{
	struct eswin_hdmi *hdmi = dev_get_drvdata(&pdev->dev);

	dw_hdmi_suspend(hdmi->hdmi);
}

static int dw_hdmi_eswin_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &dw_hdmi_eswin_ops);
	return 0;
}

static int __maybe_unused dw_hdmi_eswin_suspend(struct device *dev)
{
	struct eswin_hdmi *hdmi = dev_get_drvdata(dev);

	dw_hdmi_suspend(hdmi->hdmi);

	return 0;
}

static int __maybe_unused dw_hdmi_eswin_resume(struct device *dev)
{
	struct eswin_hdmi *hdmi = dev_get_drvdata(dev);

	dw_hdmi_resume(hdmi->hdmi);

	return 0;
}

static const struct dev_pm_ops dw_hdmi_eswin_pm = { SET_SYSTEM_SLEEP_PM_OPS(
	dw_hdmi_eswin_suspend, dw_hdmi_eswin_resume) };

struct platform_driver dw_hdmi_eswin_pltfm_driver = {
    .probe  = dw_hdmi_eswin_probe,
    .remove = dw_hdmi_eswin_remove,
    .shutdown = dw_hdmi_eswin_shutdown,
    .driver = {
        .name = "dw-hdmi-eswin",
        .pm = &dw_hdmi_eswin_pm,
        .of_match_table = dw_hdmi_eswin_dt_ids,
    },
};

//module_platform_driver(dw_hdmi_eswin_pltfm_driver);
MODULE_LICENSE("GPL");
