/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_PGTABLE_BITS_H
#define _ASM_RISCV_PGTABLE_BITS_H

/*
 * rv32 PTE format:
 * | XLEN-1  10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *       PFN      reserved for SW   D   A   G   U   X   W   R   V
 *
 * rv64 PTE format:
 * | 63 | 62 61 | 60 54 | 53 10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *   N      MT     RSV     PFN    reserved for SW   D   A   G   U   X   W   R   V
 */

#define _PAGE_ACCESSED_OFFSET 6

#define _PAGE_PRESENT   (1 << 0)
#define _PAGE_READ      (1 << 1)    /* Readable */
#define _PAGE_WRITE     (1 << 2)    /* Writable */
#define _PAGE_EXEC      (1 << 3)    /* Executable */
#define _PAGE_USER      (1 << 4)    /* User */
#define _PAGE_GLOBAL    (1 << 5)    /* Global */
#define _PAGE_ACCESSED  (1 << 6)    /* Set by hardware on any access */
#define _PAGE_DIRTY     (1 << 7)    /* Set by hardware on any write */
#define _PAGE_SOFT      (3 << 8)    /* Reserved for software */

#define _PAGE_SPECIAL   (1 << 8)    /* RSW: 0x1 */
#define _PAGE_DEVMAP    (1 << 9)    /* RSW, devmap */
#define _PAGE_TABLE     _PAGE_PRESENT

#define _PAGE_PFN_SHIFT		10
#ifdef CONFIG_64BIT
#define _PAGE_PFN_MASK		GENMASK(53, 10)
#else
#define _PAGE_PFN_MASK		GENMASK(31, 10)
#endif /* CONFIG_64BIT */

#ifdef CONFIG_RISCV_ISA_SVPBMT
/*
 * [62:61] Svpbmt Memory Type definitions:
 *
 *  00 - PMA    Normal Cacheable, No change to implied PMA memory type
 *  01 - NC     Non-cacheable, idempotent, weakly-ordered Main Memory
 *  10 - IO     Non-cacheable, non-idempotent, strongly-ordered I/O memory
 *  11 - Rsvd   Reserved for future standard use
 */
#define _PAGE_NOCACHE		(UL(1) << 61)
#define _PAGE_IO		(UL(2) << 61)
#define _PAGE_MTMASK		(UL(3) << 61)
#else
#define _PAGE_NOCACHE		0
#define _PAGE_IO		0
#define _PAGE_MTMASK		0
#endif /* CONFIG_RISCV_ISA_SVPBMT */

#ifdef CONFIG_RISCV_ISA_SVNAPOT
#define _PAGE_NAPOT_SHIFT	63
#define _PAGE_NAPOT		BIT(_PAGE_NAPOT_SHIFT)
#endif /* CONFIG_RISCV_ISA_SVNAPOT */

/*
 * _PAGE_PROT_NONE is set on not-present pages (and ignored by the hardware) to
 * distinguish them from swapped out pages
 */
#define _PAGE_PROT_NONE _PAGE_GLOBAL

/* Used for swap PTEs only. */
#define _PAGE_SWP_EXCLUSIVE _PAGE_ACCESSED

/*
 * when all of R/W/X are zero, the PTE is a pointer to the next level
 * of the page table; otherwise, it is a leaf PTE.
 */
#define _PAGE_LEAF (_PAGE_READ | _PAGE_WRITE | _PAGE_EXEC)

#endif /* _ASM_RISCV_PGTABLE_BITS_H */
