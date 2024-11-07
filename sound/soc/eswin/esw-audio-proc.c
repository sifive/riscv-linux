/*
 *
 * Copyright (C) 2021 ESWIN, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/eventfd.h>
#include <linux/ioctl.h>
#include <linux/device.h>
#include <linux/mutex.h>

// proc data definition
typedef enum
{
	ADEC_CREATE_CHN,
	ADEC_SEND_STREAM,
	ADEC_GET_FRAME,
	ADEC_PARSE_PACKET,
	ADEC_DECODE_STREAM,
	AENC_CREATE_CHN,
	AENC_SEND_FRAME,
	AENC_GET_STREAM,
	AENC_ENCODE_FRAME,
	AGC_PROCESS,
	ANS_PROCESS,
	AEC_PROCESS,
	DRC_PROCESS,
	EQ_PROCESS,
	DCBLOCK_PROCESS,
	VOLUME_PROCESS,
	SRC_HOST_PROCESS,
	SRC_DAI_PROCESS,
	HOST_PROCESS,
	DAI_PROCESS,
	AO_START,
	AO_PROCESS_FRAME,
	AO_WRITE_FRAME,
	AO_STOP,
	AI_START,
	AI_READ_FRAME,
	AI_PROCESS_FRAME,
	AI_STOP,
} PERF_MARK;

static struct proc_dir_entry *proc_esaudio;
static int g_switch = 0;

// Device data definition
#define MAX_PERF_SIZE 1024
enum DEVICES_ID{
	INVALID_DEVICE = -1,
	AO = 0,
	AI,
	AENC,
	ADEC,
	NUM_DEVICES,
};
static const char *device_names[NUM_DEVICES] = {"ao", "ai", "aenc", "adec"};
static int audio_proc_major[NUM_DEVICES] = {0};
static struct class *audio_proc_class = NULL;
static struct device *audio_proc_device[NUM_DEVICES] = {NULL};
static int32_t *g_perf_data[NUM_DEVICES] = {NULL};
static DEFINE_MUTEX(audio_proc_class_lock);

static void show_aenc_data(struct seq_file *m)
{
	seq_printf(m,"----------------------------------------------------AENC PERF STATISTIC BEGIN"
				"----------------------------------------------------\n");
	seq_printf(m, "\n");
	seq_printf(m, "------------------------------------------------------------------------"
				"----------------------------------------------------------\n");
	seq_printf(m, "audio encoder performance(us):\n");
	seq_printf(m, "%-14s%-14s%-14s%-14s\n", "create_chn", "send_frame", "get_stream", "encode_frame");
	seq_printf(m, "----------------------------------------------------------------------------------"
				"------------------------------------------------\n");
	seq_printf(m, "%-14d%-14d%-14d%-14d\n", g_perf_data[AENC][AENC_CREATE_CHN], g_perf_data[AENC][AENC_SEND_FRAME],
				g_perf_data[AENC][AENC_GET_STREAM], g_perf_data[AENC][AENC_ENCODE_FRAME]);
	seq_printf(m, "\n");
	seq_printf(m, "-----------------------------------------------------AENC PERF STATISTIC END"
	"-----------------------------------------------------\n");
}

static void show_adec_data(struct seq_file *m)
{
	seq_printf(m, "----------------------------------------------------ADEC PERF STATISTIC BEGIN"
					"----------------------------------------------------\n");
	seq_printf(m, "\n");
	seq_printf(m, "--------------------------------------------------------------------------"
				"--------------------------------------------------------\n");
	seq_printf(m, "audio decoder performance(us):\n");
	seq_printf(m, "%-14s%-14s%-14s%-14s%-14s\n", "create_chn", "send_stream", "get_frame",
				"parse_packet", "decode_stream");
	seq_printf(m, "------------------------------------------------------------------------------------"
				"----------------------------------------------\n");
	seq_printf(m, "%-14d%-14d%-14d%-14d%-14d\n", g_perf_data[ADEC][ADEC_CREATE_CHN],
				g_perf_data[ADEC][ADEC_SEND_STREAM], g_perf_data[ADEC][ADEC_GET_FRAME],
				g_perf_data[ADEC][ADEC_PARSE_PACKET], g_perf_data[ADEC][ADEC_DECODE_STREAM]);
	seq_printf(m, "\n");
	seq_printf(m,"-----------------------------------------------------ADEC PERF STATISTIC END"
				"-----------------------------------------------------\n");
}

static void show_ao_data(struct seq_file *m)
{
	seq_printf(m,"----------------------------------------------------AO PERF STATISTIC BEGIN"
	"----------------------------------------------------\n");
	seq_printf(m, "\n");
	seq_printf(m, "---------------------------------------------------------------------------"
				"-------------------------------------------------------\n");
	seq_printf(m, "audio output performance(us):\n");
	seq_printf(m, "%-14s%-24s%-24s\n", "ao_start", "ao_process_frame", "ao_write_frame");
	seq_printf(m, "----------------------------------------------------------------------------"
				"------------------------------------------------------\n");
	seq_printf(m, "%-14d%-24d%-24d\n", g_perf_data[AO][AO_START], g_perf_data[AO][AO_PROCESS_FRAME],
				g_perf_data[AO][AO_WRITE_FRAME]);
	seq_printf(m, "\n");
	seq_printf(m, "-----------------------------------------------------------------------------"
				"-----------------------------------------------------\n");
	seq_printf(m, "audio argorithm performance(ns/1ms):\n");
	seq_printf(m, "%-14s%-14s%-14s%-14s%-14s%-14s%-14s%-14s%-14s\n", "agc", "ans", "eq", "hpf",
				"volume", "src-host","src-dai", "host", "dai");
	seq_printf(m, "------------------------------------------------------------------------------"
				"----------------------------------------------------\n");
	seq_printf(m, "%-14d%-14d%-14d%-14d%-14d%-14d%-14d%-14d%-14d\n", g_perf_data[AO][AGC_PROCESS],
				g_perf_data[AO][ANS_PROCESS],g_perf_data[AO][EQ_PROCESS],
				g_perf_data[AO][DCBLOCK_PROCESS],g_perf_data[AO][VOLUME_PROCESS],
				g_perf_data[AO][SRC_HOST_PROCESS],g_perf_data[AO][SRC_DAI_PROCESS],
				g_perf_data[AO][HOST_PROCESS], g_perf_data[AO][DAI_PROCESS]);
	seq_printf(m, "\n");
	seq_printf(m,"-----------------------------------------------------AO PERF STATISTIC END"
				"-----------------------------------------------------\n");
}

static void show_ai_data(struct seq_file *m)
{
	seq_printf(m,"----------------------------------------------------AI PERF STATISTIC BEGIN"
				"----------------------------------------------------\n");
	seq_printf(m, "\n");
	seq_printf(m, "---------------------------------------------------------------------------"
				"-------------------------------------------------------\n");
	seq_printf(m, "audio input performance(us):\n");
	seq_printf(m, "%-14s%-24s%-24s\n", "ai_start", "ai_read_frame", "ai_process_frame");
	seq_printf(m, "----------------------------------------------------------------------------"
				"------------------------------------------------------\n");
	seq_printf(m, "%-14d%-24d%-24d\n", g_perf_data[AI][AI_START], g_perf_data[AI][AI_READ_FRAME],
				g_perf_data[AI][AI_PROCESS_FRAME]);
	seq_printf(m, "\n");
	seq_printf(m, "-----------------------------------------------------------------------------"
				"-----------------------------------------------------\n");
	seq_printf(m, "audio argorithm performance(ns/1ms):\n");
	seq_printf(m, "%-14s%-14s%-14s%-14s%-14s%-14s%-14s%-14s%-14s%-14s\n", "agc", "ans", "drc", "eq",
				"hpf", "volume","src-host", "src-dai", "host", "dai");
	seq_printf(m, "------------------------------------------------------------------------------"
				"----------------------------------------------------\n");
	seq_printf(m, "%-14d%-14d%-14d%-14d%-14d%-14d%-14d%-14d%-14d%-14d\n", g_perf_data[AI][AGC_PROCESS],
		g_perf_data[AI][ANS_PROCESS],g_perf_data[AI][DRC_PROCESS], g_perf_data[AI][EQ_PROCESS],
		g_perf_data[AI][DCBLOCK_PROCESS], g_perf_data[AI][VOLUME_PROCESS],g_perf_data[AI][SRC_HOST_PROCESS],
		g_perf_data[AI][SRC_DAI_PROCESS], g_perf_data[AI][HOST_PROCESS], g_perf_data[AI][DAI_PROCESS]);
	seq_printf(m, "\n");
	seq_printf(m,"-----------------------------------------------------AI PERF STATISTIC END"
				"-----------------------------------------------------\n");
}


static int audio_info_show(struct seq_file *m, void *p)
{
	int i;
	const char *fileName = m->file->f_path.dentry->d_name.name;
	enum DEVICES_ID deviceID = INVALID_DEVICE;

	pr_info("audio_info_show:%s\n", m->file->f_path.dentry->d_name.name);

	for (i = 0; i < NUM_DEVICES; ++i) {
		if (strcmp(fileName, device_names[i]) == 0) {
			deviceID = i;
			break;
		}
	}

	if (deviceID == INVALID_DEVICE) {
		pr_err("deviceID is INVALID\n");
		return -EINVAL;
	}

 	if (g_switch == 0) {
		seq_printf(m, "The switch is not turned on, pls first turn on the switch.\n");
		return 0;
	}

	switch (deviceID) {
	case AI:
		show_ai_data(m);
		break;
	case AO:
		show_ao_data(m);
		break;
	case AENC:
		show_aenc_data(m);
		break;
	case ADEC:
		show_adec_data(m);
		break;
	default:
		pr_err("deviceID is INVALID\n");
		break;
		}
	return 0;
}

static int info_open(struct inode *inode, struct file *flip)
{
	return single_open(flip, audio_info_show, NULL);
}

static int switch_show(struct seq_file *m, void *p)
{
	seq_printf(m, "--------------------AUDIO Performance Switch--------------------\n");
	seq_printf(m, "\n");
	seq_printf(m, "AUDIO Performance Switch Status Value:%d\n", g_switch);
	seq_printf(m, "\n");
	return 0;
}

static int switch_open(struct inode *inode, struct file *flip)
{
	return single_open(flip, switch_show, NULL);
}

static ssize_t switch_write(struct file *flip, const char __user *buf, size_t size, loff_t *pos)
{
	u16 data;
	u8 value;

	if (size > 2) {
		return -EINVAL;
	}
	if (copy_from_user(&data, buf, size)) {
		return -EFAULT;
	}
	value = data & 0xff;

	value -= '0';
	if (!(value == 1 || value == 0)) {
		printk("%s, %d, data=%d is not correct, pls use 1 or 0.\n", __func__, __LINE__, value);
		return -EINVAL;
	}

	g_switch = value ? 1 : 0;

	return size;
}

static struct proc_ops proc_info_fops = {
	.proc_open = info_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};

static struct proc_ops proc_switch_fops = {
	.proc_open = switch_open,
	.proc_read = seq_read,
	.proc_release = single_release,
	.proc_write = switch_write,
};

int audio_create_procfs(void)
{
	proc_esaudio = proc_mkdir("es_audio", NULL);
	if (proc_esaudio == NULL) {
		pr_err("create es_audio dir err.\n");
		return -ENOMEM;
	}

	if (!proc_create("ao", 0644, proc_esaudio, &proc_info_fops)) {
		pr_err("error create proc ao file.\n");
		goto err_ao;
	}

	if (!proc_create("ai", 0644, proc_esaudio, &proc_info_fops)) {
		pr_err("error create proc ai file.\n");
		goto err_ai;
	}

	if (!proc_create("aenc", 0644, proc_esaudio, &proc_info_fops)) {
		pr_err("error create proc aenc file.\n");
		goto err_aenc;
	}

	if (!proc_create("adec", 0644, proc_esaudio, &proc_info_fops)) {
		pr_err("error create proc adec file.\n");
		goto err_adec;
	}

	if (!proc_create("switch", 0644, proc_esaudio, &proc_switch_fops)) {
		pr_err("error create proc switch file.\n");
		goto err_switch;
	}

	return 0;

err_switch:
	remove_proc_entry("adec", proc_esaudio);
err_adec:
	remove_proc_entry("aenc", proc_esaudio);
err_aenc:
	remove_proc_entry("ai", proc_esaudio);
err_ai:
	remove_proc_entry("ao", proc_esaudio);
err_ao:
	remove_proc_entry("es_audio", NULL);
	return -1;
}

void audio_remove_procfs(void)
{
	remove_proc_entry("switch", proc_esaudio);

	remove_proc_entry("adec", proc_esaudio);

	remove_proc_entry("aenc", proc_esaudio);

	remove_proc_entry("ai", proc_esaudio);

	remove_proc_entry("ao", proc_esaudio);

	remove_proc_entry("es_audio", NULL);
}


///////////////////////////////////////////////////////////////////////////////
// audio dev implementation
static int audio_dev_mmap(struct file *file, struct vm_area_struct *vma)
{
	int i;
	const char *fileName = file->f_path.dentry->d_name.name;
	enum DEVICES_ID deviceID = INVALID_DEVICE;
	unsigned long size = vma->vm_end - vma->vm_start;

	if (size > (MAX_PERF_SIZE * sizeof(int32_t))) {
		pr_err("audio_dev_mmap: size:%ld > %ld.\n", size, MAX_PERF_SIZE * sizeof(int32_t));
		return -EINVAL;
	}

	for (i = 0; i < NUM_DEVICES; ++i) {
		if (strcmp(fileName, device_names[i]) == 0) {
			deviceID = i;
			break;
		}
	}

	if (deviceID == INVALID_DEVICE) {
		pr_err("deviceID is INVALID\n");
		return -EINVAL;
	}

	// Remap the shared memory into the process's address space
	if (remap_pfn_range(vma, vma->vm_start, virt_to_phys(g_perf_data[deviceID]) >> PAGE_SHIFT,
						size, vma->vm_page_prot)) {
		pr_err("Failed to remap shared memory.\n");
		return -EAGAIN;
	}

	return 0;
}

static const struct file_operations dev_fops = {
	.owner = THIS_MODULE,
	.mmap = audio_dev_mmap,
};

static bool g_proc_initialized = false;

int audio_proc_module_init(void)
{
	int i, ret;
	struct device *dev;

	pr_info("audio_proc_module_init enter.\n");

	mutex_lock(&audio_proc_class_lock);

	if (g_proc_initialized) {
		mutex_unlock(&audio_proc_class_lock);
		return 0;
	}

	audio_proc_class = class_create("audio_proc_class");
	if (IS_ERR(audio_proc_class)) {
		pr_err("Failed to create audio_proc_class\n");
		mutex_unlock(&audio_proc_class_lock);
		return PTR_ERR(audio_proc_class);
	}

	for (i = 0; i < NUM_DEVICES; ++i) {
		g_perf_data[i] = kmalloc(MAX_PERF_SIZE * sizeof(int32_t), GFP_KERNEL);
		if (!g_perf_data[i]) {
			pr_err("Failed to allocate shared memory for '%s'\n", device_names[i]);
			goto cleanup;
		}

		memset(g_perf_data[i], 0, MAX_PERF_SIZE * sizeof(int32_t));

		ret = register_chrdev(0, device_names[i], &dev_fops);
		if (ret < 0) {
			pr_err("Failed to register character device '%s'\n", device_names[i]);
			goto cleanup;
		}

		audio_proc_major[i] = ret;

		dev = device_create(audio_proc_class, NULL, MKDEV(audio_proc_major[i], 0), NULL, device_names[i]);
		if (IS_ERR(dev)) {
			pr_err("Failed to create device node '%s'\n", device_names[i]);
			goto cleanup;
		}

		audio_proc_device[i] = dev;
	}

	audio_create_procfs();

	g_proc_initialized = true;

	pr_info("es_audio_proc: initialized\n");
	mutex_unlock(&audio_proc_class_lock);
	return 0;

cleanup:
	for (i = 0; i < NUM_DEVICES; ++i) {
		if (g_perf_data[i]) {
			kfree(g_perf_data[i]);
			g_perf_data[i] = NULL;
		}

		if (audio_proc_major[i]) {
			unregister_chrdev(audio_proc_major[i], device_names[i]);
		}

		if (audio_proc_device[i]) {
			device_destroy(audio_proc_class, MKDEV(audio_proc_major[i], 0));
		}
	}

	class_destroy(audio_proc_class);
	mutex_unlock(&audio_proc_class_lock);
	return ret;
}

static bool g_proc_uninitialized = false;

void audio_proc_module_exit(void)
{
	int i;

	if (g_proc_uninitialized) {
		return;
	}

	audio_remove_procfs();
	for (i = 0; i < NUM_DEVICES; ++i) {
		device_destroy(audio_proc_class, MKDEV(audio_proc_major[i], 0));
		unregister_chrdev(audio_proc_major[i], device_names[i]);
		kfree(g_perf_data[i]);
		g_perf_data[i] = NULL;
	}

	class_destroy(audio_proc_class);

	g_proc_uninitialized = true;

	pr_info("es_audio_proc: uninitialized\n");
}
