/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 Sifive.
 */
#ifndef ASM_ERRATA_LIST_H
#define ASM_ERRATA_LIST_H

#include <asm/alternative.h>
#include <asm/csr.h>
#include <asm/insn-def.h>
#include <asm/hwcap.h>
#include <asm/vendorid_list.h>

#ifdef CONFIG_ERRATA_ANDES
#define ERRATA_ANDES_NO_IOCP 0
#define ERRATA_ANDES_NUMBER 1
#endif

#ifdef CONFIG_ERRATA_SIFIVE
#define	ERRATA_SIFIVE_CIP_453 0
#define	ERRATA_SIFIVE_CIP_1200 1
#define	ERRATA_SIFIVE_NUMBER 2
#endif

#ifdef CONFIG_ERRATA_THEAD
#define	ERRATA_THEAD_MAE 0
#define	ERRATA_THEAD_PMU 1
#define	ERRATA_THEAD_NUMBER 2
#endif

#ifdef __ASSEMBLY__

#define ALT_INSN_FAULT(x)						\
ALTERNATIVE(__stringify(RISCV_PTR do_trap_insn_fault),			\
	    __stringify(RISCV_PTR sifive_cip_453_insn_fault_trp),	\
	    SIFIVE_VENDOR_ID, ERRATA_SIFIVE_CIP_453,			\
	    CONFIG_ERRATA_SIFIVE_CIP_453)

#define ALT_PAGE_FAULT(x)						\
ALTERNATIVE(__stringify(RISCV_PTR do_page_fault),			\
	    __stringify(RISCV_PTR sifive_cip_453_page_fault_trp),	\
	    SIFIVE_VENDOR_ID, ERRATA_SIFIVE_CIP_453,			\
	    CONFIG_ERRATA_SIFIVE_CIP_453)
#else /* !__ASSEMBLY__ */

#define ALT_SFENCE_VMA_ASID(asid)					\
asm(ALTERNATIVE("sfence.vma x0, %0", "sfence.vma", SIFIVE_VENDOR_ID,	\
		ERRATA_SIFIVE_CIP_1200, CONFIG_ERRATA_SIFIVE_CIP_1200)	\
		: : "r" (asid) : "memory")

#define ALT_SFENCE_VMA_ADDR(addr)					\
asm(ALTERNATIVE("sfence.vma %0", "sfence.vma", SIFIVE_VENDOR_ID,	\
		ERRATA_SIFIVE_CIP_1200, CONFIG_ERRATA_SIFIVE_CIP_1200)	\
		: : "r" (addr) : "memory")

#define ALT_SFENCE_VMA_ADDR_ASID(addr, asid)				\
asm(ALTERNATIVE("sfence.vma %0, %1", "sfence.vma", SIFIVE_VENDOR_ID,	\
		ERRATA_SIFIVE_CIP_1200, CONFIG_ERRATA_SIFIVE_CIP_1200)	\
		: : "r" (addr), "r" (asid) : "memory")

/*
 * dcache.ipa rs1 (invalidate, physical address)
 * | 31 - 25 | 24 - 20 | 19 - 15 | 14 - 12 | 11 - 7 | 6 - 0 |
 *   0000001    01010      rs1       000      00000  0001011
 * dache.iva rs1 (invalida, virtual address)
 *   0000001    00110      rs1       000      00000  0001011
 *
 * dcache.cpa rs1 (clean, physical address)
 * | 31 - 25 | 24 - 20 | 19 - 15 | 14 - 12 | 11 - 7 | 6 - 0 |
 *   0000001    01001      rs1       000      00000  0001011
 * dcache.cva rs1 (clean, virtual address)
 *   0000001    00101      rs1       000      00000  0001011
 *
 * dcache.cipa rs1 (clean then invalidate, physical address)
 * | 31 - 25 | 24 - 20 | 19 - 15 | 14 - 12 | 11 - 7 | 6 - 0 |
 *   0000001    01011      rs1       000      00000  0001011
 * dcache.civa rs1 (... virtual address)
 *   0000001    00111      rs1       000      00000  0001011
 *
 * sync.s (make sure all cache operations finished)
 * | 31 - 25 | 24 - 20 | 19 - 15 | 14 - 12 | 11 - 7 | 6 - 0 |
 *   0000000    11001     00000      000      00000  0001011
 */
#define THEAD_inval_A0	".long 0x0265000b"
#define THEAD_clean_A0	".long 0x0255000b"
#define THEAD_flush_A0	".long 0x0275000b"
#define THEAD_SYNC_S	".long 0x0190000b"

#define ALT_CMO_OP(_op, _start, _size, _cachesize)			\
asm volatile(ALTERNATIVE(						\
	__nops(5),							\
	"mv a0, %1\n\t"							\
	"j 2f\n\t"							\
	"3:\n\t"							\
	CBO_##_op(a0)							\
	"add a0, a0, %0\n\t"						\
	"2:\n\t"							\
	"bltu a0, %2, 3b\n\t",						\
	0, RISCV_ISA_EXT_ZICBOM, CONFIG_RISCV_ISA_ZICBOM)		\
	: : "r"(_cachesize),						\
	    "r"((unsigned long)(_start) & ~((_cachesize) - 1UL)),	\
	    "r"((unsigned long)(_start) + (_size))			\
	: "a0")

#define THEAD_C9XX_RV_IRQ_PMU			17
#define THEAD_C9XX_CSR_SCOUNTEROF		0x5c5

#endif /* __ASSEMBLY__ */

#endif
