#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/platform_device.h>

#include <linux/version.h>
#include <linux/netlink.h>
#include <linux/proc_fs.h>
#include <linux/moduleparam.h>
#include <linux/clk.h>
#include <linux/of.h>

#include "host_lib_driver_linux_if.h"
#include "dw_hdmi_hdcp.h"

/**
 * \file
 * \ingroup HL_Driver_Kernel
 * \brief Sample Linux Host Library Driver
 * \copydoc HL_Driver_Kernel
 */

/**
 * \defgroup HL_Driver_Linux Sample Linux Host Library Driver
 * \ingroup HL_Driver
 * \brief Sample code for the Linux Host Library Driver.
 * The Linux Host Library Driver is composed of 2 parts:
 * 1. A kernel driver.
 * 2. A file access instance.
 *
 * The kernel driver is the kernel executable code enabling the firmware to execute.
 * It provides the access to the hardware register to interact with the firmware.
 *
 * The file access instance initializes the #hl_driver_t structure for the
 * host library access.  The Host Library references the file access to request the
 * kernel operations.
 */

/**
 * \defgroup HL_Driver_Kernel Sample Linux Kernel Host Library Driver
 * \ingroup HL_Driver_Linux
 * \brief Example code for the Linux Kernel Host Library Driver.
 *
 * The Sample Linux Kernel Driver operates on the linux kernel.
 * To install (requires root access):
 * \code
 insmod bin/linux_hld_module.ko verbose=0
 * \endcode
 *
 * To remove (requires root access):
 * \code
 rmmod linux_hld_module
 * \endcode
 *
 * Example Linux Host Library Code:
 * \code
 */

#define MAX_HL_DEVICES 16
#define TROOT_GRIFFIN

static bool randomize_mem = false;
module_param(randomize_mem, bool, 0);
MODULE_PARM_DESC(noverify, "Wipe memory allocations on startup (for debug)");

static struct dw_hdcp2 *g_dw_hdcp2;

static void dw_hdcp2_stop(void)
{
	printk("func: %s; line: %d\n", __func__, __LINE__);
	g_dw_hdcp2->hot_plug = 0;
	dw_hdmi_hdcp2_start(0);
}

static void dw_hdcp2_start(void)
{
	printk("func: %s; line: %d\n", __func__, __LINE__);
	dw_hdmi_hdcp2_start(1);
	g_dw_hdcp2->hot_plug = 1;
}

//
// HL Device
//
typedef struct {
	int allocated, initialized;
	int code_loaded;

	int code_is_phys_mem;
	dma_addr_t code_base;
	uint32_t code_size;
	uint8_t *code;
	int data_is_phys_mem;
	dma_addr_t data_base;
	uint32_t data_size;
	uint8_t *data;

	struct resource *hpi_resource;
	uint8_t __iomem *hpi;
} hl_device;

static hl_device hl_devices[MAX_HL_DEVICES];

/* HL_DRV_IOC_MEMINFO implementation */
static long get_meminfo(hl_device *hl_dev, void __user *arg)
{
	struct hl_drv_ioc_meminfo info;

	if ((hl_dev == 0) || (arg == 0) || (hl_dev->hpi_resource == 0)) {
		return -EFAULT;
	}

	info.hpi_base = hl_dev->hpi_resource->start;
	info.code_base = hl_dev->code_base;
	info.code_size = hl_dev->code_size;
	info.data_base = hl_dev->data_base;
	info.data_size = hl_dev->data_size;

	if (copy_to_user(arg, &info, sizeof info) != 0) {
		return -EFAULT;
	}

	return 0;
}

/* HL_DRV_IOC_LOAD_CODE implementation */
static long load_code(hl_device *hl_dev, struct hl_drv_ioc_code __user *arg)
{
	struct hl_drv_ioc_code head;

	if ((hl_dev == 0) || (arg == 0) || (hl_dev->code == 0) ||
	    (hl_dev->data == 0)) {
		return -EFAULT;
	}

	if (copy_from_user(&head, arg, sizeof head) != 0)
		return -EFAULT;

	if (head.len > hl_dev->code_size)
		return -ENOSPC;

	if (hl_dev->code_loaded)
		return -EBUSY;

	if (randomize_mem) {
		get_random_bytes(hl_dev->code, hl_dev->code_size);
		get_random_bytes(hl_dev->data, hl_dev->data_size);
	}

	if (copy_from_user(hl_dev->code, &arg->data, head.len) != 0)
		return -EFAULT;

	hl_dev->code_loaded = 1;
	return 0;
}

/* HL_DRV_IOC_WRITE_DATA implementation */
static long write_data(hl_device *hl_dev, struct hl_drv_ioc_data __user *arg)
{
	struct hl_drv_ioc_data head;

	if ((hl_dev == 0) || (arg == 0) || (hl_dev->data == 0)) {
		return -EFAULT;
	}

	if (copy_from_user(&head, arg, sizeof head) != 0)
		return -EFAULT;

	if (hl_dev->data_size < head.len)
		return -ENOSPC;
	if (hl_dev->data_size - head.len < head.offset)
		return -ENOSPC;

	if (copy_from_user(hl_dev->data + head.offset, &arg->data, head.len) !=
	    0)
		return -EFAULT;

	return 0;
}

/* HL_DRV_IOC_READ_DATA implementation */
static long read_data(hl_device *hl_dev, struct hl_drv_ioc_data __user *arg)
{
	struct hl_drv_ioc_data head;

	if ((hl_dev == 0) || (arg == 0) || (hl_dev->data == 0)) {
		return -EFAULT;
	}

	if (copy_from_user(&head, arg, sizeof head) != 0)
		return -EFAULT;

	if (hl_dev->data_size < head.len)
		return -ENOSPC;
	if (hl_dev->data_size - head.len < head.offset)
		return -ENOSPC;

	if (copy_to_user(&arg->data, hl_dev->data + head.offset, head.len) != 0)
		return -EFAULT;

	return 0;
}

/* HL_DRV_IOC_MEMSET_DATA implementation */
static long set_data(hl_device *hl_dev, void __user *arg)
{
	union {
		struct hl_drv_ioc_data data;
		unsigned char buf[sizeof(struct hl_drv_ioc_data) + 1];
	} u;

	if ((hl_dev == 0) || (arg == 0) || (hl_dev->data == 0)) {
		return -EFAULT;
	}

	if (copy_from_user(&u.data, arg, sizeof u.buf) != 0)
		return -EFAULT;

	if (hl_dev->data_size < u.data.len)
		return -ENOSPC;
	if (hl_dev->data_size - u.data.len < u.data.offset)
		return -ENOSPC;

	memset(hl_dev->data + u.data.offset, u.data.data[0], u.data.len);
	return 0;
}

/* HL_DRV_IOC_READ_HPI implementation */
static long hpi_read(hl_device *hl_dev, void __user *arg)
{
	struct hl_drv_ioc_hpi_reg reg;

	if ((hl_dev == 0) || (arg == 0) || (hl_dev->hpi_resource == 0)) {
		return -EFAULT;
	}

	if (copy_from_user(&reg, arg, sizeof reg) != 0)
		return -EFAULT;

	if ((reg.offset & 3) ||
	    reg.offset >= resource_size(hl_dev->hpi_resource))
		return -EINVAL;

	reg.value = ioread32(hl_dev->hpi + reg.offset);

	if (copy_to_user(arg, &reg, sizeof reg) != 0)
		return -EFAULT;

	return 0;
}

/* HL_DRV_IOC_WRITE_HPI implementation */
static long hpi_write(hl_device *hl_dev, void __user *arg)
{
	struct hl_drv_ioc_hpi_reg reg;

	if ((hl_dev == 0) || (arg == 0)) {
		return -EFAULT;
	}

	if (copy_from_user(&reg, arg, sizeof reg) != 0)
		return -EFAULT;

	if ((reg.offset & 3) ||
	    reg.offset >= resource_size(hl_dev->hpi_resource))
		return -EINVAL;

	iowrite32(reg.value, hl_dev->hpi + reg.offset);

#ifdef TROOT_GRIFFIN
	//  If Kill command
	//  (HL_GET_CMD_EVENT(krequest.data) == TROOT_CMD_SYSTEM_ON_EXIT_REQ))
	//
	if ((reg.offset == 0x38) && ((reg.value & 0x000000ff) == 0x08)) {
		hl_dev->code_loaded = 0;
	}
#endif
	return 0;
}

static hl_device *alloc_hl_dev_slot(const struct hl_drv_ioc_meminfo *info)
{
	int i;

	if (info == 0) {
		return 0;
	}

	/* Check if we have a matching device (same HPI base) */
	for (i = 0; i < MAX_HL_DEVICES; i++) {
		hl_device *slot = &hl_devices[i];
		if (slot->allocated &&
		    (info->hpi_base == slot->hpi_resource->start))
			return slot;
	}

	/* Find unused slot */
	for (i = 0; i < MAX_HL_DEVICES; i++) {
		hl_device *slot = &hl_devices[i];
		if (!slot->allocated) {
			slot->allocated = 1;
			return slot;
		}
	}

	return 0;
}

static void free_dma_areas(hl_device *hl_dev)
{
	if (hl_dev == 0) {
		return;
	}

	if (!hl_dev->code_is_phys_mem && hl_dev->code) {
		dma_free_coherent(0, hl_dev->code_size, hl_dev->code,
				  hl_dev->code_base);
		hl_dev->code = 0;
	}

	if (!hl_dev->data_is_phys_mem && hl_dev->data) {
		dma_free_coherent(0, hl_dev->data_size, hl_dev->data,
				  hl_dev->data_base);
		hl_dev->data = 0;
	}
}

static int alloc_dma_areas(hl_device *hl_dev,
			   const struct hl_drv_ioc_meminfo *info)
{
	if ((hl_dev == 0) || (info == 0)) {
		return -EFAULT;
	}

	hl_dev->code_size = info->code_size;
	hl_dev->code_is_phys_mem =
		(info->code_base != HL_DRIVER_ALLOCATE_DYNAMIC_MEM);

	if (hl_dev->code_is_phys_mem && (hl_dev->code == 0)) {
		/* TODO: support highmem */
		hl_dev->code_base = info->code_base;
		hl_dev->code = phys_to_virt(hl_dev->code_base);
	} else {
		hl_dev->code =
			dma_alloc_coherent(g_dw_hdcp2->dev, hl_dev->code_size,
					   &hl_dev->code_base, GFP_KERNEL);
		if (!hl_dev->code) {
			return -ENOMEM;
		}
	}

	hl_dev->data_size = info->data_size;
	hl_dev->data_is_phys_mem =
		(info->data_base != HL_DRIVER_ALLOCATE_DYNAMIC_MEM);

	if (hl_dev->data_is_phys_mem && (hl_dev->data == 0)) {
		hl_dev->data_base = info->data_base;
		hl_dev->data = phys_to_virt(hl_dev->data_base);
	} else {
		hl_dev->data =
			dma_alloc_coherent(g_dw_hdcp2->dev, hl_dev->data_size,
					   &hl_dev->data_base, GFP_KERNEL);
		if (!hl_dev->data) {
			free_dma_areas(hl_dev);
			return -ENOMEM;
		}
	}

	return 0;
}

/* HL_DRV_IOC_INIT implementation */
static long init(struct file *f, void __user *arg)
{
	struct resource *hpi_mem;
	struct hl_drv_ioc_meminfo info;
	hl_device *hl_dev;
	int rc;

	if ((f == 0) || (arg == 0)) {
		return -EFAULT;
	}

	if (copy_from_user(&info, arg, sizeof info) != 0)
		return -EFAULT;
	hl_dev = alloc_hl_dev_slot(&info);
	if (!hl_dev)
		return -EMFILE;

	if (!hl_dev->initialized) {
		rc = alloc_dma_areas(hl_dev, &info);
		if (rc < 0)
			goto err_free;

		hpi_mem = request_mem_region(info.hpi_base, 128, "hl_dev-hpi");
		if (!hpi_mem) {
			rc = -EADDRNOTAVAIL;
			goto err_free;
		}

		hl_dev->hpi = ioremap(hpi_mem->start, resource_size(hpi_mem));
		if (!hl_dev->hpi) {
			rc = -ENOMEM;
			goto err_release_region;
		}
		hl_dev->hpi_resource = hpi_mem;
		hl_dev->initialized = 1;
	}

	f->private_data = hl_dev;
	return 0;

err_release_region:
	release_resource(hpi_mem);
err_free:
	free_dma_areas(hl_dev);
	hl_dev->initialized = 0;
	hl_dev->allocated = 0;
	hl_dev->hpi_resource = 0;
	hl_dev->hpi = 0;

	return rc;
}

static void free_hl_dev_slot(hl_device *slot)
{
	if (slot == 0) {
		return;
	}

	if (!slot->allocated)
		return;

	if (slot->initialized) {
		if (slot->hpi) {
			iounmap(slot->hpi);
			slot->hpi = 0;
		}

		if (slot->hpi_resource) {
			release_mem_region(slot->hpi_resource->start, 128);
			slot->hpi_resource = 0;
		}

		free_dma_areas(slot);
	}

	slot->initialized = 0;
	slot->allocated = 0;
}

static long hld_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	hl_device *hl_dev;
	void __user *data;

	if (f == 0) {
		return -EFAULT;
	}

	hl_dev = f->private_data;
	data = (void __user *)arg;

	if (cmd == HL_DRV_IOC_INIT) {
		return init(f, data);
	} else if (!hl_dev) {
		return -EAGAIN;
	}

	switch (cmd) {
	case HL_DRV_IOC_INIT:
		return init(f, data);
	case HL_DRV_IOC_MEMINFO:
		return get_meminfo(hl_dev, data);
	case HL_DRV_IOC_READ_HPI:
		return hpi_read(hl_dev, data);
	case HL_DRV_IOC_WRITE_HPI:
		return hpi_write(hl_dev, data);
	case HL_DRV_IOC_LOAD_CODE:
		return load_code(hl_dev, data);
	case HL_DRV_IOC_WRITE_DATA:
		return write_data(hl_dev, data);
	case HL_DRV_IOC_READ_DATA:
		return read_data(hl_dev, data);
	case HL_DRV_IOC_MEMSET_DATA:
		return set_data(hl_dev, data);

	case DW_DRV_IOC_CONNECT_STATUS:
		return g_dw_hdcp2->hot_plug;
	case DW_DRV_IOC_CONNECT_SET:
		printk("set hdcp2 reset one\n");
		g_dw_hdcp2->wait_hdcp2_reset = 1;
		dw_hdmi_hdcp2_start(1);
		return 0;
	case DW_DRV_IOC_DISCONNECT_SET:
		if (g_dw_hdcp2->wait_hdcp2_reset == 1) {
			printk("set hdcp2 reset zero\n");
			g_dw_hdcp2->wait_hdcp2_reset = 0;
			dw_hdmi_hdcp2_start(0);
		}
		if (g_dw_hdcp2->auth_sucess == 1) {
			g_dw_hdcp2->auth_sucess = 0;
		}
		return 0;
	case DW_DRV_IOC_AUTH_SUCCESS:
		g_dw_hdcp2->auth_sucess = 1;
		return 0;
	case DW_DRV_IOC_AUTH_FAIL:
		g_dw_hdcp2->auth_sucess = 0;
		return 0;
	case DW_DRV_IOC_NO_CAPACITY:
		printk("set hdcp2 reset zero 3005\n");
		g_dw_hdcp2->hot_plug = 0;
		g_dw_hdcp2->wait_hdcp2_reset = 0;
		dw_hdmi_hdcp2_start(0);
		dw_hdmi_hdcp2_start(2);
		return 0;
	}

	return -ENOTTY;
}

static const struct file_operations hld_file_operations = {
#ifdef CONFIG_COMPAT
	.compat_ioctl = hld_ioctl,
#endif
	.unlocked_ioctl = hld_ioctl,
	.owner = THIS_MODULE,
};

static struct miscdevice hld_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hl_dev",
	.fops = &hld_file_operations,
};

static int hld_init(void)
{
	int i;

	printk("%s...\n", __func__);
	for (i = 0; i < MAX_HL_DEVICES; i++) {
		hl_devices[i].allocated = 0;
		hl_devices[i].initialized = 0;
		hl_devices[i].code_loaded = 0;
		hl_devices[i].code = 0;
		hl_devices[i].data = 0;
		hl_devices[i].hpi_resource = 0;
		hl_devices[i].hpi = 0;
	}
	return misc_register(&hld_device);
}

static void hld_exit(void)
{
	int i;

	for (i = 0; i < MAX_HL_DEVICES; i++) {
		free_hl_dev_slot(&hl_devices[i]);
	}

	misc_deregister(&hld_device);
}

static int dw_hdmi2_hdcp2_clk_enable(struct device *dev)
{
	struct clk *pclk;
	//struct clk *aclk;
	struct clk *hdcp2_clk_hdmi;

	pclk = devm_clk_get(dev, "pclk_hdcp2");
	if (IS_ERR(pclk)) {
		pr_err("Unable to get hdcp2 pclk\n");
		return -1;
	}
	clk_prepare_enable(pclk);
#if 0
    aclk = devm_clk_get(dev, "aclk_hdcp2");
    if (IS_ERR(aclk)) {
        pr_err("Unable to get hdcp2 aclk\n");
        return -1;
    }
    clk_prepare_enable(aclk);
#endif
	hdcp2_clk_hdmi = devm_clk_get(dev, "hdcp2_clk_hdmi");
	if (IS_ERR(hdcp2_clk_hdmi)) {
		pr_err("Unable to get hdcp2_clk_hdmi\n");
		return -1;
	}
	clk_prepare_enable(hdcp2_clk_hdmi);

	return 0;
}

static ssize_t hdcp2_show_enable(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", g_dw_hdcp2->enable);
}

static ssize_t hdcp2_store_enable(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	int enable;
	if (kstrtoint(buf, 0, &enable))
		return size;

	if (g_dw_hdcp2->enable != enable) {
		g_dw_hdcp2->enable = enable;
		if (enable) {
			dw_hdmi_hdcp2_start(3);
		} else {
			if (g_dw_hdcp2->hot_plug) {
				g_dw_hdcp2->stop();
				dw_hdmi_hdcp2_start(2);
			}
		}
	}
	return size;
}

static ssize_t hdcp2_show_status(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	if (g_dw_hdcp2->enable != 1) {
		return snprintf(buf, PAGE_SIZE, "%s\n", "no enable hdcp2");
	} else if (!g_dw_hdcp2->hot_plug) {
		return snprintf(buf, PAGE_SIZE, "%s\n", "hdcp2 no auth");
	} else {
		if (g_dw_hdcp2->auth_sucess)
			return snprintf(buf, PAGE_SIZE, "%s\n",
					"hdcp2 auth sucess");
		else
			return snprintf(buf, PAGE_SIZE, "%s\n",
					"no already auth sucess");
	}
}

static DEVICE_ATTR(enable, 0644, hdcp2_show_enable, hdcp2_store_enable);
static DEVICE_ATTR(status, 0444, hdcp2_show_status, NULL);

static int create_device_node(void)
{
	int ret;

	if (!g_dw_hdcp2)
		return -1;
	g_dw_hdcp2->mdev.minor = MISC_DYNAMIC_MINOR;
	g_dw_hdcp2->mdev.name = "hdcp2_node";
	g_dw_hdcp2->mdev.mode = 0666;
	if (misc_register(&(g_dw_hdcp2->mdev))) {
		pr_err("HDCP2: Could not add character driver\n");
		return -1;
	}

	ret = device_create_file(g_dw_hdcp2->mdev.this_device,
				 &dev_attr_enable);
	if (ret) {
		pr_err("HDCP: Could not add sys file enable\n");
		ret = -EINVAL;
		goto error0;
	}

	ret = device_create_file(g_dw_hdcp2->mdev.this_device,
				 &dev_attr_status);
	if (ret) {
		pr_err("HDCP: Could not add sys file status\n");
		ret = -EINVAL;
		goto error1;
	}

	return 0;

error1:
	device_remove_file(g_dw_hdcp2->mdev.this_device, &dev_attr_enable);
error0:
	misc_deregister(&g_dw_hdcp2->mdev);
	return ret;
}

static void end_device_node(void)
{
	if (g_dw_hdcp2)
		misc_deregister(&(g_dw_hdcp2->mdev));
}

static int eswin_hdmi_hdcp2_probe(struct platform_device *pdev)
{
	struct device *hdcp2_dev = &pdev->dev;

	printk("%s...\n", __func__);
	g_dw_hdcp2 = kmalloc(sizeof(*g_dw_hdcp2), GFP_KERNEL);
	if (!g_dw_hdcp2) {
		printk("malloc g_dw_hdcp2 error\n");
		return -ENOMEM;
	}
	memset(g_dw_hdcp2, 0, sizeof(*g_dw_hdcp2));

	g_dw_hdcp2->dev = hdcp2_dev;
	g_dw_hdcp2->stop = dw_hdcp2_stop;
	g_dw_hdcp2->start = dw_hdcp2_start;
	hld_init();
	dw_hdmi2_hdcp2_clk_enable(hdcp2_dev);
	dma_set_mask_and_coherent(hdcp2_dev, DMA_BIT_MASK(32));
	dw_hdmi_hdcp2_init(g_dw_hdcp2);
	dw_hdmi_hdcp2_start(3);

	create_device_node();
	return 0;
}

static int eswin_hdmi_hdcp2_remove(struct platform_device *pdev)
{
	printk("%s...\n", __func__);
	dw_hdmi_hdcp2_remove();
	end_device_node();
	hld_exit();
	kfree(g_dw_hdcp2);
	g_dw_hdcp2 = NULL;

	return 0;
}

static void eswin_hdmi_hdcp2_shutdown(struct platform_device *pdev)
{
	printk("%s...\n", __func__);
}

#if defined(CONFIG_OF)
static const struct of_device_id dw_hdmi_hdcp2_dt_ids[] = {
	{
		.compatible = "eswin,dw-hdmi-hdcp2",
	},
	{}
};

MODULE_DEVICE_TABLE(of, dw_hdmi_hdcp2_dt_ids);
#endif

struct platform_driver dw_hdmi_hdcp2_driver = {
        .probe = eswin_hdmi_hdcp2_probe,
        .remove = eswin_hdmi_hdcp2_remove,
        .shutdown = eswin_hdmi_hdcp2_shutdown,
        .driver = {
        .name = "dw-hdmi-hdcp2",
        .owner = THIS_MODULE,
#if defined(CONFIG_OF)
        .of_match_table = of_match_ptr(dw_hdmi_hdcp2_dt_ids),
#endif
    },
};

static int __init hdmi_hdcp2_init(void)
{
	printk("%s...\n", __func__);
	return platform_driver_register(&dw_hdmi_hdcp2_driver);
}

static void hdmi_hdcp2_exit(void)
{
	platform_driver_unregister(&dw_hdmi_hdcp2_driver);
}

late_initcall_sync(hdmi_hdcp2_init);
module_exit(hdmi_hdcp2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Synopsys, Inc.");
MODULE_DESCRIPTION("Linux Host Library Driver");
