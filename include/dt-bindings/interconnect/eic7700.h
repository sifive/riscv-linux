/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Interconnect driver for Eswin EIC7700 SoC
 *
 * Copyright (C) 2022  Beiging Eswin Co. Ltd
 * Author: Huangyifeng <huangyifeng@eswincomputing.com>
 */

#ifndef _DT_BINDINGS_INTERCONNECT_EIC7700_H_
#define _DT_BINDINGS_INTERCONNECT_EIC7700_H_

#define OFFSET0                              (0)
#define OFFSET1                              (1)
#define OFFSET2                              (2)
#define OFFSET3                              (3)
#define OFFSET4                              (4)
#define OFFSET5                              (5)
#define OFFSET6                              (6)
#define OFFSET7                              (7)
#define OFFSET8                              (8)
#define OFFSET9                              (9)
#define OFFSET10                             (10)
#define OFFSET11                             (11)
#define OFFSET12                             (12)
#define OFFSET13                             (13)
#define OFFSET14                             (14)
#define OFFSET15                             (15)
#define OFFSET16                             (16)
#define OFFSET17                             (17)
#define OFFSET18                             (18)
#define OFFSET19                             (19)
#define OFFSET20                             (20)
#define OFFSET21                             (21)
#define OFFSET22                             (22)
#define OFFSET23                             (23)

#define OFFSET31                             (31)

/*sideband manager module id defination*/
/*sys noc*/
#define SBM_AON_SNOC_SP0	0
#define SBM_DSPT_SNOC		1
#define SBM_JTAG_SNOC		2
#define SBM_MCPUT_SNOC_D2D	3
#define SBM_MCPUT_SNOC_MP	4
#define SBM_MCPUT_SNOC_SP0	5
#define SBM_MCPUT_SNOC_SP1	6
#define SBM_NPU_SNOC_SP0	7
#define SBM_NPU_SNOC_SP1	8
#define SBM_PCIET_SNOC_P	9
#define SBM_SPISLV_PCIET_SNOC	10
#define SBM_TBU4_SNOC		11
#define SBM_TCU_SNOC		12
#define SBM_SNOC_AON		13
#define SBM_SNOC_DDR0_P1	14
#define SBM_SNOC_DDR0_P2	15
#define SBM_SNOC_DDR1_P1	16
#define SBM_SNOC_DDR1_P2	17
#define SBM_SNOC_DSPT		18
#define SBM_SNOC_MCPUT_D2D	19
#define SBM_SNOC_NPU		20
#define SBM_SNOC_PCIET		21

/*cfg noc*/
#define SBM_CLMM		30
#define SBM_CNOC_AON		31
#define SBM_CNOC_DDRT0_CTRL	32
#define SBM_CNOC_DDRT0_PHY	33
#define SBM_CNOC_DDRT1_CTRL	34
#define SBM_CNOC_DDRT1_PHY	35
#define SBM_CNOC_DSPT		36
#define SBM_CNOC_GPU		37
#define SBM_CNOC_HSP		38
#define SBM_CNOC_LSP_APB2	39
#define SBM_CNOC_LSP_APB3	40
#define SBM_CNOC_LSP_APB4	41
#define SBM_CNOC_LSP_APB6	42
#define SBM_CNOC_MCPUT_D2D	43
#define SBM_CNOC_NPU		44
#define SBM_CNOC_PCIET_P	45
#define SBM_CNOC_PCIET_X	46
#define SBM_CNOC_TCU		47
#define SBM_CNOC_VC		48
#define SBM_CNOC_VI		49
#define SBM_CNOC_VO		50

/*llc noc*/
#define SBM_LNOC_NPU_LLC0		60
#define SBM_LNOC_NPU_LLC1		61
#define SBM_LNOC_DDRT0_P0		62
#define SBM_LNOC_DDRT1_P0		63

/*media noc*/
#define SBM_MNOC_GPU			70
#define SBM_MNOC_TBU2			71
#define SBM_MNOC_VC			72
#define SBM_MNOC_DDRT0_P3		73
#define SBM_MNOC_DDRT1_P3		74

/*realtime noc*/
#define SBM_RNOC_TBU0			80
#define SBM_RNOC_VO			81
#define SBM_RNOC_DDRT0_P4		82
#define SBM_RNOC_DDRT1_P4		83

/*RouteID defination*/
#define        aon_snoc_sp0_I_O        0x0
#define        dspt_snoc_I_O           0x1
#define        jtag_snoc_I_O           0x2
#define        mcput_snoc_d2d_I_O      0x3
#define        mcput_snoc_mp_I_O       0x4
#define        mcput_snoc_sp0_I_O      0x5
#define        mcput_snoc_sp1_I_O      0x6
#define        mnoc_snoc_I_O           0x7
#define        npu_snoc_sp0_I_O        0x8
#define        npu_snoc_sp1_I_O        0x9
#define        pciet_snoc_p_I_O        0xA
#define        rnoc_snoc_I_O           0xB
#define        spislv_tbu3_snoc_I_O    0xC
#define        tbu4_snoc_I_O           0xD
#define        tcu_snoc_I_O            0xE
#define        RESERVED0               0xF

#define        snoc_aon_T_O            0x0
#define        snoc_cnoc_T_O           0x1
#define        snoc_ddrt0_p1_T_O       0x2
#define        snoc_ddrt0_p2_T_O       0x3
#define        snoc_ddrt1_p1_T_O       0x4
#define        snoc_ddrt1_p2_T_O       0x5
#define        snoc_dspt_T_O           0x6
#define        snoc_lnoc_T_O           0x7
#define        snoc_mcput_d2d_T_O      0x8
#define        snoc_mnoc_T_O           0x9
#define        snoc_npu_T_O            0xA
#define        snoc_pciet_T_O          0xB
#define        snoc_rnoc_T_O           0xC
#define        snoc_service_T_O        0xD
#define        RESERVED1               0xE
#define        RESERVED2               0xF

#endif /* _DT_BINDINGS_INTERCONNECT_EIC7700_H_ */
