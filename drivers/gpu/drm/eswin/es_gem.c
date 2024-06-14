// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Eswin Holdings Co., Ltd.
 */

#include <linux/dma-buf.h>
#include <drm/drm_prime.h>

#include "es_drv.h"
#include "es_gem.h"

MODULE_IMPORT_NS(DMA_BUF);

static const struct drm_gem_object_funcs es_gem_default_funcs;

static void nonseq_free(struct page **pages, unsigned int nr_page)
{
	u32 i;

	if (!pages)
		return;

	for (i = 0; i < nr_page; i++)
		__free_page(pages[i]);
}

#ifdef CONFIG_ESWIN_MMU
static int get_pages(unsigned int nr_page, struct es_gem_object *es_obj)
{
	struct page *pages;
	u32 i, num_page, page_count = 0;
	int order = 0;
	gfp_t gfp = GFP_KERNEL;

	if (!es_obj->pages)
		return -EINVAL;

	gfp &= ~__GFP_HIGHMEM;
	gfp |= __GFP_DMA32;

	num_page = nr_page;

	do {
		pages = NULL;
		order = get_order(num_page * PAGE_SIZE);
		num_page = 1 << order;

		if ((num_page + page_count > nr_page) || (order >= MAX_ORDER)) {
			num_page = num_page >> 1;
			continue;
		}

		pages = alloc_pages(gfp, order);
		if (!pages) {
			if (num_page == 1) {
				nonseq_free(es_obj->pages, page_count);
				return -ENOMEM;
			}

			num_page = num_page >> 1;
		} else {
			for (i = 0; i < num_page; i++) {
				es_obj->pages[page_count + i] = &pages[i];
				SetPageReserved(es_obj->pages[page_count + i]);
			}

			page_count += num_page;
			num_page = nr_page - page_count;
		}

	} while (page_count < nr_page);

	es_obj->get_pages = true;

	return 0;
}
#endif

static void put_pages(unsigned int nr_page, struct es_gem_object *es_obj)
{
	u32 i;

	for (i = 0; i < nr_page; i++)
		ClearPageReserved(es_obj->pages[i]);

	nonseq_free(es_obj->pages, nr_page);

	return;
}

static int es_gem_alloc_buf(struct es_gem_object *es_obj)
{
	struct drm_device *dev = es_obj->base.dev;
	unsigned int nr_pages;
	struct sg_table sgt;
	int ret = -ENOMEM;
#ifdef CONFIG_ESWIN_MMU
	struct es_drm_private *priv = dev->dev_private;
#endif

	if (es_obj->dma_addr) {
		DRM_DEV_DEBUG_KMS(dev->dev, "already allocated.\n");
		return 0;
	}

	es_obj->dma_attrs = DMA_ATTR_WRITE_COMBINE;

	if (!is_iommu_enabled(dev))
		es_obj->dma_attrs |= DMA_ATTR_FORCE_CONTIGUOUS;

	nr_pages = es_obj->size >> PAGE_SHIFT;

	es_obj->pages = kvmalloc_array(nr_pages, sizeof(struct page *),
				       GFP_KERNEL | __GFP_ZERO);
	if (!es_obj->pages) {
		DRM_DEV_ERROR(dev->dev, "failed to allocate pages.\n");
		return -ENOMEM;
	}

	es_obj->cookie = dma_alloc_attrs(to_dma_dev(dev), es_obj->size,
					 &es_obj->dma_addr, GFP_KERNEL,
					 es_obj->dma_attrs);
	if (!es_obj->cookie) {
#ifdef CONFIG_ESWIN_MMU
		ret = get_pages(nr_pages, es_obj);
		if (ret) {
			DRM_DEV_ERROR(dev->dev, "fail to allocate buffer.\n");
			goto err_free;
		}
#else
		DRM_DEV_ERROR(dev->dev, "failed to allocate buffer.\n");
		goto err_free;
#endif
	}

#ifdef CONFIG_ESWIN_MMU
	/* MMU map*/
	if (!priv->mmu) {
		DRM_DEV_ERROR(dev->dev, "invalid mmu.\n");
		ret = -EINVAL;
		goto err_mem_free;
	}

	if (!es_obj->get_pages)
		ret = dc_mmu_map_memory(priv->mmu, (u64)es_obj->dma_addr,
					nr_pages, &es_obj->iova, true);
	else
		ret = dc_mmu_map_memory(priv->mmu, (u64)es_obj->pages, nr_pages,
					&es_obj->iova, false);

	if (ret) {
		DRM_DEV_ERROR(dev->dev, "failed to do mmu map.\n");
		goto err_mem_free;
	}
#else
	es_obj->iova = es_obj->dma_addr;
#endif

	if (!es_obj->get_pages) {
		ret = dma_get_sgtable_attrs(to_dma_dev(dev), &sgt,
					    es_obj->cookie, es_obj->dma_addr,
					    es_obj->size, es_obj->dma_attrs);
		if (ret < 0) {
			DRM_DEV_ERROR(dev->dev, "failed to get sgtable.\n");
			goto err_mem_free;
		}

		if (drm_prime_sg_to_page_array(&sgt, es_obj->pages, nr_pages)) {
			DRM_DEV_ERROR(dev->dev, "invalid sgtable.\n");
			ret = -EINVAL;
			goto err_sgt_free;
		}

		sg_free_table(&sgt);
	}

	return 0;

err_sgt_free:
	sg_free_table(&sgt);
err_mem_free:
	if (!es_obj->get_pages)
		dma_free_attrs(to_dma_dev(dev), es_obj->size, es_obj->cookie,
			       es_obj->dma_addr, es_obj->dma_attrs);
	else
		put_pages(nr_pages, es_obj);
err_free:
	kvfree(es_obj->pages);

	return ret;
}

static void es_gem_free_buf(struct es_gem_object *es_obj)
{
	struct drm_device *dev = es_obj->base.dev;
#ifdef CONFIG_ESWIN_MMU
	struct es_drm_private *priv = dev->dev_private;
	unsigned int nr_pages;
#endif

	if ((!es_obj->get_pages) && (!es_obj->dma_addr)) {
		DRM_DEV_DEBUG_KMS(dev->dev, "dma_addr is invalid.\n");
		return;
	}

#ifdef CONFIG_ESWIN_MMU
	if (!priv->mmu) {
		DRM_DEV_ERROR(dev->dev, "invalid mmu.\n");
		return;
	}

	if (!es_obj->sgt) { // dumb buffer release
		nr_pages = es_obj->size >> PAGE_SHIFT;
		if (es_obj->iova) {
			dc_mmu_unmap_memory(priv->mmu, es_obj->iova, nr_pages);
		}
	} else { // prime buffer release
		if (es_obj->iova_list) {
			if (es_obj->iova_list->iova) {
				dc_mmu_unmap_memory(
					priv->mmu, es_obj->iova_list->iova,
					es_obj->iova_list->nr_pages);
				kfree(es_obj->iova_list);
			}
		}
	}
#endif

	if (!es_obj->get_pages) {
		dma_free_attrs(to_dma_dev(dev), es_obj->size, es_obj->cookie,
			       (dma_addr_t)es_obj->dma_addr, es_obj->dma_attrs);
	} else {
		if (!es_obj->dma_addr) {
			DRM_DEV_ERROR(dev->dev, "No dma addr allocated, no need to free\n");
			return;
		}
		put_pages(es_obj->size >> PAGE_SHIFT, es_obj);
	}

	kvfree(es_obj->pages);
}

static void es_gem_free_object(struct drm_gem_object *obj)
{
	struct es_gem_object *es_obj = to_es_gem_object(obj);

#ifdef CONFIG_ESWIN_MMU
	if (es_obj)
		es_gem_free_buf(es_obj);
#endif
	if (obj->import_attach) {
		drm_prime_gem_destroy(obj, es_obj->sgt);
	}

	drm_gem_object_release(obj);

	kfree(es_obj);
}

static struct es_gem_object *es_gem_alloc_object(struct drm_device *dev,
						 size_t size)
{
	struct es_gem_object *es_obj;
	struct drm_gem_object *obj;
	int ret;

	es_obj = kzalloc(sizeof(*es_obj), GFP_KERNEL);
	if (!es_obj)
		return ERR_PTR(-ENOMEM);

	es_obj->size = size;
	obj = &es_obj->base;

	ret = drm_gem_object_init(dev, obj, size);
	if (ret)
		goto err_free;

	es_obj->base.funcs = &es_gem_default_funcs;

	ret = drm_gem_create_mmap_offset(obj);
	if (ret) {
		drm_gem_object_release(obj);
		goto err_free;
	}

	return es_obj;

err_free:
	kfree(es_obj);
	return ERR_PTR(ret);
}

struct es_gem_object *es_gem_create_object(struct drm_device *dev, size_t size)
{
	struct es_gem_object *es_obj;
	int ret;

	size = PAGE_ALIGN(size);

	es_obj = es_gem_alloc_object(dev, size);
	if (IS_ERR(es_obj))
		return es_obj;

	ret = es_gem_alloc_buf(es_obj);
	if (ret) {
		drm_gem_object_release(&es_obj->base);
		kfree(es_obj);
		return ERR_PTR(ret);
	}

	return es_obj;
}

static struct es_gem_object *es_gem_create_with_handle(struct drm_device *dev,
						       struct drm_file *file,
						       size_t size,
						       unsigned int *handle)
{
	struct es_gem_object *es_obj;
	struct drm_gem_object *obj;
	int ret;

	es_obj = es_gem_create_object(dev, size);
	if (IS_ERR(es_obj))
		return es_obj;

	obj = &es_obj->base;

	ret = drm_gem_handle_create(file, obj, handle);
	drm_gem_object_put(obj);
	if (ret) {
		pr_err("Drm GEM handle create failed\n");
		return ERR_PTR(ret);
	}

	return es_obj;
}

static int es_gem_mmap_obj(struct drm_gem_object *obj,
			   struct vm_area_struct *vma)
{
	struct es_gem_object *es_obj = to_es_gem_object(obj);
	struct drm_device *drm_dev = es_obj->base.dev;
	unsigned long vm_size;
	int ret = 0;

	vm_size = vma->vm_end - vma->vm_start;
	if (vm_size > es_obj->size)
		return -EINVAL;

	vma->vm_pgoff = 0;

	if (!es_obj->get_pages) {
		vm_flags_clear(vma, VM_PFNMAP);

		ret = dma_mmap_attrs(to_dma_dev(drm_dev), vma, es_obj->cookie,
				     es_obj->dma_addr, es_obj->size,
				     es_obj->dma_attrs);
	} else {
		u32 i, nr_pages, pfn = 0U;
		unsigned long start;

		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
		vm_flags_set(vma, VM_IO | VM_DONTCOPY | VM_DONTEXPAND | VM_DONTDUMP);
		start = vma->vm_start;
		vm_size = PAGE_ALIGN(vm_size);
		nr_pages = vm_size >> PAGE_SHIFT;

		for (i = 0; i < nr_pages; i++) {
			pfn = page_to_pfn(es_obj->pages[i]);

			ret = remap_pfn_range(vma, start, pfn, PAGE_SIZE,
					      vma->vm_page_prot);
			if (ret < 0)
				break;

			start += PAGE_SIZE;
		}
	}

	if (ret)
		drm_gem_vm_close(vma);

	return ret;
}

struct sg_table *es_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct es_gem_object *es_obj = to_es_gem_object(obj);

	return drm_prime_pages_to_sg(obj->dev, es_obj->pages,
				     es_obj->size >> PAGE_SHIFT);
}

static int es_gem_prime_vmap(struct drm_gem_object *obj,
			     struct iosys_map *map)
{
	struct es_gem_object *es_obj = to_es_gem_object(obj);

	void * vaddr = es_obj->dma_attrs & DMA_ATTR_NO_KERNEL_MAPPING ?
		       page_address(es_obj->cookie) : es_obj->cookie;

	iosys_map_set_vaddr(map, vaddr);

	return 0;
}

static void es_gem_prime_vunmap(struct drm_gem_object *obj,
				struct iosys_map *map)
{
	/* Nothing to do */
}

static const struct vm_operations_struct es_vm_ops = {
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static const struct drm_gem_object_funcs es_gem_default_funcs = {
	.free = es_gem_free_object,
	.get_sg_table = es_gem_prime_get_sg_table,
	.vmap = es_gem_prime_vmap,
	.vunmap = es_gem_prime_vunmap,
	.vm_ops = &es_vm_ops,
};

int es_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
		       struct drm_mode_create_dumb *args)
{
	struct es_drm_private *priv = dev->dev_private;
	struct es_gem_object *es_obj;
	unsigned int pitch = args->width * DIV_ROUND_UP(args->bpp, 8);

	args->pitch = ALIGN(pitch, priv->pitch_alignment);
	args->size = PAGE_ALIGN(args->pitch * args->height);

	es_obj =
		es_gem_create_with_handle(dev, file, args->size, &args->handle);
	return PTR_ERR_OR_ZERO(es_obj);
}

struct drm_gem_object *es_gem_prime_import(struct drm_device *dev,
					   struct dma_buf *dma_buf)
{
	return drm_gem_prime_import_dev(dev, dma_buf, to_dma_dev(dev));
}

struct drm_gem_object *
es_gem_prime_import_sg_table(struct drm_device *dev,
			     struct dma_buf_attachment *attach,
			     struct sg_table *sgt)
{
	struct es_gem_object *es_obj;
	int npages;
	int ret;
	struct scatterlist *s = NULL;
	u32 i = 0;
	dma_addr_t expected;
	size_t size = attach->dmabuf->size;
#ifdef CONFIG_ESWIN_MMU
	u32 iova, j;
	struct scatterlist **splist;
	struct es_drm_private *priv = dev->dev_private;

	if (!priv->mmu) {
		DRM_ERROR("invalid mmu.\n");
		ret = -EINVAL;
		return ERR_PTR(ret);
	}
#endif

	size = PAGE_ALIGN(size);

	es_obj = es_gem_alloc_object(dev, size);
	if (IS_ERR(es_obj))
		return ERR_CAST(es_obj);

	npages = es_obj->size >> PAGE_SHIFT;
	es_obj->pages =
		kvmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (!es_obj->pages) {
		ret = -ENOMEM;
		goto err_gemalloc;
	}

	ret = drm_prime_sg_to_page_array(sgt, es_obj->pages, npages);
	if (ret)
		goto err_free_page;

	expected = sg_dma_address(sgt->sgl);
#ifdef CONFIG_ESWIN_MMU
	splist = (struct scatterlist **)kzalloc(sizeof(s) * sgt->nents,
						GFP_KERNEL);
	if (!splist) {
		DRM_ERROR("Allocate splist failed");
		ret = -ENOMEM;
		goto err_free_page;
	}

	es_obj->iova_list =
		(iova_info_t *)kzalloc(sizeof(iova_info_t), GFP_KERNEL);
	if (!es_obj->iova_list) {
		DRM_ERROR("Allocate splist failed");
		ret = -ENOMEM;
		goto err_sp;
	}

	for_each_sg (sgt->sgl, s, sgt->nents, i) {
		splist[i] = s;
	}
	i = 0;
	es_obj->nr_iova = sgt->nents;

	for (j = sgt->nents; j > 0; j--) {
		s = splist[j - 1];
#else
	for_each_sg (sgt->sgl, s, sgt->nents, i) {
#endif
		if (sg_dma_address(s) != expected) {
#ifndef CONFIG_ESWIN_MMU
			DRM_ERROR("sg_table is not contiguous");
			ret = -EINVAL;
			goto err;
#endif
		}

		if (sg_dma_len(s) & (PAGE_SIZE - 1)) {
			ret = -EINVAL;
			goto err;
		}

#ifdef CONFIG_ESWIN_MMU
		iova = 0;

		if (j == 1) {
			ret = dc_mmu_map_memory(priv->mmu, (u64)es_obj->pages,
						npages, &iova, false);
			if (ret) {
				DRM_ERROR("failed to do mmu map.\n");
				goto err;
			}
			es_obj->iova_list->iova = iova;
			es_obj->iova_list->nr_pages = npages;
		}

		if (i == 0)
			es_obj->iova = iova;
#else
		if (i == 0)
			es_obj->iova = sg_dma_address(s);
#endif

		expected = sg_dma_address(s) + sg_dma_len(s);
	}

	es_obj->dma_addr = sg_dma_address(sgt->sgl);

	es_obj->sgt = sgt;
#ifdef CONFIG_ESWIN_MMU
	kfree(splist);
#endif

	return &es_obj->base;

#ifdef CONFIG_ESWIN_MMU
err:
	kfree(es_obj->iova_list);
err_sp:
	kfree(splist);
#endif
err_free_page:
	kvfree(es_obj->pages);
err_gemalloc:
	es_gem_free_object(&es_obj->base);

	return ERR_PTR(ret);
}

int es_gem_prime_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	int ret = 0;

	ret = drm_gem_mmap_obj(obj, obj->size, vma);
	if (ret < 0)
		return ret;

	return es_gem_mmap_obj(obj, vma);
}

int es_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_gem_object *obj;
	int ret;

	ret = drm_gem_mmap(filp, vma);
	if (ret)
		return ret;

	obj = vma->vm_private_data;

	if (obj->import_attach)
		return dma_buf_mmap(obj->dma_buf, vma, 0);

	return es_gem_mmap_obj(obj, vma);
}
