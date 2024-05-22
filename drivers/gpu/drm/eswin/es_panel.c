#include "es_panel.h"
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/backlight.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/media-bus-format.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/gpio/consumer.h>
#include <drm/drm_print.h>
#include <linux/version.h>

#define ES_PANEL_MAJOR 0
#define ES_PANEL_NR_DEVS 1
#define ES_PANEL_NAME "es_panel"

static int panel_major = ES_PANEL_MAJOR;
static int panel_minor = 0;
static int panel_nr_devs = ES_PANEL_NR_DEVS;
static struct device *pr_dev = NULL;

typedef struct user_cmd_buffer_s {
	struct list_head head;
	u32 sleepUs;
	int buf_size;
	u8 *buf;
} user_cmd_buffer_t;

struct es_panel {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;
	bool prepared;
	bool enabled;
	int error;

	// chr dev:
	struct cdev cdev;
	dev_t devt;
	struct class *es_panel_class;

	// user data:
	MIPI_MODE_INFO_S mode_info;
	struct drm_display_mode user_mode;
	bool user_mode_inited;

	struct list_head init_cmd_list;
	int init_cmd_writted;
	user_cmd_buffer_t enable_cmd_buf;
	user_cmd_buffer_t disable_cmd_buf;

	struct gpio_desc *gpio_backlight0;
	struct gpio_desc *gpio_reset;
};

static void es_panel_dcs_write(struct es_panel *ctx, const void *data,
			       size_t len);

#define es_panel_dcs_write_seq_static(ctx, seq...)         \
	({                                                 \
		static const u8 d[] = { seq };             \
		es_panel_dcs_write(ctx, d, ARRAY_SIZE(d)); \
		udelay(10);                                \
	})

#define es_panel_check_retval(x)     \
	do {                         \
		if ((x))             \
			return -EIO; \
	} while (0)

// #ifdef CONFIG_1080P_4LANES_PANEL
#define HFP (60)
#define HSA (60)
#define HBP (60)
#define VFP (8)
#define VSA (8)
#define VBP (8)
#define VAC (1920)
#define HAC (1080)
#define LANES 4
#define PIX_CLK 148500
// #elif CONFIG_720P_3LANES_PANEL
// #define HFP (30)
// #define HSA (30)
// #define HBP (30)
// #define VFP (30)
// #define VSA (30)
// #define VBP (30)
// #define VAC (1280)
// #define HAC (720)
// #define LANES 3
// #define PIX_CLK 59400
// #else
// #define HFP (60)
// #define HSA (60)
// #define HBP (60)
// #define VFP (8)
// #define VSA (8)
// #define VBP (8)
// #define VAC (1920)
// #define HAC (1080)
// #define LANES 4
// #define PIX_CLK 148500
// #endif

static struct drm_display_mode default_mode = {
	.clock = PIX_CLK,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	.flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
};

void GP_COMMAD_PA(struct es_panel *ctx, u8 a)
{
	return;
	// u8 b[3] = {0};
	// b[0] = 0xBC;
	// b[1] = a;
	// b[2] = a >> 8;
	// es_panel_dcs_write(ctx, b, 3);
	// es_panel_dcs_write_seq_static(ctx, 0xBf);
}

static void free_cmd_buf(user_cmd_buffer_t *pcmd_buf)
{
	if (pcmd_buf->buf) {
		pcmd_buf->buf_size = 0;
		vfree(pcmd_buf->buf);
		pcmd_buf->buf = NULL;
	}
	return;
}

void free_cmd_list(struct list_head *list)
{
	if (!list_empty(list)) {
		user_cmd_buffer_t *pos = NULL, *n = NULL;
		list_for_each_entry_safe(pos, n, list, head) {
			if (pos) {
				list_del_init(&pos->head);
				if (pos->buf) {
					vfree(pos->buf);
					pos->buf = NULL;
				}
				vfree(pos);
				pos = NULL;
			}
		}
	}
}

static int parser_and_creat_cmd_buf(MIPI_CMD_S *cmd, user_cmd_buffer_t *cmd_buf)
{
	if (!cmd) {
		dev_err(pr_dev, "cmd invalid\n");
		return -EPERM;
	}
	if (!cmd_buf) {
		dev_err(pr_dev, "cmd_buf invalid\n");
		return -EPERM;
	}

	if (cmd_buf->buf) {
		vfree(cmd_buf->buf);
		cmd_buf->buf = NULL;
	}

	if (cmd->dataType == 0x13) {
		cmd_buf->buf_size = 1;
		cmd_buf->buf = vmalloc(cmd_buf->buf_size);
		cmd_buf->buf[0] = cmd->cmdSize;
	} else if (cmd->dataType == 0x23) {
		cmd_buf->buf_size = 2;
		cmd_buf->buf = vmalloc(cmd_buf->buf_size);
		cmd_buf->buf[0] = cmd->cmdSize >> 8;
		cmd_buf->buf[1] = cmd->cmdSize & 0x00ff;
	} else if (cmd->dataType == 0x29) {
		cmd_buf->buf_size = cmd->cmdSize;
		cmd_buf->buf = vmalloc(cmd_buf->buf_size);
		if (copy_from_user(cmd_buf->buf, (u8 __user *)cmd->pCmd,
				   cmd_buf->buf_size)) {
			dev_err(pr_dev, "cmd buffer copy_from_user failed\n");
			return -ENOMEM;
		}
	}
	cmd_buf->sleepUs = cmd->sleepUs;
	dev_dbg(pr_dev, "user set cmd size:%d, sleep:%dus,cmd:0x%x \n",
		cmd_buf->buf_size, cmd_buf->sleepUs, cmd_buf->buf[0]);
	return 0;
}

int es_panel_fops_open(struct inode *inode, struct file *filp)
{
	struct es_panel *ctx;
	ctx = container_of(inode->i_cdev, struct es_panel, cdev);
	filp->private_data = ctx;
	return 0;
}

int es_panel_fops_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static long es_panel_fops_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	struct es_panel *ctx;
	struct mipi_dsi_device *dsi;
	MIPI_CMD_S ucmd;
	int ret = 0;

	ctx = (struct es_panel *)file->private_data;
	dsi = to_mipi_dsi_device(ctx->dev);

	switch (cmd) {
	case IOC_ES_MIPI_TX_SET_MODE:
		es_panel_check_retval(copy_from_user(
			&ctx->mode_info, (MIPI_MODE_INFO_S __user *)arg,
			sizeof(MIPI_MODE_INFO_S)));
		dev_dbg(pr_dev,
			"cmd: IOC_ES_MIPI_TX_SET_MODE, devId:%d, hdisplay:%u, hsyncStart:%u, hsyncEnd:%u, htotal:%u, hskew:%u\n"
			"vdisplay:%u vsyncStart:%u vsyncEnd:%u vtotal:%u vscan:%u vrefresh:%d flags:%d type:%d "
			"outputFormat:%d outputMode:%d lanes:%d\n",
			ctx->mode_info.devId, ctx->mode_info.hdisplay,
			ctx->mode_info.hsyncStart, ctx->mode_info.hsyncEnd,
			ctx->mode_info.htotal, ctx->mode_info.hskew,
			ctx->mode_info.vdisplay, ctx->mode_info.vsyncStart,
			ctx->mode_info.vsyncEnd, ctx->mode_info.vtotal,
			ctx->mode_info.vscan, ctx->mode_info.vrefresh,
			ctx->mode_info.flags, ctx->mode_info.type,
			ctx->mode_info.outputFormat, ctx->mode_info.outputMode,
			ctx->mode_info.lanes);

		memset(&ctx->user_mode, 0, sizeof(struct drm_display_mode));
		ctx->user_mode.clock = 12500;
		ctx->user_mode.hdisplay = ctx->mode_info.hdisplay;
		ctx->user_mode.hsync_start = ctx->mode_info.hsyncStart;
		ctx->user_mode.hsync_end = ctx->mode_info.hsyncEnd;
		ctx->user_mode.htotal = ctx->mode_info.htotal;
		ctx->user_mode.vdisplay = ctx->mode_info.vdisplay;
		ctx->user_mode.vsync_start = ctx->mode_info.vsyncStart;
		ctx->user_mode.vsync_end = ctx->mode_info.vsyncEnd;
		ctx->user_mode.vtotal = ctx->mode_info.vtotal;
		ctx->user_mode.width_mm = 36;
		ctx->user_mode.height_mm = 49;
		ctx->user_mode.flags = DRM_MODE_FLAG_NVSYNC |
				       DRM_MODE_FLAG_NHSYNC;
		ctx->user_mode_inited = true;

		dsi->lanes = ctx->mode_info.lanes;
		if (ctx->mode_info.outputFormat == MIPI_OUT_FORMAT_RGB_24_BIT) {
			dsi->format = MIPI_DSI_FMT_RGB888;
		} else {
			dev_err(pr_dev, "panel not support fmt:%d now",
				ctx->mode_info.outputFormat);
		}
		dsi->mode_flags = MIPI_DSI_MODE_VIDEO;

		break;
	case IOC_ES_MIPI_TX_SET_INIT_CMD:
		user_cmd_buffer_t *cmd_buf = NULL;

		if (ctx->init_cmd_writted) {
			dev_info(pr_dev,
				 "init_cmd_writted : clean init cmd list\n");
			free_cmd_list(&ctx->init_cmd_list);
			INIT_LIST_HEAD(&ctx->init_cmd_list);
			ctx->init_cmd_writted = 0;
		}
		cmd_buf = vmalloc(sizeof(user_cmd_buffer_t));
		if (!cmd_buf) {
			dev_err(pr_dev, "cmd buffer alloc failed\n");
			return -ENOMEM;
		}
		memset(cmd_buf, 0, sizeof(user_cmd_buffer_t));

		es_panel_check_retval(copy_from_user(
			&ucmd, (MIPI_CMD_S __user *)arg, sizeof(MIPI_CMD_S)));
		ret = parser_and_creat_cmd_buf(&ucmd, cmd_buf);
		if (ret) {
			break;
		}
		if (!cmd_buf) {
			dev_err(pr_dev, "cmd_buf creat failed!!!!");
			break;
		}
		list_add_tail(&cmd_buf->head, &ctx->init_cmd_list);

		break;
	case IOC_ES_MIPI_TX_ENABLE:
		user_cmd_buffer_t *enable_cmd_buf = &ctx->enable_cmd_buf;
		dev_dbg(pr_dev, "IOC_ES_MIPI_TX_ENABLE In");
		es_panel_check_retval(copy_from_user(
			&ucmd, (MIPI_CMD_S __user *)arg, sizeof(MIPI_CMD_S)));
		ret = parser_and_creat_cmd_buf(&ucmd, enable_cmd_buf);
		break;
	case IOC_ES_MIPI_TX_DISABLE:
		user_cmd_buffer_t *disable_cmd_buf = &ctx->disable_cmd_buf;
		dev_dbg(pr_dev, "IOC_ES_MIPI_TX_DISABLE In");
		es_panel_check_retval(copy_from_user(
			&ucmd, (MIPI_CMD_S __user *)arg, sizeof(MIPI_CMD_S)));
		ret = parser_and_creat_cmd_buf(&ucmd, disable_cmd_buf);
		break;
	default:
		dev_err(pr_dev, "unsupported command %d", cmd);
		break;
	}
	return ret;
}

struct file_operations es_panel_fops = {
	.owner = THIS_MODULE,
	.open = es_panel_fops_open,
	.release = es_panel_fops_release,
	.unlocked_ioctl = es_panel_fops_ioctl,
};

int es_panel_chrdev_create(struct es_panel *ctx)
{
	int ret = 0;
	dev_dbg(pr_dev, "---BEGIN es_panel_chrdev_create---\n");

	ret = alloc_chrdev_region(&ctx->devt, panel_minor, panel_nr_devs,
				  ES_PANEL_NAME);
	if (ret < 0) {
		dev_err(pr_dev,
			"es-panel-chrdev: can't allocate major number\n");
		goto fail;
	}
	panel_major = MAJOR(ctx->devt);

	cdev_init(&ctx->cdev, &es_panel_fops);
	ctx->cdev.owner = THIS_MODULE;

	ret = cdev_add(&ctx->cdev, ctx->devt, 1);
	if (ret) {
		dev_err(pr_dev, "failed to add cdev\n");
		goto unregister_chrdev;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
	ctx->es_panel_class = class_create(ES_PANEL_NAME);
#else
	ctx->es_panel_class = class_create(THIS_MODULE, ES_PANEL_NAME);
#endif
	if (IS_ERR(ctx->es_panel_class)) {
		dev_err(pr_dev, "class_create error\n");
		ret = PTR_ERR(ctx->es_panel_class);
		goto del_cdev;
	}

	if (device_create(ctx->es_panel_class, NULL, ctx->devt, ctx,
			  ES_PANEL_NAME) == NULL) {
		dev_err(pr_dev, "device_create error\n");
		ret = -EINVAL;
		goto destroy_class;
	}

	dev_dbg(pr_dev, "---END es_panel_chrdev_create---\n");
	return 0;

destroy_class:
	class_destroy(ctx->es_panel_class);
del_cdev:
	cdev_del(&ctx->cdev);
unregister_chrdev:
	unregister_chrdev_region(ctx->devt, panel_nr_devs);
fail:
	return ret;
}

void es_panel_chrdev_destroy(struct es_panel *ctx)
{
	device_destroy(ctx->es_panel_class, MKDEV(panel_major, panel_minor));
	class_destroy(ctx->es_panel_class);

	cdev_del(&ctx->cdev);

	unregister_chrdev_region(ctx->devt, panel_nr_devs);
	dev_dbg(pr_dev, "es_panel_chrdev_destroy done\n");
}

static inline struct es_panel *panel_to_es_panel(struct drm_panel *panel)
{
	return container_of(panel, struct es_panel, panel);
}

static void es_panel_dcs_write(struct es_panel *ctx, const void *data,
			       size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
	dev_dbg(ctx->dev, "write cmd size:%ld, data0[0x%x]  ret:%d\n", len,
		addr[0], ctx->error);
}

#ifdef PANEL_SUPPORT_READBACK
static int es_panel_dcs_read(struct es_panel *ctx, u8 cmd, void *data,
			     size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void es_panel_panel_get_data(struct es_panel *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	if (ret == 0) {
		ret = es_panel_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

void es_panel_panel_init(struct es_panel *ctx)
{
	user_cmd_buffer_t *pos = NULL;
	list_for_each_entry(pos, &ctx->init_cmd_list, head) {
		if (pos) {
			es_panel_dcs_write(ctx, pos->buf, pos->buf_size);
			if (pos->sleepUs) {
				msleep(pos->sleepUs / 1000);
			}
		}
	}
	ctx->init_cmd_writted = 1;
}

void es_panel_3_lanes_panel_init(struct es_panel *ctx)
{
	dev_info(ctx->dev, "init 3lanes panel\n");
	GP_COMMAD_PA(ctx, 0x04);
	es_panel_dcs_write_seq_static(ctx, 0xB9, 0xF1, 0x12, 0x83);
	GP_COMMAD_PA(ctx, 0x1C);
	es_panel_dcs_write_seq_static(ctx, 0xBA, 0x32, 0x81, 0x05, 0xF9, 0x0E,
				      0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				      0x00, 0x00, 0x44, 0x25, 0x00, 0x91, 0x0A,
				      0x00, 0x00, 0x02, 0x4F, 0xD1, 0x00, 0x00,
				      0x37);
	GP_COMMAD_PA(ctx, 0x05);
	es_panel_dcs_write_seq_static(ctx, 0xB8, 0x26, 0x22, 0x20, 0x00);
	GP_COMMAD_PA(ctx, 0x04);
	es_panel_dcs_write_seq_static(ctx, 0xBF, 0x02, 0x11, 0x00);
	GP_COMMAD_PA(ctx, 0x0B);
	es_panel_dcs_write_seq_static(ctx, 0xB3, 0x0C, 0x10, 0x0A, 0x50, 0x03,
				      0xFF, 0x00, 0x00, 0x00, 0x00);
	GP_COMMAD_PA(ctx, 0x0A);
	es_panel_dcs_write_seq_static(ctx, 0xB3, 0x0C, 0x10, 0x0A, 0x50, 0x03,
				      0xFF, 0x00, 0x00, 0x00, 0x00);
	GP_COMMAD_PA(ctx, 0x02);
	es_panel_dcs_write_seq_static(ctx, 0xBC, 0x46);
	GP_COMMAD_PA(ctx, 0x02);
	es_panel_dcs_write_seq_static(ctx, 0xCC, 0x0B);
	GP_COMMAD_PA(ctx, 0x02);
	es_panel_dcs_write_seq_static(ctx, 0xB4, 0x80);
	GP_COMMAD_PA(ctx, 0x04);
	es_panel_dcs_write_seq_static(ctx, 0xB2, 0xC8, 0x02, 0x30);
	GP_COMMAD_PA(ctx, 0x0F);
	es_panel_dcs_write_seq_static(ctx, 0xE3, 0x07, 0x07, 0x0B, 0x0B, 0x03,
				      0x0B, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x80,
				      0xC0, 0x10);
	GP_COMMAD_PA(ctx, 0x0D);
	es_panel_dcs_write_seq_static(ctx, 0xC1, 0x25, 0x00, 0x1E, 0x1E, 0x77,
				      0xF1, 0xFF, 0xFF, 0xCC, 0xCC, 0x77, 0x77);
	GP_COMMAD_PA(ctx, 0x03);
	es_panel_dcs_write_seq_static(ctx, 0xB5, 0x0A, 0x0A);
	GP_COMMAD_PA(ctx, 0x03);
	es_panel_dcs_write_seq_static(ctx, 0xB6, 0x50, 0x50);
	GP_COMMAD_PA(ctx, 0x40);
	es_panel_dcs_write_seq_static(
		ctx, 0xE9, 0xC4, 0x10, 0x0F, 0x00, 0x00, 0xB2, 0xB8, 0x12, 0x31,
		0x23, 0x48, 0x8B, 0xB2, 0xB8, 0x47, 0x20, 0x00, 0x00, 0x30,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x02,
		0x46, 0x02, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0xF8,
		0x13, 0x57, 0x13, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
		0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00);
	GP_COMMAD_PA(ctx, 0x3E);
	es_panel_dcs_write_seq_static(ctx, 0xEA, 0x00, 0x1A, 0x00, 0x00, 0x00,
				      0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
				      0x00, 0x75, 0x31, 0x31, 0x88, 0x88, 0x88,
				      0x88, 0x88, 0x88, 0x88, 0x8F, 0x64, 0x20,
				      0x20, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
				      0x88, 0x8F, 0x23, 0x10, 0x00, 0x00, 0x02,
				      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				      0x30, 0x02, 0xA0, 0x00, 0x00, 0x00, 0x00);
	GP_COMMAD_PA(ctx, 0x23);
	es_panel_dcs_write_seq_static(ctx, 0xE0, 0x01, 0x11, 0x16, 0x28, 0x30,
				      0x38, 0x45, 0x39, 0x07, 0x0B, 0x0D, 0x12,
				      0x14, 0x12, 0x14, 0x11, 0x19, 0x01, 0x11,
				      0x16, 0x28, 0x30, 0x38, 0x45, 0x39, 0x07,
				      0x0B, 0x0D, 0x12, 0x14, 0x12, 0x14, 0x11,
				      0x19);

	es_panel_dcs_write_seq_static(ctx, 0xC7, 0xE7, 0x00);
	// es_panel_dcs_write_seq_static(ctx,0xC8, 0x10, 0x40);
	es_panel_dcs_write_seq_static(ctx, 0x51,
				      0xFF); // brightness [0x00-> 0xFF]
	es_panel_dcs_write_seq_static(ctx, 0x53, (1 << 2) | (1 << 5));
	msleep(20);
}

void es_panel_4_lanes_panel_init(struct es_panel *ctx)
{
	dev_info(ctx->dev, "init 4lanes panel\n");
	es_panel_dcs_write_seq_static(ctx, 0xB0, 0x98, 0x85, 0x0A);
	es_panel_dcs_write_seq_static(ctx, 0xC1, 0x00, 0x00,
				      0x00); // 01反扫  00正扫
	es_panel_dcs_write_seq_static(ctx, 0xC4, 0x70, 0x19, 0x23, 0x00, 0x0F,
				      0x0F, 0x00);
	es_panel_dcs_write_seq_static(
		ctx, 0xD0, 0x33, 0x05, 0x21, 0xE7, 0x62, 0x00, 0x88, 0xA0, 0xA0,
		0x03, 0x02, 0x80, 0xD0, 0x1B,
		0x10); // VGH dual mode,      VGL single mode,      VGH=12V,      VGL=-12V
	es_panel_dcs_write_seq_static(ctx, 0xD2, 0x13, 0x13, 0xEA, 0x22);
	es_panel_dcs_write_seq_static(ctx, 0xD1, 0x09, 0x09,
				      0xc2); ////4003 & 4003B EN
	es_panel_dcs_write_seq_static(ctx, 0xD3, 0x44, 0x33, 0x05, 0x03, 0x4A,
				      0x4A, 0x33, 0x17, 0x22, 0x43,
				      0x6E); // set GVDDP=4.1V, GVDDN=-4.1V,
		// VGHO=12V,      VGLO=-11V
	es_panel_dcs_write_seq_static(ctx, 0xD5, 0x8B, 0x00, 0x00, 0x00, 0x01,
				      0x7D, 0x01, 0x7D, 0x01, 0x7D, 0x00, 0x00,
				      0x00, 0x00); // set VCOM
	es_panel_dcs_write_seq_static(ctx, 0xD6, 0x00, 0x00, 0x08, 0x17, 0x23,
				      0x65, 0x77, 0x44, 0x87, 0x00, 0x00,
				      0x09); // P12_D[3] for sleep in
		// current reduce setting
	es_panel_dcs_write_seq_static(ctx, 0xEC, 0x76, 0x1E, 0x32, 0x00, 0x46,
				      0x00, 0x00);
	es_panel_dcs_write_seq_static(
		ctx, 0xC7, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x2D, 0x00, 0x43, 0x00,
		0x58, 0x00, 0x6C, 0x00, 0x75, 0x00, 0x8C, 0x00, 0x9A, 0x00,
		0xCA, 0x00, 0xF1, 0x01, 0x2F, 0x01, 0x5F, 0x01, 0xAE, 0x01,
		0xEC, 0x01, 0xEE, 0x02, 0x25, 0x02, 0x62, 0x02, 0x8A, 0x02,
		0xC4, 0x02, 0xEA, 0x03, 0x1F, 0x03, 0x33, 0x03, 0x3E, 0x03,
		0x59, 0x03, 0x70, 0x03, 0x88, 0x03, 0xB4, 0x03, 0xC8, 0x03,
		0xE8, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x2D, 0x00, 0x43, 0x00,
		0x58, 0x00, 0x6C, 0x00, 0x75, 0x00, 0x8C, 0x00, 0x9A, 0x00,
		0xCA, 0x00, 0xF1, 0x01, 0x2F, 0x01, 0x5F, 0x01, 0xAE, 0x01,
		0xEC, 0x01, 0xEE, 0x02, 0x25, 0x02, 0x62, 0x02, 0x8A, 0x02,
		0xC4, 0x02, 0xEA, 0x03, 0x1F, 0x03, 0x33, 0x03, 0x3E, 0x03,
		0x59, 0x03, 0x70, 0x03, 0x88, 0x03, 0xB4, 0x03, 0xC8, 0x03,
		0xE8, 0x01, 0x01);
	es_panel_dcs_write_seq_static(
		ctx, 0xE5, 0x6D, 0x92, 0x80, 0x34, 0x76, 0x12, 0x12, 0x00, 0x00,
		0x36, 0x36, 0x24, 0x24, 0x26, 0xF6, 0xF6, 0xF6, 0xF6, 0xF6,
		0xF6, 0x6D, 0x9B, 0x89, 0x34, 0x76, 0x1B, 0x1B, 0x09, 0x09,
		0x3F, 0x3F, 0x2D, 0x2D, 0x26, 0xF6, 0xF6, 0xF6, 0xF6, 0xF6,
		0xF6, 0x04, 0x40, 0x90, 0x00, 0xD6, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x01, 0x44, 0x00, 0xD6, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x03,
		0x03, 0x03, 0x00, 0x07);
	es_panel_dcs_write_seq_static(
		ctx, 0xEA, 0x70, 0x00, 0x03, 0x03, 0x00, 0x00, 0x00, 0x40, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x89,
		0x8a, 0x05, 0x05, 0x22, 0x0a, 0x0a, 0x0b, 0x00, 0x08, 0x00,
		0x30, 0x01, 0x09, 0x00, 0x40, 0x80, 0xc0, 0x00, 0x00, 0x01,
		0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xDD, 0x22, 0x22,
		0xCC, 0xDD, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xCC, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33);
	es_panel_dcs_write_seq_static(ctx, 0xEE, 0x22, 0x10, 0x02, 0x02, 0x0F,
				      0x40, 0x00, 0x07, 0x00, 0x04, 0x00, 0x00,
				      0x00, 0xB9, 0x77, 0x00, 0x55, 0x05,
				      0x00); // 4003
	es_panel_dcs_write_seq_static(ctx, 0xEB, 0xa3, 0xcf, 0x73, 0x18, 0x54,
				      0x55, 0x55, 0x55, 0x55, 0x00, 0x55, 0x55,
				      0x55, 0x55, 0x55, 0x25, 0x0D, 0x0F, 0x00,
				      0x00, 0x00, 0x00, 0x00, 0x55, 0x55, 0x55,
				      0x55, 0x32, 0x3E, 0x55, 0x43, 0x55);

	es_panel_dcs_write_seq_static(ctx, 0x51, 0x0F,
				      0xFF); // brightness [0x00-> 0xFFF]
	es_panel_dcs_write_seq_static(ctx, 0x53, (1 << 2) | (1 << 5));
}

static int es_panel_disable(struct drm_panel *panel)
{
	struct es_panel *ctx = panel_to_es_panel(panel);
	int ret = 0;
	user_cmd_buffer_t *cmd_buf = &ctx->disable_cmd_buf;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(panel->dev);

	dev_dbg(pr_dev, "%s:%d size:%x ctx->enabled:%d\n", __func__, __LINE__,
		ctx->disable_cmd_buf.buf_size, ctx->enabled);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}
#if 1
	gpiod_set_value(ctx->gpio_backlight0, 0);
#endif

	ctx->enabled = false;
	if (cmd_buf->buf_size > 0) {
		if (cmd_buf->sleepUs) {
			msleep(cmd_buf->sleepUs / 1000);
		}
		es_panel_dcs_write(ctx, cmd_buf->buf, cmd_buf->buf_size);
	} else {
		dev_warn(pr_dev, "%s[%d]:Disable cmd invalid,Please set it\n",
			 __func__, __LINE__);
	}

	dev_info(pr_dev, "[%s]display off... sleep in..\n", __func__);
	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0)
		dev_err(ctx->dev, "Failed to turn off the display: %d\n", ret);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0)
		dev_err(ctx->dev, "Failed to enter sleep mode: %d\n", ret);

	return ret;
}

static int es_panel_unprepare(struct drm_panel *panel)
{
	struct es_panel *ctx = panel_to_es_panel(panel);

	if (!ctx->prepared)
		return 0;

	ctx->error = 0;
	dev_dbg(pr_dev, "%s:%d\n", __func__, __LINE__);
	ctx->prepared = false;
	return 0;
}

static int es_panel_prepare(struct drm_panel *panel)
{
	struct es_panel *ctx = panel_to_es_panel(panel);
	int ret;

	if (ctx->prepared)
		return 0;
	dev_dbg(pr_dev, "%s:%d\n", __func__, __LINE__);

	// #ifdef CONFIG_1080P_4LANES_PANEL
	es_panel_4_lanes_panel_init(ctx);
	// #elif CONFIG_720P_3LANES_PANEL
	//     es_panel_3_lanes_panel_init(ctx);
	// #else
	//     es_panel_panel_init(ctx);
	// #endif

	ret = ctx->error;
	if (ret < 0)
		es_panel_unprepare(panel);
	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	es_panel_panel_get_data(ctx);
#endif

	return ret;
}

static int es_panel_enable(struct drm_panel *panel)
{
	struct es_panel *ctx = panel_to_es_panel(panel);
	int ret = 0;
	user_cmd_buffer_t *cmd_buf = &ctx->enable_cmd_buf;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(panel->dev);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}
#if 1
	gpiod_set_value(ctx->gpio_backlight0, 1);
#endif

	ctx->enabled = true;
	if (cmd_buf->buf_size > 0) {
		if (cmd_buf->sleepUs) {
			msleep(cmd_buf->sleepUs / 1000);
		}
		es_panel_dcs_write(ctx, cmd_buf->buf, cmd_buf->buf_size);
	} else {
		dev_warn(pr_dev, "%s[%d]:Enable cmd invalid,Please set it\n",
			 __func__, __LINE__);
	}

	dev_info(pr_dev, "[%s] sleep out.., display on...\n", __func__);
	msleep(20);
	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(ctx->dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	/* Panel is operational 120 msec after reset */
	msleep(200);
	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret)
		return ret;
	msleep(50);

	return ret;
}

static int es_panel_get_modes(struct drm_panel *panel,
			      struct drm_connector *connector)
{
	u32 bus_format = MEDIA_BUS_FMT_RGB888_1X24;
	struct es_panel *ctx = panel_to_es_panel(panel);
	struct drm_display_mode *mode;

	dev_dbg(pr_dev, "%s:%d\n", __func__, __LINE__);
	if (ctx->user_mode_inited == false) {
		mode = drm_mode_duplicate(connector->dev, &default_mode);
		if (!mode) {
			dev_err(panel->dev, "failed to add mode %ux%ux\n",
				default_mode.hdisplay, default_mode.vdisplay);
			return -ENOMEM;
		}
	} else {
		mode = drm_mode_duplicate(connector->dev, &ctx->user_mode);
		if (!mode) {
			dev_err(panel->dev, "failed to add mode %ux%ux\n",
				ctx->user_mode.hdisplay,
				ctx->user_mode.vdisplay);
			return -ENOMEM;
		}
	}
	dev_info(
		pr_dev,
		"get modes user_mode_inited:%d, clock:%d, hdisplay:%u, hsyncStart:%u, hsyncEnd:%u, htotal:%u\n"
		"vdisplay:%u vsyncStart:%u vsyncEnd:%u vtotal:%u width_mm:%d, height_mm:%d flags:0x%08x \n",
		ctx->user_mode_inited, mode->clock, mode->hdisplay,
		mode->hsync_start, mode->hsync_end, mode->htotal,
		mode->vdisplay, mode->vsync_start, mode->vsync_end,
		mode->vtotal, mode->width_mm, mode->height_mm, mode->flags);

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = ctx->user_mode.width_mm;
	connector->display_info.height_mm = ctx->user_mode.height_mm;
	drm_display_info_set_bus_formats(&connector->display_info, &bus_format,
					 1);
	connector->display_info.bus_flags = DRM_BUS_FLAG_PIXDATA_DRIVE_NEGEDGE;

	return 1;
}

static const struct drm_panel_funcs es_panel_drm_funcs = {
	.disable = es_panel_disable,
	.unprepare = es_panel_unprepare,
	.prepare = es_panel_prepare,
	.enable = es_panel_enable,
	.get_modes = es_panel_get_modes,
};

int es_panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct es_panel *ctx;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;

	// for print
	pr_dev = dev;

	dev_info(pr_dev, "[%s] Enter\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				dev_err(pr_dev,
					"No panel connected,skip probe es_panel\n");
				return -ENODEV;
			}
			dev_info(pr_dev, "device node name:%s\n",
				 remote_node->name);
		}
	}

	/*
        if (remote_node != dev->of_node) {
            dev_info(pr_dev,"%s+ skip probe due to not current es_panel\n", __func__);
            return -ENODEV;
        }
    */
	ctx = devm_kzalloc(dev, sizeof(struct es_panel), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dev = dev;

	// default val, if other value, set it on:IOC_ES_MIPI_TX_SET_MODE.
	dsi->lanes = LANES;
	dsi->format = MIPI_DSI_FMT_RGB888;
	// dsi->mode_flags = MIPI_DSI_MODE_VIDEO;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_LPM |
			  MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

	ctx->gpio_backlight0 =
		devm_gpiod_get(ctx->dev, "backlight0", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->gpio_backlight0)) {
		dev_err(ctx->dev, "failed to get backlight0 gpio, err: %ld\n",
			PTR_ERR(ctx->gpio_backlight0));
		return PTR_ERR(ctx->gpio_backlight0);
	}

	ctx->gpio_reset = devm_gpiod_get(ctx->dev, "rst", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->gpio_reset)) {
		dev_err(ctx->dev, "failed to get rst gpio, err: %ld\n",
			PTR_ERR(ctx->gpio_reset));
		return PTR_ERR(ctx->gpio_reset);
	}
	msleep(50);

	gpiod_set_value(ctx->gpio_reset, 1);
	gpiod_set_value(ctx->gpio_backlight0, 1);

	ctx->prepared = false;
	ctx->enabled = false;
	ctx->user_mode_inited = false;

	drm_panel_init(&ctx->panel, dev, &es_panel_drm_funcs,
		       DRM_MODE_CONNECTOR_DPI);
	//	ctx->panel.dev = dev;
	//	ctx->panel.funcs = &es_panel_drm_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	es_panel_chrdev_create(ctx);
	INIT_LIST_HEAD(&ctx->init_cmd_list);
	ctx->init_cmd_writted = 0;
	memset(&ctx->enable_cmd_buf, 0, sizeof(user_cmd_buffer_t));
	memset(&ctx->disable_cmd_buf, 0, sizeof(user_cmd_buffer_t));
	return ret;
}

void es_panel_remove(struct mipi_dsi_device *dsi)
{
	struct es_panel *ctx = mipi_dsi_get_drvdata(dsi);

	free_cmd_list(&ctx->init_cmd_list);
	free_cmd_buf(&ctx->enable_cmd_buf);
	free_cmd_buf(&ctx->disable_cmd_buf);

	devm_gpiod_put(ctx->dev, ctx->gpio_backlight0);
	devm_gpiod_put(ctx->dev, ctx->gpio_reset);

	es_panel_chrdev_destroy(ctx);
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return;
}

static const struct of_device_id es_panel_of_match[] = {
	{
		.compatible = "eswin,generic-panel",
	},
	{}
};
MODULE_DEVICE_TABLE(of, es_panel_of_match);

struct mipi_dsi_driver es_panel_driver = {
    .probe = es_panel_probe,
    .remove = es_panel_remove,
    .driver =
        {
            .name = "es-panel",
            .of_match_table = es_panel_of_match,
        },
};

MODULE_AUTHOR("Eswin");
MODULE_DESCRIPTION("Eswin Generic Panel Driver");
MODULE_LICENSE("GPL v2");
