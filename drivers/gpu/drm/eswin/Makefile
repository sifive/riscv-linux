# SPDX-License-Identifier: GPL-2.0

es_drm-objs := es_dc_hw.o \
		es_dc.o \
		es_crtc.o \
		es_drv.o \
		es_fb.o \
		es_gem.o \
		es_plane.o \
		es_simple_enc.o

es_drm-$(CONFIG_ESWIN_VIRTUAL_DISPLAY) += es_virtual.o
es_drm-$(CONFIG_ESWIN_MMU) += es_dc_mmu.o
es_drm-$(CONFIG_ESWIN_DW_HDMI) += eswin_dw_hdmi.o dw_hdmi.o

es_drm-$(CONFIG_DW_HDMI_I2S_AUDIO) += dw_hdmi_i2s_audio.o
es_drm-$(CONFIG_DW_HDMI_CEC) += dw_hdmi_cec.o
es_drm-$(CONFIG_DW_HDMI_HDCP) += dw_hdmi_hdcp.o

dw_hdcp2-$(CONFIG_DW_HDMI_HDCP2) += dw_hdmi_hdcp2.o

obj-$(CONFIG_DRM_ESWIN) += es_drm.o
obj-$(CONFIG_DW_HDMI_HDCP2) += dw_hdcp2.o