/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */

#ifndef _DT_BINDINGS_RISCV_PHYSICAL_MEMORY_H
#define _DT_BINDINGS_RISCV_PHYSICAL_MEMORY_H

#define PMA_READ			(1 << 0)
#define PMA_WRITE			(1 << 1)
#define PMA_EXECUTE			(1 << 2)
#define PMA_AMO_MASK			(3 << 4)
#define PMA_AMO_NONE			(0 << 4)
#define PMA_AMO_SWAP			(1 << 4)
#define PMA_AMO_LOGICAL			(2 << 4)
#define PMA_AMO_ARITHMETIC		(3 << 4)
#define PMA_RSRV_MASK			(3 << 6)
#define PMA_RSRV_NONE			(0 << 6)
#define PMA_RSRV_NON_EVENTUAL		(1 << 6)
#define PMA_RSRV_EVENTUAL		(2 << 6)

#define PMA_RW				(PMA_READ | PMA_WRITE)
#define PMA_RWA				(PMA_RW | PMA_AMO_ARITHMETIC | PMA_RSRV_EVENTUAL)
#define PMA_RWX				(PMA_RW | PMA_EXECUTE)
#define PMA_RWXA			(PMA_RWA | PMA_EXECUTE)

#define PMA_ORDER_MASK			(3 << 8)
#define PMA_ORDER_IO_RELAXED		(0 << 8)
#define PMA_ORDER_IO_STRONG		(1 << 8)
#define PMA_ORDER_MEMORY		(2 << 8)
#define PMA_READ_IDEMPOTENT		(1 << 10)
#define PMA_WRITE_IDEMPOTENT		(1 << 11)
#define PMA_CACHEABLE			(1 << 12)
#define PMA_COHERENT			(1 << 13)

#define PMA_UNSAFE			(1 << 15)

#define PMA_IO				(PMA_ORDER_IO_RELAXED)
#define PMA_NONCACHEABLE_MEMORY		(PMA_ORDER_MEMORY | PMA_READ_IDEMPOTENT | \
						PMA_WRITE_IDEMPOTENT)
#define PMA_NONCOHERENT_MEMORY		(PMA_NONCACHEABLE_MEMORY | PMA_CACHEABLE)
#define PMA_NORMAL_MEMORY		(PMA_NONCOHERENT_MEMORY | PMA_COHERENT)

#define PMR_ALIAS_MASK			(0xff << 24)
#define PMR_ALIAS(n)			((n) << 24)

#endif /* _DT_BINDINGS_RISCV_PHYSICAL_MEMORY_H */
