#ifndef _ES_PANEL_H__
#define _ES_PANEL_H__

#include <drm/drm_mipi_dsi.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

enum {
	IOC_ES_MIPI_TX_SET_MODE = 0x100,
	IOC_ES_MIPI_TX_SET_INIT_CMD,
	IOC_ES_MIPI_TX_SET_CMD,
	IOC_ES_MIPI_TX_ENABLE,
	IOC_ES_MIPI_TX_DISABLE,
};

typedef enum MIPI_OUT_FORMAT_E {
	MIPI_OUT_FORMAT_RGB_16_BIT = 0x0,
	MIPI_OUT_FORMAT_RGB_18_BIT,
	MIPI_OUT_FORMAT_RGB_24_BIT,
	MIPI_OUT_FORMAT_BUTT
} MIPI_OUT_FORMAT_E;

typedef enum MIPI_OUT_MODE_E {
	MIPI_OUTPUT_MODE_CSI = 0x0,
	MIPI_OUTPUT_MODE_DSI_VIDEO,
	MIPI_OUTPUT_MODE_DSI_CMD,
	MIPI_OUTPUT_MODE_DSI_BUTT
} MIPI_OUT_MODE_E;

typedef struct MIPI_MODE_INFO {
	u32 devId;
	u16 hdisplay;
	u16 hsyncStart;
	u16 hsyncEnd;
	u16 htotal;
	u16 hskew;
	u16 vdisplay;
	u16 vsyncStart;
	u16 vsyncEnd;
	u16 vtotal;
	u16 vscan;

	u32 vrefresh;
	u32 flags;
	u32 type;

	MIPI_OUT_FORMAT_E outputFormat;
	MIPI_OUT_MODE_E outputMode;
	u32 lanes;
} MIPI_MODE_INFO_S;

typedef struct MIPI_CMD_S {
	int devId;
	u16 dataType;
	u16 cmdSize;
	u32 sleepUs;
	u8 *pCmd;
} MIPI_CMD_S;

extern struct mipi_dsi_driver es_panel_driver;
void es_panel_remove(struct mipi_dsi_device *dsi);
int es_panel_probe(struct mipi_dsi_device *dsi);

#endif //_ES_PANEL_H__