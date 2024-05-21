#ifndef __ES_DRM_H__
#define __ES_DRM_H__

#include <drm/drm.h>

enum drm_es_degamma_mode {
	ES_DEGAMMA_DISABLE = 0,
	ES_DEGAMMA_BT709 = 1,
	ES_DEGAMMA_BT2020 = 2,
};

enum drm_es_sync_dc_mode {
	ES_SINGLE_DC = 0,
	ES_MULTI_DC_PRIMARY = 1,
	ES_MULTI_DC_SECONDARY = 2,
};

enum drm_es_mmu_prefetch_mode {
	ES_MMU_PREFETCH_DISABLE = 0,
	ES_MMU_PREFETCH_ENABLE = 1,
};

#endif /* __ES_DRM_H__ */