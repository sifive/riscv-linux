/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2024 SiFive
 */

#include <linux/bits.h>
#include <linux/linkage.h>
#include <asm/asm.h>
#include <asm/pgtable.h>

#define CACHED_BASE_OFFSET	(0 * RISCV_SZPTR)
#define NONCACHED_BASE_OFFSET	(1 * RISCV_SZPTR)
#define SIZE_OFFSET		(2 * RISCV_SZPTR)

#define SIZEOF_PAIR		(4 * RISCV_SZPTR)

SYM_CODE_START(riscv_fixup_memory_alias)
	addi	sp, sp, -4 * SZREG
	REG_S	t2, (0 * SZREG)(sp)
	REG_S	t3, (1 * SZREG)(sp)
	REG_S	t4, (2 * SZREG)(sp)
#ifdef CONFIG_RISCV_ISA_SVNAPOT
	REG_S	t5, (3 * SZREG)(sp)

	/* Save and mask off _PAGE_NAPOT if present. */
	li	t5, _PAGE_NAPOT
	and	t5, t1, t5
	xor	t1, t1, t5
#endif

	lla	t2, memory_alias_pairs
.Lfixup_loop:
	REG_L	t3, SIZE_OFFSET(t2)
	beqz	t3, .Lfixup_end
	REG_L	t4, CACHED_BASE_OFFSET(t2)
	sub	t4, t1, t4
	bltu	t4, t3, .Lfixup_found
	addi	t2, t2, SIZEOF_PAIR
	j	.Lfixup_loop

.Lfixup_found:
	REG_L	t3, NONCACHED_BASE_OFFSET(t2)
	add	t1, t3, t4

.Lfixup_end:
#ifdef CONFIG_RISCV_ISA_SVNAPOT
	xor	t1, t1, t5

	REG_L	t5, (3 * SZREG)(sp)
#endif
	REG_L	t4, (2 * SZREG)(sp)
	REG_L	t3, (1 * SZREG)(sp)
	REG_L	t2, (0 * SZREG)(sp)
	addi	sp, sp, 4 * SZREG
	jr	t0
SYM_CODE_END(riscv_fixup_memory_alias)

SYM_CODE_START(riscv_unfix_memory_alias)
	addi	sp, sp, -4 * SZREG
	REG_S	t2, (0 * SZREG)(sp)
	REG_S	t3, (1 * SZREG)(sp)
	REG_S	t4, (2 * SZREG)(sp)
#ifdef CONFIG_RISCV_ISA_SVNAPOT
	REG_S	t5, (3 * SZREG)(sp)

	/* Save and mask off _PAGE_NAPOT if present. */
	li	t5, _PAGE_NAPOT
	and	t5, t1, t5
	xor	t1, t1, t5
#endif

	lla	t2, memory_alias_pairs
.Lunfix_loop:
	REG_L	t3, SIZE_OFFSET(t2)
	beqz	t3, .Lunfix_end
	REG_L	t4, NONCACHED_BASE_OFFSET(t2)
	sub	t4, t1, t4
	bltu	t4, t3, .Lunfix_found
	addi	t2, t2, SIZEOF_PAIR
	j	.Lunfix_loop

.Lunfix_found:
	REG_L	t3, CACHED_BASE_OFFSET(t2)
	add	t1, t3, t4

	/* PFN was in the noncached alias, so mark it as such. */
	li	t2, _PAGE_NOCACHE
	or	t1, t1, t2

.Lunfix_end:
#ifdef CONFIG_RISCV_ISA_SVNAPOT
	xor	t1, t1, t5

	REG_L	t5, (3 * SZREG)(sp)
#endif
	REG_L	t4, (2 * SZREG)(sp)
	REG_L	t3, (1 * SZREG)(sp)
	REG_L	t2, (0 * SZREG)(sp)
	addi	sp, sp, 4 * SZREG
	jr	t0
SYM_CODE_END(riscv_unfix_memory_alias)
