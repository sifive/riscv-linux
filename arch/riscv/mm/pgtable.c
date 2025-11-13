// SPDX-License-Identifier: GPL-2.0

#include <asm/pgalloc.h>
#include <dt-bindings/riscv/physical-memory.h>
#include <linux/bitfield.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/pgtable.h>

int ptep_set_access_flags(struct vm_area_struct *vma,
			  unsigned long address, pte_t *ptep,
			  pte_t entry, int dirty)
{
	asm goto(ALTERNATIVE("nop", "j %l[svvptc]", 0, RISCV_ISA_EXT_SVVPTC, 1)
		 : : : : svvptc);

	if (!pte_same(ptep_get(ptep), entry))
		__set_pte_at(vma->vm_mm, ptep, entry);
	/*
	 * update_mmu_cache will unconditionally execute, handling both
	 * the case that the PTE changed and the spurious fault case.
	 */
	return true;

svvptc:
	if (!pte_same(ptep_get(ptep), entry)) {
		__set_pte_at(vma->vm_mm, ptep, entry);
		/* Here only not svadu is impacted */
		flush_tlb_page(vma, address);
		return true;
	}

	return false;
}

int ptep_test_and_clear_young(struct vm_area_struct *vma,
			      unsigned long address,
			      pte_t *ptep)
{
	if (!pte_young(ptep_get(ptep)))
		return 0;
	return test_and_clear_bit(_PAGE_ACCESSED_OFFSET, &pte_val(*ptep));
}
EXPORT_SYMBOL_GPL(ptep_test_and_clear_young);

#ifdef CONFIG_64BIT
pud_t *pud_offset(p4d_t *p4dp, unsigned long address)
{
	return pud_offset_lockless(p4dp, p4dp_get(p4dp), address);
}

p4d_t *p4d_offset(pgd_t *pgdp, unsigned long address)
{
	return p4d_offset_lockless(pgdp, pgdp_get(pgdp), address);
}
#endif

#ifdef CONFIG_HAVE_ARCH_HUGE_VMAP
int p4d_set_huge(p4d_t *p4d, phys_addr_t addr, pgprot_t prot)
{
	return 0;
}

void p4d_clear_huge(p4d_t *p4d)
{
}

int pud_set_huge(pud_t *pud, phys_addr_t phys, pgprot_t prot)
{
	pud_t new_pud = pfn_pud(__phys_to_pfn(phys), prot);

	set_pud(pud, new_pud);
	return 1;
}

int pud_clear_huge(pud_t *pud)
{
	if (!pud_leaf(pudp_get(pud)))
		return 0;
	pud_clear(pud);
	return 1;
}

int pud_free_pmd_page(pud_t *pud, unsigned long addr)
{
	pmd_t *pmd = pud_pgtable(pudp_get(pud));
	int i;

	pud_clear(pud);

	flush_tlb_kernel_range(addr, addr + PUD_SIZE);

	for (i = 0; i < PTRS_PER_PMD; i++) {
		if (!pmd_none(pmdp_get(pmd + i))) {
			pte_t *pte = (pte_t *)pmd_page_vaddr(pmdp_get(pmd + i));

			pte_free_kernel(NULL, pte);
		}
	}

	pmd_free(NULL, pmd);

	return 1;
}

int pmd_set_huge(pmd_t *pmd, phys_addr_t phys, pgprot_t prot)
{
	pmd_t new_pmd = pfn_pmd(__phys_to_pfn(phys), prot);

	set_pmd(pmd, new_pmd);
	return 1;
}

int pmd_clear_huge(pmd_t *pmd)
{
	if (!pmd_leaf(pmdp_get(pmd)))
		return 0;
	pmd_clear(pmd);
	return 1;
}

int pmd_free_pte_page(pmd_t *pmd, unsigned long addr)
{
	pte_t *pte = (pte_t *)pmd_page_vaddr(pmdp_get(pmd));

	pmd_clear(pmd);

	flush_tlb_kernel_range(addr, addr + PMD_SIZE);
	pte_free_kernel(NULL, pte);
	return 1;
}

#endif /* CONFIG_HAVE_ARCH_HUGE_VMAP */
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
pmd_t pmdp_collapse_flush(struct vm_area_struct *vma,
					unsigned long address, pmd_t *pmdp)
{
	pmd_t pmd = pmdp_huge_get_and_clear(vma->vm_mm, address, pmdp);

	VM_BUG_ON(address & ~HPAGE_PMD_MASK);
	VM_BUG_ON(pmd_trans_huge(pmdp_get(pmdp)));
	/*
	 * When leaf PTE entries (regular pages) are collapsed into a leaf
	 * PMD entry (huge page), a valid non-leaf PTE is converted into a
	 * valid leaf PTE at the level 1 page table.  Since the sfence.vma
	 * forms that specify an address only apply to leaf PTEs, we need a
	 * global flush here.  collapse_huge_page() assumes these flushes are
	 * eager, so just do the fence here.
	 */
	flush_tlb_mm(vma->vm_mm);
	return pmd;
}

pud_t pudp_invalidate(struct vm_area_struct *vma, unsigned long address,
		      pud_t *pudp)
{
	VM_WARN_ON_ONCE(!pud_present(pudp_get(pudp)));
	pud_t old = pudp_establish(vma, address, pudp,
				   pud_mkinvalid(pudp_get(pudp)));

	flush_pud_tlb_range(vma, address, address + HPAGE_PUD_SIZE);
	return old;
}
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */

#ifdef CONFIG_RISCV_ISA_XLINUXMEMALIAS
struct memory_alias_pair {
	unsigned long cached_base;
	unsigned long noncached_base;
	unsigned long size;
	int index;
} memory_alias_pairs[5];

bool __init riscv_have_memory_alias(void)
{
	return memory_alias_pairs[0].size;
}

void __init riscv_init_memory_alias(void)
{
	int na = of_n_addr_cells(of_root);
	int ns = of_n_size_cells(of_root);
	int nc = na + ns + 2;
	const __be32 *prop;
	int pairs = 0;
	int len;

	prop = of_get_property(of_root, "riscv,physical-memory-regions", &len);
	if (!prop)
		return;

	len /= sizeof(__be32);
	for (int i = 0; len >= nc; i++, prop += nc, len -= nc) {
		unsigned long base = of_read_ulong(prop, na);
		unsigned long size = of_read_ulong(prop + na, ns);
		unsigned long flags = be32_to_cpup(prop + na + ns);
		struct memory_alias_pair *pair;

		/* We only care about non-coherent memory. */
		if ((flags & PMA_ORDER_MASK) != PMA_ORDER_MEMORY || (flags & PMA_COHERENT))
			continue;

		/* The cacheable alias must be usable memory. */
		if ((flags & PMA_CACHEABLE) &&
		    !memblock_overlaps_region(&memblock.memory, base, size))
			continue;

		if (flags & PMR_IS_ALIAS) {
			int alias = FIELD_GET(PMR_ALIAS_MASK, flags);

			pair = NULL;
			for (int j = 0; j < pairs; j++) {
				if (alias == memory_alias_pairs[j].index) {
					pair = &memory_alias_pairs[j];
					break;
				}
			}
			if (!pair)
				continue;
		} else {
			/* Leave room for the null sentinel. */
			if (pairs == ARRAY_SIZE(memory_alias_pairs) - 1)
				continue;
			pair = &memory_alias_pairs[pairs++];
			pair->index = i;
		}

		/* Align the address and size with the page table PFN field. */
		base >>= PAGE_SHIFT - _PAGE_PFN_SHIFT;
		size >>= PAGE_SHIFT - _PAGE_PFN_SHIFT;

		if (flags & PMA_CACHEABLE)
			pair->cached_base = base;
		else
			pair->noncached_base = base;
		pair->size = min_not_zero(pair->size, size);
	}

	/* Remove any unmatched pairs. */
	for (int i = 0; i < pairs; i++) {
		struct memory_alias_pair *pair = &memory_alias_pairs[i];

		if (pair->cached_base && pair->noncached_base && pair->size)
			continue;

		for (int j = i + 1; j < pairs; j++)
			memory_alias_pairs[j - 1] = memory_alias_pairs[j];
		memory_alias_pairs[--pairs].size = 0;
	}
}
#endif /* CONFIG_RISCV_ISA_XLINUXMEMALIAS */
