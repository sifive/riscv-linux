/*************************************************************************/ /*!
@File           sysconfig.c
@Title          System Configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    System Configuration functions
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include "img_defs.h"
#include "physheap.h"
#include "pvrsrv.h"
#include "rgxdevice.h"
#include "interrupt_support.h"
#include "sysconfig.h"

#if defined(SUPPORT_GPUVIRT_VALIDATION)
#include "vz_validation.h"
#endif

static void SysCpuPAToDevPA(IMG_HANDLE hPrivData, IMG_UINT32 ui32NumOfAddr,
                            IMG_DEV_PHYADDR *psDevPA, IMG_CPU_PHYADDR *psCpuPA);
static void SysDevPAToCpuPA(IMG_HANDLE hPrivData, IMG_UINT32 ui32NumOfAddr,
                            IMG_CPU_PHYADDR *psCpuPA, IMG_DEV_PHYADDR *psDevPA);

typedef struct _SYS_DATA_
{
	IMG_HANDLE hSysLISRData;
	PFN_LISR pfnDeviceLISR;
	void *pvDeviceLISRData;
} SYS_DATA;

typedef enum _PHYS_HEAP_IDX_
{
	PHYS_HEAP_IDX_SYSMEM,
	PHYS_HEAP_IDX_FIRMWARE,
	PHYS_HEAP_IDX_COUNT,
} PHYS_HEAP_IDX;

static PHYS_HEAP_CONFIG         gsPhysHeapConfig[PHYS_HEAP_IDX_COUNT];
static PVRSRV_DEVICE_CONFIG     gsDevCfg;
static SYS_DATA                 gsSysData = {NULL, NULL, NULL};

static RGX_TIMING_INFORMATION   gsTimingInfo = {
	DEFAULT_CLOCK_RATE,     /* ui32CoreClockSpeed */
	IMG_FALSE,              /* bEnableActivePM */
	IMG_FALSE,              /* bEnableRDPowIsland */
	0                       /* ui32ActivePMLatencyms */
};

static RGX_DATA                 gsRGXData = {&gsTimingInfo};

static PHYS_HEAP_FUNCTIONS      gsPhysHeapFuncs = {
	SysCpuPAToDevPA,        /* pfnCpuPAddrToDevPAddr */
	SysDevPAToCpuPA,        /* pfnDevPAddrToCpuPAddr */
};

/*
 * CPU Physical to Device Physical address translation:
 * Template implementation below assumes CPU and GPU views of physical memory are identical
 */
static void SysCpuPAToDevPA(IMG_HANDLE hPrivData, IMG_UINT32 ui32NumOfAddr,
                            IMG_DEV_PHYADDR *psDevPA, IMG_CPU_PHYADDR *psCpuPA)
{
	/* Optimise common case */
	psDevPA[0].uiAddr = psCpuPA[0].uiAddr;
	if (ui32NumOfAddr > 1)
	{
		IMG_UINT32 ui32Idx;
		for (ui32Idx = 1; ui32Idx < ui32NumOfAddr; ++ui32Idx)
		{
			psDevPA[ui32Idx].uiAddr = psCpuPA[ui32Idx].uiAddr;
		}
	}

	PVR_UNREFERENCED_PARAMETER(hPrivData);
}

/*
 * Device Physical to CPU Physical address translation:
 * Template implementation below assumes CPU and GPU views of physical memory are identical
 */
static void SysDevPAToCpuPA(IMG_HANDLE hPrivData, IMG_UINT32 ui32NumOfAddr,
                            IMG_CPU_PHYADDR *psCpuPA, IMG_DEV_PHYADDR *psDevPA)
{
	/* Optimise common case */
	psCpuPA[0].uiAddr = psDevPA[0].uiAddr;
	if (ui32NumOfAddr > 1)
	{
		IMG_UINT32 ui32Idx;
		for (ui32Idx = 1; ui32Idx < ui32NumOfAddr; ++ui32Idx)
		{
			psCpuPA[ui32Idx].uiAddr = psDevPA[ui32Idx].uiAddr;
		}
	}

	PVR_UNREFERENCED_PARAMETER(hPrivData);
}

static IMG_BOOL SystemISRHandler(void *pvData)
{
	SYS_DATA *psSysData = pvData;
	IMG_BOOL bHandled;

	/* Any special system interrupt handling goes here */

	bHandled = psSysData->pfnDeviceLISR(psSysData->pvDeviceLISRData);
	return bHandled;
}

PVRSRV_ERROR SysInstallDeviceLISR(IMG_HANDLE hSysData,
                                  IMG_UINT32 ui32IRQ,
                                  const IMG_CHAR *pszName,
                                  PFN_LISR pfnLISR,
                                  void *pvData,
                                  IMG_HANDLE *phLISRData)
{
	SYS_DATA *psSysData = (SYS_DATA *)hSysData;
	PVRSRV_ERROR eError;

	if (psSysData->hSysLISRData)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: ISR for %s already installed!", __func__, pszName));
		return PVRSRV_ERROR_CANT_REGISTER_CALLBACK;
	}

	/* Wrap the device LISR */
	psSysData->pfnDeviceLISR = pfnLISR;
	psSysData->pvDeviceLISRData = pvData;

	eError = OSInstallSystemLISR(&psSysData->hSysLISRData, ui32IRQ, pszName,
	                             SystemISRHandler, psSysData,
	                             SYS_IRQ_FLAG_TRIGGER_DEFAULT);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	*phLISRData = psSysData;

	PVR_LOG(("Installed device LISR %s on IRQ %d", pszName, ui32IRQ));

	return PVRSRV_OK;
}

PVRSRV_ERROR SysUninstallDeviceLISR(IMG_HANDLE hLISRData)
{
	SYS_DATA *psSysData = (SYS_DATA *)hLISRData;
	PVRSRV_ERROR eError;

	PVR_ASSERT(psSysData);

	eError = OSUninstallSystemLISR(psSysData->hSysLISRData);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	/* clear interrupt data */
	psSysData->pfnDeviceLISR    = NULL;
	psSysData->pvDeviceLISRData = NULL;
	psSysData->hSysLISRData     = NULL;

	return PVRSRV_OK;
}

static PVRSRV_ERROR SysPrePower(IMG_HANDLE hSysData,
                                PVRSRV_SYS_POWER_STATE eNewPowerState,
                                PVRSRV_SYS_POWER_STATE eCurrentPowerState,
                                PVRSRV_POWER_FLAGS ePwrFlags)
{
	PVRSRV_VZ_RET_IF_MODE(GUEST, PVRSRV_OK);
	PVR_LOG_RETURN_IF_FALSE((eNewPowerState != eCurrentPowerState), "no power change", PVRSRV_OK);

	PVR_UNREFERENCED_PARAMETER(hSysData);
	PVR_UNREFERENCED_PARAMETER(ePwrFlags);

	/* on powering down */
	if (eNewPowerState != PVRSRV_SYS_POWER_STATE_ON)
	{
		IMG_CPU_PHYADDR sSoCRegBase = {SOC_REGBANK_BASE};

		void* pvSocRegs = OSMapPhysToLin(sSoCRegBase,
		                                 SOC_REGBANK_SIZE,
		                                 PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);

		OSWriteHWReg32(pvSocRegs, POW_DOMAIN_DISABLE_REG, POW_DOMAIN_GPU);
		OSUnMapPhysToLin(pvSocRegs, SOC_REGBANK_SIZE);
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR SysPostPower(IMG_HANDLE hSysData,
                                 PVRSRV_SYS_POWER_STATE eNewPowerState,
                                 PVRSRV_SYS_POWER_STATE eCurrentPowerState,
                                 PVRSRV_POWER_FLAGS ePwrFlags)
{
	PVRSRV_VZ_RET_IF_MODE(GUEST, PVRSRV_OK);
	PVR_LOG_RETURN_IF_FALSE((eNewPowerState != eCurrentPowerState), "no power change", PVRSRV_OK);

	PVR_UNREFERENCED_PARAMETER(hSysData);
	PVR_UNREFERENCED_PARAMETER(ePwrFlags);

	/* on powering up */
	if (eCurrentPowerState != PVRSRV_SYS_POWER_STATE_ON)
	{
		IMG_CPU_PHYADDR sSoCRegBase = {SOC_REGBANK_BASE};

		void* pvSocRegs = OSMapPhysToLin(sSoCRegBase,
		                                 SOC_REGBANK_SIZE,
		                                 PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);

		OSWriteHWReg32(pvSocRegs, POW_DOMAIN_ENABLE_REG, POW_DOMAIN_GPU);
		OSUnMapPhysToLin(pvSocRegs, SOC_REGBANK_SIZE);
	}

	return PVRSRV_OK;
}

static PVRSRV_SYS_POWER_STATE RGXGpuDomainPower(PVRSRV_DEVICE_NODE *psDevNode)
{
	IMG_CPU_PHYADDR sSoCRegBase = {SOC_REGBANK_BASE};

	void* pvSocRegs = OSMapPhysToLin(sSoCRegBase,
	                                 SOC_REGBANK_SIZE,
	                                 PVRSRV_MEMALLOCFLAG_CPU_UNCACHED);

	IMG_UINT32 ui32SocDomainPower = OSReadHWReg32(pvSocRegs, POW_DOMAIN_STATUS_REG);

	bool bGpuDomainIsPowered = BITMASK_HAS(ui32SocDomainPower, POW_DOMAIN_GPU);

	PVR_UNREFERENCED_PARAMETER(psDevNode);
	OSUnMapPhysToLin(pvSocRegs, SOC_REGBANK_SIZE);

	return (bGpuDomainIsPowered) ? PVRSRV_SYS_POWER_STATE_ON : PVRSRV_SYS_POWER_STATE_OFF;
}

static void SysDevFeatureDepInit(PVRSRV_DEVICE_CONFIG *psDevConfig, IMG_UINT64 ui64Features)
{
	PVR_UNREFERENCED_PARAMETER(ui64Features);
	psDevConfig->eCacheSnoopingMode = PVRSRV_DEVICE_SNOOP_NONE;
}

static PVRSRV_DRIVER_MODE GetDriverMode(struct platform_device *psDev)
{
	PVRSRV_DRIVER_MODE eDriverMode;

#if (RGX_NUM_OS_SUPPORTED > 1)
	if (of_property_read_u32(psDev->dev.of_node, "vz-mode", (IMG_UINT32*) &eDriverMode))
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: Cannot retrieve driver mode from Device Tree. "
								  "Default to native mode.", __func__));
		eDriverMode = DRIVER_MODE_NATIVE;
	}
#else
	eDriverMode = DRIVER_MODE_NATIVE;
#endif

	return eDriverMode;
}

static PVRSRV_ERROR DeviceConfigCreate(void *pvOSDevice,
									   PVRSRV_DEVICE_CONFIG *psDevCfg)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	struct platform_device *psDev;
	struct resource *dev_res = NULL;
	int dev_irq;
	PVRSRV_DATA *psPVRSRVData = PVRSRVGetPVRSRVData();

	psDev = to_platform_device((struct device *)pvOSDevice);

	dma_set_mask(pvOSDevice, DMA_BIT_MASK(40));

	dev_irq = platform_get_irq(psDev, 0);
	if (dev_irq < 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: platform_get_irq failed (%d)", __func__, -dev_irq));
		eError = PVRSRV_ERROR_INVALID_DEVICE;
		return eError;
	}

	dev_res = platform_get_resource(psDev, IORESOURCE_MEM, 0);
	if (dev_res == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: platform_get_resource failed", __func__));
		eError =  PVRSRV_ERROR_INVALID_DEVICE;
		return eError;
	}

	/* Device Setup */
	psDevCfg->pvOSDevice              = pvOSDevice;
	psDevCfg->pszName                 = "pvrsrvkm";
	psDevCfg->pszVersion              = NULL;

	/* Device setup information */
	psDevCfg->sRegsCpuPBase.uiAddr    = dev_res->start;
	psDevCfg->ui32RegsSize            = (unsigned int)(dev_res->end - dev_res->start);
	psDevCfg->ui32IRQ                 = dev_irq;

	/* Power management */
	psDevCfg->pfnPrePowerState        = SysPrePower;
	psDevCfg->pfnPostPowerState       = SysPostPower;
	psDevCfg->pfnGpuDomainPower       = PVRSRV_VZ_MODE_IS(GUEST) ? NULL : RGXGpuDomainPower;

	/* Minimal configuration */
	psDevCfg->pfnClockFreqGet         = NULL;
	psDevCfg->hDevData                = &gsRGXData;
	psDevCfg->hSysData                = &gsSysData;
	psDevCfg->pfnSysDevFeatureDepInit = SysDevFeatureDepInit;
	psDevCfg->bHasFBCDCVersion31      = IMG_FALSE;

	/* If driver mode is not overridden by the apphint, set it here */
	if (!psPVRSRVData->bForceApphintDriverMode)
	{
		psPVRSRVData->eDriverMode = GetDriverMode(psDev);
	}

#if defined(SUPPORT_GPUVIRT_VALIDATION)
	psDevCfg->pfnSysDevVirtInit = SysInitValidation;

	CreateMPUWatchdogThread();
#endif

	return eError;
}

#if defined(RGX_VZ_STATIC_CARVEOUT_FW_HEAPS)
/* Obtain the IPA of the carveout range reserved for this VM */
static IMG_UINT64 GetCarveoutBase(PVRSRV_DEVICE_CONFIG *psDevCfg)
{
	struct platform_device *psDev = to_platform_device((struct device *)psDevCfg->pvOSDevice);
	IMG_UINT64 ui64BaseAddress;

	if (of_property_read_u64(psDev->dev.of_node, "fw-carveout", &ui64BaseAddress))
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: Cannot retrieve firmware carveout address from Device Tree."
								  " Using default Base Address: 0x%llX",
								  __func__, FW_CARVEOUT_IPA_BASE));
		ui64BaseAddress = FW_CARVEOUT_IPA_BASE;
	}

	return ui64BaseAddress;
}
#endif

static PVRSRV_ERROR PhysHeapCfgCreate(PVRSRV_DEVICE_CONFIG *psDevCfg)
{
	IMG_CPU_PHYADDR sCpuBase;
	IMG_DEV_PHYADDR sDeviceBase;
	PVRSRV_ERROR eError = PVRSRV_OK;

	/* Heap configuration for general use */
	gsPhysHeapConfig[PHYS_HEAP_IDX_SYSMEM].pszPDumpMemspaceName = "SYSMEM";
	gsPhysHeapConfig[PHYS_HEAP_IDX_SYSMEM].eType = PHYS_HEAP_TYPE_UMA;
	gsPhysHeapConfig[PHYS_HEAP_IDX_SYSMEM].psMemFuncs = &gsPhysHeapFuncs;
	gsPhysHeapConfig[PHYS_HEAP_IDX_SYSMEM].hPrivData = NULL;
	gsPhysHeapConfig[PHYS_HEAP_IDX_SYSMEM].ui32UsageFlags = PHYS_HEAP_USAGE_GPU_LOCAL;

	/* Heap configuration for memory shared with the firmware */
	gsPhysHeapConfig[PHYS_HEAP_IDX_FIRMWARE].pszPDumpMemspaceName = "SYSMEM_FW";
	gsPhysHeapConfig[PHYS_HEAP_IDX_FIRMWARE].psMemFuncs = &gsPhysHeapFuncs;
	gsPhysHeapConfig[PHYS_HEAP_IDX_FIRMWARE].hPrivData = NULL;
	gsPhysHeapConfig[PHYS_HEAP_IDX_FIRMWARE].ui32UsageFlags = PHYS_HEAP_USAGE_FW_MAIN;
	gsPhysHeapConfig[PHYS_HEAP_IDX_FIRMWARE].uiSize = RGX_FIRMWARE_RAW_HEAP_SIZE;

#if defined(RGX_VZ_STATIC_CARVEOUT_FW_HEAPS)
	{
		/*
		 * In a static memory VZ setup, the size and base addresses of
		 * all Host and Guest drivers' Firmware heaps are laid out
		 * consecutively in a physically contiguous memory range known
		 * in advance by the Host driver.
		 *
		 * During the Host driver initialisation, it maps the entire range
		 * into the Firmware's virtual address space. No other mapping
		 * operations into the Firmware's VA space are needed after this.
		 * Guest driver must know only the base address of the range
		 * assign to it during system planning stage.
		 *
		 * The system integrator must ensure that:
		 *  - physically contiguous RAM region used as a Firmware heap
		 *    is not managed by any OS or Hypervisor (a carveout)
		 *  - Host driver must come online before any Guest drivers
		 *  - Host driver sets up the Firmware before Guests submits work
		 */

		sCpuBase.uiAddr = GetCarveoutBase(psDevCfg);
		SysCpuPAToDevPA(NULL, 1, &sDeviceBase, &sCpuBase);

		gsPhysHeapConfig[PHYS_HEAP_IDX_FIRMWARE].eType = PHYS_HEAP_TYPE_LMA;
		gsPhysHeapConfig[PHYS_HEAP_IDX_FIRMWARE].sStartAddr = sCpuBase;
		gsPhysHeapConfig[PHYS_HEAP_IDX_FIRMWARE].sCardBase = sDeviceBase;
	}
#else
	/* Dynamic Firmware heap allocation */
	if (PVRSRV_VZ_MODE_IS(GUEST))
	{
		/*
		 * Guest drivers must provide a physically contiguous memory
		 * range to the Host via a PVZ call to have it mapped into
		 * the Firmware's address space. Guest drivers use the OS
		 * kernel's DMA/CMA allocator to obtain a DMA buffer to be
		 * used as a firmware heap. This memory will be managed
		 * internally by the Guest driver after the heap is created.
		 */
		DMA_ALLOC *psDmaAlloc = OSAllocZMem(sizeof(DMA_ALLOC));

		eError = (psDmaAlloc == NULL) ? PVRSRV_ERROR_OUT_OF_MEMORY : PVRSRV_OK;
		if (eError == PVRSRV_OK)
		{
			psDmaAlloc->pvOSDevice = psDevCfg->pvOSDevice;
			psDmaAlloc->ui64Size = RGX_FIRMWARE_RAW_HEAP_SIZE;

			eError = SysDmaAllocMem(psDmaAlloc);
			if (eError == PVRSRV_OK)
			{
				eError = SysDmaRegisterForIoRemapping(psDmaAlloc);

				if (eError == PVRSRV_OK)
				{
					sCpuBase.uiAddr = psDmaAlloc->sBusAddr.uiAddr;
					SysCpuPAToDevPA(NULL, 1, &sDeviceBase, &sCpuBase);

					gsPhysHeapConfig[PHYS_HEAP_IDX_FIRMWARE].eType = PHYS_HEAP_TYPE_DMA;
					gsPhysHeapConfig[PHYS_HEAP_IDX_FIRMWARE].hPrivData = psDmaAlloc;
					gsPhysHeapConfig[PHYS_HEAP_IDX_FIRMWARE].sStartAddr = sCpuBase;
					gsPhysHeapConfig[PHYS_HEAP_IDX_FIRMWARE].sCardBase = sDeviceBase;
				}
				else
				{
					OSFreeMem(psDmaAlloc);
					SysDmaFreeMem(psDmaAlloc);
				}
			}
			else
			{
				OSFreeMem(psDmaAlloc);
			}
		}
	}
	else
	{
		/*
		 * The Host or Native driver uses memory managed by
		 * the kernel on a page granularity and creates on-demand
		 * mappings into the Firmware's address space.
		 */
		gsPhysHeapConfig[PHYS_HEAP_IDX_FIRMWARE].eType = PHYS_HEAP_TYPE_UMA;
	}
#endif

	/* Device's physical heaps */
	psDevCfg->pasPhysHeaps = gsPhysHeapConfig;
	psDevCfg->eDefaultHeap = PVRSRV_PHYS_HEAP_GPU_LOCAL;
	psDevCfg->ui32PhysHeapCount = PHYS_HEAP_IDX_COUNT;

	return eError;
}

static void PhysHeapCfgDestroy(PVRSRV_DEVICE_CONFIG *psDevCfg)
{
#if defined(RGX_VZ_STATIC_CARVEOUT_FW_HEAPS)
	if (PVRSRV_VZ_MODE_IS(GUEST))
	{
		DMA_ALLOC *psDmaAlloc = psDevCfg->pasPhysHeaps[PHYS_HEAP_IDX_FIRMWARE].hPrivData;

		SysDmaDeregisterForIoRemapping(psDmaAlloc);
		SysDmaFreeMem(psDmaAlloc);
		OSFreeMem(psDmaAlloc);
	}
#endif

#if defined(SUPPORT_GPUVIRT_VALIDATION)
	DestroyMPUWatchdogThread();
#endif
}

PVRSRV_ERROR SysDevInit(void *pvOSDevice, PVRSRV_DEVICE_CONFIG **ppsDevConfig)
{
	PVRSRV_ERROR eError;

	eError = DeviceConfigCreate(pvOSDevice, &gsDevCfg);
	if (eError == PVRSRV_OK)
	{
		eError = PhysHeapCfgCreate(&gsDevCfg);
		*ppsDevConfig = &gsDevCfg;
	}

	return eError;
}

void SysDevDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig)
{
	PhysHeapCfgDestroy(psDevConfig);
	psDevConfig->pvOSDevice = NULL;
}

PVRSRV_ERROR SysDebugInfo(PVRSRV_DEVICE_CONFIG *psDevConfig,
						  DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
						  void *pvDumpDebugFile)
{
	/* print here any system information useful for debug dumps */

	PVR_UNREFERENCED_PARAMETER(psDevConfig);
	PVR_UNREFERENCED_PARAMETER(pfnDumpDebugPrintf);
	PVR_UNREFERENCED_PARAMETER(pvDumpDebugFile);
	return PVRSRV_OK;
}

/******************************************************************************
 End of file (sysconfig.c)
******************************************************************************/
