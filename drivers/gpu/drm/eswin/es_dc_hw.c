// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Eswin Holdings Co., Ltd.
 */

#include <linux/io.h>
#include <linux/bits.h>
#include <linux/media-bus-format.h>
#include <linux/delay.h>

#include <drm/drm_fourcc.h>
#include <drm/drm_blend.h>
#include <drm/drm_connector.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_atomic_helper.h>

#include "es_drm.h"
#include "es_type.h"
#include "es_dc_hw.h"

static const u32 horKernel[] = {
	0x00000000, 0x20000000, 0x00002000, 0x00000000, 0x00000000, 0x00000000,
	0x23fd1c03, 0x00000000, 0x00000000, 0x00000000, 0x181f0000, 0x000027e1,
	0x00000000, 0x00000000, 0x00000000, 0x2b981468, 0x00000000, 0x00000000,
	0x00000000, 0x10f00000, 0x00002f10, 0x00000000, 0x00000000, 0x00000000,
	0x32390dc7, 0x00000000, 0x00000000, 0x00000000, 0x0af50000, 0x0000350b,
	0x00000000, 0x00000000, 0x00000000, 0x3781087f, 0x00000000, 0x00000000,
	0x00000000, 0x06660000, 0x0000399a, 0x00000000, 0x00000000, 0x00000000,
	0x3b5904a7, 0x00000000, 0x00000000, 0x00000000, 0x033c0000, 0x00003cc4,
	0x00000000, 0x00000000, 0x00000000, 0x3de1021f, 0x00000000, 0x00000000,
	0x00000000, 0x01470000, 0x00003eb9, 0x00000000, 0x00000000, 0x00000000,
	0x3f5300ad, 0x00000000, 0x00000000, 0x00000000, 0x00480000, 0x00003fb8,
	0x00000000, 0x00000000, 0x00000000, 0x3fef0011, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00004000, 0x00000000, 0x00000000, 0x00000000,
	0x20002000, 0x00000000, 0x00000000, 0x00000000, 0x1c030000, 0x000023fd,
	0x00000000, 0x00000000, 0x00000000, 0x27e1181f, 0x00000000, 0x00000000,
	0x00000000, 0x14680000, 0x00002b98, 0x00000000, 0x00000000, 0x00000000,
	0x2f1010f0, 0x00000000, 0x00000000, 0x00000000, 0x0dc70000, 0x00003239,
	0x00000000, 0x00000000, 0x00000000, 0x350b0af5, 0x00000000, 0x00000000,
	0x00000000, 0x087f0000, 0x00003781, 0x00000000, 0x00000000, 0x00000000,
	0x399a0666, 0x00000000, 0x00000000, 0x00000000, 0x04a70000, 0x00003b59,
	0x00000000, 0x00000000, 0x00000000, 0x3cc4033c, 0x00000000, 0x00000000,
	0x00000000, 0x021f0000,
};
#define H_COEF_SIZE (sizeof(horKernel) / sizeof(u32))

static const u32 verKernel[] = {
	0x00000000, 0x20000000, 0x00002000, 0x00000000, 0x00000000, 0x00000000,
	0x23fd1c03, 0x00000000, 0x00000000, 0x00000000, 0x181f0000, 0x000027e1,
	0x00000000, 0x00000000, 0x00000000, 0x2b981468, 0x00000000, 0x00000000,
	0x00000000, 0x10f00000, 0x00002f10, 0x00000000, 0x00000000, 0x00000000,
	0x32390dc7, 0x00000000, 0x00000000, 0x00000000, 0x0af50000, 0x0000350b,
	0x00000000, 0x00000000, 0x00000000, 0x3781087f, 0x00000000, 0x00000000,
	0x00000000, 0x06660000, 0x0000399a, 0x00000000, 0x00000000, 0x00000000,
	0x3b5904a7, 0x00000000, 0x00000000, 0x00000000, 0x033c0000, 0x00003cc4,
	0x00000000, 0x00000000, 0x00000000, 0x3de1021f, 0x00000000, 0x00000000,
	0x00000000, 0x01470000, 0x00003eb9, 0x00000000, 0x00000000, 0x00000000,
	0x3f5300ad, 0x00000000, 0x00000000, 0x00000000, 0x00480000, 0x00003fb8,
	0x00000000, 0x00000000, 0x00000000, 0x3fef0011, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00004000, 0x00000000, 0xcdcd0000, 0xfdfdfdfd,
	0xabababab, 0xabababab, 0x00000000, 0x00000000, 0x5ff5f456, 0x000f5f58,
	0x02cc6c78, 0x02cc0c28, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee, 0xfeeefeee,
	0xfeeefeee, 0xfeeefeee,
};
#define V_COEF_SIZE (sizeof(verKernel) / sizeof(u32))

/*
 * RGB 709->2020 conversion parameters
 */
static u16 RGB2RGB[RGB_TO_RGB_TABLE_SIZE] = { 10279, 5395, 709,	 1132, 15065,
					      187,   269,  1442, 14674 };

/*
 * YUV601 to RGB conversion parameters
 * YUV2RGB[0]  - [8] : C0 - C8;
 * YUV2RGB[9]  - [11]: D0 - D2;
 * YUV2RGB[12] - [13]: Y clamp min & max calue;
 * YUV2RGB[14] - [15]: UV clamp min & max calue;
 */
static s32 YUV601_2RGB[YUV_TO_RGB_TABLE_SIZE] = {
	1196, 0,       1640,   1196,	 -404, -836, 1196, 2076,
	0,    -916224, 558336, -1202944, 64,   940,  64,   960
};

/*
 * YUV709 to RGB conversion parameters
 * YUV2RGB[0]  - [8] : C0 - C8;
 * YUV2RGB[9]  - [11]: D0 - D2;
 * YUV2RGB[12] - [13]: Y clamp min & max calue;
 * YUV2RGB[14] - [15]: UV clamp min & max calue;
 */
static s32 YUV709_2RGB[YUV_TO_RGB_TABLE_SIZE] = {
	1196, 0,	1844,	1196,	  -220, -548, 1196, 2172,
	0,    -1020672, 316672, -1188608, 64,	940,  64,   960
};

/*
 * YUV2020 to RGB conversion parameters
 * YUV2RGB[0]  - [8] : C0 - C8;
 * YUV2RGB[9]  - [11]: D0 - D2;
 * YUV2RGB[12] - [13]: Y clamp min & max calue;
 * YUV2RGB[14] - [15]: UV clamp min & max calue;
 */
static s32 YUV2020_2RGB[YUV_TO_RGB_TABLE_SIZE] = {
	1196, 0,       1724,   1196,	 -192, -668, 1196, 2200,
	0,    -959232, 363776, -1202944, 64,   940,  64,   960
};

/*
 * RGB to YUV2020 conversion parameters
 * RGB2YUV[0] - [8] : C0 - C8;
 * RGB2YUV[9] - [11]: D0 - D2;
 */
static s16 RGB2YUV[RGB_TO_YUV_TABLE_SIZE] = { 230, 594,	 52,  -125, -323, 448,
					      448, -412, -36, 64,   512,  512 };

/*
 * Degamma table for 709 color space data.
 */
static u16 DEGAMMA_709[DEGAMMA_SIZE] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0002, 0x0004, 0x0005, 0x0007,
	0x000a, 0x000d, 0x0011, 0x0015, 0x0019, 0x001e, 0x0024, 0x002a, 0x0030,
	0x0038, 0x003f, 0x0048, 0x0051, 0x005a, 0x0064, 0x006f, 0x007b, 0x0087,
	0x0094, 0x00a1, 0x00af, 0x00be, 0x00ce, 0x00de, 0x00ef, 0x0101, 0x0114,
	0x0127, 0x013b, 0x0150, 0x0166, 0x017c, 0x0193, 0x01ac, 0x01c4, 0x01de,
	0x01f9, 0x0214, 0x0230, 0x024d, 0x026b, 0x028a, 0x02aa, 0x02ca, 0x02ec,
	0x030e, 0x0331, 0x0355, 0x037a, 0x03a0, 0x03c7, 0x03ef, 0x0418, 0x0441,
	0x046c, 0x0498, 0x04c4, 0x04f2, 0x0520, 0x0550, 0x0581, 0x05b2, 0x05e5,
	0x0618, 0x064d, 0x0682, 0x06b9, 0x06f0, 0x0729, 0x0763, 0x079d, 0x07d9,
	0x0816, 0x0854, 0x0893, 0x08d3, 0x0914, 0x0956, 0x0999, 0x09dd, 0x0a23,
	0x0a69, 0x0ab1, 0x0afa, 0x0b44, 0x0b8f, 0x0bdb, 0x0c28, 0x0c76, 0x0cc6,
	0x0d17, 0x0d69, 0x0dbb, 0x0e10, 0x0e65, 0x0ebb, 0x0f13, 0x0f6c, 0x0fc6,
	0x1021, 0x107d, 0x10db, 0x113a, 0x119a, 0x11fb, 0x125d, 0x12c1, 0x1325,
	0x138c, 0x13f3, 0x145b, 0x14c5, 0x1530, 0x159c, 0x160a, 0x1678, 0x16e8,
	0x175a, 0x17cc, 0x1840, 0x18b5, 0x192b, 0x19a3, 0x1a1c, 0x1a96, 0x1b11,
	0x1b8e, 0x1c0c, 0x1c8c, 0x1d0c, 0x1d8e, 0x1e12, 0x1e96, 0x1f1c, 0x1fa3,
	0x202c, 0x20b6, 0x2141, 0x21ce, 0x225c, 0x22eb, 0x237c, 0x240e, 0x24a1,
	0x2536, 0x25cc, 0x2664, 0x26fc, 0x2797, 0x2832, 0x28cf, 0x296e, 0x2a0e,
	0x2aaf, 0x2b51, 0x2bf5, 0x2c9b, 0x2d41, 0x2dea, 0x2e93, 0x2f3e, 0x2feb,
	0x3099, 0x3148, 0x31f9, 0x32ab, 0x335f, 0x3414, 0x34ca, 0x3582, 0x363c,
	0x36f7, 0x37b3, 0x3871, 0x3930, 0x39f1, 0x3ab3, 0x3b77, 0x3c3c, 0x3d02,
	0x3dcb, 0x3e94, 0x3f5f, 0x402c, 0x40fa, 0x41ca, 0x429b, 0x436d, 0x4442,
	0x4517, 0x45ee, 0x46c7, 0x47a1, 0x487d, 0x495a, 0x4a39, 0x4b19, 0x4bfb,
	0x4cde, 0x4dc3, 0x4eaa, 0x4f92, 0x507c, 0x5167, 0x5253, 0x5342, 0x5431,
	0x5523, 0x5616, 0x570a, 0x5800, 0x58f8, 0x59f1, 0x5aec, 0x5be9, 0x5ce7,
	0x5de6, 0x5ee7, 0x5fea, 0x60ef, 0x61f5, 0x62fc, 0x6406, 0x6510, 0x661d,
	0x672b, 0x683b, 0x694c, 0x6a5f, 0x6b73, 0x6c8a, 0x6da2, 0x6ebb, 0x6fd6,
	0x70f3, 0x7211, 0x7331, 0x7453, 0x7576, 0x769b, 0x77c2, 0x78ea, 0x7a14,
	0x7b40, 0x7c6d, 0x7d9c, 0x7ecd, 0x3f65, 0x3f8c, 0x3fb2, 0x3fd8
};

/*
 * Degamma table for 2020 color space data.
 */
static u16 DEGAMMA_2020[DEGAMMA_SIZE] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0001,
	0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0002, 0x0002, 0x0002,
	0x0002, 0x0002, 0x0003, 0x0003, 0x0003, 0x0003, 0x0004, 0x0004, 0x0004,
	0x0005, 0x0005, 0x0006, 0x0006, 0x0006, 0x0007, 0x0007, 0x0008, 0x0008,
	0x0009, 0x000a, 0x000a, 0x000b, 0x000c, 0x000c, 0x000d, 0x000e, 0x000f,
	0x000f, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0016, 0x0017, 0x0018,
	0x0019, 0x001b, 0x001c, 0x001e, 0x001f, 0x0021, 0x0022, 0x0024, 0x0026,
	0x0028, 0x002a, 0x002c, 0x002e, 0x0030, 0x0033, 0x0035, 0x0038, 0x003a,
	0x003d, 0x0040, 0x0043, 0x0046, 0x0049, 0x004d, 0x0050, 0x0054, 0x0057,
	0x005b, 0x005f, 0x0064, 0x0068, 0x006d, 0x0071, 0x0076, 0x007c, 0x0081,
	0x0086, 0x008c, 0x0092, 0x0098, 0x009f, 0x00a5, 0x00ac, 0x00b4, 0x00bb,
	0x00c3, 0x00cb, 0x00d3, 0x00dc, 0x00e5, 0x00ee, 0x00f8, 0x0102, 0x010c,
	0x0117, 0x0123, 0x012e, 0x013a, 0x0147, 0x0154, 0x0161, 0x016f, 0x017e,
	0x018d, 0x019c, 0x01ac, 0x01bd, 0x01ce, 0x01e0, 0x01f3, 0x0206, 0x021a,
	0x022f, 0x0244, 0x025a, 0x0272, 0x0289, 0x02a2, 0x02bc, 0x02d6, 0x02f2,
	0x030f, 0x032c, 0x034b, 0x036b, 0x038b, 0x03ae, 0x03d1, 0x03f5, 0x041b,
	0x0443, 0x046b, 0x0495, 0x04c1, 0x04ee, 0x051d, 0x054e, 0x0580, 0x05b4,
	0x05ea, 0x0622, 0x065c, 0x0698, 0x06d6, 0x0717, 0x075a, 0x079f, 0x07e7,
	0x0831, 0x087e, 0x08cd, 0x0920, 0x0976, 0x09ce, 0x0a2a, 0x0a89, 0x0aec,
	0x0b52, 0x0bbc, 0x0c2a, 0x0c9b, 0x0d11, 0x0d8b, 0x0e0a, 0x0e8d, 0x0f15,
	0x0fa1, 0x1033, 0x10ca, 0x1167, 0x120a, 0x12b2, 0x1360, 0x1415, 0x14d1,
	0x1593, 0x165d, 0x172e, 0x1806, 0x18e7, 0x19d0, 0x1ac1, 0x1bbb, 0x1cbf,
	0x1dcc, 0x1ee3, 0x2005, 0x2131, 0x2268, 0x23ab, 0x24fa, 0x2656, 0x27be,
	0x2934, 0x2ab8, 0x2c4a, 0x2dec, 0x2f9d, 0x315f, 0x3332, 0x3516, 0x370d,
	0x3916, 0x3b34, 0x3d66, 0x3fad, 0x420b, 0x4480, 0x470d, 0x49b3, 0x4c73,
	0x4f4e, 0x5246, 0x555a, 0x588e, 0x5be1, 0x5f55, 0x62eb, 0x66a6, 0x6a86,
	0x6e8c, 0x72bb, 0x7714, 0x7b99, 0x3dcb, 0x3e60, 0x3ef5, 0x3f8c
};

/* one is for primary plane and the other is for all overlay planes */
static const struct dc_hw_plane_reg dc_plane_reg[] = {
	{
		.y_address = DC_FRAMEBUFFER_ADDRESS,
		.u_address = DC_FRAMEBUFFER_U_ADDRESS,
		.v_address = DC_FRAMEBUFFER_V_ADDRESS,
		.y_stride = DC_FRAMEBUFFER_STRIDE,
		.u_stride = DC_FRAMEBUFFER_U_STRIDE,
		.v_stride = DC_FRAMEBUFFER_V_STRIDE,
		.size = DC_FRAMEBUFFER_SIZE,
		.scale_factor_x = DC_FRAMEBUFFER_SCALE_FACTOR_X,
		.scale_factor_y = DC_FRAMEBUFFER_SCALE_FACTOR_Y,
		.h_filter_coef_index = DC_FRAMEBUFFER_H_FILTER_COEF_INDEX,
		.h_filter_coef_data = DC_FRAMEBUFFER_H_FILTER_COEF_DATA,
		.v_filter_coef_index = DC_FRAMEBUFFER_V_FILTER_COEF_INDEX,
		.v_filter_coef_data = DC_FRAMEBUFFER_V_FILTER_COEF_DATA,
		.init_offset = DC_FRAMEBUFFER_INIT_OFFSET,
		.color_key = DC_FRAMEBUFFER_COLOR_KEY,
		.color_key_high = DC_FRAMEBUFFER_COLOR_KEY_HIGH,
		.clear_value = DC_FRAMEBUFFER_CLEAR_VALUE,
		.color_table_index = DC_FRAMEBUFFER_COLOR_TABLE_INDEX,
		.color_table_data = DC_FRAMEBUFFER_COLOR_TABLE_DATA,
		.scale_config = DC_FRAMEBUFFER_SCALE_CONFIG,
		.degamma_index = DC_FRAMEBUFFER_DEGAMMA_INDEX,
		.degamma_data = DC_FRAMEBUFFER_DEGAMMA_DATA,
		.degamma_ex_data = DC_FRAMEBUFFER_DEGAMMA_EX_DATA,
		.roi_origin = DC_FRAMEBUFFER_ROI_ORIGIN,
		.roi_size = DC_FRAMEBUFFER_ROI_SIZE,
		.YUVToRGBCoef0 = DC_FRAMEBUFFER_YUVTORGB_COEF0,
		.YUVToRGBCoef1 = DC_FRAMEBUFFER_YUVTORGB_COEF1,
		.YUVToRGBCoef2 = DC_FRAMEBUFFER_YUVTORGB_COEF2,
		.YUVToRGBCoef3 = DC_FRAMEBUFFER_YUVTORGB_COEF3,
		.YUVToRGBCoef4 = DC_FRAMEBUFFER_YUVTORGB_COEF4,
		.YUVToRGBCoefD0 = DC_FRAMEBUFFER_YUVTORGB_COEFD0,
		.YUVToRGBCoefD1 = DC_FRAMEBUFFER_YUVTORGB_COEFD1,
		.YUVToRGBCoefD2 = DC_FRAMEBUFFER_YUVTORGB_COEFD2,
		.YClampBound = DC_FRAMEBUFFER_Y_CLAMP_BOUND,
		.UVClampBound = DC_FRAMEBUFFER_UV_CLAMP_BOUND,
		.RGBToRGBCoef0 = DC_FRAMEBUFFER_RGBTORGB_COEF0,
		.RGBToRGBCoef1 = DC_FRAMEBUFFER_RGBTORGB_COEF1,
		.RGBToRGBCoef2 = DC_FRAMEBUFFER_RGBTORGB_COEF2,
		.RGBToRGBCoef3 = DC_FRAMEBUFFER_RGBTORGB_COEF3,
		.RGBToRGBCoef4 = DC_FRAMEBUFFER_RGBTORGB_COEF4,
	},
	{
		.y_address = DC_OVERLAY_ADDRESS,
		.u_address = DC_OVERLAY_U_ADDRESS,
		.v_address = DC_OVERLAY_V_ADDRESS,
		.y_stride = DC_OVERLAY_STRIDE,
		.u_stride = DC_OVERLAY_U_STRIDE,
		.v_stride = DC_OVERLAY_V_STRIDE,
		.size = DC_OVERLAY_SIZE,
		.scale_factor_x = DC_OVERLAY_SCALE_FACTOR_X,
		.scale_factor_y = DC_OVERLAY_SCALE_FACTOR_Y,
		.h_filter_coef_index = DC_OVERLAY_H_FILTER_COEF_INDEX,
		.h_filter_coef_data = DC_OVERLAY_H_FILTER_COEF_DATA,
		.v_filter_coef_index = DC_OVERLAY_V_FILTER_COEF_INDEX,
		.v_filter_coef_data = DC_OVERLAY_V_FILTER_COEF_DATA,
		.init_offset = DC_OVERLAY_INIT_OFFSET,
		.color_key = DC_OVERLAY_COLOR_KEY,
		.color_key_high = DC_OVERLAY_COLOR_KEY_HIGH,
		.clear_value = DC_OVERLAY_CLEAR_VALUE,
		.color_table_index = DC_OVERLAY_COLOR_TABLE_INDEX,
		.color_table_data = DC_OVERLAY_COLOR_TABLE_DATA,
		.scale_config = DC_OVERLAY_SCALE_CONFIG,
		.degamma_index = DC_OVERLAY_DEGAMMA_INDEX,
		.degamma_data = DC_OVERLAY_DEGAMMA_DATA,
		.degamma_ex_data = DC_OVERLAY_DEGAMMA_EX_DATA,
		.roi_origin = DC_OVERLAY_ROI_ORIGIN,
		.roi_size = DC_OVERLAY_ROI_SIZE,
		.YUVToRGBCoef0 = DC_OVERLAY_YUVTORGB_COEF0,
		.YUVToRGBCoef1 = DC_OVERLAY_YUVTORGB_COEF1,
		.YUVToRGBCoef2 = DC_OVERLAY_YUVTORGB_COEF2,
		.YUVToRGBCoef3 = DC_OVERLAY_YUVTORGB_COEF3,
		.YUVToRGBCoef4 = DC_OVERLAY_YUVTORGB_COEF4,
		.YUVToRGBCoefD0 = DC_OVERLAY_YUVTORGB_COEFD0,
		.YUVToRGBCoefD1 = DC_OVERLAY_YUVTORGB_COEFD1,
		.YUVToRGBCoefD2 = DC_OVERLAY_YUVTORGB_COEFD2,
		.YClampBound = DC_OVERLAY_Y_CLAMP_BOUND,
		.UVClampBound = DC_OVERLAY_UV_CLAMP_BOUND,
		.RGBToRGBCoef0 = DC_OVERLAY_RGBTORGB_COEF0,
		.RGBToRGBCoef1 = DC_OVERLAY_RGBTORGB_COEF1,
		.RGBToRGBCoef2 = DC_OVERLAY_RGBTORGB_COEF2,
		.RGBToRGBCoef3 = DC_OVERLAY_RGBTORGB_COEF3,
		.RGBToRGBCoef4 = DC_OVERLAY_RGBTORGB_COEF4,
	},
};

static const u32 read_reg[] = {
	DC_FRAMEBUFFER_CONFIG,
	DC_OVERLAY_CONFIG,
	DC_OVERLAY_SCALE_CONFIG,
};
#define READ_REG_COUNT ARRAY_SIZE(read_reg)

static const u32 primary_overlay_formats[] = {
	DRM_FORMAT_XRGB4444, DRM_FORMAT_ARGB4444,    DRM_FORMAT_XRGB1555,
	DRM_FORMAT_ARGB1555, DRM_FORMAT_RGB565,	     DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888, DRM_FORMAT_ARGB2101010, DRM_FORMAT_RGBA4444,
	DRM_FORMAT_RGBA5551, DRM_FORMAT_RGBA8888,    DRM_FORMAT_RGBA1010102,
	DRM_FORMAT_ABGR4444, DRM_FORMAT_ABGR1555,    DRM_FORMAT_BGR565,
	DRM_FORMAT_ABGR8888, DRM_FORMAT_ABGR2101010, DRM_FORMAT_BGRA4444,
	DRM_FORMAT_BGRA5551, DRM_FORMAT_BGRA8888,    DRM_FORMAT_BGRA1010102,
	DRM_FORMAT_XBGR4444, DRM_FORMAT_RGBX4444,    DRM_FORMAT_BGRX4444,
	DRM_FORMAT_XBGR1555, DRM_FORMAT_RGBX5551,    DRM_FORMAT_BGRX5551,
	DRM_FORMAT_XBGR8888, DRM_FORMAT_RGBX8888,    DRM_FORMAT_BGRX8888,
	DRM_FORMAT_YUYV,     DRM_FORMAT_YVYU,	     DRM_FORMAT_UYVY,
	DRM_FORMAT_VYUY,     DRM_FORMAT_NV12,	     DRM_FORMAT_NV21,
	DRM_FORMAT_NV16,     DRM_FORMAT_NV61,	     DRM_FORMAT_P010,
	/* TODO. */
};

static const u32 cursor_formats[] = { DRM_FORMAT_ARGB8888 };

static const u64 cursor_dumy_modifiers[] = { DRM_FORMAT_MOD_LINEAR,
					     DRM_FORMAT_MOD_INVALID };

static const u64 format_modifiers[] = { DRM_FORMAT_MOD_LINEAR,
					DRM_FORMAT_MOD_INVALID };

#define FRAC_16_16(mult, div) (((mult) << 16) / (div))

static const struct es_plane_info dc_hw_planes[][PLANE_NUM] = {
	{
		/* DC_REV_5551 */
		{
			.name = "Primary",
			.id = PRIMARY_PLANE,
			.type = DRM_PLANE_TYPE_PRIMARY,
			.num_formats = ARRAY_SIZE(primary_overlay_formats),
			.formats = primary_overlay_formats,
			.modifiers = format_modifiers,
			.min_width = 0,
			.min_height = 0,
			.max_width = 4096,
			.max_height = 4096,
			.rotation = DRM_MODE_ROTATE_0,
			.color_encoding = BIT(DRM_COLOR_YCBCR_BT601) |
					  BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
			.degamma_size = 0,
			.min_scale = FRAC_16_16(1, 3),
			.max_scale = FRAC_16_16(10, 1),
		},
		{
			.name = "Cursor",
			.id = CURSOR_PLANE,
			.type = DRM_PLANE_TYPE_CURSOR,
			.num_formats = ARRAY_SIZE(cursor_formats),
			.formats = cursor_formats,
			.modifiers = NULL,
			.min_width = 32,
			.min_height = 32,
			.max_width = 32,
			.max_height = 32,
			.rotation = DRM_MODE_ROTATE_0,
			.degamma_size = 0,
			.min_scale = DRM_PLANE_NO_SCALING,
			.max_scale = DRM_PLANE_NO_SCALING,
		},
		{
			.name = "Overlay",
			.id = OVERLAY_PLANE,
			.type = DRM_PLANE_TYPE_OVERLAY,
			.num_formats = ARRAY_SIZE(primary_overlay_formats),
			.formats = primary_overlay_formats,
			.modifiers = format_modifiers,
			.min_width = 0,
			.min_height = 0,
			.max_width = 4096,
			.max_height = 4096,
			.rotation = DRM_MODE_ROTATE_0,
			.blend_mode = BIT(DRM_MODE_BLEND_PIXEL_NONE) |
				      BIT(DRM_MODE_BLEND_PREMULTI) |
				      BIT(DRM_MODE_BLEND_COVERAGE),
			.color_encoding = BIT(DRM_COLOR_YCBCR_BT601) |
					  BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
			.degamma_size = 0,
			.min_scale = FRAC_16_16(1, 3),
			.max_scale = FRAC_16_16(10, 1),
		},
	},
	{
		/* DC_REV_5551_306 */
		{
			.name = "Primary",
			.id = PRIMARY_PLANE,
			.type = DRM_PLANE_TYPE_PRIMARY,
			.num_formats = ARRAY_SIZE(primary_overlay_formats),
			.formats = primary_overlay_formats,
			.modifiers = format_modifiers,
			.min_width = 0,
			.min_height = 0,
			.max_width = 4096,
			.max_height = 4096,
			.rotation = DRM_MODE_ROTATE_0 | DRM_MODE_ROTATE_90 |
				    DRM_MODE_ROTATE_180 | DRM_MODE_ROTATE_270 |
				    DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y,
			.color_encoding = BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
			.degamma_size = 0,
			.min_scale = FRAC_16_16(1, 3),
			.max_scale = FRAC_16_16(10, 1),
		},
		{
			.name = "Cursor",
			.id = CURSOR_PLANE,
			.type = DRM_PLANE_TYPE_CURSOR,
			.num_formats = ARRAY_SIZE(cursor_formats),
			.formats = cursor_formats,
			.modifiers = NULL,
			.min_width = 32,
			.min_height = 32,
			.max_width = 32,
			.max_height = 32,
			.rotation = 0,
			.color_encoding = 0,
			.degamma_size = 0,
			.min_scale = DRM_PLANE_NO_SCALING,
			.max_scale = DRM_PLANE_NO_SCALING,
		},
		{
			.name = "Overlay",
			.id = OVERLAY_PLANE,
			.type = DRM_PLANE_TYPE_OVERLAY,
			.num_formats = ARRAY_SIZE(primary_overlay_formats),
			.formats = primary_overlay_formats,
			.modifiers = format_modifiers,
			.min_width = 0,
			.min_height = 0,
			.max_width = 4096,
			.max_height = 4096,
			.rotation = DRM_MODE_ROTATE_0 | DRM_MODE_ROTATE_90 |
				    DRM_MODE_ROTATE_180 | DRM_MODE_ROTATE_270 |
				    DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y,
			.blend_mode = BIT(DRM_MODE_BLEND_PIXEL_NONE) |
				      BIT(DRM_MODE_BLEND_PREMULTI) |
				      BIT(DRM_MODE_BLEND_COVERAGE),
			.color_encoding = BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
			.degamma_size = 0,
			.min_scale = FRAC_16_16(1, 3),
			.max_scale = FRAC_16_16(10, 1),
		},
	},
	{
		/* DC_REV_5701_303 */
		{
			.name = "Primary",
			.id = PRIMARY_PLANE,
			.type = DRM_PLANE_TYPE_PRIMARY,
			.num_formats = ARRAY_SIZE(primary_overlay_formats),
			.formats = primary_overlay_formats,
			.modifiers = format_modifiers,
			.min_width = 0,
			.min_height = 0,
			.max_width = 4096,
			.max_height = 4096,
			.rotation = DRM_MODE_ROTATE_0,
			.color_encoding = BIT(DRM_COLOR_YCBCR_BT601) |
					  BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
			.degamma_size = DEGAMMA_SIZE,
			.min_scale = FRAC_16_16(1, 3),
			.max_scale = FRAC_16_16(10, 1),
		},
		{
			.name = "Cursor",
			.id = CURSOR_PLANE,
			.type = DRM_PLANE_TYPE_CURSOR,
			.num_formats = ARRAY_SIZE(cursor_formats),
			.formats = cursor_formats,
			.modifiers = NULL,
			.min_width = 32,
			.min_height = 32,
			.max_width = 256,
			.max_height = 256,
			.rotation = DRM_MODE_ROTATE_0,
			.degamma_size = 0,
			.min_scale = DRM_PLANE_NO_SCALING,
			.max_scale = DRM_PLANE_NO_SCALING,
		},
		{
			.name = "Overlay",
			.id = OVERLAY_PLANE,
			.type = DRM_PLANE_TYPE_OVERLAY,
			.num_formats = ARRAY_SIZE(primary_overlay_formats),
			.formats = primary_overlay_formats,
			.modifiers = format_modifiers,
			.min_width = 0,
			.min_height = 0,
			.max_width = 4096,
			.max_height = 4096,
			.rotation = DRM_MODE_ROTATE_0,
			.blend_mode = BIT(DRM_MODE_BLEND_PIXEL_NONE) |
				      BIT(DRM_MODE_BLEND_PREMULTI) |
				      BIT(DRM_MODE_BLEND_COVERAGE),
			.color_encoding = BIT(DRM_COLOR_YCBCR_BT601) |
					  BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
			.degamma_size = DEGAMMA_SIZE,
			.min_scale = FRAC_16_16(1, 3),
			.max_scale = FRAC_16_16(10, 1),
		},
	},
	{
		/* DC_REV_5701_309 */
		{
			.name = "Primary",
			.id = PRIMARY_PLANE,
			.type = DRM_PLANE_TYPE_PRIMARY,
			.num_formats = ARRAY_SIZE(primary_overlay_formats),
			.formats = primary_overlay_formats,
			.modifiers = format_modifiers,
			.min_width = 0,
			.min_height = 0,
			.max_width = 4096,
			.max_height = 4096,
			.rotation = DRM_MODE_ROTATE_0 | DRM_MODE_ROTATE_180,
			.color_encoding = BIT(DRM_COLOR_YCBCR_BT601) |
					  BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
			.degamma_size = DEGAMMA_SIZE,
			.min_scale = FRAC_16_16(1, 32),
			.max_scale = FRAC_16_16(2, 1),
			.roi = true,
			.color_mgmt = true,
			.background = true,
		},
		{
			.name = "Cursor",
			.id = CURSOR_PLANE,
			.type = DRM_PLANE_TYPE_CURSOR,
			.num_formats = ARRAY_SIZE(cursor_formats),
			.formats = cursor_formats,
			.modifiers = cursor_dumy_modifiers,
			.min_width = 32,
			.min_height = 32,
			.max_width = 256,
			.max_height = 256,
			.rotation = DRM_MODE_ROTATE_0,
			.degamma_size = 0,
			.min_scale = DRM_PLANE_NO_SCALING,
			.max_scale = DRM_PLANE_NO_SCALING,
		},
		{
			.name = "Overlay",
			.id = OVERLAY_PLANE,
			.type = DRM_PLANE_TYPE_OVERLAY,
			.num_formats = ARRAY_SIZE(primary_overlay_formats),
			.formats = primary_overlay_formats,
			.modifiers = format_modifiers,
			.min_width = 0,
			.min_height = 0,
			.max_width = 4096,
			.max_height = 4096,
			.rotation = DRM_MODE_ROTATE_0 | DRM_MODE_ROTATE_180,
			.blend_mode = BIT(DRM_MODE_BLEND_PIXEL_NONE) |
				      BIT(DRM_MODE_BLEND_PREMULTI) |
				      BIT(DRM_MODE_BLEND_COVERAGE),
			.color_encoding = BIT(DRM_COLOR_YCBCR_BT601) |
					  BIT(DRM_COLOR_YCBCR_BT709) |
					  BIT(DRM_COLOR_YCBCR_BT2020),
			.degamma_size = DEGAMMA_SIZE,
			.min_scale = FRAC_16_16(1, 32),
			.max_scale = FRAC_16_16(2, 1),
			.roi = true,
			.color_mgmt = true,
		},
	},
};

static const struct es_dc_info dc_info[] = {
	{
		/* DC_REV_5551 */
		.name = "DisplayControl",
		.plane_num = PLANE_NUM,
		.planes = dc_hw_planes[DC_REV_5551],
		.max_bpc = 10,
		.color_formats = DRM_COLOR_FORMAT_RGB444,
		.gamma_size = GAMMA_SIZE,
		.gamma_bits = 10,
		.pitch_alignment = 128,
		.pipe_sync = false,
		.mmu_prefetch = false,
	},
	{
		/* DC_REV_5551_306 */
		.name = "DisplayControl",
		.plane_num = PLANE_NUM,
		.planes = dc_hw_planes[DC_REV_5551_306],
		.max_bpc = 10,
		.color_formats = DRM_COLOR_FORMAT_RGB444,
		.gamma_size = GAMMA_SIZE,
		.gamma_bits = 10,
		.pitch_alignment = 128,
		.pipe_sync = false,
		.mmu_prefetch = false,
	},
	{
		/* DC_REV_5701_303 */
		.name = "DisplayControl",
		.plane_num = PLANE_NUM,
		.planes = dc_hw_planes[DC_REV_5701_303],
		.max_bpc = 10,
		.color_formats =
			DRM_COLOR_FORMAT_RGB444 | DRM_COLOR_FORMAT_YCBCR444 |
			DRM_COLOR_FORMAT_YCBCR422 | DRM_COLOR_FORMAT_YCBCR420,
		.gamma_size = GAMMA_EX_SIZE,
		.gamma_bits = 12,
		.pitch_alignment = 128,
		.pipe_sync = false,
		.mmu_prefetch = false,
	},
	{
		/* DC_REV_5701_309 */
		.name = "DisplayControl",
		.plane_num = PLANE_NUM,
		.planes = dc_hw_planes[DC_REV_5701_309],
		.max_bpc = 10,
		.color_formats =
			DRM_COLOR_FORMAT_RGB444 | DRM_COLOR_FORMAT_YCBCR444 |
			DRM_COLOR_FORMAT_YCBCR422 | DRM_COLOR_FORMAT_YCBCR420,
		.gamma_size = GAMMA_EX_SIZE,
		.gamma_bits = 12,
		.pitch_alignment = 128,
		.pipe_sync = true,
		.mmu_prefetch = true,
		.background = true,
	},
};

static const struct dc_hw_funcs hw_func[];

static inline u32 hi_write(struct dc_hw *hw, u32 reg, u32 val)
{
	writel(val, hw->hi_base + reg);

	return 0;
}

static inline u32 hi_read(struct dc_hw *hw, u32 reg)
{
	u32 value;

	value = readl(hw->hi_base + reg);
	return value;
}

static inline int dc_get_read_index(struct dc_hw *hw, u32 reg)
{
	int i;

	for (i = 0; i < READ_REG_COUNT; i++) {
		if (hw->read_block[i].reg == reg)
			return i;
	}
	return i;
}

static inline void dc_write(struct dc_hw *hw, u32 reg, u32 value)
{
	writel(value, hw->reg_base + reg - DC_REG_BASE);

	if (hw->read_block) {
		int i = dc_get_read_index(hw, reg);

		if (i < READ_REG_COUNT)
			hw->read_block[i].value = value;
	}
}

static inline u32 dc_read(struct dc_hw *hw, u32 reg)
{
	u32 value = readl(hw->reg_base + reg - DC_REG_BASE);

	if (hw->read_block) {
		int i = dc_get_read_index(hw, reg);

		if (i < READ_REG_COUNT) {
			value &= ~BIT(0);
			value |= hw->read_block[i].value & BIT(0);
		}
	}

	return value;
}

static inline void dc_set_clear(struct dc_hw *hw, u32 reg, u32 set, u32 clear)
{
	u32 value = dc_read(hw, reg);

	value &= ~clear;
	value |= set;
	dc_write(hw, reg, value);
}

static void load_default_filter(struct dc_hw *hw,
				const struct dc_hw_plane_reg *reg)
{
	u8 i;

	dc_write(hw, reg->scale_config, 0x33);
	dc_write(hw, reg->init_offset, 0x80008000);
	dc_write(hw, reg->h_filter_coef_index, 0x00);
	for (i = 0; i < H_COEF_SIZE; i++)
		dc_write(hw, reg->h_filter_coef_data, horKernel[i]);

	dc_write(hw, reg->v_filter_coef_index, 0x00);
	for (i = 0; i < V_COEF_SIZE; i++)
		dc_write(hw, reg->v_filter_coef_data, verKernel[i]);
}

static void load_rgb_to_rgb(struct dc_hw *hw, const struct dc_hw_plane_reg *reg,
			    u16 *table)
{
	dc_write(hw, reg->RGBToRGBCoef0, table[0] | (table[1] << 16));
	dc_write(hw, reg->RGBToRGBCoef1, table[2] | (table[3] << 16));
	dc_write(hw, reg->RGBToRGBCoef2, table[4] | (table[5] << 16));
	dc_write(hw, reg->RGBToRGBCoef3, table[6] | (table[7] << 16));
	dc_write(hw, reg->RGBToRGBCoef4, table[8]);
}

static void load_yuv_to_rgb(struct dc_hw *hw, const struct dc_hw_plane_reg *reg,
			    s32 *table)
{
	dc_write(hw, reg->YUVToRGBCoef0,
		 (0xFFFF & table[0]) | (table[1] << 16));
	dc_write(hw, reg->YUVToRGBCoef1,
		 (0xFFFF & table[2]) | (table[3] << 16));
	dc_write(hw, reg->YUVToRGBCoef2,
		 (0xFFFF & table[4]) | (table[5] << 16));
	dc_write(hw, reg->YUVToRGBCoef3,
		 (0xFFFF & table[6]) | (table[7] << 16));
	dc_write(hw, reg->YUVToRGBCoef4, table[8]);
	dc_write(hw, reg->YUVToRGBCoefD0, table[9]);
	dc_write(hw, reg->YUVToRGBCoefD1, table[10]);
	dc_write(hw, reg->YUVToRGBCoefD2, table[11]);
	dc_write(hw, reg->YClampBound, table[12] | (table[13] << 16));
	dc_write(hw, reg->UVClampBound, table[14] | (table[15] << 16));
}

static void load_rgb_to_yuv(struct dc_hw *hw, s16 *table)
{
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF0, table[0] | (table[1] << 16));
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF1, table[2] | (table[3] << 16));
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF2, table[4] | (table[5] << 16));
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF3, table[6] | (table[7] << 16));
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEF4, table[8]);
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEFD0, table[9]);
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEFD1, table[10]);
	dc_write(hw, DC_DISPLAY_RGBTOYUV_COEFD2, table[11]);
}

static bool is_rgb(enum dc_hw_color_format format)
{
	switch (format) {
	case FORMAT_X4R4G4B4:
	case FORMAT_A4R4G4B4:
	case FORMAT_X1R5G5B5:
	case FORMAT_A1R5G5B5:
	case FORMAT_R5G6B5:
	case FORMAT_X8R8G8B8:
	case FORMAT_A8R8G8B8:
	case FORMAT_A2R10G10B10:
		return true;
	default:
		return false;
	}
}

static void load_degamma_table(struct dc_hw *hw,
			       const struct dc_hw_plane_reg *reg, u16 *table)
{
	u16 i;
	u32 value;

	for (i = 0; i < DEGAMMA_SIZE; i++) {
		dc_write(hw, reg->degamma_index, i);
		value = table[i] | (table[i] << 16);
		dc_write(hw, reg->degamma_data, value);
		dc_write(hw, reg->degamma_ex_data, table[i]);
	}
}

int dc_hw_init(struct dc_hw *hw)
{
	u8 i;
	u32 revision = hi_read(hw, DC_HW_REVISION);
	u32 cid = hi_read(hw, DC_HW_CHIP_CID);

	switch (revision) {
	case 0x5551:
		if (cid < 0x306)
			hw->rev = DC_REV_5551;
		else
			hw->rev = DC_REV_5551_306;
		break;
	case 0x5701:
		if (cid < 0x309)
			hw->rev = DC_REV_5701_303;
		else
			hw->rev = DC_REV_5701_309;
		break;
	default:
		return -ENXIO;
	}

	if (hw->rev == DC_REV_5551) {
		hw->read_block = kzalloc(
			sizeof(struct dc_hw_read) * READ_REG_COUNT, GFP_KERNEL);
		if (!hw->read_block)
			return -ENOMEM;
		for (i = 0; i < READ_REG_COUNT; i++)
			hw->read_block[i].reg = read_reg[i];
	}

	/*
     * Do soft reset before configuring DC
     * Sleep 50ms to ensure no data transfer on AXI bus,
     * because dc may be initialized in uboot
     */
	{
		u32 val;

		val = dc_read(hw, DC_FRAMEBUFFER_CONFIG);
		val &= ~BIT(0);
		val |= BIT(3);
		dc_write(hw, DC_FRAMEBUFFER_CONFIG, val);
		mdelay(50);
		hi_write(hw, 0x0, 0x1000);
		mdelay(10);
	}

	hw->info = (struct es_dc_info *)&dc_info[hw->rev];
	hw->func = (struct dc_hw_funcs *)&hw_func[hw->rev];

	for (i = 0; i < ARRAY_SIZE(dc_plane_reg); i++) {
		load_default_filter(hw, &dc_plane_reg[i]);
		if ((hw->rev == DC_REV_5701_303) ||
		    (hw->rev == DC_REV_5701_309))
			load_rgb_to_rgb(hw, &dc_plane_reg[i], RGB2RGB);
	}

	if (hw->rev == DC_REV_5701_303)
		load_rgb_to_yuv(hw, RGB2YUV);

	dc_write(hw, DC_CURSOR_BACKGROUND, 0x00FFFFFF);
	dc_write(hw, DC_CURSOR_FOREGROUND, 0x00AAAAAA);
	dc_write(hw, DC_DISPLAY_PANEL_CONFIG, 0x111);

	return 0;
}

void dc_hw_deinit(struct dc_hw *hw)
{
	if (hw->read_block)
		kfree(hw->read_block);
}

void dc_hw_update_plane(struct dc_hw *hw, enum dc_hw_plane_id id,
			struct dc_hw_fb *fb, struct dc_hw_scale *scale)
{
	struct dc_hw_plane *plane = NULL;

	if (id == PRIMARY_PLANE)
		plane = &hw->primary;
	else if (id == OVERLAY_PLANE)
		plane = &hw->overlay.plane;

	if (plane) {
		if (fb) {
			if (fb->enable == false)
				plane->fb.enable = false;
			else
				memcpy(&plane->fb, fb,
				       sizeof(*fb) - sizeof(fb->dirty));
			plane->fb.dirty = true;
		}
		if (scale) {
			memcpy(&plane->scale, scale,
			       sizeof(*scale) - sizeof(scale->dirty));
			plane->scale.dirty = true;
		}
	}
}

void dc_hw_update_degamma(struct dc_hw *hw, enum dc_hw_plane_id id, u32 mode)
{
	struct dc_hw_plane *plane = NULL;
	int i = (int)id;

	if (id == PRIMARY_PLANE)
		plane = &hw->primary;
	else if (id == OVERLAY_PLANE)
		plane = &hw->overlay.plane;

	if (plane) {
		if (hw->info->planes[i].degamma_size) {
			plane->degamma.mode = mode;
			plane->degamma.dirty = true;
		} else {
			plane->degamma.dirty = false;
		}
	}
}

void dc_hw_set_position(struct dc_hw *hw, struct dc_hw_position *pos)
{
	memcpy(&hw->overlay.pos, pos, sizeof(*pos) - sizeof(pos->dirty));
	hw->overlay.pos.dirty = true;
}

void dc_hw_set_blend(struct dc_hw *hw, struct dc_hw_blend *blend)
{
	memcpy(&hw->overlay.blend, blend,
	       sizeof(*blend) - sizeof(blend->dirty));
	hw->overlay.blend.dirty = true;
}

void dc_hw_update_cursor(struct dc_hw *hw, struct dc_hw_cursor *cursor)
{
	memcpy(&hw->cursor, cursor, sizeof(*cursor) - sizeof(cursor->dirty));
	hw->cursor.dirty = true;
}

void dc_hw_update_gamma(struct dc_hw *hw, u16 index, u16 r, u16 g, u16 b)
{
	if (index >= hw->info->gamma_size)
		return;

	hw->gamma.gamma[index][0] = r;
	hw->gamma.gamma[index][1] = g;
	hw->gamma.gamma[index][2] = b;
	hw->gamma.dirty = true;
}

void dc_hw_enable_gamma(struct dc_hw *hw, bool enable)
{
	hw->gamma.enable = enable;
	hw->gamma.dirty = true;
}

void dc_hw_enable_dump(struct dc_hw *hw, u32 addr, u32 pitch)
{
	dc_write(hw, 0x14F0, addr);
	dc_write(hw, 0x14E8, addr);
	dc_write(hw, 0x1500, pitch);
	dc_write(hw, 0x14F8, 0x30000);
}

void dc_hw_disable_dump(struct dc_hw *hw)
{
	dc_write(hw, 0x14F8, 0x00);
}

void dc_hw_setup_display(struct dc_hw *hw, struct dc_hw_display *display)
{
	memcpy(&hw->display, display, sizeof(*display));

	hw->func->display(hw, display);
}

void dc_hw_enable_interrupt(struct dc_hw *hw, bool enable)
{
	dc_write(hw, DC_DISPLAY_INT_ENABLE, enable);
}

u32 dc_hw_get_interrupt(struct dc_hw *hw)
{
	return dc_read(hw, DC_DISPLAY_INT);
}

bool dc_hw_flip_in_progress(struct dc_hw *hw)
{
	return dc_read(hw, DC_FRAMEBUFFER_CONFIG) & BIT(6);
}

bool dc_hw_check_underflow(struct dc_hw *hw)
{
	return dc_read(hw, DC_FRAMEBUFFER_CONFIG) & BIT(5);
}

void dc_hw_enable_shadow_register(struct dc_hw *hw, bool enable)
{
	if (enable)
		dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG, 0, BIT(3));
	else
		dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG, BIT(3), 0);
}

void dc_hw_set_out(struct dc_hw *hw, enum dc_hw_out out)
{
	if (out <= OUT_DP)
		hw->out = out;
}

static void gamma_commit(struct dc_hw *hw)
{
	u16 i;
	u32 value;

	if (hw->gamma.dirty) {
		if (hw->gamma.enable) {
			dc_write(hw, DC_DISPLAY_GAMMA_INDEX, 0x00);
			for (i = 0; i < GAMMA_SIZE; i++) {
				value = hw->gamma.gamma[i][2] |
					(hw->gamma.gamma[i][1] << 10) |
					(hw->gamma.gamma[i][0] << 20);
				dc_write(hw, DC_DISPLAY_GAMMA_DATA, value);
			}

			dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG, BIT(2), 0);
		} else {
			dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG, 0, BIT(2));
		}
		hw->gamma.dirty = false;
	}
}

static void gamma_ex_commit(struct dc_hw *hw)
{
	u16 i;
	u32 value;

	if (hw->gamma.dirty) {
		if (hw->gamma.enable) {
			dc_write(hw, DC_DISPLAY_GAMMA_EX_INDEX, 0x00);
			for (i = 0; i < GAMMA_EX_SIZE; i++) {
				value = hw->gamma.gamma[i][2] |
					(hw->gamma.gamma[i][1] << 16);
				dc_write(hw, DC_DISPLAY_GAMMA_EX_DATA, value);
				dc_write(hw, DC_DISPLAY_GAMMA_EX_ONE_DATA,
					 hw->gamma.gamma[i][0]);
			}
			dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG, BIT(2), 0);
		} else {
			dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG, 0, BIT(2));
		}
		hw->gamma.dirty = false;
	}
}

static void plane_commit(struct dc_hw *hw)
{
	struct dc_hw_plane *plane;
	const struct dc_hw_plane_reg *reg;
	u16 i;

	for (i = 0; i < PLANE_NUM; i++) {
		if (i == PRIMARY_PLANE) {
			plane = &hw->primary;
			reg = &dc_plane_reg[0];
		} else if (i == OVERLAY_PLANE) {
			plane = &hw->overlay.plane;
			reg = &dc_plane_reg[1];
		} else {
			continue;
		}

		if (plane->fb.dirty) {
			if (plane->fb.enable) {
				// sifive_l2_flush64_range(plane->fb.y_address, plane->fb.width * plane->fb.height * 4);

				dc_write(hw, reg->y_address,
					 plane->fb.y_address);
				dc_write(hw, reg->u_address,
					 plane->fb.u_address);
				dc_write(hw, reg->v_address,
					 plane->fb.v_address);
				dc_write(hw, reg->y_stride, plane->fb.y_stride);
				dc_write(hw, reg->u_stride, plane->fb.u_stride);
				dc_write(hw, reg->v_stride, plane->fb.v_stride);
				dc_write(hw, reg->size,
					 plane->fb.width |
						 (plane->fb.height << 15));
				if (plane->fb.clear_enable)
					dc_write(hw, reg->clear_value,
						 plane->fb.clear_value);
			} else {
				dc_hw_enable_shadow_register(hw, true);
				if (i == PRIMARY_PLANE) {
					u32 reg_clr = BIT(24), reg_set = 0;
					dc_set_clear(hw, DC_OVERLAY_CONFIG,
						     reg_set, reg_clr);

					reg_clr = (0x1F << 26),
					reg_set = BIT(8) | (05 << 26);
					dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG,
						     reg_set, reg_clr);
				}
				mdelay(50);
				dc_hw_enable_shadow_register(hw, false);
			}

			if (i == PRIMARY_PLANE) {
				u32 reg_clr = 0, reg_set = 0;

				reg_clr = (0x1F << 26) | BIT(25) |
					  (0x03 << 23) | (0x1F << 17) |
					  (0x07 << 14) | (0x07 << 11) | BIT(0);
				reg_set = (plane->fb.format << 26) |
					  (plane->fb.uv_swizzle << 25) |
					  (plane->fb.swizzle << 23) |
					  (plane->fb.tile_mode << 17) |
					  (plane->fb.yuv_color_space << 14) |
					  (plane->fb.rotation << 11) |
					  (plane->fb.clear_enable << 8) |
					  plane->fb.enable;
				if (plane->fb.enable) {
					reg_clr |= BIT(8);
					reg_set |= BIT(4);
				}

				dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG, reg_set,
					     reg_clr);
			} else
				dc_set_clear(
					hw, DC_OVERLAY_CONFIG,
					(plane->fb.format << 16) |
						(plane->fb.uv_swizzle << 15) |
						(plane->fb.swizzle << 13) |
						(plane->fb.tile_mode << 8) |
						(plane->fb.yuv_color_space
						 << 5) |
						(plane->fb.rotation << 2) |
						(plane->fb.enable << 24) |
						(plane->fb.clear_enable << 25),
					(0x1F << 16) | BIT(15) | (0x03 << 13) |
						(0x1F << 8) | (0x07 << 5) |
						(0x07 << 2) | BIT(24));
			plane->fb.dirty = false;
		}

		if (plane->scale.dirty) {
			if (plane->scale.enable) {
				dc_write(hw, reg->scale_factor_x,
					 plane->scale.scale_factor_x);
				dc_write(hw, reg->scale_factor_y,
					 plane->scale.scale_factor_y);
				if (i == PRIMARY_PLANE)
					dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG,
						     BIT(22), 0);
				else
					dc_set_clear(hw,
						     DC_OVERLAY_SCALE_CONFIG,
						     BIT(8), 0);

			} else {
				if (i == PRIMARY_PLANE)
					dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG,
						     0, BIT(22));
				else
					dc_set_clear(hw,
						     DC_OVERLAY_SCALE_CONFIG, 0,
						     BIT(8));
			}
			plane->scale.dirty = false;
		}

		if (plane->colorkey.dirty) {
			dc_write(hw, reg->color_key, plane->colorkey.colorkey);
			dc_write(hw, reg->color_key_high,
				 plane->colorkey.colorkey_high);

			if (i == PRIMARY_PLANE)
				dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG,
					     plane->colorkey.transparency << 9,
					     0x03 << 9);
			else
				dc_set_clear(hw, DC_OVERLAY_CONFIG,
					     plane->colorkey.transparency,
					     0x03);

			plane->colorkey.dirty = false;
		}

		if (plane->roi.dirty) {
			if (plane->roi.enable) {
				dc_write(hw, reg->roi_origin,
					 plane->roi.x | (plane->roi.y << 16));
				dc_write(hw, reg->roi_size,
					 plane->roi.width |
						 (plane->roi.height << 16));
				if (i == PRIMARY_PLANE)
					dc_set_clear(hw,
						     DC_FRAMEBUFFER_CONFIG_EX,
						     BIT(0), 0);
			} else {
				// if (i == PRIMARY_PLANE)
				dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX, 0,
					     BIT(0));
			}
			plane->roi.dirty = false;
		}
	}
}

static void plane_ex_commit(struct dc_hw *hw)
{
	struct dc_hw_plane *plane;
	const struct dc_hw_plane_reg *reg;
	u16 i;

	for (i = 0; i < PLANE_NUM; i++) {
		if (i == PRIMARY_PLANE) {
			plane = &hw->primary;
			reg = &dc_plane_reg[0];
		} else if (i == OVERLAY_PLANE) {
			plane = &hw->overlay.plane;
			reg = &dc_plane_reg[1];
		} else {
			continue;
		}

		if (plane->fb.dirty) {
			if (is_rgb(plane->fb.format)) {
				if (i == PRIMARY_PLANE)
					dc_set_clear(hw,
						     DC_FRAMEBUFFER_CONFIG_EX,
						     BIT(6), BIT(8));
				else
					dc_set_clear(hw, DC_OVERLAY_CONFIG,
						     BIT(29), BIT(30));
			} else {
				if (i == PRIMARY_PLANE)
					dc_set_clear(hw,
						     DC_FRAMEBUFFER_CONFIG_EX,
						     BIT(8), BIT(6));
				else
					dc_set_clear(hw, DC_OVERLAY_CONFIG,
						     BIT(30), BIT(29));
				switch (plane->fb.yuv_color_space) {
				case COLOR_SPACE_601:
					load_yuv_to_rgb(hw, reg, YUV601_2RGB);
					break;
				case COLOR_SPACE_709:
					load_yuv_to_rgb(hw, reg, YUV709_2RGB);
					break;
				case COLOR_SPACE_2020:
					load_yuv_to_rgb(hw, reg, YUV2020_2RGB);
					break;
				default:
					break;
				}
			}
		}
		if (plane->degamma.dirty) {
			switch (plane->degamma.mode) {
			case ES_DEGAMMA_DISABLE:
				if (i == PRIMARY_PLANE)
					dc_set_clear(hw,
						     DC_FRAMEBUFFER_CONFIG_EX,
						     0, BIT(5));
				else
					dc_set_clear(hw, DC_OVERLAY_CONFIG, 0,
						     BIT(28));
				break;
			case ES_DEGAMMA_BT709:
				load_degamma_table(hw, reg, DEGAMMA_709);
				if (i == PRIMARY_PLANE)
					dc_set_clear(hw,
						     DC_FRAMEBUFFER_CONFIG_EX,
						     BIT(5), 0);
				else
					dc_set_clear(hw, DC_OVERLAY_CONFIG,
						     BIT(28), 0);
				break;
			case ES_DEGAMMA_BT2020:
				load_degamma_table(hw, reg, DEGAMMA_2020);
				if (i == PRIMARY_PLANE)
					dc_set_clear(hw,
						     DC_FRAMEBUFFER_CONFIG_EX,
						     BIT(5), 0);
				else
					dc_set_clear(hw, DC_OVERLAY_CONFIG,
						     BIT(28), 0);
				break;
			default:
				break;
			}
			plane->degamma.dirty = false;
		}
	}
	plane_commit(hw);
}

static void setup_display(struct dc_hw *hw, struct dc_hw_display *display)
{
	u32 dpi_cfg;

	if (hw->display.enable) {
		switch (display->bus_format) {
		case MEDIA_BUS_FMT_RGB565_1X16:
			dpi_cfg = 0;
			break;
		case MEDIA_BUS_FMT_RGB666_1X18:
			dpi_cfg = 3;
			break;
		case MEDIA_BUS_FMT_RGB666_1X24_CPADHI:
			dpi_cfg = 4;
			break;
		case MEDIA_BUS_FMT_RGB888_1X24:
			dpi_cfg = 5;
			break;
		case MEDIA_BUS_FMT_RGB101010_1X30:
			dpi_cfg = 6;
			break;
		default:
			dpi_cfg = 5;
			break;
		}
		dc_write(hw, DC_DISPLAY_DPI_CONFIG, dpi_cfg);

		dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG, 0, BIT(4));

		dc_write(hw, DC_DISPLAY_H,
			 hw->display.h_active | (hw->display.h_total << 16));
		dc_write(hw, DC_DISPLAY_H_SYNC,
			 hw->display.h_sync_start |
				 (hw->display.h_sync_end << 15) |
				 (hw->display.h_sync_polarity ? 0 : BIT(31)) |
				 BIT(30));
		dc_write(hw, DC_DISPLAY_V,
			 hw->display.v_active | (hw->display.v_total << 16));
		dc_write(hw, DC_DISPLAY_V_SYNC,
			 hw->display.v_sync_start |
				 (hw->display.v_sync_end << 15) |
				 (hw->display.v_sync_polarity ? 0 : BIT(31)) |
				 BIT(30));

		if (hw->info->pipe_sync) {
			switch (display->sync_mode) {
			case ES_SINGLE_DC:
				dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX, 0,
					     BIT(3) | BIT(4));
				break;
			case ES_MULTI_DC_PRIMARY:
				dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX,
					     BIT(3) | BIT(4), 0);
				break;
			case ES_MULTI_DC_SECONDARY:
				dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX,
					     BIT(3), BIT(4));
				break;
			default:
				break;
			}
		}

		if (hw->display.dither_enable) {
			dc_write(hw, DC_DISPLAY_DITHER_TABLE_LOW,
				 DC_DISPLAY_DITHERTABLE_LOW);
			dc_write(hw, DC_DISPLAY_DITHER_TABLE_HIGH,
				 DC_DISPLAY_DITHERTABLE_HIGH);
			dc_write(hw, DC_DISPLAY_DITHER_CONFIG, BIT(31));
		} else {
			dc_write(hw, DC_DISPLAY_DITHER_CONFIG, 0);
		}

		if (hw->info->background)
			dc_write(hw, DC_FRAMEBUFFER_BG_COLOR,
				 hw->display.bg_color);

		dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG, BIT(4), 0);
	} else {
		dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG, 0, BIT(4) | BIT(0));
	}
}

static void setup_display_ex(struct dc_hw *hw, struct dc_hw_display *display)
{
	u32 dp_cfg;
	bool is_yuv = false;

	if (hw->display.enable && hw->out == OUT_DP) {
		switch (display->bus_format) {
		case MEDIA_BUS_FMT_RGB565_1X16:
			dp_cfg = 0;
			break;
		case MEDIA_BUS_FMT_RGB666_1X18:
			dp_cfg = 1;
			break;
		case MEDIA_BUS_FMT_RGB888_1X24:
			dp_cfg = 2;
			break;
		case MEDIA_BUS_FMT_RGB101010_1X30:
			dp_cfg = 3;
			break;
		case MEDIA_BUS_FMT_UYYVYY8_0_5X24:
			dp_cfg = 0;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_UYVY8_1X16:
			dp_cfg = 2 << 4;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_YUV8_1X24:
			dp_cfg = 4 << 4;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_UYYVYY10_0_5X30:
			dp_cfg = 6 << 4;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_UYVY10_1X20:
			dp_cfg = 8 << 4;
			is_yuv = true;
			break;
		case MEDIA_BUS_FMT_YUV10_1X30:
			dp_cfg = 10 << 4;
			is_yuv = true;
			break;
		default:
			dp_cfg = 2;
			break;
		}
		if (is_yuv)
			dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX, BIT(7), 0);
		else
			dc_set_clear(hw, DC_FRAMEBUFFER_CONFIG_EX, 0, BIT(7));
		dc_write(hw, DC_DISPLAY_DP_CONFIG, dp_cfg | BIT(3));
	}

	if (hw->out == OUT_DPI)
		dc_set_clear(hw, DC_DISPLAY_DP_CONFIG, 0, BIT(3));

	setup_display(hw, display);
}

static const struct dc_hw_funcs hw_func[] = {
	{
		.gamma = &gamma_commit,
		.plane = &plane_commit,
		.display = setup_display,
	},
	{
		.gamma = &gamma_commit,
		.plane = &plane_commit,
		.display = setup_display,
	},
	{
		.gamma = &gamma_ex_commit,
		.plane = &plane_ex_commit,
		.display = setup_display_ex,
	},
	{
		.gamma = &gamma_ex_commit,
		.plane = &plane_ex_commit,
		.display = setup_display_ex,
	},
};

void dc_hw_commit(struct dc_hw *hw)
{
	hw->func->gamma(hw);
	hw->func->plane(hw);

	if (hw->overlay.pos.dirty) {
		dc_write(hw, DC_OVERLAY_TOP_LEFT,
			 hw->overlay.pos.start_x |
				 (hw->overlay.pos.start_y << 15));
		dc_write(hw, DC_OVERLAY_BOTTOM_RIGHT,
			 hw->overlay.pos.end_x | (hw->overlay.pos.end_y << 15));
		hw->overlay.pos.dirty = false;
	}

	if (hw->overlay.blend.dirty) {
		dc_write(hw, DC_OVERLAY_SRC_GLOBAL_COLOR,
			 hw->overlay.blend.alpha << 24);
		dc_write(hw, DC_OVERLAY_DST_GLOBAL_COLOR,
			 hw->overlay.blend.alpha << 24);
		switch (hw->overlay.blend.blend_mode) {
		case BLEND_PREMULTI:
			dc_write(hw, DC_OVERLAY_BLEND_CONFIG, 0x3450);
			break;
		case BLEND_COVERAGE:
			dc_write(hw, DC_OVERLAY_BLEND_CONFIG, 0x3950);
			break;
		case BLEND_PIXEL_NONE:
			dc_write(hw, DC_OVERLAY_BLEND_CONFIG, 0x3548);
			break;
		default:
			break;
		}
		hw->overlay.blend.dirty = false;
	}

	if (hw->cursor.dirty) {
		if (hw->cursor.enable) {
			dc_write(hw, DC_CURSOR_ADDRESS, hw->cursor.address);
			dc_write(hw, DC_CURSOR_LOCATION,
				 hw->cursor.x | (hw->cursor.y << 16));
			dc_write(hw, DC_CURSOR_CONFIG,
				  (hw->cursor.hot_x << 16) |
				  (hw->cursor.hot_y << 8) |
				  (hw->cursor.size << 5) | 0x06);
		} else {
			dc_write(hw, DC_CURSOR_CONFIG, 0x00);
		}
		hw->cursor.dirty = false;
	}
}

#ifdef CONFIG_ESWIN_MMU
static u32 mmu_read(struct dc_hw *hw, u32 reg)
{
	return readl(hw->mmu_base + reg - MMU_REG_BASE);
}

static void mmu_write(struct dc_hw *hw, u32 reg, u32 value)
{
	writel(value, hw->mmu_base + reg - MMU_REG_BASE);
}

static void mmu_set_clear(struct dc_hw *hw, u32 reg, u32 set, u32 clear)
{
	u32 value = mmu_read(hw, reg);

	value &= ~clear;
	value |= set;
	mmu_write(hw, reg, value);
}

int dc_hw_mmu_init(struct dc_hw *hw, dc_mmu_pt mmu)
{
	u32 mtlb = 0, ext_mtlb = 0;
	u32 safe_addr = 0, ext_safe_addr = 0;
	u32 config = 0;

	mtlb = (u32)(mmu->mtlb_physical & 0xFFFFFFFF);
	ext_mtlb = (u32)(mmu->mtlb_physical >> 32);

	/* more than 40bit physical address */
	if (ext_mtlb & 0xFFFFFF00) {
		pr_err("Mtlb address out of range.\n");
		return -EFAULT;
	}

	config = (ext_mtlb << 20) | (mtlb >> 12);
	if (mmu->mode == MMU_MODE_1K)
		mmu_set_clear(hw, MMU_REG_CONTEXT_PD, (config << 4) | BIT(0),
			      (0xFFFFFFF << 4) | (0x03));
	else
		mmu_set_clear(hw, MMU_REG_CONTEXT_PD, (config << 4),
			      (0xFFFFFFF << 4) | (0x03));

	safe_addr = (u32)(mmu->safe_page_physical & 0xFFFFFFFF);
	ext_safe_addr = (u32)(mmu->safe_page_physical >> 32);

	if ((safe_addr & 0x3F) || (ext_safe_addr & 0xFFFFFF00)) {
		pr_err("Invalid safe_address.\n");
		return -EFAULT;
	}

	mmu_write(hw, MMU_REG_TABLE_ARRAY_SIZE, 1);
	mmu_write(hw, MMU_REG_SAFE_SECURE, safe_addr);
	mmu_write(hw, MMU_REG_SAFE_NON_SECURE, safe_addr);

	mmu_set_clear(hw, MMU_REG_SAFE_EXT_ADDRESS,
		      (ext_safe_addr << 16) | ext_safe_addr,
		      BIT(31) | (0xFF << 16) | BIT(15) | 0xFF);

	mmu_write(hw, MMU_REG_CONTROL, BIT(5) | BIT(0));

	mmu_write(hw, DEC_REG_CONTROL, DEC_REG_CONTROL_VALUE);

	return 0;
}

void dc_hw_enable_mmu_prefetch(struct dc_hw *hw, bool enable)
{
	if (enable)
		dc_write(hw, DC_MMU_PREFETCH, BIT(0));
	else
		dc_write(hw, DC_MMU_PREFETCH, 0);
}

void dc_mmu_flush(struct dc_hw *hw)
{
	u32 config, read;

	read = mmu_read(hw, MMU_REG_CONFIG);
	config = read | BIT(4);

	mmu_write(hw, MMU_REG_CONFIG, config);
	mmu_write(hw, MMU_REG_CONFIG, read);
}
#endif

void dc_hw_update_roi(struct dc_hw *hw, enum dc_hw_plane_id id,
		      struct dc_hw_roi *roi)
{
	struct dc_hw_plane *plane = NULL;

	if (id == PRIMARY_PLANE)
		plane = &hw->primary;
	else if (id == OVERLAY_PLANE)
		plane = &hw->overlay.plane;

	if (plane) {
		memcpy(&plane->roi, roi, sizeof(*roi) - sizeof(roi->dirty));
		plane->roi.dirty = true;
	}
}

void dc_hw_update_colorkey(struct dc_hw *hw, enum dc_hw_plane_id id,
			   struct dc_hw_colorkey *colorkey)
{
	struct dc_hw_plane *plane = NULL;

	if (id == PRIMARY_PLANE)
		plane = &hw->primary;
	else if (id == OVERLAY_PLANE)
		plane = &hw->overlay.plane;

	if (plane) {
		memcpy(&plane->colorkey, colorkey,
		       sizeof(*colorkey) - sizeof(colorkey->dirty));
		plane->colorkey.dirty = true;
	}
}
