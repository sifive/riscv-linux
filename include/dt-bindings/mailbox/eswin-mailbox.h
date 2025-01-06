// SPDX-License-Identifier: GPL-2.0
/*
 * ESWIN Mailbox Driver
 *
 * Copyright 2024, Beijing ESWIN Computing Technology Co., Ltd.. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Authors: HuangYiFeng<huangyifeng@eswincomputing.com>
 */

#ifndef _DTS_ESWIN_MAILBOX_H_
#define _DTS_ESWIN_MAILBOX_H_

#define ESWIN_MAILBOX_U84_TO_SCPU_REG_BASE   0x50a00000 /*maibox 0*/
#define ESWIN_MAILBOX_SCPU_TO_U84_REG_BASE   0x50a10000 /*maibox 1*/

#define ESWIN_MAILBOX_U84_TO_LPCPU_REG_BASE  0x50a20000 /*maibox 2*/
#define ESWIN_MAILBOX_LPCPU_TO_U84_REG_BASE  0x50a30000 /*maibox 3*/

#define ESWIN_MAILBOX_U84_TO_NPU_0_REG_BASE  0x50a40000 /*maibox 4*/
#define ESWIN_MAILBOX_NPU_0_TO_U84_REG_BASE  0x50a50000 /*maibox 5*/

#define ESWIN_MAILBOX_U84_TO_NPU_1_REG_BASE  0x50a60000 /*maibox 6*/
#define ESWIN_MAILBOX_NP1_0_TO_U84_REG_BASE  0x50a70000 /*maibox 7*/

#define ESWIN_MAILBOX_U84_TO_DSP_0_REG_BASE  0x50a80000 /*maibox 8*/
#define ESWIN_MAILBOX_DSP_0_TO_U84_REG_BASE  0x50a90000 /*maibox 9*/

#define ESWIN_MAILBOX_U84_TO_DSP_1_REG_BASE  0x50aa0000 /*maibox 10*/
#define ESWIN_MAILBOX_DSP_1_TO_U84_REG_BASE  0x50ab0000 /*maibox 11*/

#define ESWIN_MAILBOX_U84_TO_DSP_2_REG_BASE  0x50ac0000 /*maibox 12*/
#define ESWIN_MAILBOX_DSP_2_TO_U84_REG_BASE  0x50ad0000 /*maibox 13*/

#define ESWIN_MAILBOX_U84_TO_DSP_3_REG_BASE  0x50ae0000 /*maibox 14*/
#define ESWIN_MAILBOX_DSP_3_TO_U84_REG_BASE  0x50af0000 /*maibox 15*/

#define BIT0                              (1 << 0)
#define BIT1                              (1 << 1)
#define BIT2                              (1 << 2)
#define BIT3                              (1 << 3)
#define BIT4                              (1 << 4)
#define BIT5                              (1 << 5)
#define BIT6                              (1 << 6)
#define BIT7                              (1 << 7)
#define BIT8                              (1 << 8)
#define BIT9                              (1 << 9)
#define BIT10                             (1 << 10)
#define BIT11                             (1 << 11)
#define BIT12                             (1 << 12)
#define BIT13                             (1 << 13)
#define BIT14                             (1 << 14)
#define BIT31                             (1 << 31)

#define ESWIN_MAILBOX_WR_LOCK_BIT_U84      BIT0
#define ESWIN_MAILBOX_WR_LOCK_BIT_SCPU     BIT1
#define ESWIN_MAILBOX_WR_LOCK_BIT_LPCPU    BIT2
#define ESWIN_MAILBOX_WR_LOCK_BIT_NPU_0    BIT3
#define ESWIN_MAILBOX_WR_LOCK_BIT_NPU_1    BIT4
#define ESWIN_MAILBOX_WR_LOCK_BIT_DSP_0    BIT5
#define ESWIN_MAILBOX_WR_LOCK_BIT_DSP_1    BIT6
#define ESWIN_MAILBOX_WR_LOCK_BIT_DSP_2    BIT7
#define ESWIN_MAILBOX_WR_LOCK_BIT_DSP_3    BIT8


#define ESWIN_MAIBOX_U84_IRQ_BIT           BIT0
#define ESWIN_MAIBOX_SCPU_IRQ_BIT          BIT1
#define ESWIN_MAIBOX_LPCPU_IRQ_BIT         BIT2
#define ESWIN_MAIBOX_NPU_0_IRQ_BIT         BIT3
#define ESWIN_MAIBOX_NPU_1_IRQ_BIT         BIT4
#define ESWIN_MAIBOX_DSP_0_IRQ_BIT         BIT5
#define ESWIN_MAIBOX_DSP_1_IRQ_BIT         BIT6
#define ESWIN_MAIBOX_DSP_2_IRQ_BIT         BIT7
#define ESWIN_MAIBOX_DSP_3_IRQ_BIT         BIT8

#endif /* _DTS_ESWIN_MAILBOX_H_ */
