/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Eswin Holdings Co., Ltd.
 */

#ifndef __ES_FB_H__
#define __ES_FB_H__

struct es_gem_object *es_fb_get_gem_obj(struct drm_framebuffer *fb,
					unsigned char plane);

void es_mode_config_init(struct drm_device *dev);
#endif /* __ES_FB_H__ */
