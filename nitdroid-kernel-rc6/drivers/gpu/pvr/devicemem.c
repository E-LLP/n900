/**********************************************************************
 *
 * Copyright(c) 2008 Imagination Technologies Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful but, except
 * as otherwise stated in writing, without any warranty; without even the
 * implied warranty of merchantability or fitness for a particular purpose.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * Contact Information:
 * Imagination Technologies Ltd. <gpl-support@imgtec.com>
 * Home Park Estate, Kings Langley, Herts, WD4 8LZ, UK
 *
 ******************************************************************************/

#include <stddef.h>

#include "services_headers.h"
#include "buffer_manager.h"
#include "pdump_km.h"
#include "sgxmmu.h"
#include "sgxapi_km.h"
#include "pvr_bridge_km.h"

#include "linux/kernel.h"
#include "linux/pagemap.h"

static enum PVRSRV_ERROR AllocDeviceMem(void *hDevCookie, void *hDevMemHeap,
				   u32 ui32Flags, u32 ui32Size,
				   u32 ui32Alignment,
				   struct PVRSRV_KERNEL_MEM_INFO **ppsMemInfo);

enum PVRSRV_ERROR PVRSRVGetDeviceMemHeapsKM(void *hDevCookie,
					struct PVRSRV_HEAP_INFO *psHeapInfo)
{
	struct PVRSRV_DEVICE_NODE *psDeviceNode;
	u32 ui32HeapCount;
	struct DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;
	u32 i;

	psDeviceNode = (struct PVRSRV_DEVICE_NODE *)hDevCookie;
	PVR_ASSERT(psDeviceNode != NULL);

	ui32HeapCount = psDeviceNode->sDevMemoryInfo.ui32HeapCount;
	psDeviceMemoryHeap = psDeviceNode->sDevMemoryInfo.psDeviceMemoryHeap;

	PVR_ASSERT(ui32HeapCount <= PVRSRV_MAX_CLIENT_HEAPS);

	for (i = 0; i < ui32HeapCount; i++) {

		psHeapInfo[i].ui32HeapID = psDeviceMemoryHeap[i].ui32HeapID;
		psHeapInfo[i].hDevMemHeap = psDeviceMemoryHeap[i].hDevMemHeap;
		psHeapInfo[i].sDevVAddrBase =
		    psDeviceMemoryHeap[i].sDevVAddrBase;
		psHeapInfo[i].ui32HeapByteSize =
		    psDeviceMemoryHeap[i].ui32HeapSize;
		psHeapInfo[i].ui32Attribs = psDeviceMemoryHeap[i].ui32Attribs;
	}

	for (; i < PVRSRV_MAX_CLIENT_HEAPS; i++) {
		OSMemSet(psHeapInfo + i, 0, sizeof(*psHeapInfo));
		psHeapInfo[i].ui32HeapID = (u32) SGX_UNDEFINED_HEAP_ID;
	}

	return PVRSRV_OK;
}

enum PVRSRV_ERROR PVRSRVCreateDeviceMemContextKM(void *hDevCookie,
			     struct PVRSRV_PER_PROCESS_DATA *psPerProc,
			     void **phDevMemContext,
			     u32 *pui32ClientHeapCount,
			     struct PVRSRV_HEAP_INFO *psHeapInfo,
			     IMG_BOOL *pbCreated, IMG_BOOL *pbShared)
{
	struct PVRSRV_DEVICE_NODE *psDeviceNode;
	u32 ui32HeapCount, ui32ClientHeapCount = 0;
	struct DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;
	void *hDevMemContext;
	void *hDevMemHeap;
	struct IMG_DEV_PHYADDR sPDDevPAddr;
	u32 i;

	psDeviceNode = (struct PVRSRV_DEVICE_NODE *)hDevCookie;
	PVR_ASSERT(psDeviceNode != NULL);

	ui32HeapCount = psDeviceNode->sDevMemoryInfo.ui32HeapCount;
	psDeviceMemoryHeap = psDeviceNode->sDevMemoryInfo.psDeviceMemoryHeap;

	PVR_ASSERT(ui32HeapCount <= PVRSRV_MAX_CLIENT_HEAPS);

	hDevMemContext = BM_CreateContext(psDeviceNode,
					  &sPDDevPAddr, psPerProc, pbCreated);
	if (hDevMemContext == NULL) {
		PVR_DPF(PVR_DBG_ERROR, "PVRSRVCreateDeviceMemContextKM: "
			  "Failed BM_CreateContext");
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	for (i = 0; i < ui32HeapCount; i++)
		switch (psDeviceMemoryHeap[i].DevMemHeapType) {
		case DEVICE_MEMORY_HEAP_SHARED_EXPORTED:
			{

				psHeapInfo[ui32ClientHeapCount].ui32HeapID =
				    psDeviceMemoryHeap[i].ui32HeapID;
				psHeapInfo[ui32ClientHeapCount].hDevMemHeap =
				    psDeviceMemoryHeap[i].hDevMemHeap;
				psHeapInfo[ui32ClientHeapCount].sDevVAddrBase =
				    psDeviceMemoryHeap[i].sDevVAddrBase;
				psHeapInfo[ui32ClientHeapCount].
				    ui32HeapByteSize =
				    psDeviceMemoryHeap[i].ui32HeapSize;
				psHeapInfo[ui32ClientHeapCount].ui32Attribs =
				    psDeviceMemoryHeap[i].ui32Attribs;
				pbShared[ui32ClientHeapCount] = IMG_TRUE;
				ui32ClientHeapCount++;
				break;
			}
		case DEVICE_MEMORY_HEAP_PERCONTEXT:
			{
				hDevMemHeap = BM_CreateHeap(hDevMemContext,
							    &psDeviceMemoryHeap
							    [i]);

				psHeapInfo[ui32ClientHeapCount].ui32HeapID =
				    psDeviceMemoryHeap[i].ui32HeapID;
				psHeapInfo[ui32ClientHeapCount].hDevMemHeap =
				    hDevMemHeap;
				psHeapInfo[ui32ClientHeapCount].sDevVAddrBase =
				    psDeviceMemoryHeap[i].sDevVAddrBase;
				psHeapInfo[ui32ClientHeapCount].
				    ui32HeapByteSize =
				    psDeviceMemoryHeap[i].ui32HeapSize;
				psHeapInfo[ui32ClientHeapCount].ui32Attribs =
				    psDeviceMemoryHeap[i].ui32Attribs;
				pbShared[ui32ClientHeapCount] = IMG_FALSE;

				ui32ClientHeapCount++;
				break;
			}
		}

	*pui32ClientHeapCount = ui32ClientHeapCount;
	*phDevMemContext = hDevMemContext;

	return PVRSRV_OK;
}

enum PVRSRV_ERROR PVRSRVDestroyDeviceMemContextKM(void *hDevCookie,
					     void *hDevMemContext,
					     IMG_BOOL *pbDestroyed)
{
	int destroyed;

	PVR_UNREFERENCED_PARAMETER(hDevCookie);

	destroyed = pvr_put_ctx(hDevMemContext);
	if (pbDestroyed)
		*pbDestroyed = destroyed ? IMG_TRUE : IMG_FALSE;

	return PVRSRV_OK;
}

enum PVRSRV_ERROR PVRSRVGetDeviceMemHeapInfoKM(void *hDevCookie,
					  void *hDevMemContext,
					  u32 *pui32ClientHeapCount,
					  struct PVRSRV_HEAP_INFO *psHeapInfo,
					  IMG_BOOL *pbShared)
{
	struct PVRSRV_DEVICE_NODE *psDeviceNode;
	u32 ui32HeapCount, ui32ClientHeapCount = 0;
	struct DEVICE_MEMORY_HEAP_INFO *psDeviceMemoryHeap;
	void *hDevMemHeap;
	u32 i;

	psDeviceNode = (struct PVRSRV_DEVICE_NODE *)hDevCookie;
	PVR_ASSERT(psDeviceNode != NULL);

	ui32HeapCount = psDeviceNode->sDevMemoryInfo.ui32HeapCount;
	psDeviceMemoryHeap = psDeviceNode->sDevMemoryInfo.psDeviceMemoryHeap;

	PVR_ASSERT(ui32HeapCount <= PVRSRV_MAX_CLIENT_HEAPS);

	for (i = 0; i < ui32HeapCount; i++)
		switch (psDeviceMemoryHeap[i].DevMemHeapType) {
		case DEVICE_MEMORY_HEAP_SHARED_EXPORTED:
			{

				psHeapInfo[ui32ClientHeapCount].ui32HeapID =
				    psDeviceMemoryHeap[i].ui32HeapID;
				psHeapInfo[ui32ClientHeapCount].hDevMemHeap =
				    psDeviceMemoryHeap[i].hDevMemHeap;
				psHeapInfo[ui32ClientHeapCount].sDevVAddrBase =
				    psDeviceMemoryHeap[i].sDevVAddrBase;
				psHeapInfo[ui32ClientHeapCount].
				    ui32HeapByteSize =
				    psDeviceMemoryHeap[i].ui32HeapSize;
				psHeapInfo[ui32ClientHeapCount].ui32Attribs =
				    psDeviceMemoryHeap[i].ui32Attribs;
				pbShared[ui32ClientHeapCount] = IMG_TRUE;
				ui32ClientHeapCount++;
				break;
			}
		case DEVICE_MEMORY_HEAP_PERCONTEXT:
			{
				hDevMemHeap = BM_CreateHeap(hDevMemContext,
							    &psDeviceMemoryHeap
							    [i]);

				psHeapInfo[ui32ClientHeapCount].ui32HeapID =
				    psDeviceMemoryHeap[i].ui32HeapID;
				psHeapInfo[ui32ClientHeapCount].hDevMemHeap =
				    hDevMemHeap;
				psHeapInfo[ui32ClientHeapCount].sDevVAddrBase =
				    psDeviceMemoryHeap[i].sDevVAddrBase;
				psHeapInfo[ui32ClientHeapCount].
				    ui32HeapByteSize =
				    psDeviceMemoryHeap[i].ui32HeapSize;
				psHeapInfo[ui32ClientHeapCount].ui32Attribs =
				    psDeviceMemoryHeap[i].ui32Attribs;
				pbShared[ui32ClientHeapCount] = IMG_FALSE;

				ui32ClientHeapCount++;
				break;
			}
		}

	*pui32ClientHeapCount = ui32ClientHeapCount;

	return PVRSRV_OK;
}

static enum PVRSRV_ERROR AllocDeviceMem(void *hDevCookie, void *hDevMemHeap,
				   u32 ui32Flags, u32 ui32Size,
				   u32 ui32Alignment,
				   struct PVRSRV_KERNEL_MEM_INFO **ppsMemInfo)
{
	struct PVRSRV_KERNEL_MEM_INFO *psMemInfo;
	void *hBuffer;

	struct PVRSRV_MEMBLK *psMemBlock;
	IMG_BOOL bBMError;

	PVR_UNREFERENCED_PARAMETER(hDevCookie);

	*ppsMemInfo = NULL;

	if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		       sizeof(struct PVRSRV_KERNEL_MEM_INFO),
		       (void **) &psMemInfo, NULL) != PVRSRV_OK) {
		PVR_DPF(PVR_DBG_ERROR,
			 "AllocDeviceMem: Failed to alloc memory for block");
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psMemBlock = &(psMemInfo->sMemBlk);

	psMemInfo->ui32Flags = ui32Flags | PVRSRV_MEM_RAM_BACKED_ALLOCATION;

	bBMError = BM_Alloc(hDevMemHeap, NULL, ui32Size,
			    &psMemInfo->ui32Flags, ui32Alignment, &hBuffer);

	if (!bBMError) {
		PVR_DPF(PVR_DBG_ERROR, "AllocDeviceMem: BM_Alloc Failed");
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
			  sizeof(struct PVRSRV_KERNEL_MEM_INFO), psMemInfo,
			  NULL);
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psMemBlock->sDevVirtAddr = BM_HandleToDevVaddr(hBuffer);
	psMemBlock->hOSMemHandle = BM_HandleToOSMemHandle(hBuffer);

	psMemBlock->hBuffer = (void *) hBuffer;
	psMemInfo->pvLinAddrKM = BM_HandleToCpuVaddr(hBuffer);
	psMemInfo->sDevVAddr = psMemBlock->sDevVirtAddr;
	psMemInfo->ui32AllocSize = ui32Size;
	*ppsMemInfo = psMemInfo;

	return PVRSRV_OK;
}

static enum PVRSRV_ERROR FreeDeviceMem(struct PVRSRV_KERNEL_MEM_INFO *psMemInfo)
{
	void *hBuffer;

	if (!psMemInfo)
		return PVRSRV_ERROR_INVALID_PARAMS;

	hBuffer = psMemInfo->sMemBlk.hBuffer;
	BM_Free(hBuffer, psMemInfo->ui32Flags);
	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
		  sizeof(struct PVRSRV_KERNEL_MEM_INFO),
		  psMemInfo, NULL);

	return PVRSRV_OK;
}

enum PVRSRV_ERROR PVRSRVAllocSyncInfoKM(void *hDevCookie, void *hDevMemContext,
		    struct PVRSRV_KERNEL_SYNC_INFO **ppsKernelSyncInfo)
{
	void *hSyncDevMemHeap;
	struct DEVICE_MEMORY_INFO *psDevMemoryInfo;
	struct BM_CONTEXT *pBMContext;
	enum PVRSRV_ERROR eError;
	struct PVRSRV_KERNEL_SYNC_INFO *psKernelSyncInfo;
	struct PVRSRV_SYNC_DATA *psSyncData;

	eError = OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
			    sizeof(struct PVRSRV_KERNEL_SYNC_INFO),
			    (void **) &psKernelSyncInfo, NULL);
	if (eError != PVRSRV_OK) {
		PVR_DPF(PVR_DBG_ERROR,
			 "PVRSRVAllocSyncInfoKM: Failed to alloc memory");
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	pBMContext = (struct BM_CONTEXT *)hDevMemContext;
	psDevMemoryInfo = &pBMContext->psDeviceNode->sDevMemoryInfo;

	hSyncDevMemHeap = psDevMemoryInfo->psDeviceMemoryHeap[psDevMemoryInfo->
						ui32SyncHeapID].hDevMemHeap;

	eError = AllocDeviceMem(hDevCookie, hSyncDevMemHeap, 0,
				sizeof(struct PVRSRV_SYNC_DATA),
				sizeof(u32),
				&psKernelSyncInfo->psSyncDataMemInfoKM);

	if (eError != PVRSRV_OK) {

		PVR_DPF(PVR_DBG_ERROR,
			 "PVRSRVAllocSyncInfoKM: Failed to alloc memory");
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
			  sizeof(struct PVRSRV_KERNEL_SYNC_INFO),
			  psKernelSyncInfo, NULL);
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psKernelSyncInfo->psSyncData =
	    psKernelSyncInfo->psSyncDataMemInfoKM->pvLinAddrKM;
	psSyncData = psKernelSyncInfo->psSyncData;

	psSyncData->ui32WriteOpsPending = 0;
	psSyncData->ui32WriteOpsComplete = 0;
	psSyncData->ui32ReadOpsPending = 0;
	psSyncData->ui32ReadOpsComplete = 0;
	psSyncData->ui32LastOpDumpVal = 0;
	psSyncData->ui32LastReadOpDumpVal = 0;

	psKernelSyncInfo->sWriteOpsCompleteDevVAddr.uiAddr =
	    psKernelSyncInfo->psSyncDataMemInfoKM->sDevVAddr.uiAddr +
	    offsetof(struct PVRSRV_SYNC_DATA, ui32WriteOpsComplete);
	psKernelSyncInfo->sReadOpsCompleteDevVAddr.uiAddr =
	    psKernelSyncInfo->psSyncDataMemInfoKM->sDevVAddr.uiAddr +
	    offsetof(struct PVRSRV_SYNC_DATA, ui32ReadOpsComplete);

	psKernelSyncInfo->psSyncDataMemInfoKM->psKernelSyncInfo = NULL;

	*ppsKernelSyncInfo = psKernelSyncInfo;

	return PVRSRV_OK;
}

enum PVRSRV_ERROR PVRSRVFreeSyncInfoKM(
			struct PVRSRV_KERNEL_SYNC_INFO *psKernelSyncInfo)
{
	FreeDeviceMem(psKernelSyncInfo->psSyncDataMemInfoKM);
	OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
		sizeof(struct PVRSRV_KERNEL_SYNC_INFO), psKernelSyncInfo, NULL);

	return PVRSRV_OK;
}

static enum PVRSRV_ERROR FreeDeviceMemCallBack(void *pvParam, u32 ui32Param)
{
	enum PVRSRV_ERROR eError = PVRSRV_OK;
	struct PVRSRV_KERNEL_MEM_INFO *psMemInfo = pvParam;

	PVR_UNREFERENCED_PARAMETER(ui32Param);

	if (psMemInfo->psKernelSyncInfo)
		eError = PVRSRVFreeSyncInfoKM(psMemInfo->psKernelSyncInfo);

	if (eError == PVRSRV_OK)
		eError = FreeDeviceMem(psMemInfo);

	return eError;
}

enum PVRSRV_ERROR PVRSRVFreeDeviceMemKM(void *hDevCookie,
				   struct PVRSRV_KERNEL_MEM_INFO *psMemInfo)
{
	PVR_UNREFERENCED_PARAMETER(hDevCookie);

	if (!psMemInfo)
		return PVRSRV_ERROR_INVALID_PARAMS;

	if (psMemInfo->sMemBlk.hResItem != NULL)
		ResManFreeResByPtr(psMemInfo->sMemBlk.hResItem);
	else
		FreeDeviceMemCallBack(psMemInfo, 0);

	return PVRSRV_OK;
}

enum PVRSRV_ERROR PVRSRVAllocDeviceMemKM(void *hDevCookie,
				     struct PVRSRV_PER_PROCESS_DATA *psPerProc,
				     void *hDevMemHeap, u32 ui32Flags,
				     u32 ui32Size, u32 ui32Alignment,
				     struct PVRSRV_KERNEL_MEM_INFO **ppsMemInfo)
{
	struct PVRSRV_KERNEL_MEM_INFO *psMemInfo;
	enum PVRSRV_ERROR eError;
	struct BM_HEAP *psBMHeap;
	void *hDevMemContext;

	if (!hDevMemHeap || (ui32Size == 0))
		return PVRSRV_ERROR_INVALID_PARAMS;

	eError = AllocDeviceMem(hDevCookie, hDevMemHeap, ui32Flags, ui32Size,
				ui32Alignment, &psMemInfo);

	if (eError != PVRSRV_OK)
		return eError;

	if (ui32Flags & PVRSRV_MEM_NO_SYNCOBJ) {
		psMemInfo->psKernelSyncInfo = NULL;
	} else {

		psBMHeap = (struct BM_HEAP *)hDevMemHeap;
		hDevMemContext = (void *) psBMHeap->pBMContext;
		eError = PVRSRVAllocSyncInfoKM(hDevCookie,
					       hDevMemContext,
					       &psMemInfo->psKernelSyncInfo);
		if (eError != PVRSRV_OK)
			goto free_mainalloc;
	}

	*ppsMemInfo = psMemInfo;

	if (ui32Flags & PVRSRV_MEM_NO_RESMAN) {
		psMemInfo->sMemBlk.hResItem = NULL;
	} else {

		psMemInfo->sMemBlk.hResItem =
		    ResManRegisterRes(psPerProc->hResManContext,
				      RESMAN_TYPE_DEVICEMEM_ALLOCATION,
				      psMemInfo, 0, FreeDeviceMemCallBack);
		if (psMemInfo->sMemBlk.hResItem == NULL) {

			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto free_mainalloc;
		}
	}

	return PVRSRV_OK;

free_mainalloc:
	FreeDeviceMem(psMemInfo);

	return eError;
}

enum PVRSRV_ERROR PVRSRVDissociateDeviceMemKM(void *hDevCookie,
				  struct PVRSRV_KERNEL_MEM_INFO *psMemInfo)
{
	enum PVRSRV_ERROR eError = PVRSRV_OK;
	struct PVRSRV_DEVICE_NODE *psDeviceNode = hDevCookie;

	PVR_UNREFERENCED_PARAMETER(hDevCookie);

	if (!psMemInfo)
		return PVRSRV_ERROR_INVALID_PARAMS;

	ResManDissociateRes(psMemInfo->sMemBlk.hResItem,
			    psDeviceNode->hResManContext);

	return eError;
}

enum PVRSRV_ERROR PVRSRVGetFreeDeviceMemKM(u32 ui32Flags, u32 *pui32Total,
				      u32 *pui32Free, u32 *pui32LargestBlock)
{

	PVR_UNREFERENCED_PARAMETER(ui32Flags);
	PVR_UNREFERENCED_PARAMETER(pui32Total);
	PVR_UNREFERENCED_PARAMETER(pui32Free);
	PVR_UNREFERENCED_PARAMETER(pui32LargestBlock);

	return PVRSRV_OK;
}

enum PVRSRV_ERROR PVRSRVUnwrapExtMemoryKM(
		struct PVRSRV_KERNEL_MEM_INFO *psMemInfo)
{
	if (!psMemInfo)
		return PVRSRV_ERROR_INVALID_PARAMS;

	ResManFreeResByPtr(psMemInfo->sMemBlk.hResItem);
	return PVRSRV_OK;
}

static enum PVRSRV_ERROR UnwrapExtMemoryCallBack(void *pvParam, u32 ui32Param)
{
	enum PVRSRV_ERROR eError = PVRSRV_OK;
	struct PVRSRV_KERNEL_MEM_INFO *psMemInfo = pvParam;
	void *hOSWrapMem = NULL;
	struct BM_BUF *psBMBuf;

	PVR_UNREFERENCED_PARAMETER(ui32Param);

	psBMBuf = (struct BM_BUF *)psMemInfo->sMemBlk.hBuffer;

	if ((psBMBuf->ui32RefCount == 1) && (psMemInfo->psKernelSyncInfo)) {
		eError = PVRSRVFreeSyncInfoKM(psMemInfo->psKernelSyncInfo);
		hOSWrapMem = psBMBuf->hOSWrapMem;
	}

	if (eError == PVRSRV_OK)
		eError = FreeDeviceMem(psMemInfo);

	if (hOSWrapMem)
		OSReleasePhysPageAddr(hOSWrapMem, IMG_TRUE);

	return eError;
}

enum PVRSRV_ERROR PVRSRVWrapExtMemoryKM(void *hDevCookie,
				   struct PVRSRV_PER_PROCESS_DATA *psPerProc,
				   u32 ui32ByteSize, u32 ui32PageOffset,
				   IMG_BOOL bPhysContig,
				   struct IMG_SYS_PHYADDR *psExtSysPAddr,
				   void *pvLinAddr,
				   struct PVRSRV_KERNEL_MEM_INFO **ppsMemInfo)
{
	struct PVRSRV_KERNEL_MEM_INFO *psMemInfo = NULL;
	struct DEVICE_MEMORY_INFO *psDevMemoryInfo;
	u32 ui32HostPageSize = HOST_PAGESIZE();
	void *hDevMemHeap, *hDevMemContext;
	struct PVRSRV_DEVICE_NODE *psDeviceNode;
	void *hBuffer;
	struct PVRSRV_MEMBLK *psMemBlock;
	IMG_BOOL bBMError;
	struct BM_HEAP *psBMHeap;
	enum PVRSRV_ERROR eError;
	void *pvPageAlignedCPUVAddr;
	struct IMG_SYS_PHYADDR *psIntSysPAddr = NULL;
	void *hOSWrapMem = NULL;
	struct BM_BUF *psBMBuf;
	struct IMG_SYS_PHYADDR *pPageList = psExtSysPAddr;
	u32 ui32PageCount;

	u32 ui32CalculatedPageOffset = ((u32)pvLinAddr) & ~PAGE_MASK;
	if (ui32CalculatedPageOffset != ui32PageOffset)	{
		PVR_DPF(PVR_DBG_ERROR, "PVRSRVWrapExtMemoryKM: "
			"offset from address not match offset param");
		return PVRSRV_ERROR_BAD_MAPPING;
	}

	psDeviceNode = (struct PVRSRV_DEVICE_NODE *)hDevCookie;
	PVR_ASSERT(psDeviceNode != NULL);
	psDevMemoryInfo = &psDeviceNode->sDevMemoryInfo;
	hDevMemHeap =
	    psDevMemoryInfo->psDeviceMemoryHeap[psDevMemoryInfo->
						ui32MappingHeapID].hDevMemHeap;

	ui32PageCount =
	    HOST_PAGEALIGN(ui32ByteSize + ui32PageOffset) / ui32HostPageSize;

	if (pvLinAddr) {
		pvPageAlignedCPUVAddr =
		    (void *) ((u8 *) pvLinAddr - ui32PageOffset);

		if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
			       ui32PageCount * sizeof(struct IMG_SYS_PHYADDR),
			       (void **) &psIntSysPAddr,
			       NULL) != PVRSRV_OK) {
			PVR_DPF(PVR_DBG_ERROR, "PVRSRVWrapExtMemoryKM: "
					"Failed to alloc memory for block");
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}

		/* let's start by getting the address of the first page */
		eError = OSAcquirePhysPageAddr(pvPageAlignedCPUVAddr,
					       ui32HostPageSize,
					       psIntSysPAddr, &hOSWrapMem,
					       IMG_TRUE);
		if (eError != PVRSRV_OK) {
			PVR_DPF(PVR_DBG_ERROR, "PVRSRVWrapExtMemoryKM: "
					"Failed to alloc memory for block");
			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto ErrorExitPhase1;
		}
		/* now check if this memory address is already wrapped */
		if (BM_IsWrapped(hDevMemHeap, ui32PageOffset,
				 psIntSysPAddr[0])) {
			/* already wrapped */
			OSReleasePhysPageAddr(hOSWrapMem, IMG_TRUE);
			hOSWrapMem = NULL;
		} else if (ui32PageCount > 1) {
			OSReleasePhysPageAddr(hOSWrapMem, IMG_TRUE);
			hOSWrapMem = NULL;
			/* the memory is going to wrapped for the first time,
			 * so we need full page list */
			eError = OSAcquirePhysPageAddr(pvPageAlignedCPUVAddr,
						       ui32PageCount *
						       ui32HostPageSize,
						       psIntSysPAddr,
						       &hOSWrapMem,
						       IMG_TRUE);
			if (eError != PVRSRV_OK) {
				PVR_DPF(PVR_DBG_ERROR, "PVRSRVWrapExtMemoryKM:"
					  " Failed to alloc memory for block");
				eError = PVRSRV_ERROR_OUT_OF_MEMORY;
				goto ErrorExitPhase1;
			}
		}

		psExtSysPAddr = psIntSysPAddr;
	} else {

	}

	if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		       sizeof(struct PVRSRV_KERNEL_MEM_INFO),
		       (void **) &psMemInfo, NULL) != PVRSRV_OK) {
		PVR_DPF(PVR_DBG_ERROR, "PVRSRVWrapExtMemoryKM: "
					"Failed to alloc memory for block");
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorExitPhase2;
	}

	OSMemSet(psMemInfo, 0, sizeof(*psMemInfo));
	psMemBlock = &(psMemInfo->sMemBlk);
	bBMError = BM_Wrap(hDevMemHeap, ui32ByteSize, ui32PageOffset,
			   bPhysContig, psExtSysPAddr,
			   IMG_TRUE, NULL, &psMemInfo->ui32Flags, &hBuffer);
	if (!bBMError) {
		/* Alloc failed from current mapping heap, try the other one */
		psDevMemoryInfo->ui32MappingHeapID =
		    psDevMemoryInfo->ui32MappingHeapID ==
		    SGX_GENERAL_MAPPING_HEAP_ID ? SGX_ALT_MAPPING_HEAP_ID :
		    SGX_GENERAL_MAPPING_HEAP_ID;
		hDevMemHeap =
		    psDevMemoryInfo->psDeviceMemoryHeap[psDevMemoryInfo->
						ui32MappingHeapID]. hDevMemHeap;
		bBMError =
		    BM_Wrap(hDevMemHeap, ui32ByteSize, ui32PageOffset,
			    bPhysContig, psExtSysPAddr, IMG_TRUE, NULL,
			    &psMemInfo->ui32Flags, &hBuffer);
		if (!bBMError) {
			PVR_DPF(PVR_DBG_ERROR,
				 "PVRSRVWrapExtMemoryKM: BM_Wrap Failed");
			eError = PVRSRV_ERROR_BAD_MAPPING;
			goto ErrorExitPhase2;
		}
	}
	/* wrap was successful and BM_Wrap has taken ownership of the page list,
	 * clear psIntSysPAddr here, so we don't double free the memory */
	psIntSysPAddr = NULL;

	psMemBlock->sDevVirtAddr = BM_HandleToDevVaddr(hBuffer);
	psMemBlock->hOSMemHandle = BM_HandleToOSMemHandle(hBuffer);

	psMemBlock->hBuffer = (void *) hBuffer;

	psMemInfo->pvLinAddrKM = BM_HandleToCpuVaddr(hBuffer);
	psMemInfo->sDevVAddr = psMemBlock->sDevVirtAddr;
	psMemInfo->ui32AllocSize = ui32ByteSize;

	psBMHeap = (struct BM_HEAP *)hDevMemHeap;
	hDevMemContext = (void *) psBMHeap->pBMContext;
	psBMBuf = (struct BM_BUF *)hBuffer;
	if (psBMBuf->ui32RefCount == 1) {
		eError = PVRSRVAllocSyncInfoKM(hDevCookie, hDevMemContext,
					       &psMemInfo->psKernelSyncInfo);
		if (eError != PVRSRV_OK)
			goto ErrorExitPhase2;
		psBMBuf->pvKernelSyncInfo = psMemInfo->psKernelSyncInfo;
		psBMBuf->hOSWrapMem = hOSWrapMem;
	} else {
		psMemInfo->psKernelSyncInfo = psBMBuf->pvKernelSyncInfo;
	}

	psMemInfo->sMemBlk.hResItem =
	    ResManRegisterRes(psPerProc->hResManContext,
			      RESMAN_TYPE_DEVICEMEM_WRAP, psMemInfo, 0,
			      UnwrapExtMemoryCallBack);
	/* check if we were passed a page list
	 * but we didn't use use it */
	if (pPageList && (pPageList != psExtSysPAddr))
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
			  ui32PageCount * sizeof(struct IMG_SYS_PHYADDR),
			  (void *)pPageList, NULL);

	*ppsMemInfo = psMemInfo;

	return PVRSRV_OK;

ErrorExitPhase2:
	if (psMemInfo)
		FreeDeviceMem(psMemInfo);

	if (hOSWrapMem)
		OSReleasePhysPageAddr(hOSWrapMem, IMG_TRUE);
ErrorExitPhase1:
	if (psIntSysPAddr)
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
			  ui32PageCount * sizeof(struct IMG_SYS_PHYADDR),
			  psIntSysPAddr, NULL);

	return eError;
}

enum PVRSRV_ERROR PVRSRVUnmapDeviceMemoryKM(
		struct PVRSRV_KERNEL_MEM_INFO *psMemInfo)
{
	if (!psMemInfo)
		return PVRSRV_ERROR_INVALID_PARAMS;

	ResManFreeResByPtr(psMemInfo->sMemBlk.hResItem);
	return PVRSRV_OK;
}

static enum PVRSRV_ERROR UnmapDeviceMemoryCallBack(void *pvParam, u32 ui32Param)
{
	struct PVRSRV_KERNEL_MEM_INFO *psMemInfo = pvParam;

	PVR_UNREFERENCED_PARAMETER(ui32Param);

	return FreeDeviceMem(psMemInfo);
}

enum PVRSRV_ERROR PVRSRVMapDeviceMemoryKM(
		struct PVRSRV_PER_PROCESS_DATA *psPerProc,
			      struct PVRSRV_KERNEL_MEM_INFO *psSrcMemInfo,
			      void *hDstDevMemHeap,
			      struct PVRSRV_KERNEL_MEM_INFO **ppsDstMemInfo)
{
	enum PVRSRV_ERROR eError;
	u32 i;
	u32 ui32PageCount, ui32PageOffset;
	u32 ui32HostPageSize = HOST_PAGESIZE();
	struct IMG_SYS_PHYADDR *psSysPAddr = NULL;
	struct IMG_DEV_PHYADDR sDevPAddr;
	struct BM_BUF *psBuf;
	struct IMG_DEV_VIRTADDR sDevVAddr;
	struct PVRSRV_KERNEL_MEM_INFO *psMemInfo = NULL;
	void *hBuffer;
	struct PVRSRV_MEMBLK *psMemBlock;
	IMG_BOOL bBMError;
	struct PVRSRV_DEVICE_NODE *psDeviceNode;
	void *pvPageAlignedCPUVAddr;

	if (!psSrcMemInfo || !hDstDevMemHeap || !ppsDstMemInfo) {
		PVR_DPF(PVR_DBG_ERROR,
			 "PVRSRVMapDeviceMemoryKM: invalid parameters");
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	*ppsDstMemInfo = NULL;

	ui32PageOffset =
	    psSrcMemInfo->sDevVAddr.uiAddr & (ui32HostPageSize - 1);
	ui32PageCount =
	    HOST_PAGEALIGN(psSrcMemInfo->ui32AllocSize +
			   ui32PageOffset) / ui32HostPageSize;
	pvPageAlignedCPUVAddr =
	    (void *) ((u8 *) psSrcMemInfo->pvLinAddrKM -
			  ui32PageOffset);

	if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		       ui32PageCount * sizeof(struct IMG_SYS_PHYADDR),
		       (void **) &psSysPAddr, NULL) != PVRSRV_OK) {
		PVR_DPF(PVR_DBG_ERROR, "PVRSRVMapDeviceMemoryKM: "
					"Failed to alloc memory for block");
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psBuf = psSrcMemInfo->sMemBlk.hBuffer;

	psDeviceNode = psBuf->pMapping->pBMHeap->pBMContext->psDeviceNode;

	sDevVAddr.uiAddr = psSrcMemInfo->sDevVAddr.uiAddr - ui32PageOffset;
	for (i = 0; i < ui32PageCount; i++) {
		eError =
		    BM_GetPhysPageAddr(psSrcMemInfo, sDevVAddr, &sDevPAddr);
		if (eError != PVRSRV_OK) {
			PVR_DPF(PVR_DBG_ERROR, "PVRSRVMapDeviceMemoryKM: "
				"Failed to retrieve page list from device");
			goto ErrorExit;
		}

		psSysPAddr[i] =
		    SysDevPAddrToSysPAddr(psDeviceNode->sDevId.eDeviceType,
					  sDevPAddr);

		sDevVAddr.uiAddr += ui32HostPageSize;
	}

	if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		       sizeof(struct PVRSRV_KERNEL_MEM_INFO),
		       (void **) &psMemInfo, NULL) != PVRSRV_OK) {
		PVR_DPF(PVR_DBG_ERROR, "PVRSRVMapDeviceMemoryKM: "
					"Failed to alloc memory for block");
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto ErrorExit;
	}

	OSMemSet(psMemInfo, 0, sizeof(*psMemInfo));

	psMemBlock = &(psMemInfo->sMemBlk);

	bBMError = BM_Wrap(psBuf->pMapping->pBMHeap,
			   psSrcMemInfo->ui32AllocSize,
			   ui32PageOffset, IMG_FALSE, psSysPAddr,
			   IMG_TRUE, pvPageAlignedCPUVAddr,
			   &psMemInfo->ui32Flags, &hBuffer);

	if (!bBMError) {
		PVR_DPF(PVR_DBG_ERROR,
			 "PVRSRVMapDeviceMemoryKM: BM_Wrap Failed");
		eError = PVRSRV_ERROR_BAD_MAPPING;
		goto ErrorExit;
	}

	psMemBlock->sDevVirtAddr = BM_HandleToDevVaddr(hBuffer);
	psMemBlock->hOSMemHandle = BM_HandleToOSMemHandle(hBuffer);

	psMemBlock->hBuffer = (void *) hBuffer;

	psMemInfo->pvLinAddrKM = psSrcMemInfo->pvLinAddrKM;

	psMemInfo->sDevVAddr = psMemBlock->sDevVirtAddr;
	psMemInfo->ui32AllocSize = psSrcMemInfo->ui32AllocSize;
	psMemInfo->psKernelSyncInfo = psSrcMemInfo->psKernelSyncInfo;

	psMemInfo->sMemBlk.hResItem =
	    ResManRegisterRes(psPerProc->hResManContext,
			      RESMAN_TYPE_DEVICEMEM_MAPPING, psMemInfo, 0,
			      UnmapDeviceMemoryCallBack);

	*ppsDstMemInfo = psMemInfo;

	return PVRSRV_OK;

ErrorExit:

	if (psSysPAddr)
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP, ui32PageCount *
			    sizeof(struct IMG_SYS_PHYADDR), psSysPAddr, NULL);

	if (psMemInfo)
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
			  sizeof(struct PVRSRV_KERNEL_MEM_INFO), psMemInfo,
			  NULL);

	return eError;
}

enum PVRSRV_ERROR PVRSRVUnmapDeviceClassMemoryKM(
				struct PVRSRV_KERNEL_MEM_INFO *psMemInfo)
{
	if (!psMemInfo)
		return PVRSRV_ERROR_INVALID_PARAMS;

	ResManFreeResByPtr(psMemInfo->sMemBlk.hResItem);
	return PVRSRV_OK;
}

static enum PVRSRV_ERROR UnmapDeviceClassMemoryCallBack(void *pvParam,
							u32 ui32Param)
{
	struct PVRSRV_KERNEL_MEM_INFO *psMemInfo = pvParam;

	PVR_UNREFERENCED_PARAMETER(ui32Param);

	return FreeDeviceMem(psMemInfo);
}

enum PVRSRV_ERROR PVRSRVMapDeviceClassMemoryKM(
				struct PVRSRV_PER_PROCESS_DATA *psPerProc,
				void *hDeviceClassBuffer,
				struct PVRSRV_KERNEL_MEM_INFO **ppsMemInfo,
				void **phOSMapInfo)
{
	enum PVRSRV_ERROR eError;
	struct PVRSRV_KERNEL_MEM_INFO *psMemInfo;
	struct PVRSRV_DEVICECLASS_BUFFER *psDeviceClassBuffer;
	struct IMG_SYS_PHYADDR *psSysPAddr;
	void *pvCPUVAddr, *pvPageAlignedCPUVAddr;
	IMG_BOOL bPhysContig;
	struct BM_CONTEXT *psBMContext;
	struct DEVICE_MEMORY_INFO *psDevMemoryInfo;
	void *hDevMemHeap;
	u32 ui32ByteSize;
	u32 ui32Offset;
	u32 ui32PageSize = HOST_PAGESIZE();
	void *hBuffer;
	struct PVRSRV_MEMBLK *psMemBlock;
	IMG_BOOL bBMError;

	if (!hDeviceClassBuffer || !ppsMemInfo || !phOSMapInfo) {
		PVR_DPF(PVR_DBG_ERROR,
			 "PVRSRVMapDeviceClassMemoryKM: invalid parameters");
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDeviceClassBuffer = (struct PVRSRV_DEVICECLASS_BUFFER *)
							hDeviceClassBuffer;

	eError =
	    psDeviceClassBuffer->pfnGetBufferAddr(psDeviceClassBuffer->
						  hExtDevice,
						  psDeviceClassBuffer->
						  hExtBuffer, &psSysPAddr,
						  &ui32ByteSize,
						  (void __iomem **)&pvCPUVAddr,
						  phOSMapInfo, &bPhysContig);
	if (eError != PVRSRV_OK) {
		PVR_DPF(PVR_DBG_ERROR, "PVRSRVMapDeviceClassMemoryKM: "
				"unable to get buffer address");
		return PVRSRV_ERROR_GENERIC;
	}

	psBMContext = (struct BM_CONTEXT *)psDeviceClassBuffer->hDevMemContext;
	psDevMemoryInfo = &psBMContext->psDeviceNode->sDevMemoryInfo;
	hDevMemHeap =
	    psDevMemoryInfo->psDeviceMemoryHeap[SGX_FB_MAPPING_HEAP_ID].
	    hDevMemHeap;

	ui32Offset = ((u32) pvCPUVAddr) & (ui32PageSize - 1);
	pvPageAlignedCPUVAddr =
	    (void *) ((u8 *) pvCPUVAddr - ui32Offset);

	if (OSAllocMem(PVRSRV_OS_PAGEABLE_HEAP,
		       sizeof(struct PVRSRV_KERNEL_MEM_INFO),
		       (void **) &psMemInfo, NULL) != PVRSRV_OK) {
		PVR_DPF(PVR_DBG_ERROR, "PVRSRVMapDeviceClassMemoryKM: "
			 "Failed to alloc memory for block");
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	OSMemSet(psMemInfo, 0, sizeof(*psMemInfo));

	psMemBlock = &(psMemInfo->sMemBlk);

	bBMError = BM_Wrap(hDevMemHeap,
			   ui32ByteSize,
			   ui32Offset,
			   bPhysContig,
			   psSysPAddr,
			   IMG_FALSE,
			   pvPageAlignedCPUVAddr,
			   &psMemInfo->ui32Flags, &hBuffer);

	if (!bBMError) {
		PVR_DPF(PVR_DBG_ERROR,
			 "PVRSRVMapDeviceClassMemoryKM: BM_Wrap Failed");
		OSFreeMem(PVRSRV_OS_PAGEABLE_HEAP,
			  sizeof(struct PVRSRV_KERNEL_MEM_INFO),
			  psMemInfo, NULL);
		return PVRSRV_ERROR_BAD_MAPPING;
	}

	psMemBlock->sDevVirtAddr = BM_HandleToDevVaddr(hBuffer);
	psMemBlock->hOSMemHandle = BM_HandleToOSMemHandle(hBuffer);

	psMemBlock->hBuffer = (void *) hBuffer;

	psMemInfo->pvLinAddrKM = BM_HandleToCpuVaddr(hBuffer);

	psMemInfo->sDevVAddr = psMemBlock->sDevVirtAddr;
	psMemInfo->ui32AllocSize = ui32ByteSize;
	psMemInfo->psKernelSyncInfo = psDeviceClassBuffer->psKernelSyncInfo;

	psMemInfo->sMemBlk.hResItem =
	    ResManRegisterRes(psPerProc->hResManContext,
			      RESMAN_TYPE_DEVICECLASSMEM_MAPPING, psMemInfo, 0,
			      UnmapDeviceClassMemoryCallBack);

	*ppsMemInfo = psMemInfo;

	return PVRSRV_OK;
}
