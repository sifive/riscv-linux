/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Eswin Holdings Co., Ltd.
 */

#ifndef __ES_VIRTUAL_H_
#define __ES_VIRTUAL_H_

struct es_virtual_display {
	struct drm_encoder encoder;
	struct drm_connector connector;
	struct device *dc;
	u32 bus_format;

	struct dentry *dump_debugfs;
	struct debugfs_blob_wrapper dump_blob;
	struct es_gem_object *dump_obj;
	unsigned int pitch;
};

static inline struct es_virtual_display *
to_virtual_display_with_connector(struct drm_connector *connector)
{
	return container_of(connector, struct es_virtual_display, connector);
}

static inline struct es_virtual_display *
to_virtual_display_with_encoder(struct drm_encoder *encoder)
{
	return container_of(encoder, struct es_virtual_display, encoder);
}

extern struct platform_driver virtual_display_platform_driver;
#endif /* __ES_VIRTUAL_H_ */
