// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Eswin Holdings Co., Ltd.
 */

#include <linux/of_graph.h>
#include <linux/component.h>
#include <linux/iommu.h>
#include <linux/version.h>
#include <linux/of_address.h>
#include <linux/dma-map-ops.h>

#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_of.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_debugfs.h>
#include <drm/drm_vblank.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_atomic_helper.h>

#include "es_drv.h"
#include "es_fb.h"
#include "es_gem.h"
#include "es_plane.h"
#include "es_crtc.h"
#include "es_simple_enc.h"
#include "es_dc.h"
#include "es_virtual.h"
#ifdef CONFIG_ESWIN_DW_HDMI
#include "dw-hdmi.h"
#endif

#define DRV_NAME "es_drm"
#define DRV_DESC "Eswin DRM driver"
#define DRV_DATE "20191101"
#define DRV_MAJOR 1
#define DRV_MINOR 0

static bool has_iommu = true;
static struct platform_driver es_drm_platform_driver;

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.compat_ioctl = drm_compat_ioctl,
	.poll = drm_poll,
	.read = drm_read,
	.mmap = es_gem_mmap,
};

#ifdef CONFIG_DEBUG_FS
static int es_debugfs_planes_show(struct seq_file *s, void *data)
{
	struct drm_info_node *node = (struct drm_info_node *)s->private;
	struct drm_device *dev = node->minor->dev;
	struct drm_plane *plane = NULL;

	list_for_each_entry (plane, &dev->mode_config.plane_list, head) {
		struct drm_plane_state *state = plane->state;
		struct es_plane_state *plane_state = to_es_plane_state(state);

		seq_printf(s, "plane[%u]: %s\n", plane->base.id, plane->name);
		seq_printf(s, "\tcrtc = %s\n",
			   state->crtc ? state->crtc->name : "(null)");
		seq_printf(s, "\tcrtc id = %u\n",
			   state->crtc ? state->crtc->base.id : 0);
		seq_printf(s, "\tcrtc-pos = " DRM_RECT_FMT "\n",
			   DRM_RECT_ARG(&plane_state->status.dest));
		seq_printf(s, "\tsrc-pos = " DRM_RECT_FP_FMT "\n",
			   DRM_RECT_FP_ARG(&plane_state->status.src));
		seq_printf(s, "\tformat = %p4cc\n",
			   state->fb ? &state->fb->format->format : NULL);
		seq_printf(s, "\trotation = 0x%x\n", state->rotation);
		seq_printf(s, "\ttiling = %u\n", plane_state->status.tile_mode);

		seq_puts(s, "\n");
	}

	return 0;
}

static struct drm_info_list es_debugfs_list[] = {
	{ "planes", es_debugfs_planes_show, 0, NULL },
};

static void es_debugfs_init(struct drm_minor *minor)
{
	drm_debugfs_create_files(es_debugfs_list, ARRAY_SIZE(es_debugfs_list),
				 minor->debugfs_root, minor);
}
#endif

static struct drm_driver es_drm_driver = {
	.driver_features =
		DRIVER_MODESET | DRIVER_ATOMIC | DRIVER_GEM | DRIVER_SYNCOBJ,
	.lastclose = drm_fb_helper_lastclose,
	.gem_prime_import = es_gem_prime_import,
	.gem_prime_import_sg_table = es_gem_prime_import_sg_table,
	.dumb_create = es_gem_dumb_create,
#ifdef CONFIG_DEBUG_FS
	.debugfs_init = es_debugfs_init,
#endif
	.fops = &fops,
	.name = DRV_NAME,
	.desc = DRV_DESC,
	.date = DRV_DATE,
	.major = DRV_MAJOR,
	.minor = DRV_MINOR,
};

int es_drm_iommu_attach_device(struct drm_device *drm_dev, struct device *dev)
{
	struct es_drm_private *priv = drm_dev->dev_private;
	int ret;

	if (!has_iommu)
		return 0;

	if (!priv->domain) {
		priv->domain = iommu_get_domain_for_dev(dev);
		if (IS_ERR(priv->domain))
			return PTR_ERR(priv->domain);
		priv->dma_dev = dev;
	}

	ret = iommu_attach_device(priv->domain, dev);
	if (ret) {
		DRM_DEV_ERROR(dev, "Failed to attach iommu device\n");
		return ret;
	}

	return 0;
}

void es_drm_iommu_detach_device(struct drm_device *drm_dev, struct device *dev)
{
	struct es_drm_private *priv = drm_dev->dev_private;

	if (!has_iommu)
		return;

	iommu_detach_device(priv->domain, dev);

	if (priv->dma_dev == dev)
		priv->dma_dev = drm_dev->dev;
}

void es_drm_update_pitch_alignment(struct drm_device *drm_dev,
				   unsigned int alignment)
{
	struct es_drm_private *priv = drm_dev->dev_private;

	if (alignment > priv->pitch_alignment)
		priv->pitch_alignment = alignment;
}

#ifdef CONFIG_ESWIN_DW_HDMI
static int es_drm_create_properties(struct drm_device *dev)
{
	struct drm_property *prop;
	struct es_drm_private *private = dev->dev_private;

	prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC, "EOTF", 0,
					 5);
	if (!prop)
		return -ENOMEM;
	private->eotf_prop = prop;

	prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC,
					 "COLOR_SPACE", 0, 12);
	if (!prop)
		return -ENOMEM;
	private->color_space_prop = prop;

	prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC,
					 "GLOBAL_ALPHA", 0, 255);
	if (!prop)
		return -ENOMEM;
	private->global_alpha_prop = prop;

	prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC,
					 "BLEND_MODE", 0, 1);
	if (!prop)
		return -ENOMEM;
	private->blend_mode_prop = prop;

	prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC,
					 "ALPHA_SCALE", 0, 1);
	if (!prop)
		return -ENOMEM;
	private->alpha_scale_prop = prop;

	prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC,
					 "ASYNC_COMMIT", 0, 1);
	if (!prop)
		return -ENOMEM;
	private->async_commit_prop = prop;

	prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC, "SHARE_ID",
					 0, UINT_MAX);
	if (!prop)
		return -ENOMEM;
	private->share_id_prop = prop;

	prop = drm_property_create_range(dev, DRM_MODE_PROP_ATOMIC,
					 "CONNECTOR_ID", 0, 0xf);
	if (!prop)
		return -ENOMEM;
	private->connector_id_prop = prop;

	return drm_mode_create_tv_properties(dev, 0);
}
#endif

static int es_drm_bind(struct device *dev)
{
	struct drm_device *drm_dev;
	struct es_drm_private *priv;
	int ret;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
	static u64 dma_mask = DMA_BIT_MASK(40);
#else
	static u64 dma_mask = DMA_40BIT_MASK;
#endif

	drm_dev = drm_dev_alloc(&es_drm_driver, dev);
	if (IS_ERR(drm_dev))
		return PTR_ERR(drm_dev);

	dev_set_drvdata(dev, drm_dev);

	priv = devm_kzalloc(drm_dev->dev, sizeof(struct es_drm_private),
			    GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err_put_dev;
	}

	priv->pitch_alignment = 64;
	priv->dma_dev = drm_dev->dev;
	priv->dma_dev->coherent_dma_mask = dma_mask;
	dma_set_mask_and_coherent(priv->dma_dev, DMA_BIT_MASK(40));

	drm_dev->dev_private = priv;

	drm_mode_config_init(drm_dev);

#ifdef CONFIG_ESWIN_DW_HDMI
	es_drm_create_properties(drm_dev);
#endif

	/* Now try and bind all our sub-components */
	ret = component_bind_all(dev, drm_dev);
	if (ret)
		goto err_mode;

	es_mode_config_init(drm_dev);

	ret = drm_vblank_init(drm_dev, drm_dev->mode_config.num_crtc);
	if (ret)
		goto err_bind;

	drm_mode_config_reset(drm_dev);

	drm_kms_helper_poll_init(drm_dev);

	ret = drm_dev_register(drm_dev, 0);
	if (ret)
		goto err_helper;

	return 0;

err_helper:
	drm_kms_helper_poll_fini(drm_dev);
err_bind:
	component_unbind_all(drm_dev->dev, drm_dev);
err_mode:
	drm_mode_config_cleanup(drm_dev);
	if (priv->domain)
		iommu_domain_free(priv->domain);
err_put_dev:
	drm_dev->dev_private = NULL;
	dev_set_drvdata(dev, NULL);
	drm_dev_put(drm_dev);
	return ret;
}

static void es_drm_unbind(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
	struct es_drm_private *priv = drm_dev->dev_private;

	drm_dev_unregister(drm_dev);
	drm_kms_helper_poll_fini(drm_dev);
	drm_atomic_helper_shutdown(drm_dev);
	component_unbind_all(drm_dev->dev, drm_dev);
	drm_mode_config_cleanup(drm_dev);

	if (priv->domain) {
		iommu_domain_free(priv->domain);
		priv->domain = NULL;
	}

	drm_dev->dev_private = NULL;
	dev_set_drvdata(dev, NULL);
	drm_dev_put(drm_dev);
}

static const struct component_master_ops es_drm_ops = {
	.bind = es_drm_bind,
	.unbind = es_drm_unbind,
};

static struct platform_driver *drm_sub_drivers[] = {
	/* put display control driver at start */
	&dc_platform_driver,

/* connector */

/* bridge */
#if 1
#ifdef CONFIG_ESWIN_DW_HDMI
	&dw_hdmi_eswin_pltfm_driver,
#endif
#ifdef CONFIG_DW_HDMI_I2S_AUDIO
	&snd_dw_hdmi_driver,
#endif

#ifdef CONFIG_DW_HDMI_CEC
	&dw_hdmi_cec_driver,
#endif

#ifdef CONFIG_DW_HDMI_HDCP
	&dw_hdmi_hdcp_driver,
#endif
#endif

#ifdef CONFIG_ESWIN_VIRTUAL_DISPLAY
	&virtual_display_platform_driver,
#endif
};
#define NUM_DRM_DRIVERS                                                        \
	(sizeof(drm_sub_drivers) / sizeof(struct platform_driver *))

static int compare_dev(struct device *dev, void *data)
{
	return dev == (struct device *)data;
}

static struct component_match *es_drm_match_add(struct device *dev)
{
	struct component_match *match = NULL;
	int i;

	for (i = 0; i < NUM_DRM_DRIVERS; ++i) {
		struct platform_driver *drv = drm_sub_drivers[i];
		struct device *p = NULL, *d;

		while ((d = platform_find_device_by_driver(p, &drv->driver))) {
			put_device(p);

			component_match_add(dev, &match, compare_dev, d);
			p = d;
		}
		put_device(p);
	}

	return match ?: ERR_PTR(-ENODEV);
}

static int es_drm_platform_of_probe(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *port;
	bool found = false;
	int i;

	if (!np)
		return -ENODEV;

	for (i = 0;; i++) {
		struct device_node *iommu;

		port = of_parse_phandle(np, "ports", i);
		if (!port)
			break;

		if (!of_device_is_available(port->parent)) {
			of_node_put(port);
			continue;
		}

		iommu = of_parse_phandle(port->parent, "iommus", 0);

		/*
         * if there is a crtc not support iommu, force set all
         * crtc use non-iommu buffer.
         */
		if (!iommu || !of_device_is_available(iommu->parent))
			has_iommu = false;

		found = true;

		of_node_put(iommu);
		of_node_put(port);
	}

	if (i == 0) {
		DRM_DEV_ERROR(dev, "missing 'ports' property\n");
		return -ENODEV;
	}

	if (!found) {
		DRM_DEV_ERROR(dev, "No available DC found.\n");
		return -ENODEV;
	}

	return 0;
}

static int es_drm_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct component_match *match;
	int ret;

	ret = es_drm_platform_of_probe(dev);
	if (ret)
		return ret;

	match = es_drm_match_add(dev);
	if (IS_ERR(match))
		return PTR_ERR(match);

	return component_master_add_with_match(dev, &es_drm_ops, match);
}

static int es_drm_platform_remove(struct platform_device *pdev)
{
	component_master_del(&pdev->dev, &es_drm_ops);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int es_drm_suspend(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	return drm_mode_config_helper_suspend(drm);
}

static int es_drm_resume(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	return drm_mode_config_helper_resume(drm);
}
#endif

static SIMPLE_DEV_PM_OPS(es_drm_pm_ops, es_drm_suspend, es_drm_resume);

static const struct of_device_id es_drm_dt_ids[] = {

	{
		.compatible = "eswin,display-subsystem",
	},

	{ /* sentinel */ },

};

MODULE_DEVICE_TABLE(of, es_drm_dt_ids);

static struct platform_driver es_drm_platform_driver = {
    .probe = es_drm_platform_probe,
    .remove = es_drm_platform_remove,

    .driver = {
        .name = DRV_NAME,
        .of_match_table = es_drm_dt_ids,
        .pm = &es_drm_pm_ops,
    },
};

static int __init es_drm_init(void)
{
	int ret;

	ret = platform_register_drivers(drm_sub_drivers, NUM_DRM_DRIVERS);
	if (ret)
		return ret;

	ret = platform_driver_register(&es_drm_platform_driver);
	if (ret)
		platform_unregister_drivers(drm_sub_drivers, NUM_DRM_DRIVERS);

	return ret;
}

static void __exit es_drm_fini(void)
{
	platform_driver_unregister(&es_drm_platform_driver);
	platform_unregister_drivers(drm_sub_drivers, NUM_DRM_DRIVERS);
}

module_init(es_drm_init);
module_exit(es_drm_fini);

MODULE_DESCRIPTION("Eswin DRM Driver");
MODULE_LICENSE("GPL v2");
