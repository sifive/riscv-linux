/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_PGTABLE_64_H
#define _ASM_RISCV_PGTABLE_64_H

#include <linux/bits.h>
#include <linux/const.h>
#include <asm/alternative-macros.h>

extern bool pgtable_l4_enabled;
extern bool pgtable_l5_enabled;

#define PGDIR_SHIFT_L3  30
#define PGDIR_SHIFT_L4  39
#define PGDIR_SHIFT_L5  48
#define PGDIR_SHIFT     (pgtable_l5_enabled ? PGDIR_SHIFT_L5 : \
		(pgtable_l4_enabled ? PGDIR_SHIFT_L4 : PGDIR_SHIFT_L3))
/* Size of region mapped by a page global directory */
#define PGDIR_SIZE      (_AC(1, UL) << PGDIR_SHIFT)
#define PGDIR_MASK      (~(PGDIR_SIZE - 1))

/* p4d is folded into pgd in case of 4-level page table */
#define P4D_SHIFT_L3   30
#define P4D_SHIFT_L4   39
#define P4D_SHIFT_L5   39
#define P4D_SHIFT      (pgtable_l5_enabled ? P4D_SHIFT_L5 : \
		(pgtable_l4_enabled ? P4D_SHIFT_L4 : P4D_SHIFT_L3))
#define P4D_SIZE       (_AC(1, UL) << P4D_SHIFT)
#define P4D_MASK       (~(P4D_SIZE - 1))

/* pud is folded into pgd in case of 3-level page table */
#define PUD_SHIFT      30
#define PUD_SIZE       (_AC(1, UL) << PUD_SHIFT)
#define PUD_MASK       (~(PUD_SIZE - 1))

#define PMD_SHIFT       21
/* Size of region mapped by a page middle directory */
#define PMD_SIZE        (_AC(1, UL) << PMD_SHIFT)
#define PMD_MASK        (~(PMD_SIZE - 1))

/* Page 4th Directory entry */
typedef struct {
	unsigned long p4d;
} p4d_t;

#define p4d_val(x)	((x).p4d)
#define __p4d(x)	((p4d_t) { (x) })
#define PTRS_PER_P4D	(PAGE_SIZE / sizeof(p4d_t))

/* Page Upper Directory entry */
typedef struct {
	unsigned long pud;
} pud_t;

#define pud_val(x)      ((x).pud)
#define __pud(x)        ((pud_t) { (x) })
#define PTRS_PER_PUD    (PAGE_SIZE / sizeof(pud_t))

/* Page Middle Directory entry */
typedef struct {
	unsigned long pmd;
} pmd_t;

#define pmd_val(x)      ((x).pmd)
#define __pmd(x)        ((pmd_t) { (x) })
#define PTRS_PER_PMD    (PAGE_SIZE / sizeof(pmd_t))

#define MAX_POSSIBLE_PHYSMEM_BITS 56

/*
 * Only 64KB (order 4) napot ptes supported.
 */
#define NAPOT_CONT_ORDER_BASE 4
enum napot_cont_order {
	NAPOT_CONT64KB_ORDER = NAPOT_CONT_ORDER_BASE,
	NAPOT_ORDER_MAX,
};

#define for_each_napot_order(order)						\
	for (order = NAPOT_CONT_ORDER_BASE; order < NAPOT_ORDER_MAX; order++)
#define for_each_napot_order_rev(order)						\
	for (order = NAPOT_ORDER_MAX - 1;					\
	     order >= NAPOT_CONT_ORDER_BASE; order--)
#define napot_cont_order(val)	(__builtin_ctzl((val.pte >> _PAGE_PFN_SHIFT) << 1))

#define napot_cont_shift(order)	((order) + PAGE_SHIFT)
#define napot_cont_size(order)	BIT(napot_cont_shift(order))
#define napot_cont_mask(order)	(~(napot_cont_size(order) - 1UL))
#define napot_pte_num(order)	BIT(order)

#ifdef CONFIG_RISCV_ISA_SVNAPOT
#define HUGE_MAX_HSTATE		(2 + (NAPOT_ORDER_MAX - NAPOT_CONT_ORDER_BASE))
#else
#define HUGE_MAX_HSTATE		2
#endif

#if defined(CONFIG_RISCV_ISA_SVPBMT) || defined(CONFIG_RISCV_ISA_XLINUXMEMALIAS) || \
	defined(CONFIG_ERRATA_THEAD_MAE)

/*
 * ALT_FIXUP_MT
 *
 * On systems that do not support any form of page-based memory type
 * configuration, this code sequence clears the memory type bits in the PTE.
 *
 * On systems that support Svpbmt, the memory type bits are left alone.
 *
 * On systems that support XLinuxMemalias, PTEs with a nonzero memory type have
 * the memory type bits cleared and the PFN replaced with the matching alias.
 *
 * On systems that support XTheadMae, a Svpbmt memory type is transformed
 * into the corresponding XTheadMae memory type.
 *
 * [63:59] T-Head Memory Type definitions:
 * bit[63] SO - Strong Order
 * bit[62] C - Cacheable
 * bit[61] B - Bufferable
 * bit[60] SH - Shareable
 * bit[59] Sec - Trustable
 * 01110 - PMA  Weakly-ordered, Cacheable, Bufferable, Shareable, Non-trustable
 * 00110 - NC   Weakly-ordered, Non-cacheable, Bufferable, Shareable, Non-trustable
 * 10010 - IO   Strongly-ordered, Non-cacheable, Non-bufferable, Shareable, Non-trustable
 *
 * Pseudocode operating on bits [63:60]:
 *   t0 = mt << 1
 *   if (t0 == 0)
 *     t0 |= 2
 *   t0 ^= 0x5
 *   mt ^= t0
 */

#define ALT_FIXUP_MT(_val)								\
	asm(ALTERNATIVE_3("addi	t0, zero, 0x3\n\t"					\
			  "slli	t0, t0, 61\n\t"						\
			  "not	t0, t0\n\t"						\
			  "and	%0, %0, t0\n\t"						\
			  "nop\n\t"							\
			  "nop\n\t"							\
			  "nop\n\t"							\
			  "nop",							\
			  __nops(8),							\
			  0, RISCV_ISA_EXT_SVPBMT, CONFIG_RISCV_ISA_SVPBMT,		\
			  "addi	t0, zero, 0x3\n\t"					\
			  "slli	t0, t0, 61\n\t"						\
			  "and	t0, %0, t0\n\t"						\
			  "beqz	t0, 2f\n\t"						\
			  "xor	t1, %0, t0\n\t"						\
			  "1: auipc t0, %%pcrel_hi(riscv_fixup_memory_alias)\n\t"	\
			  "jalr	t0, t0, %%pcrel_lo(1b)\n\t"				\
			  "mv	%0, t1\n"						\
			  "2:",								\
			  0, RISCV_ISA_EXT_XLINUXMEMALIAS,				\
				CONFIG_RISCV_ISA_XLINUXMEMALIAS,			\
			  "srli	t0, %0, 59\n\t"						\
			  "seqz	t1, t0\n\t"						\
			  "slli	t1, t1, 1\n\t"						\
			  "or	t0, t0, t1\n\t"						\
			  "xori	t0, t0, 0x5\n\t"					\
			  "slli	t0, t0, 60\n\t"						\
			  "xor	%0, %0, t0\n\t"						\
			  "nop",							\
			  THEAD_VENDOR_ID, ERRATA_THEAD_MAE, CONFIG_ERRATA_THEAD_MAE)	\
			  : "+r" (_val) :: "t0", "t1")

#else

#define ALT_FIXUP_MT(_val)

#endif /* CONFIG_RISCV_ISA_SVPBMT || CONFIG_RISCV_ISA_XLINUXMEMALIAS || CONFIG_ERRATA_THEAD_MAE */

#if defined(CONFIG_RISCV_ISA_XLINUXMEMALIAS) || defined(CONFIG_ERRATA_THEAD_MAE)

/*
 * ALT_UNFIX_MT
 *
 * On systems that support Svpbmt, or do not support any form of page-based
 * memory type configuration, the memory type bits are left alone.
 *
 * On systems that support XLinuxMemalias, PTEs with an aliased PFN have the
 * matching memory type set and the PFN replaced with the normal memory alias.
 *
 * On systems that support XTheadMae, the XTheadMae memory type (or zero) is
 * transformed back into the corresponding Svpbmt memory type.
 *
 * Pseudocode operating on bits [63:60]:
 *   t0 = mt & 0xd
 *   t0 ^= t0 >> 1
 *   mt ^= t0
 */

#define ALT_UNFIX_MT(_val)								\
	asm(ALTERNATIVE_2(__nops(6),							\
			  "mv	t1, %0\n\t"						\
			  "1: auipc t0, %%pcrel_hi(riscv_unfix_memory_alias)\n\t"	\
			  "jalr	t0, t0, %%pcrel_lo(1b)\n\t"				\
			  "mv	%0, t1\n\t"						\
			  "nop\n\t"							\
			  "nop",							\
			  0, RISCV_ISA_EXT_XLINUXMEMALIAS,				\
				CONFIG_RISCV_ISA_XLINUXMEMALIAS,			\
			  "srli	t0, %0, 60\n\t"						\
			  "andi	t0, t0, 0xd\n\t"					\
			  "srli	t1, t0, 1\n\t"						\
			  "xor	t0, t0, t1\n\t"						\
			  "slli	t0, t0, 60\n\t"						\
			  "xor	%0, %0, t0",						\
			  THEAD_VENDOR_ID, ERRATA_THEAD_MAE, CONFIG_ERRATA_THEAD_MAE)	\
			  : "+r" (_val) :: "t0", "t1")

#define ptep_get ptep_get
static inline pte_t ptep_get(pte_t *ptep)
{
	pte_t pte = READ_ONCE(*ptep);

	ALT_UNFIX_MT(pte);

	return pte;
}

#define pmdp_get pmdp_get
static inline pmd_t pmdp_get(pmd_t *pmdp)
{
	pmd_t pmd = READ_ONCE(*pmdp);

	ALT_UNFIX_MT(pmd);

	return pmd;
}

#define pudp_get pudp_get
static inline pud_t pudp_get(pud_t *pudp)
{
	pud_t pud = READ_ONCE(*pudp);

	ALT_UNFIX_MT(pud);

	return pud;
}

#define p4dp_get p4dp_get
static inline p4d_t p4dp_get(p4d_t *p4dp)
{
	p4d_t p4d = READ_ONCE(*p4dp);

	ALT_UNFIX_MT(p4d);

	return p4d;
}

#define pgdp_get pgdp_get
static inline pgd_t pgdp_get(pgd_t *pgdp)
{
	pgd_t pgd = READ_ONCE(*pgdp);

	ALT_UNFIX_MT(pgd);

	return pgd;
}

#else

#define ALT_UNFIX_MT(_val)

#endif /* CONFIG_RISCV_ISA_XLINUXMEMALIAS || CONFIG_ERRATA_THEAD_MAE */

static inline int pud_present(pud_t pud)
{
	return (pud_val(pud) & _PAGE_PRESENT);
}

static inline int pud_none(pud_t pud)
{
	return (pud_val(pud) == 0);
}

static inline int pud_bad(pud_t pud)
{
	return !pud_present(pud) || (pud_val(pud) & _PAGE_LEAF);
}

#define pud_leaf	pud_leaf
static inline bool pud_leaf(pud_t pud)
{
	return pud_present(pud) && (pud_val(pud) & _PAGE_LEAF);
}

static inline int pud_user(pud_t pud)
{
	return pud_val(pud) & _PAGE_USER;
}

static inline void set_pud(pud_t *pudp, pud_t pud)
{
	ALT_FIXUP_MT(pud);
	WRITE_ONCE(*pudp, pud);
}

static inline void pud_clear(pud_t *pudp)
{
	set_pud(pudp, __pud(0));
}

static inline pud_t pfn_pud(unsigned long pfn, pgprot_t prot)
{
	return __pud((pfn << _PAGE_PFN_SHIFT) | pgprot_val(prot));
}

static inline unsigned long _pud_pfn(pud_t pud)
{
	return __page_val_to_pfn(pud_val(pud));
}

static inline pmd_t *pud_pgtable(pud_t pud)
{
	return (pmd_t *)pfn_to_virt(__page_val_to_pfn(pud_val(pud)));
}

static inline struct page *pud_page(pud_t pud)
{
	return pfn_to_page(__page_val_to_pfn(pud_val(pud)));
}

#define mm_p4d_folded  mm_p4d_folded
static inline bool mm_p4d_folded(struct mm_struct *mm)
{
	if (pgtable_l5_enabled)
		return false;

	return true;
}

#define mm_pud_folded  mm_pud_folded
static inline bool mm_pud_folded(struct mm_struct *mm)
{
	if (pgtable_l4_enabled)
		return false;

	return true;
}

#define pmd_index(addr) (((addr) >> PMD_SHIFT) & (PTRS_PER_PMD - 1))

static inline pmd_t pfn_pmd(unsigned long pfn, pgprot_t prot)
{
	return __pmd((pfn << _PAGE_PFN_SHIFT) | pgprot_val(prot));
}

static inline unsigned long _pmd_pfn(pmd_t pmd)
{
	return __page_val_to_pfn(pmd_val(pmd));
}

#define pmd_offset_lockless(pudp, pud, address) \
	(pud_pgtable(pud) + pmd_index(address))

#define pmd_ERROR(e) \
	pr_err("%s:%d: bad pmd %016lx.\n", __FILE__, __LINE__, pmd_val(e))

#define pud_ERROR(e)   \
	pr_err("%s:%d: bad pud %016lx.\n", __FILE__, __LINE__, pud_val(e))

#define p4d_ERROR(e)   \
	pr_err("%s:%d: bad p4d %016lx.\n", __FILE__, __LINE__, p4d_val(e))

static inline void set_p4d(p4d_t *p4dp, p4d_t p4d)
{
	ALT_FIXUP_MT(p4d);
	WRITE_ONCE(*p4dp, p4d);
}

static inline int p4d_none(p4d_t p4d)
{
	if (pgtable_l4_enabled)
		return (p4d_val(p4d) == 0);

	return 0;
}

static inline int p4d_present(p4d_t p4d)
{
	if (pgtable_l4_enabled)
		return (p4d_val(p4d) & _PAGE_PRESENT);

	return 1;
}

static inline int p4d_bad(p4d_t p4d)
{
	if (pgtable_l4_enabled)
		return !p4d_present(p4d);

	return 0;
}

static inline void p4d_clear(p4d_t *p4d)
{
	if (pgtable_l4_enabled)
		set_p4d(p4d, __p4d(0));
}

static inline p4d_t pfn_p4d(unsigned long pfn, pgprot_t prot)
{
	return __p4d((pfn << _PAGE_PFN_SHIFT) | pgprot_val(prot));
}

static inline unsigned long _p4d_pfn(p4d_t p4d)
{
	return __page_val_to_pfn(p4d_val(p4d));
}

static inline pud_t *p4d_pgtable(p4d_t p4d)
{
	if (pgtable_l4_enabled)
		return (pud_t *)pfn_to_virt(__page_val_to_pfn(p4d_val(p4d)));

	return (pud_t *)pud_pgtable((pud_t) { p4d_val(p4d) });
}
#define p4d_page_vaddr(p4d)	((unsigned long)p4d_pgtable(p4d))

static inline struct page *p4d_page(p4d_t p4d)
{
	return pfn_to_page(__page_val_to_pfn(p4d_val(p4d)));
}

#define pud_index(addr) (((addr) >> PUD_SHIFT) & (PTRS_PER_PUD - 1))

#define pud_offset_lockless(p4dp, p4d, address) \
	(pgtable_l4_enabled ? p4d_pgtable(p4d) + pud_index(address) : (pud_t *)(p4dp))

#define pud_offset pud_offset
pud_t *pud_offset(p4d_t *p4dp, unsigned long address);

static inline void set_pgd(pgd_t *pgdp, pgd_t pgd)
{
	ALT_FIXUP_MT(pgd);
	WRITE_ONCE(*pgdp, pgd);
}

static inline int pgd_none(pgd_t pgd)
{
	if (pgtable_l5_enabled)
		return (pgd_val(pgd) == 0);

	return 0;
}

static inline int pgd_present(pgd_t pgd)
{
	if (pgtable_l5_enabled)
		return (pgd_val(pgd) & _PAGE_PRESENT);

	return 1;
}

static inline int pgd_bad(pgd_t pgd)
{
	if (pgtable_l5_enabled)
		return !pgd_present(pgd);

	return 0;
}

static inline void pgd_clear(pgd_t *pgd)
{
	if (pgtable_l5_enabled)
		set_pgd(pgd, __pgd(0));
}

static inline p4d_t *pgd_pgtable(pgd_t pgd)
{
	if (pgtable_l5_enabled)
		return (p4d_t *)pfn_to_virt(__page_val_to_pfn(pgd_val(pgd)));

	return (p4d_t *)p4d_pgtable((p4d_t) { pgd_val(pgd) });
}
#define pgd_page_vaddr(pgd)	((unsigned long)pgd_pgtable(pgd))

static inline struct page *pgd_page(pgd_t pgd)
{
	return pfn_to_page(__page_val_to_pfn(pgd_val(pgd)));
}
#define pgd_page(pgd)	pgd_page(pgd)

#define p4d_index(addr) (((addr) >> P4D_SHIFT) & (PTRS_PER_P4D - 1))

#define p4d_offset_lockless(pgdp, pgd, address) \
	(pgtable_l5_enabled ? pgd_pgtable(pgd) + p4d_index(address) : (p4d_t *)(pgdp))

#define p4d_offset p4d_offset
p4d_t *p4d_offset(pgd_t *pgdp, unsigned long address);

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
static inline pte_t pmd_pte(pmd_t pmd);
static inline pte_t pud_pte(pud_t pud);
#endif

#endif /* _ASM_RISCV_PGTABLE_64_H */
