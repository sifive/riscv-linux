/*************************************************************************/ /*
########################################################################### ###
#@File
#@Copyright ESWIN
#@Auther: Limei<limei@eswin.com>
#@Date:2020-04-03
#@History:
#  ChenShuo 2020-08-21 adapt for zhimo-kernel
### ###########################################################################

*/ /**************************************************************************/

#include "pvrsrv.h"
#include "pvrsrv_device.h"
#include "syscommon.h"
#include "sysinfo.h"

#include "physheap.h"

#include <linux/dma-mapping.h>

#include "rgxdevice.h"
#include "interrupt_support.h"
#include "osfunc.h"

#include <linux/version.h>

#include "sysinfo.h"
#include "apollo_regs.h"


#include "vz_vmm_pvz.h"
#include "allocmem.h"
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <soc/sifive/sifive_l2_cache.h>

#include <linux/clk.h>
#include <linux/reset.h>
#include "rgxdevice.h"
#if defined(SUPPORT_ION)
#include "ion_support.h"
#endif

#if defined(SUPPORT_VALIDATION) && defined(PDUMP)
#include "validation_soc.h"
#endif

extern unsigned int _corefreq_div;

IMG_UINT64 *cpu_cache_flush_addr = NULL;
// static int cache_reg_map = 0;
#if 0
extern void eswin_l2_flush64(phys_addr_t addr, size_t size);
#else
void eswin_l2_flush64(phys_addr_t addr, size_t size) {
		sifive_l2_flush64_range(addr,size);
};
#endif
void riscv_invalidate_addr(phys_addr_t addr, size_t size,IMG_BOOL virtual) {

 printk(KERN_ALERT "eswin:%s not implement now start 0x%x size=0x%x\n",__func__,addr,size);	
}
void riscv_flush_addr(IMG_UINT64 cpuaddr,IMG_UINT64 bytes_size, IMG_BOOL virtual)
{
		// printk(KERN_ALERT "eswin print %s: ----------riscv_flush_addr--run-----------\n", __func__);
    IMG_UINT64 bStart = 0;
    IMG_UINT64 bEnd = 0;
    // IMG_UINT64 bBase;
    IMG_UINT64 line_size = (IMG_UINT64)cache_line_size();

    if (virtual)//if cpuaddr is virtual address, here need to convert it to physical address
        bStart =  page_to_phys(virt_to_page(cpuaddr)) & RGX_ESWIN_CPU_ADDR_MASK;
    else
        bStart =  cpuaddr & RGX_ESWIN_CPU_ADDR_MASK;;

    bEnd = bStart + bytes_size;

    bEnd = PVR_ALIGN((IMG_UINT64)bEnd, line_size);

    #if 1
    eswin_l2_flush64((phys_addr_t)bStart, (size_t)(bEnd-bStart));
    #else
    if (cache_reg_map == 0){
        cache_reg_map = 1;
	    cpu_cache_flush_addr = (IMG_UINT64 __iomem *)ioremap(RGX_ESWIN_CPU_CACHE_FLUSH_ADDR, 0x10);
    }
    if (cpu_cache_flush_addr == NULL){
        printk(KERN_ALERT "NULL pointer file %s line %d\n", __FILE__, __LINE__);
        return;
    }
    mb();
    for (bBase = bStart; bBase < bEnd; bBase += line_size)
    {
        *(IMG_UINT64 *)cpu_cache_flush_addr = bBase;
    }
    mb();
    #endif
}


PVRSRV_ERROR SysInstallDeviceLISR(IMG_HANDLE hSysData,
				  IMG_UINT32 ui32IRQ,
				  const IMG_CHAR *pszName,
				  PFN_LISR pfnLISR,
				  void *pvData,
				  IMG_HANDLE *phLISRData)
{
    PVRSRV_ERROR eError;
    PVR_UNREFERENCED_PARAMETER(hSysData);

    #ifndef NO_HARDWARE
    eError = OSInstallSystemLISR(phLISRData, ui32IRQ, pszName, pfnLISR, pvData, SYS_IRQ_FLAG_TRIGGER_DEFAULT | SYS_IRQ_FLAG_SHARED);	
    if (eError != PVRSRV_OK)
		PVR_DPF((PVR_DBG_ERROR, "%s: install error %d != PVRSRV_OK", __func__, eError));
    #endif
    return eError;
}

PVRSRV_ERROR SysUninstallDeviceLISR(IMG_HANDLE hLISRData)
{
	return OSUninstallSystemLISR(hLISRData);
}

PVRSRV_ERROR SysDebugInfo(PVRSRV_DEVICE_CONFIG *psDevConfig,
				DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
				void *pvDumpDebugFile)
{
	PVR_UNREFERENCED_PARAMETER(psDevConfig);
	PVR_UNREFERENCED_PARAMETER(pfnDumpDebugPrintf);
	PVR_UNREFERENCED_PARAMETER(pvDumpDebugFile);
	return PVRSRV_OK;
}


/*
	CPU to Device physical address translation
*/
static
void UMAPhysHeapCpuPAddrToDevPAddr(IMG_HANDLE hPrivData,
								   IMG_UINT32 ui32NumOfAddr,
								   IMG_DEV_PHYADDR *psDevPAddr,
								   IMG_CPU_PHYADDR *psCpuPAddr)
{
    IMG_UINT32 ui32Idx;
	PVR_UNREFERENCED_PARAMETER(hPrivData);

	/* Optimise common case */
	for (ui32Idx = 0; ui32Idx < ui32NumOfAddr; ui32Idx++)
	{
		psDevPAddr[ui32Idx].uiAddr = psCpuPAddr[ui32Idx].uiAddr;
	}
}

/*
	Device to CPU physical address translation
*/
static
void UMAPhysHeapDevPAddrToCpuPAddr(IMG_HANDLE hPrivData,
								   IMG_UINT32 ui32NumOfAddr,
								   IMG_CPU_PHYADDR *psCpuPAddr,
								   IMG_DEV_PHYADDR *psDevPAddr)
{
    IMG_UINT32 ui32Idx;
	PVR_UNREFERENCED_PARAMETER(hPrivData);

	/* Optimise common case */
	for (ui32Idx = 0; ui32Idx < ui32NumOfAddr; ui32Idx++)
	{
		psCpuPAddr[ui32Idx].uiAddr = psDevPAddr[ui32Idx].uiAddr;
	}
}


static PHYS_HEAP_FUNCTIONS gsPhysHeapFuncs =
{
	/* pfnCpuPAddrToDevPAddr */
	UMAPhysHeapCpuPAddrToDevPAddr,
	/* pfnDevPAddrToCpuPAddr */
	UMAPhysHeapDevPAddrToCpuPAddr,
	/* pfnGetRegionId */
	NULL,
};

static PVRSRV_ERROR PhysHeapsCreate(PHYS_HEAP_CONFIG **ppasPhysHeapsOut,
									IMG_UINT32 *puiPhysHeapCountOut)
{
	PHYS_HEAP_CONFIG *pasPhysHeaps;
	IMG_UINT32 ui32NextHeapID = 0;
	IMG_UINT32 uiHeapCount = 1;
    

	uiHeapCount += !PVRSRV_VZ_MODE_IS(NATIVE) ? 1:0;

	pasPhysHeaps = OSAllocZMem(sizeof(*pasPhysHeaps) * uiHeapCount);
	if (!pasPhysHeaps)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	pasPhysHeaps[ui32NextHeapID].ui32UsageFlags = PHYS_HEAP_USAGE_GPU_LOCAL;
	pasPhysHeaps[ui32NextHeapID].pszPDumpMemspaceName = "SYSMEM";
	pasPhysHeaps[ui32NextHeapID].eType = PHYS_HEAP_TYPE_UMA;
	pasPhysHeaps[ui32NextHeapID].psMemFuncs = &gsPhysHeapFuncs;
	ui32NextHeapID++;

	if (! PVRSRV_VZ_MODE_IS(NATIVE))
	{
		pasPhysHeaps[ui32NextHeapID].ui32UsageFlags = PHYS_HEAP_USAGE_GPU_LOCAL;
		pasPhysHeaps[ui32NextHeapID].pszPDumpMemspaceName = "SYSMEM";
		pasPhysHeaps[ui32NextHeapID].eType = PHYS_HEAP_TYPE_UMA;
		pasPhysHeaps[ui32NextHeapID].psMemFuncs = &gsPhysHeapFuncs;
		ui32NextHeapID++;
	}

	*ppasPhysHeapsOut = pasPhysHeaps;
	*puiPhysHeapCountOut = uiHeapCount;

	return PVRSRV_OK;
}

static void PhysHeapsDestroy(PHYS_HEAP_CONFIG *pasPhysHeaps)
{
	OSFreeMem(pasPhysHeaps);
}

void SysDevDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
        int ret;

	PhysHeapsDestroy(psDevConfig->pasPhysHeaps);

	/* eswin, assert the reset */
	if (psDevConfig->rstc_spu)
        {
		ret = reset_control_assert(psDevConfig->rstc_spu);
                WARN_ON(0 != ret);
                reset_control_put(psDevConfig->rstc_spu);
        }
	if (psDevConfig->rstc_jones)
        {
		ret = reset_control_assert(psDevConfig->rstc_jones);
                WARN_ON(0 != ret);
                reset_control_put(psDevConfig->rstc_jones);
        }
	if (psDevConfig->rstc_gray)
        {
		ret = reset_control_assert(psDevConfig->rstc_gray);
                WARN_ON(0 != ret);
                reset_control_put(psDevConfig->rstc_gray);
        }
	if (psDevConfig->rstc_cfg)
        {
		ret = reset_control_assert(psDevConfig->rstc_cfg);
                WARN_ON(0 != ret);
                reset_control_put(psDevConfig->rstc_cfg);
        }
	if (psDevConfig->rstc_axi)
        {
		ret = reset_control_assert(psDevConfig->rstc_axi);
                WARN_ON(0 != ret);
                reset_control_put(psDevConfig->rstc_axi);
        }
        printk(KERN_DEBUG "eswin print %s: gpu reset assert ok\n", __func__);

	/* eswin, close the clk */
        if (psDevConfig->aclk)
        {
		clk_disable_unprepare(psDevConfig->aclk);
		clk_put(psDevConfig->aclk);
        }
        if (psDevConfig->cfg_clk)
        {
            clk_disable_unprepare(psDevConfig->cfg_clk);
	    clk_put(psDevConfig->cfg_clk);
        }
        if (psDevConfig->gray_clk)
        {
            clk_disable_unprepare(psDevConfig->gray_clk);
	    clk_put(psDevConfig->gray_clk);
        }
        printk(KERN_DEBUG "eswin print %s: gpu clk disable ok\n", __func__);

	OSFreeMem(psDevConfig);
}

void riscv_flush_cache_range(IMG_HANDLE hSysData,
                                        PVRSRV_CACHE_OP eRequestType,
                                        void *pvVirtStart,
                                        void *pvVirtEnd,
                                        IMG_CPU_PHYADDR sCPUPhysStart,
                                        IMG_CPU_PHYADDR sCPUPhysEnd)
{
	IMG_UINT64 bStart = sCPUPhysStart.uiAddr;
	IMG_UINT64 bEnd = sCPUPhysEnd.uiAddr;

	PVR_UNREFERENCED_PARAMETER(hSysData);
	// PVR_UNREFERENCED_PARAMETER(eRequestType);
	PVR_UNREFERENCED_PARAMETER(pvVirtStart);
	PVR_UNREFERENCED_PARAMETER(pvVirtEnd);
	switch (eRequestType)
	{
	case PVRSRV_CACHE_OP_FLUSH:
		riscv_flush_addr(bStart, (bEnd - bStart), IMG_FALSE);
		break;
	case PVRSRV_CACHE_OP_INVALIDATE:
		riscv_flush_addr(bStart, (bEnd - bStart), IMG_FALSE);
		//printk(KERN_ALERT "%s: ---eswin----warning-----PVRSRV_CACHE_OP_INVALIDATE not implement now ------- \n", __func__);
		break;
	case PVRSRV_CACHE_OP_CLEAN:
		riscv_flush_addr(bStart, (bEnd - bStart), IMG_FALSE);
		break;
	default:
		printk(KERN_ALERT "%s: unhandled eRequestType val=0x%x \n", __func__, eRequestType);
	}
}


static PVRSRV_ERROR DeviceConfigCreate(void *pvOSDevice, PVRSRV_DEVICE_CONFIG **ppsDevConfig)
{
	PVRSRV_DEVICE_CONFIG *psDevConfig;
	RGX_DATA *psRGXData;
	RGX_TIMING_INFORMATION *psRGXTimingInfo;
	PHYS_HEAP_CONFIG *pasPhysHeaps;
	IMG_UINT32 uiPhysHeapCount;
	PVRSRV_ERROR eError;
#ifndef NO_HARDWARE
	IMG_UINT32 rgx_freq=0;
	struct resource res;
	struct device_node *np=NULL;
	int ret;
	struct platform_device *pdev = to_platform_device((struct device *)pvOSDevice);
	struct device *dev = (struct device *)pvOSDevice;
#endif //CONFIG_SPARSE_IRQ 

	psDevConfig = OSAllocZMem(sizeof(*psDevConfig) +
							  sizeof(*psRGXData) +
							  sizeof(*psRGXTimingInfo));
	if (!psDevConfig)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psRGXData = (RGX_DATA *)((IMG_CHAR *)psDevConfig + sizeof(*psDevConfig));
	psRGXTimingInfo = (RGX_TIMING_INFORMATION *)((IMG_CHAR *)psRGXData + sizeof(*psRGXData));

	eError = PhysHeapsCreate(&pasPhysHeaps, &uiPhysHeapCount);
	if (eError)
	{
		goto ErrorFreeDevConfig;
	}

	/* Setup RGX specific timing data */
#ifndef NO_HARDWARE
	np = dev->of_node;

	if (!np)
	{
		pr_err("%s:%d can not find node img,gpu\n", __func__, __LINE__);
		eError = PVRSRV_ERROR_NO_DEVICENODE_FOUND;
		goto ErrorFreeDevConfig;
	}
	// gpu clk enable
	psDevConfig->aclk = of_clk_get_by_name(np, "aclk");
	if (IS_ERR(psDevConfig->aclk))
	{
		ret = PTR_ERR(psDevConfig->aclk);
		dev_err(&pdev->dev, "failed to get aclk: %d\n", ret);
		printk(KERN_ALERT "eswin print %s: failed to get aclk\n", __func__);
		return ret;
	}

	// get gpu clk, it is supposed to be 800MHz
	rgx_freq = clk_get_rate(psDevConfig->aclk);
	printk(KERN_ALERT "%s:%d, eswin print : read back aclk  %dHZ \n", __func__, __LINE__, rgx_freq);

	ret = clk_prepare_enable(psDevConfig->aclk);
	if (ret)
	{
		dev_err(&pdev->dev, "failed to enable aclk: %d\n", ret);
		printk(KERN_ALERT "eswin print %s: failed to enable aclk\n", __func__);
		return ret;
	}

	psDevConfig->cfg_clk = of_clk_get_by_name(np, "cfg_clk");
	if (IS_ERR(psDevConfig->cfg_clk))
	{
		ret = PTR_ERR(psDevConfig->cfg_clk);
		dev_err(&pdev->dev, "failed to get cfg_clk: %d\n", ret);
		printk(KERN_ALERT "eswin print %s: failed to get cfg_clk\n", __func__);
		return ret;
	}
	ret = clk_prepare_enable(psDevConfig->cfg_clk);
	if (ret)
	{
		dev_err(&pdev->dev, "failed to enable cfg_clk: %d\n", ret);
		printk(KERN_ALERT "eswin print %s: failed to enable cfg_clk\n", __func__);
		return ret;
	}

	psDevConfig->gray_clk = of_clk_get_by_name(np, "gray_clk");
	if (IS_ERR(psDevConfig->gray_clk))
	{
		ret = PTR_ERR(psDevConfig->gray_clk);
		dev_err(&pdev->dev, "failed to get gray_clk: %d\n", ret);
		printk(KERN_ALERT "eswin print %s: failed to get gray_clk\n", __func__);
		return ret;
	}
	ret = clk_prepare_enable(psDevConfig->gray_clk);
	if (ret)
	{
		dev_err(&pdev->dev, "failed to enable gray_clk: %d\n", ret);
		printk(KERN_ALERT "eswin print %s: failed to enable gray_clk\n", __func__);
		return ret;
	}
	printk(KERN_ALERT "eswin print %s: gpu clk enable ok\n", __func__);
	clk_disable_unprepare(psDevConfig->aclk);
	clk_disable_unprepare(psDevConfig->cfg_clk);
	clk_disable_unprepare(psDevConfig->gray_clk);
	printk(KERN_ALERT "eswin print %s: gpu clk disable ok\n", __func__);

	if (_corefreq_div > 800000000)
		_corefreq_div = 800000000;

	rgx_freq= clk_round_rate(psDevConfig->aclk, _corefreq_div);//24M -> 800M
	if (rgx_freq > 0) {
		ret = clk_set_rate(psDevConfig->aclk, rgx_freq);
		if (ret) {
			dev_err(&pdev->dev, "failed to set aclk: %d\n", ret);
			return ret;
		}
		printk(KERN_ALERT "eswin print : set aclk to %dHZ \n", rgx_freq);
	}
	rgx_freq = clk_get_rate(psDevConfig->aclk);
	printk(KERN_ALERT "eswin print : read back aclk  %dHZ \n", rgx_freq);
	psRGXTimingInfo->ui32CoreClockSpeed = rgx_freq;

	clk_prepare_enable(psDevConfig->aclk);
	clk_prepare_enable(psDevConfig->cfg_clk);
	clk_prepare_enable(psDevConfig->gray_clk);

	printk(KERN_ALERT "eswin print %s: gpu clk enable ok\n", __func__);

	// reset gpu
	psDevConfig->rstc_axi = of_reset_control_get_optional_exclusive(np, "axi");
	if (IS_ERR_OR_NULL(psDevConfig->rstc_axi))
	{
		dev_err(&pdev->dev, "Failed to get axi reset handle\n");
		printk(KERN_ALERT "eswin print %s: Failed to get axi reset handle\n", __func__);
		return -EFAULT;
	}
	ret = reset_control_reset(psDevConfig->rstc_axi);
	WARN_ON(0 != ret);

	psDevConfig->rstc_cfg = of_reset_control_get_optional_exclusive(np, "cfg");
	if (IS_ERR_OR_NULL(psDevConfig->rstc_cfg))
	{
		dev_err(&pdev->dev, "Failed to get cfg reset handle\n");
		printk(KERN_ALERT "eswin print %s: Failed to get cfg reset handle\n", __func__);
		return -EFAULT;
	}
	ret = reset_control_reset(psDevConfig->rstc_cfg);
	WARN_ON(0 != ret);

	psDevConfig->rstc_gray = of_reset_control_get_optional_exclusive(np, "gray");
	if (IS_ERR_OR_NULL(psDevConfig->rstc_gray))
	{
		dev_err(&pdev->dev, "Failed to get gray reset handle\n");
		printk(KERN_ALERT "eswin print %s: Failed to get gray reset handle\n", __func__);
		return -EFAULT;
	}
	ret = reset_control_reset(psDevConfig->rstc_gray);
	WARN_ON(0 != ret);

	psDevConfig->rstc_jones = of_reset_control_get_optional_exclusive(np, "jones");
	if (IS_ERR_OR_NULL(psDevConfig->rstc_jones))
	{
		dev_err(&pdev->dev, "Failed to get jones reset handle\n");
		printk(KERN_ALERT "eswin print %s: Failed to get jones reset handle\n", __func__);
		return -EFAULT;
	}
	ret = reset_control_reset(psDevConfig->rstc_jones);
	WARN_ON(0 != ret);

	psDevConfig->rstc_spu = of_reset_control_get_optional_exclusive(np, "spu");
	if (IS_ERR_OR_NULL(psDevConfig->rstc_spu))
	{
		dev_err(&pdev->dev, "Failed to get spu reset handle\n");
		printk(KERN_ALERT "eswin print %s: Failed to get spu reset handle\n", __func__);
		return -EFAULT;
	}
	ret = reset_control_reset(psDevConfig->rstc_spu);
	WARN_ON(0 != ret);
	printk(KERN_ALERT "eswin print %s: gpu clk reset ok\n", __func__);
#else
	psRGXTimingInfo->ui32CoreClockSpeed        = RGX_NOHW_CORE_CLOCK_SPEED;
#endif //CONFIG_SPARSE_IRQ
	psRGXTimingInfo->bEnableActivePM           = IMG_FALSE;
	psRGXTimingInfo->bEnableRDPowIsland        = IMG_FALSE;
	psRGXTimingInfo->ui32ActivePMLatencyms     = SYS_RGX_ACTIVE_POWER_LATENCY_MS;

	/* Set up the RGX data */
	psRGXData->psRGXTimingInfo = psRGXTimingInfo;

	/* Setup the device config */
	psDevConfig->pvOSDevice				= pvOSDevice;
	psDevConfig->pszVersion             = NULL;
#ifndef NO_HARDWARE
	psDevConfig->pszName = pdev->name;
	printk(KERN_ALERT "%s: --------------->dev_name=%s\n", __func__, psDevConfig->pszName);

	if (of_address_to_resource(np, 0, &res))
	{
		printk(KERN_ALERT "%s: failed to get resource of rgx register\n", __func__);
		eError = PVRSRV_ERROR_INVALID_MEMINFO;
		goto ErrorFreeDevConfig;
	}
	psDevConfig->sRegsCpuPBase.uiAddr = res.start;
	psDevConfig->ui32RegsSize = resource_size(&res);
	pr_err("%s: --------------->reg_base=%llx, reg_size=%llx\n", __func__, res.start, resource_size(&res));

	psDevConfig->ui32IRQ = irq_of_parse_and_map(np, 0);
	pr_err("%s: --------------->ui32IRQ=%d\n", __func__, psDevConfig->ui32IRQ);
	if (!psDevConfig->ui32IRQ)
	{
		printk(KERN_ALERT "%s: failed to map GPU interrupt\n", __func__);
		eError = PVRSRV_ERROR_MAPPING_NOT_FOUND;
		goto ErrorFreeDevConfig;
	}
#else
	/* Device setup information */
	psDevConfig->pszName                = SYS_RGX_DEV_NAME;
	psDevConfig->sRegsCpuPBase.uiAddr   = RGX_ESWIN_GPU_REG_BASE;
	psDevConfig->ui32RegsSize           = RGX_ESWIN_GPU_REG_SIZE;
	psDevConfig->ui32IRQ                = RGX_ESWIN_IRQ_ID;
#endif

	psDevConfig->pasPhysHeaps			= pasPhysHeaps;
	psDevConfig->ui32PhysHeapCount		= uiPhysHeapCount;

	psDevConfig->eDefaultHeap = PVRSRV_PHYS_HEAP_GPU_LOCAL;

	if (! PVRSRV_VZ_MODE_IS(NATIVE))
	{
		/* Virtualization support services needs to know which heap ID corresponds to FW */
		// psDevConfig->aui32PhysHeapID[PVRSRV_DEVICE_PHYS_HEAP_FW_LOCAL] = PHYS_HEAP_IDX_VIRTFW;
	}

	/* No power management on no HW system */
	psDevConfig->pfnPrePowerState       = NULL;
	psDevConfig->pfnPostPowerState      = NULL;

	/* No clock frequency either */
	psDevConfig->pfnClockFreqGet        = NULL;

	psDevConfig->hDevData               = psRGXData;
	psDevConfig->hSysData               = NULL;
	psDevConfig->bDevicePA0IsValid       = IMG_FALSE;
	psDevConfig->pfnSysDevFeatureDepInit = NULL;

	/* Pdump validation system registers */
#if defined(SUPPORT_VALIDATION) && defined(PDUMP)
	PVRSRVConfigureSysCtrl(NULL, PDUMP_FLAGS_CONTINUOUS);
#if defined(SUPPORT_SECURITY_VALIDATION)
	PVRSRVConfigureTrustedDevice(NULL, PDUMP_FLAGS_CONTINUOUS);
#endif
#endif

	psDevConfig->bHasFBCDCVersion31 = IMG_FALSE;

	*ppsDevConfig = psDevConfig;

	return PVRSRV_OK;

ErrorFreeDevConfig:
	OSFreeMem(psDevConfig);
	return eError;
}

PVRSRV_ERROR SysDevInit(void *pvOSDevice, PVRSRV_DEVICE_CONFIG **ppsDevConfig)
{
	PVRSRV_ERROR eError;
    dma_set_mask(pvOSDevice, DMA_BIT_MASK(40)); //?why is '40', maybe need ask the hw engineer

    eError = DeviceConfigCreate(pvOSDevice, ppsDevConfig);

    (*ppsDevConfig)->pfnHostCacheMaintenance = riscv_flush_cache_range;
    (*ppsDevConfig)->bHasPhysicalCacheMaintenance = OS_CACHE_OP_ADDR_TYPE_PHYSICAL;
	return eError;
}

/******************************************************************************
 End of file (sysconfig.c)
******************************************************************************/

