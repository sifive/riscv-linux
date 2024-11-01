/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_PGTABLE_32_H
#define _ASM_RISCV_PGTABLE_32_H

#include <asm-generic/pgtable-nopmd.h>
#include <linux/bits.h>
#include <linux/const.h>

/* Size of region mapped by a page global directory */
#define PGDIR_SHIFT     22
#define PGDIR_SIZE      (_AC(1, UL) << PGDIR_SHIFT)
#define PGDIR_MASK      (~(PGDIR_SIZE - 1))

#define MAX_POSSIBLE_PHYSMEM_BITS 34

#define ALT_FIXUP_MT(_val)
#define ALT_UNFIX_MT(_val)

#define pud_pfn(pud)				(pmd_pfn((pmd_t){ pud }))
#define p4d_pfn(p4d)				(pud_pfn((pud_t){ p4d }))
#define pgd_pfn(pgd)				(p4d_pfn((p4d_t){ pgd }))

static const __maybe_unused int pgtable_l4_enabled;
static const __maybe_unused int pgtable_l5_enabled;

#endif /* _ASM_RISCV_PGTABLE_32_H */
