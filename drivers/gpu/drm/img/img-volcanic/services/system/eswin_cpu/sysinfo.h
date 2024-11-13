/*************************************************************************/ /*!
 ########################################################################### ###
 #@File
 #@Copyright ESWIN
 #@Auther: Limei<limei@eswin.com>
 #@Date:2020-04-03
 #@History:
 #  ChenShuo 2020-08-21 adapt for zhimo-kernel
 ### ###########################################################################
*/ /**************************************************************************/

#if !defined(__SYSINFO_H__)
#define __SYSINFO_H__

#if 0
#include "config_kernel.h"
#endif
//#define RGX_ESWIN_PROJECT_V91

#define SYS_RGX_DEV_NAME    "pvrsrvkm"
#define EVENT_OBJECT_TIMEOUT_US                  (100000)

#define RGX_ESWIN_DISP_NUM_BUFS 2 //pay attention to this val

#ifndef RGX_ESWIN_PROJECT_U84
#define RGX_ESWIN_PROJECT_U84
#endif

#ifdef RGX_ESWIN_PROJECT_U84
    #define RGX_ESWIN_CPU_ADDR_W        41/*this is based on hw, Notes:eswin support 41bits width addrees, 3D-GPU support 40bits(1TB)*/
    #define RGX_ESWIN_CPU_ADDR_MASK     ((1UL<<RGX_ESWIN_CPU_ADDR_W)-1)/*this is based on hw*/
    #define RGX_ESWIN_IRQ_ID           14 
    /* display infor resolution and rate */
    #define RGX_ESWIN_DISP_DEF_W        1280
    #define RGX_ESWIN_DISP_DEF_H        720
    #define RGX_ESWIN_DISP_DEF_R        60
    /* ddr address from the view of CPU*/
    #define RGX_ESWIN_MEM_BASE_CPU          (0x80000000UL) //0x8,000,0000, ddr mem address from CPU view

    #ifdef  RGX_ESWIN_DISP_OFFSCREEN    /* fix address used by disp/vby1 for offscreen test */
    #define RGX_ESWIN_DISP_MEM_START    (0x81B00000UL) //(0x9B000000)
    #define RGX_ESWIN_MEM_SIZE          (RGX_ESWIN_DISP_MEM_START - RGX_ESWIN_MEM_BASE_CPU)
    #else
    #define RGX_ESWIN_MEM_SIZE          (0x20000000UL)
    #endif
    /* the cache flush reg address of CPU */
    #define RGX_ESWIN_CPU_CACHE_FLUSH_ADDR  0x02010200
    /* GPU reg address and size*/
    #ifndef NO_HARDWARE
	#define RGX_ESWIN_GPU_REG_BASE      0x51400000 //0x51400000 on haps
    #else
    #define RGX_ESWIN_GPU_REG_BASE      0x21400000 
    #endif
    #define RGX_ESWIN_GPU_REG_SIZE      0xFFFFF
    /* GPU clk */
    #define RGX_NOHW_CORE_CLOCK_SPEED   5000000//850000000// 850MHz GPU clk //100000000 //e.g. 100 MHz

    #ifdef LMA  /* mem address offset from the view of GPU */
        #define RGX_ESWIN_MEM_BASE_GPU   0x80000000UL //0x4,0000,0000
    #endif
#else
    #error "please update the proper Project infor"
    #define RGX_ESWIN_DISP_DEF_W        4096
    #define RGX_ESWIN_DISP_DEF_H        2160
    #define RGX_NOHW_CORE_CLOCK_SPEED   100000000 //e.g. 100 MHz
#endif

#define RGX_ESWIN_DISP_DEF_TOTAL_SIZE   (RGX_ESWIN_DISP_DEF_W*RGX_ESWIN_DISP_DEF_H*4*RGX_ESWIN_DISP_NUM_BUFS)

#define SYS_RGX_ACTIVE_POWER_LATENCY_MS (0)

/*!< System specific poll/timeout details */
#define MAX_HW_TIME_US                           (240000000)
#define DEVICES_WATCHDOG_POWER_ON_SLEEP_TIMEOUT  (120000) 
#define DEVICES_WATCHDOG_POWER_OFF_SLEEP_TIMEOUT (3600000)
#define WAIT_TRY_COUNT                           (10000)

#define SYS_RGX_OF_COMPATIBLE "img,gpu"

#endif	/* !defined(__SYSINFO_H__) */

