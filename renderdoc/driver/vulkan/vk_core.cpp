/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "vk_core.h"
#include "vk_debug.h"

#include "serialise/string_utils.h"

#include "maths/formatpacking.h"

#include "jpeg-compressor/jpge.h"

static bool operator <(const VkExtensionProperties &a, const VkExtensionProperties &b)
{
	int cmp = strcmp(a.extName, b.extName);
	if(cmp == 0)
		return a.specVersion < b.specVersion;

	return cmp < 0;
}

const char *VkChunkNames[] =
{
	"WrappedVulkan::Initialisation",
	"vkCreateInstance",
	"vkEnumeratePhysicalDevices",
	"vkCreateDevice",
	"vkGetDeviceQueue",
	
	"vkAllocMemory",
	"vkUnmapMemory",
	"vkFlushMappedMemoryRanges",
	"vkFreeMemory",

	"vkCreateCommandPool",
	"vkResetCommandPool",

	"vkCreateCommandBuffer",
	"vkCreateFramebuffer",
	"vkCreateRenderPass",
	"vkCreateDescriptorPool",
	"vkCreateDescriptorSetLayout",
	"vkCreateBuffer",
	"vkCreateBufferView",
	"vkCreateImage",
	"vkCreateImageView",
	"vkCreateDepthTargetView",
	"vkCreateSampler",
	"vkCreateShader",
	"vkCreateShaderModule",
	"vkCreatePipelineLayout",
	"vkCreatePipelineCache",
	"vkCreateGraphicsPipelines",
	"vkCreateComputePipelines",
	"vkGetSwapchainImagesKHR",

	"vkCreateSemaphore",
	"vkCreateFence",
	"vkGetFenceStatus",
	"vkResetFences",
	"vkWaitForFences",
	
	"vkCreateEvent",
	"vkGetEventStatus",
	"vkSetEvent",
	"vkResetEvent",

	"vkCreateQueryPool",

	"vkAllocDescriptorSets",
	"vkUpdateDescriptorSets",

	"vkResetCommandBuffer",
	"vkBeginCommandBuffer",
	"vkEndCommandBuffer",

	"vkQueueSignalSemaphore",
	"vkQueueWaitSemaphore",
	"vkQueueWaitIdle",
	"vkDeviceWaitIdle",

	"vkQueueSubmit",
	"vkBindBufferMemory",
	"vkBindImageMemory",

	"vkCmdBeginRenderPass",
	"vkCmdNextSubpass",
	"vkCmdExecuteCommands",
	"vkCmdEndRenderPass",

	"vkCmdBindPipeline",

	"vkCmdSetViewport",
	"vkCmdSetScissor",
	"vkCmdSetLineWidth",
	"vkCmdSetDepthBias",
	"vkCmdSetBlendConstants",
	"vkCmdSetDepthBounds",
	"vkCmdSetStencilCompareMask",
	"vkCmdSetStencilWriteMask",
	"vkCmdSetStencilReference",

	"vkCmdBindDescriptorSet",
	"vkCmdBindVertexBuffers",
	"vkCmdBindIndexBuffer",
	"vkCmdCopyBufferToImage",
	"vkCmdCopyImageToBuffer",
	"vkCmdCopyBuffer",
	"vkCmdCopyImage",
	"vkCmdBlitImage",
	"vkCmdResolveImage",
	"vkCmdUpdateBuffer",
	"vkCmdFillBuffer",
	"vkCmdPushConstants",

	"vkCmdClearColorImage",
	"vkCmdClearDepthStencilImage",
	"vkCmdClearColorAttachment",
	"vkCmdClearDepthStencilAttachment",
	"vkCmdPipelineBarrier",

	"vkCmdWriteTimestamp",
	"vkCmdCopyQueryPoolResults",
	"vkCmdBeginQuery",
	"vkCmdEndQuery",
	"vkCmdResetQueryPool",

	"vkCmdSetEvent",
	"vkCmdResetEvent",
	"vkCmdWaitEvents",

	"vkCmdDraw",
	"vkCmdDrawIndirect",
	"vkCmdDrawIndexed",
	"vkCmdDrawIndexedIndirect",
	"vkCmdDispatch",
	"vkCmdDispatchIndirect",
	
	"vkCmdDbgMarkerBegin",
	"vkCmdDbgMarker", // no equivalent function at the moment
	"vkCmdDbgMarkerEnd",

	"vkCreateSwapchainKHR",

	"Capture",
	"BeginCapture",
	"EndCapture",
};

VkInitParams::VkInitParams()
{
	SerialiseVersion = VK_SERIALISE_VERSION;
}

ReplayCreateStatus VkInitParams::Serialise()
{
	Serialiser *localSerialiser = GetSerialiser();

	SERIALISE_ELEMENT(uint32_t, ver, VK_SERIALISE_VERSION); SerialiseVersion = ver;

	if(ver != VK_SERIALISE_VERSION)
	{
		RDCERR("Incompatible Vulkan serialise version, expected %d got %d", VK_SERIALISE_VERSION, ver);
		return eReplayCreate_APIIncompatibleVersion;
	}

	localSerialiser->Serialise("AppName", AppName);
	localSerialiser->Serialise("EngineName", EngineName);
	localSerialiser->Serialise("AppVersion", AppVersion);
	localSerialiser->Serialise("EngineVersion", EngineVersion);
	localSerialiser->Serialise("APIVersion", APIVersion);

	localSerialiser->Serialise("Layers", Layers);
	localSerialiser->Serialise("Extensions", Extensions);

	localSerialiser->Serialise("InstanceID", InstanceID);

	return eReplayCreate_Success;
}

void VkInitParams::Set(const VkInstanceCreateInfo* pCreateInfo, ResourceId inst)
{
	RDCASSERT(pCreateInfo);

	if(pCreateInfo->pAppInfo)
	{
		// we don't support any extensions on appinfo structure
		RDCASSERT(pCreateInfo->pAppInfo->pNext == NULL);

		AppName = pCreateInfo->pAppInfo->pAppName ? pCreateInfo->pAppInfo->pAppName : "";
		EngineName = pCreateInfo->pAppInfo->pEngineName ? pCreateInfo->pAppInfo->pEngineName : "";

		AppVersion = pCreateInfo->pAppInfo->appVersion;
		EngineVersion = pCreateInfo->pAppInfo->engineVersion;
		APIVersion = pCreateInfo->pAppInfo->apiVersion;
	}
	else
	{
		AppName = "";
		EngineName = "";

		AppVersion = 0;
		EngineVersion = 0;
		APIVersion = 0;
	}

	Layers.resize(pCreateInfo->layerCount);
	Extensions.resize(pCreateInfo->extensionCount);

	for(uint32_t i=0; i < pCreateInfo->layerCount; i++)
		Layers[i] = pCreateInfo->ppEnabledLayerNames[i];

	for(uint32_t i=0; i < pCreateInfo->extensionCount; i++)
		Extensions[i] = pCreateInfo->ppEnabledExtensionNames[i];

	InstanceID = inst;
}

WrappedVulkan::WrappedVulkan(const char *logFilename)
{
#if defined(RELEASE)
	const bool debugSerialiser = false;
#else
	const bool debugSerialiser = true;
#endif

	if(RenderDoc::Inst().IsReplayApp())
	{
		m_State = READING;
		if(logFilename)
		{
			m_pSerialiser = new Serialiser(logFilename, Serialiser::READING, debugSerialiser);
		}
		else
		{
			byte dummy[4];
			m_pSerialiser = new Serialiser(4, dummy, false);
		}
	}
	else
	{
		m_State = WRITING_IDLE;
		m_pSerialiser = new Serialiser(NULL, Serialiser::WRITING, debugSerialiser);
	}

	m_Replay.SetDriver(this);

	m_FrameCounter = 0;

	m_AppControlledCapture = false;

	m_FrameTimer.Restart();

	threadSerialiserTLSSlot = Threading::AllocateTLSSlot();
	tempMemoryTLSSlot = Threading::AllocateTLSSlot();

	m_TotalTime = m_AvgFrametime = m_MinFrametime = m_MaxFrametime = 0.0;
	
	m_RootEventID = 1;
	m_RootDrawcallID = 1;
	m_FirstEventID = 0;
	m_LastEventID = ~0U;

	m_LastCmdBufferID = ResourceId();

	m_PartialReplayData.renderPassActive = false;
	m_PartialReplayData.resultPartialCmdBuffer = VK_NULL_HANDLE;
	m_PartialReplayData.singleDrawCmdBuffer = VK_NULL_HANDLE;
	m_PartialReplayData.partialParent = ResourceId();
	m_PartialReplayData.baseEvent = 0;

	m_DrawcallStack.push_back(&m_ParentDrawcall);

	m_ResourceManager = new VulkanResourceManager(m_State, m_pSerialiser, this);

	m_pSerialiser->SetUserData(m_ResourceManager);

	m_HeaderChunk = NULL;

	if(!RenderDoc::Inst().IsReplayApp())
	{
		m_FrameCaptureRecord = GetResourceManager()->AddResourceRecord(ResourceIDGen::GetNewUniqueID());
		m_FrameCaptureRecord->DataInSerialiser = false;
		m_FrameCaptureRecord->Length = 0;
		m_FrameCaptureRecord->NumSubResources = 0;
		m_FrameCaptureRecord->SpecialResource = true;
		m_FrameCaptureRecord->SubResources = NULL;
	}
	else
	{
		m_FrameCaptureRecord = NULL;

		ResourceIDGen::SetReplayResourceIDs();
	}
		
#if !defined(RELEASE)
	RDCDEBUG("Debug Text enabled - for development! remove before release!");
	m_pSerialiser->SetDebugText(true);
#endif
	
	m_pSerialiser->SetChunkNameLookup(&GetChunkName);

	//////////////////////////////////////////////////////////////////////////
	// Compile time asserts

	RDCCOMPILE_ASSERT(ARRAY_COUNT(VkChunkNames) == NUM_VULKAN_CHUNKS-FIRST_CHUNK_ID, "Not right number of chunk names");
}

WrappedVulkan::~WrappedVulkan()
{
	// records must be deleted before resource manager shutdown
	if(m_FrameCaptureRecord)
	{
		RDCASSERT(m_FrameCaptureRecord->GetRefCount() == 1);
		m_FrameCaptureRecord->Delete(GetResourceManager());
		m_FrameCaptureRecord = NULL;
	}

	// in case the application leaked some objects, avoid crashing trying
	// to release them ourselves by clearing the resource manager.
	// In a well-behaved application, this should be a no-op.
	m_ResourceManager->ClearWithoutReleasing();
	SAFE_DELETE(m_ResourceManager);
		
	SAFE_DELETE(m_pSerialiser);

	for(size_t i=0; i < m_MemIdxMaps.size(); i++)
		delete[] m_MemIdxMaps[i];

	for(size_t i=0; i < m_ThreadSerialisers.size(); i++)
		delete m_ThreadSerialisers[i];

	for(size_t i=0; i < m_ThreadTempMem.size(); i++)
	{
		delete[] m_ThreadTempMem[i]->memory;
		delete m_ThreadTempMem[i];
	}
}

VkCmdBuffer WrappedVulkan::GetNextCmd()
{
	VkCmdBuffer ret;

	if(!m_InternalCmds.freecmds.empty())
	{
		ret = m_InternalCmds.freecmds.back();
		m_InternalCmds.freecmds.pop_back();

		ObjDisp(ret)->ResetCommandBuffer(Unwrap(ret), 0);
	}
	else
	{	
		VkCmdBufferCreateInfo cmdInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_CREATE_INFO, NULL, Unwrap(m_InternalCmds.m_CmdPool), VK_CMD_BUFFER_LEVEL_PRIMARY, 0 };
		VkResult vkr = ObjDisp(m_Device)->CreateCommandBuffer(Unwrap(m_Device), &cmdInfo, &ret);
		RDCASSERT(vkr == VK_SUCCESS);

		GetResourceManager()->WrapResource(Unwrap(m_Device), ret);
	}

	m_InternalCmds.pendingcmds.push_back(ret);

	return ret;
}

void WrappedVulkan::SubmitCmds()
{
	// nothing to do
	if(m_InternalCmds.pendingcmds.empty())
		return;

	vector<VkCmdBuffer> cmds = m_InternalCmds.pendingcmds;
	for(size_t i=0; i < cmds.size(); i++) cmds[i] = Unwrap(cmds[i]);

	ObjDisp(m_Queue)->QueueSubmit(Unwrap(m_Queue), (uint32_t)cmds.size(), &cmds[0], VK_NULL_HANDLE);

	m_InternalCmds.submittedcmds.insert(m_InternalCmds.submittedcmds.end(), m_InternalCmds.pendingcmds.begin(), m_InternalCmds.pendingcmds.end());
	m_InternalCmds.pendingcmds.clear();
}

void WrappedVulkan::FlushQ()
{
	// VKTODOLOW could do away with the need for this function by keeping
	// commands until N presents later, or something, or checking on fences

	ObjDisp(m_Queue)->QueueWaitIdle(Unwrap(m_Queue));

	if(!m_InternalCmds.submittedcmds.empty())
	{
		m_InternalCmds.freecmds.insert(m_InternalCmds.freecmds.end(), m_InternalCmds.submittedcmds.begin(), m_InternalCmds.submittedcmds.end());
		m_InternalCmds.submittedcmds.clear();
	}
}

const char * WrappedVulkan::GetChunkName(uint32_t idx)
{
	if(idx < FIRST_CHUNK_ID || idx >= NUM_VULKAN_CHUNKS)
		return "<unknown>";
	return VkChunkNames[idx-FIRST_CHUNK_ID];
}

byte *WrappedVulkan::GetTempMemory(size_t s)
{
	TempMem *mem = (TempMem *)Threading::GetTLSValue(tempMemoryTLSSlot);
	if(mem && mem->size >= s) return mem->memory;

	// alloc or grow alloc
	TempMem *newmem = mem;

	if(!newmem) newmem = new TempMem();

	// free old memory, don't need to keep contents
	if(newmem->memory) delete[] newmem->memory;

	// alloc new memory
	newmem->size = s;
	newmem->memory = new byte[s];

	Threading::SetTLSValue(tempMemoryTLSSlot, (void *)newmem);

	// if this is entirely new, save it for deletion on shutdown
	if(!mem)
	{
		SCOPED_LOCK(m_ThreadTempMemLock);
		m_ThreadTempMem.push_back(newmem);
	}

	return newmem->memory;
}

Serialiser *WrappedVulkan::GetThreadSerialiser()
{
	Serialiser *ser = (Serialiser *)Threading::GetTLSValue(threadSerialiserTLSSlot);
	if(ser) return ser;

	// slow path, but rare

#if defined(RELEASE)
	const bool debugSerialiser = false;
#else
	const bool debugSerialiser = true;
#endif

	ser = new Serialiser(NULL, Serialiser::WRITING, debugSerialiser);
	ser->SetUserData(m_ResourceManager);
	
#if !defined(RELEASE)
	RDCDEBUG("Debug Text enabled - for development! remove before release!");
	ser->SetDebugText(true);
#endif
	
	ser->SetChunkNameLookup(&GetChunkName);

	Threading::SetTLSValue(threadSerialiserTLSSlot, (void *)ser);

	{
		SCOPED_LOCK(m_ThreadSerialisersLock);
		m_ThreadSerialisers.push_back(ser);
	}
	
	return ser;
}

void WrappedVulkan::Serialise_CaptureScope(uint64_t offset)
{
	uint32_t FrameNumber = m_FrameCounter;
	GetMainSerialiser()->Serialise("FrameNumber", FrameNumber); // must use main serialiser here to match resource manager below

	if(m_State >= WRITING)
	{
		GetResourceManager()->Serialise_InitialContentsNeeded();
	}
	else
	{
		FetchFrameRecord record;
		record.frameInfo.fileOffset = offset;
		record.frameInfo.firstEvent = 1;//m_pImmediateContext->GetEventID();
		record.frameInfo.frameNumber = FrameNumber;
		record.frameInfo.immContextId = ResourceId();
		m_FrameRecord.push_back(record);

		GetResourceManager()->CreateInitialContents();
	}
}

void WrappedVulkan::EndCaptureFrame(VkImage presentImage)
{
	// must use main serialiser here to match resource manager
	Serialiser *localSerialiser = GetMainSerialiser();

	SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_FOOTER);
	
	SERIALISE_ELEMENT(ResourceId, bbid, GetResID(presentImage));

	bool HasCallstack = RenderDoc::Inst().GetCaptureOptions().CaptureCallstacks != 0;
	localSerialiser->Serialise("HasCallstack", HasCallstack);	

	if(HasCallstack)
	{
		Callstack::Stackwalk *call = Callstack::Collect();

		RDCASSERT(call->NumLevels() < 0xff);

		size_t numLevels = call->NumLevels();
		uint64_t *stack = (uint64_t *)call->GetAddrs();

		localSerialiser->SerialisePODArray("callstack", stack, numLevels);

		delete call;
	}

	m_FrameCaptureRecord->AddChunk(scope.Get());
}

void WrappedVulkan::AttemptCapture()
{
	{
		RDCDEBUG("Attempting capture");

		//m_SuccessfulCapture = true;

		m_FrameCaptureRecord->LockChunks();
		while(m_FrameCaptureRecord->HasChunks())
		{
			Chunk *chunk = m_FrameCaptureRecord->GetLastChunk();

			SAFE_DELETE(chunk);
			m_FrameCaptureRecord->PopChunk();
		}
		m_FrameCaptureRecord->UnlockChunks();
	}
}

bool WrappedVulkan::Serialise_BeginCaptureFrame(bool applyInitialState)
{
	if(m_State < WRITING && !applyInitialState)
	{
		m_pSerialiser->SkipCurrentChunk();
		return true;
	}

	vector<VkImageMemoryBarrier> imgTransitions;
	
	{
		SCOPED_LOCK(m_ImageLayoutsLock); // not needed on replay, but harmless also
		GetResourceManager()->SerialiseImageStates(m_ImageLayouts, imgTransitions);
	}

	if(applyInitialState && !imgTransitions.empty())
	{
		VkCmdBuffer cmd = GetNextCmd();

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		VkResult vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		
		VkPipelineStageFlags src_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dest_stages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		if(!imgTransitions.empty())
		{
			vector<void *> barriers;
			for(size_t i=0; i < imgTransitions.size(); i++)
				barriers.push_back(&imgTransitions[i]);
			ObjDisp(cmd)->CmdPipelineBarrier(Unwrap(cmd), src_stages, dest_stages, false, (uint32_t)imgTransitions.size(), (const void *const *)&barriers[0]);
		}

		vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);

		SubmitCmds();
		// don't need to flush here
	}

	return true;
}
	
void WrappedVulkan::BeginCaptureFrame()
{
	// must use main serialiser here to match resource manager
	Serialiser *localSerialiser = GetMainSerialiser();

	SCOPED_SERIALISE_CONTEXT(CONTEXT_CAPTURE_HEADER);

	Serialise_BeginCaptureFrame(false);

	// need to hold onto this as it must come right after the capture chunk,
	// before any command buffers
	m_HeaderChunk = scope.Get();
}

void WrappedVulkan::FinishCapture()
{
	m_State = WRITING_IDLE;

	//m_SuccessfulCapture = false;

	ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(GetDev()));

	{
		SCOPED_LOCK(m_CoherentMapsLock);
		for(auto it = m_CoherentMaps.begin(); it != m_CoherentMaps.end(); ++it)
		{
			Serialiser::FreeAlignedBuffer((*it)->memMapState->refData);
			(*it)->memMapState->refData = NULL;
		}
	}
}

void WrappedVulkan::StartFrameCapture(void *dev, void *wnd)
{
	if(m_State != WRITING_IDLE) return;
	
	RenderDoc::Inst().SetCurrentDriver(RDC_Vulkan);

	m_AppControlledCapture = true;
	
	FetchFrameRecord record;
	record.frameInfo.frameNumber = m_FrameCounter+1;
	record.frameInfo.captureTime = Timing::GetUnixTimestamp();
	m_FrameRecord.push_back(record);

	GetResourceManager()->ClearReferencedResources();

	GetResourceManager()->MarkResourceFrameReferenced(GetResID(m_Instance), eFrameRef_Read);

	// need to do all this atomically so that no other commands
	// will check to see if they need to markdirty or markpendingdirty
	// and go into the frame record.
	{
		SCOPED_LOCK(m_CapTransitionLock);
		GetResourceManager()->PrepareInitialContents();

		AttemptCapture();
		BeginCaptureFrame();

		m_State = WRITING_CAPFRAME;
	}

	RDCLOG("Starting capture, frame %u", m_FrameCounter);
}

bool WrappedVulkan::EndFrameCapture(void *dev, void *wnd)
{
	if(m_State != WRITING_CAPFRAME) return true;
	
	VkSwapchainKHR swap = VK_NULL_HANDLE;
	
	if(wnd)
	{
		{
			SCOPED_LOCK(m_SwapLookupLock);
			auto it = m_SwapLookup.find(wnd);
			if(it != m_SwapLookup.end())
				swap = it->second;
		}

		if(swap == VK_NULL_HANDLE)
		{
			RDCERR("Output window %p provided for frame capture corresponds with no known swap chain", wnd);
			return false;
		}
	}

	RDCLOG("Finished capture, Frame %u", m_FrameCounter);

	VkImage backbuffer = VK_NULL_HANDLE;
	VkResourceRecord *swaprecord = NULL;

	if(swap != VK_NULL_HANDLE)
	{
		GetResourceManager()->MarkResourceFrameReferenced(GetResID(swap), eFrameRef_Read);
	
		swaprecord = GetRecord(swap);
		RDCASSERT(swaprecord->swapInfo);

		const SwapchainInfo &swapInfo = *swaprecord->swapInfo;
		
		backbuffer = swapInfo.images[swapInfo.lastPresent].im;
	}
	
	// transition back to IDLE atomically
	{
		SCOPED_LOCK(m_CapTransitionLock);
		EndCaptureFrame(backbuffer);
		FinishCapture();
	}

	byte *thpixels = NULL;
	uint32_t thwidth = 0;
	uint32_t thheight = 0;

	// gather backbuffer screenshot
	const int32_t maxSize = 1024;

	if(swap != VK_NULL_HANDLE)
	{
		VkDevice dev = GetDev();
		VkCmdBuffer cmd = GetNextCmd();

		const VkLayerDispatchTable *vt = ObjDisp(dev);

		vt->DeviceWaitIdle(Unwrap(dev));
		
		const SwapchainInfo &swapInfo = *swaprecord->swapInfo;

		// since these objects are very short lived (only this scope), we
		// don't wrap them.
		VkImage readbackIm = VK_NULL_HANDLE;
		VkDeviceMemory readbackMem = VK_NULL_HANDLE;

		VkResult vkr = VK_SUCCESS;

		// create identical image
		VkImageCreateInfo imInfo = {
			VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL,
			VK_IMAGE_TYPE_2D, swapInfo.format,
			{ swapInfo.extent.width, swapInfo.extent.height, 1 }, 1, 1, 1,
			VK_IMAGE_TILING_LINEAR, VK_IMAGE_USAGE_TRANSFER_DESTINATION_BIT,
			0,
			VK_SHARING_MODE_EXCLUSIVE, 0, NULL,
			VK_IMAGE_LAYOUT_UNDEFINED,
		};
		vt->CreateImage(Unwrap(dev), &imInfo, &readbackIm);
		RDCASSERT(vkr == VK_SUCCESS);

		VkMemoryRequirements mrq;
		vkr = vt->GetImageMemoryRequirements(Unwrap(dev), readbackIm, &mrq);
		RDCASSERT(vkr == VK_SUCCESS);

		VkImageSubresource subr = { VK_IMAGE_ASPECT_COLOR, 0, 0 };
		VkSubresourceLayout layout = { 0 };
		vt->GetImageSubresourceLayout(Unwrap(dev), readbackIm, &subr, &layout);

		// allocate readback memory
		VkMemoryAllocInfo allocInfo = {
			VK_STRUCTURE_TYPE_MEMORY_ALLOC_INFO, NULL,
			mrq.size, GetReadbackMemoryIndex(mrq.memoryTypeBits),
		};

		vkr = vt->AllocMemory(Unwrap(dev), &allocInfo, &readbackMem);
		RDCASSERT(vkr == VK_SUCCESS);
		vkr = vt->BindImageMemory(Unwrap(dev), readbackIm, readbackMem, 0);
		RDCASSERT(vkr == VK_SUCCESS);

		VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

		// do image copy
		vkr = vt->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
		RDCASSERT(vkr == VK_SUCCESS);

		VkImageCopy cpy = {
			{ VK_IMAGE_ASPECT_COLOR, 0, 0, 1 },
			{ 0, 0, 0 },
			{ VK_IMAGE_ASPECT_COLOR, 0, 0, 1 },
			{ 0, 0, 0 },
			{ imInfo.extent.width, imInfo.extent.height, 1 },
		};

		VkImageMemoryBarrier bbTrans = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, 0, VK_IMAGE_LAYOUT_PRESENT_SOURCE_KHR, VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			Unwrap(backbuffer),
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		VkImageMemoryBarrier readTrans = {
			VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL,
			0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			readbackIm, // was never wrapped
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};

		VkImageMemoryBarrier *barriers[] = {
			&bbTrans,
			&readTrans,
		};

		vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 2, (void **)barriers);

		vt->CmdCopyImage(Unwrap(cmd), Unwrap(backbuffer), VK_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL, readbackIm, VK_IMAGE_LAYOUT_TRANSFER_DESTINATION_OPTIMAL, 1, &cpy);

		// transition backbuffer back
		std::swap(bbTrans.oldLayout, bbTrans.newLayout);

		readTrans.oldLayout = readTrans.newLayout;
		readTrans.newLayout = VK_IMAGE_LAYOUT_GENERAL;

		vt->CmdPipelineBarrier(Unwrap(cmd), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, false, 2, (void **)barriers);

		vkr = vt->EndCommandBuffer(Unwrap(cmd));
		RDCASSERT(vkr == VK_SUCCESS);

		SubmitCmds();
		FlushQ(); // need to wait so we can readback

		// map memory and readback
		byte *pData = NULL;
		vkr = vt->MapMemory(Unwrap(dev), readbackMem, 0, 0, 0, (void **)&pData);
		RDCASSERT(vkr == VK_SUCCESS);

		RDCASSERT(pData != NULL);

		// point sample info into raw buffer
		{
			ResourceFormat fmt = MakeResourceFormat(imInfo.format);

			byte *data = (byte *)pData;

			data += layout.offset;

			float widthf = float(imInfo.extent.width);
			float heightf = float(imInfo.extent.height);

			float aspect = widthf/heightf;

			thwidth = RDCMIN(maxSize, imInfo.extent.width);
			thwidth &= ~0x7; // align down to multiple of 8
			thheight = uint32_t(float(thwidth)/aspect);

			thpixels = new byte[3*thwidth*thheight];

			uint32_t stride = fmt.compByteWidth*fmt.compCount;

			bool buf1010102 = false;
			bool bufBGRA = false;

			if(fmt.special && fmt.specialFormat == eSpecial_R10G10B10A2)
			{
				stride = 4;
				buf1010102 = true;
			}
			if(fmt.special && fmt.specialFormat == eSpecial_B8G8R8A8)
			{
				stride = 4;
				bufBGRA = true;
			}

			byte *dst = thpixels;

			for(uint32_t y=0; y < thheight; y++)
			{
				for(uint32_t x=0; x < thwidth; x++)
				{
					float xf = float(x)/float(thwidth);
					float yf = float(y)/float(thheight);

					byte *src = &data[ stride*uint32_t(xf*widthf) + layout.rowPitch*uint32_t(yf*heightf) ];

					if(buf1010102)
					{
						uint32_t *src1010102 = (uint32_t *)src;
						Vec4f unorm = ConvertFromR10G10B10A2(*src1010102);
						dst[0] = (byte)(unorm.x*255.0f);
						dst[1] = (byte)(unorm.y*255.0f);
						dst[2] = (byte)(unorm.z*255.0f);
					}
					else if(bufBGRA)
					{
						dst[0] = src[2];
						dst[1] = src[1];
						dst[2] = src[0];
					}
					else if(fmt.compByteWidth == 2) // R16G16B16A16 backbuffer
					{
						uint16_t *src16 = (uint16_t *)src;

						float linearR = RDCCLAMP(ConvertFromHalf(src16[0]), 0.0f, 1.0f);
						float linearG = RDCCLAMP(ConvertFromHalf(src16[1]), 0.0f, 1.0f);
						float linearB = RDCCLAMP(ConvertFromHalf(src16[2]), 0.0f, 1.0f);

						if(linearR < 0.0031308f) dst[0] = byte(255.0f*(12.92f * linearR));
						else                     dst[0] = byte(255.0f*(1.055f * powf(linearR, 1.0f/2.4f) - 0.055f));

						if(linearG < 0.0031308f) dst[1] = byte(255.0f*(12.92f * linearG));
						else                     dst[1] = byte(255.0f*(1.055f * powf(linearG, 1.0f/2.4f) - 0.055f));

						if(linearB < 0.0031308f) dst[2] = byte(255.0f*(12.92f * linearB));
						else                     dst[2] = byte(255.0f*(1.055f * powf(linearB, 1.0f/2.4f) - 0.055f));
					}
					else
					{
						dst[0] = src[0];
						dst[1] = src[1];
						dst[2] = src[2];
					}

					dst += 3;
				}
			}
		}

		vt->UnmapMemory(Unwrap(dev), readbackMem);

		// delete all
		vt->DestroyImage(Unwrap(dev), readbackIm);
		vt->FreeMemory(Unwrap(dev), readbackMem);
	}

	byte *jpgbuf = NULL;
	int len = thwidth*thheight;

	if(wnd)
	{
		jpgbuf = new byte[len];

		jpge::params p;

		p.m_quality = 40;

		bool success = jpge::compress_image_to_jpeg_file_in_memory(jpgbuf, len, thwidth, thheight, 3, thpixels, p);

		if(!success)
		{
			RDCERR("Failed to compress to jpg");
			SAFE_DELETE_ARRAY(jpgbuf);
			thwidth = 0;
			thheight = 0;
		}
	}

	Serialiser *m_pFileSerialiser = RenderDoc::Inst().OpenWriteSerialiser(m_FrameCounter, &m_InitParams, jpgbuf, len, thwidth, thheight);

	{
		CACHE_THREAD_SERIALISER();

		SCOPED_SERIALISE_CONTEXT(DEVICE_INIT);

		m_pFileSerialiser->Insert(scope.Get(true));
	}

	RDCDEBUG("Inserting Resource Serialisers");	

	GetResourceManager()->InsertReferencedChunks(m_pFileSerialiser);

	GetResourceManager()->InsertInitialContentsChunks(m_pFileSerialiser);

	RDCDEBUG("Creating Capture Scope");	

	{
		Serialiser *localSerialiser = GetMainSerialiser();

		SCOPED_SERIALISE_CONTEXT(CAPTURE_SCOPE);

		Serialise_CaptureScope(0);

		m_pFileSerialiser->Insert(scope.Get(true));

		m_pFileSerialiser->Insert(m_HeaderChunk);
	}

	// don't need to lock access to m_CmdBufferRecords as we are no longer 
	// in capframe (the transition is thread-protected) so nothing will be
	// pushed to the vector

	{
		RDCDEBUG("Flushing %u command buffer records to file serialiser", (uint32_t)m_CmdBufferRecords.size());	

		map<int32_t, Chunk *> recordlist;

		// ensure all command buffer records within the frame evne if recorded before, but
		// otherwise order must be preserved (vs. queue submits and desc set updates)
		for(size_t i=0; i < m_CmdBufferRecords.size(); i++)
		{
			m_CmdBufferRecords[i]->Insert(recordlist);

			RDCDEBUG("Adding %u chunks to file serialiser from command buffer %llu", (uint32_t)recordlist.size(), m_CmdBufferRecords[i]->GetResourceID());	
		}

		m_FrameCaptureRecord->Insert(recordlist);

		RDCDEBUG("Flushing %u chunks to file serialiser from context record", (uint32_t)recordlist.size());	

		for(auto it = recordlist.begin(); it != recordlist.end(); ++it)
			m_pFileSerialiser->Insert(it->second);

		RDCDEBUG("Done");	
	}

	m_pFileSerialiser->FlushToDisk();

	RenderDoc::Inst().SuccessfullyWrittenLog();

	SAFE_DELETE(m_pFileSerialiser);
	SAFE_DELETE(m_HeaderChunk);

	m_State = WRITING_IDLE;

	// delete cmd buffers now - had to keep them alive until after serialiser flush.
	for(size_t i=0; i < m_CmdBufferRecords.size(); i++)
		m_CmdBufferRecords[i]->Delete(GetResourceManager());

	m_CmdBufferRecords.clear();

	GetResourceManager()->MarkUnwrittenResources();

	GetResourceManager()->ClearReferencedResources();

	GetResourceManager()->FreeInitialContents();

	GetResourceManager()->FlushPendingDirty();

	return true;
}

void WrappedVulkan::ReadLogInitialisation()
{
	uint64_t lastFrame = 0;
	uint64_t firstFrame = 0;

	m_pSerialiser->SetDebugText(true);

	m_pSerialiser->Rewind();

	while(!m_pSerialiser->AtEnd())
	{
		m_pSerialiser->SkipToChunk(CAPTURE_SCOPE);

		// found a capture chunk
		if(!m_pSerialiser->AtEnd())
		{
			lastFrame = m_pSerialiser->GetOffset();
			if(firstFrame == 0)
				firstFrame = m_pSerialiser->GetOffset();

			// skip this chunk
			m_pSerialiser->PushContext(NULL, NULL, CAPTURE_SCOPE, false);
			m_pSerialiser->SkipCurrentChunk();
			m_pSerialiser->PopContext(CAPTURE_SCOPE);
		}
	}

	m_pSerialiser->Rewind();

	int chunkIdx = 0;

	struct chunkinfo
	{
		chunkinfo() : count(0), totalsize(0), total(0.0) {}
		int count;
		uint64_t totalsize;
		double total;
	};

	map<VulkanChunkType,chunkinfo> chunkInfos;

	SCOPED_TIMER("chunk initialisation");

	while(1)
	{
		PerformanceTimer timer;

		uint64_t offset = m_pSerialiser->GetOffset();

		VulkanChunkType context = (VulkanChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

		if(context == CAPTURE_SCOPE)
		{
			// immediately read rest of log into memory
			m_pSerialiser->SetPersistentBlock(offset);
		}

		chunkIdx++;

		ProcessChunk(offset, context);

		m_pSerialiser->PopContext(context);
		
		RenderDoc::Inst().SetProgress(FileInitialRead, float(m_pSerialiser->GetOffset())/float(m_pSerialiser->GetSize()));

		if(context == CAPTURE_SCOPE)
		{
			GetResourceManager()->ApplyInitialContents();

			SubmitCmds();
			FlushQ();

			ContextReplayLog(READING, 0, 0, false);
		}

		uint64_t offset2 = m_pSerialiser->GetOffset();

		chunkInfos[context].total += timer.GetMilliseconds();
		chunkInfos[context].totalsize += offset2 - offset;
		chunkInfos[context].count++;

		if(context == CAPTURE_SCOPE)
		{
			if(m_pSerialiser->GetOffset() > lastFrame)
				break;
		}

		if(m_pSerialiser->AtEnd())
		{
			break;
		}
	}

	for(auto it=chunkInfos.begin(); it != chunkInfos.end(); ++it)
	{
		double dcount = double(it->second.count);

		RDCDEBUG("% 5d chunks - Time: %9.3fms total/%9.3fms avg - Size: %8.3fMB total/%7.3fMB avg - %s (%u)",
				it->second.count,
				it->second.total, it->second.total/dcount,
				double(it->second.totalsize)/(1024.0*1024.0),
				double(it->second.totalsize)/(dcount*1024.0*1024.0),
				GetChunkName(it->first), uint32_t(it->first)
				);
	}

	RDCDEBUG("Allocating %llu persistant bytes of memory for the log.", m_pSerialiser->GetSize() - firstFrame);
	
	m_pSerialiser->SetDebugText(false);

	// ensure the capture at least created a device and fetched a queue.
	RDCASSERT(m_Device != VK_NULL_HANDLE && m_Queue != VK_NULL_HANDLE && m_InternalCmds.m_CmdPool != VK_NULL_HANDLE);
}

void WrappedVulkan::ContextReplayLog(LogState readType, uint32_t startEventID, uint32_t endEventID, bool partial)
{
	m_State = readType;

	VulkanChunkType header = (VulkanChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);
	RDCASSERT(header == CONTEXT_CAPTURE_HEADER);

	WrappedVulkan *context = this;

	Serialise_BeginCaptureFrame(!partial);
	
	ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(GetDev()));

	m_pSerialiser->PopContext(header);

	m_RootEvents.clear();

	m_CmdBuffersInProgress = 0;
	
	if(m_State == EXECUTING)
	{
		FetchAPIEvent ev = GetEvent(startEventID);
		m_RootEventID = ev.eventID;

		// if not partial, we need to be sure to replay
		// past the command buffer records, so can't
		// skip to the file offset of the first event
		if(partial)
			m_pSerialiser->SetOffset(ev.fileOffset);

		m_FirstEventID = startEventID;
		m_LastEventID = endEventID;
	}
	else if(m_State == READING)
	{
		m_RootEventID = 1;
		m_RootDrawcallID = 1;
		m_FirstEventID = 0;
		m_LastEventID = ~0U;
	}

	while(1)
	{
		if(m_State == EXECUTING && m_RootEventID > endEventID)
		{
			// we can just break out if we've done all the events desired.
			// note that the command buffer events aren't 'real' and we just blaze through them
			break;
		}

		uint64_t offset = m_pSerialiser->GetOffset();

		VulkanChunkType context = (VulkanChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

		m_LastCmdBufferID = ResourceId();

		ContextProcessChunk(offset, context, false);
		
		RenderDoc::Inst().SetProgress(FileInitialRead, float(offset)/float(m_pSerialiser->GetSize()));
		
		// for now just abort after capture scope. Really we'd need to support multiple frames
		// but for now this will do.
		if(context == CONTEXT_CAPTURE_FOOTER)
			break;
		
		// break out if we were only executing one event
		if(m_State == EXECUTING && startEventID == endEventID)
			break;

		if(m_LastCmdBufferID != ResourceId())
		{
			// these events are completely omitted, so don't increment the curEventID
			if(context != BEGIN_CMD_BUFFER && context != END_CMD_BUFFER)
				m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID++;
		}
		else
		{
			m_RootEventID++;
		}
	}

	if(m_State == READING)
	{
		GetFrameRecord().back().drawcallList = m_ParentDrawcall.Bake();

		struct SortEID
		{
			bool operator() (const FetchAPIEvent &a, const FetchAPIEvent &b) { return a.eventID < b.eventID; }
		};

		std::sort(m_Events.begin(), m_Events.end(), SortEID());
		m_ParentDrawcall.children.clear();
	}

	if(m_PartialReplayData.resultPartialCmdBuffer != VK_NULL_HANDLE)
	{
		ObjDisp(GetDev())->DeviceWaitIdle(Unwrap(m_PartialReplayData.partialDevice));

		// deliberately call our own function, so this is destroyed as a wrapped object
		vkDestroyCommandBuffer(m_PartialReplayData.partialDevice, m_PartialReplayData.resultPartialCmdBuffer);
		m_PartialReplayData.resultPartialCmdBuffer = VK_NULL_HANDLE;
	}

	m_State = READING;
}

void WrappedVulkan::ContextProcessChunk(uint64_t offset, VulkanChunkType chunk, bool forceExecute)
{
	m_CurChunkOffset = offset;

	uint64_t cOffs = m_pSerialiser->GetOffset();

	LogState state = m_State;

	if(forceExecute)
		m_State = EXECUTING;

	m_AddedDrawcall = false;

	ProcessChunk(offset, chunk);

	m_pSerialiser->PopContext(chunk);
	
	if(m_State == READING && chunk == SET_MARKER)
	{
		// no push/pop necessary
	}
	else if(m_State == READING && chunk == BEGIN_EVENT)
	{
		// push down the drawcallstack to the latest drawcall
		GetDrawcallStack().push_back(&GetDrawcallStack().back()->children.back());
	}
	else if(m_State == READING && chunk == END_EVENT)
	{
		// refuse to pop off further than the root drawcall (mismatched begin/end events e.g.)
		RDCASSERT(GetDrawcallStack().size() > 1);
		if(GetDrawcallStack().size() > 1)
			GetDrawcallStack().pop_back();
	}
	else if(m_State == READING && (chunk == BEGIN_CMD_BUFFER || chunk == END_CMD_BUFFER))
	{
		// don't add these events - they will be handled when inserted in-line into queue submit
	}
	else if(m_State == READING)
	{
		if(!m_AddedDrawcall)
			AddEvent(chunk, m_pSerialiser->GetDebugStr());
	}

	m_AddedDrawcall = false;
	
	if(forceExecute)
		m_State = state;
}

void WrappedVulkan::ProcessChunk(uint64_t offset, VulkanChunkType context)
{
	switch(context)
	{
	case DEVICE_INIT:
		{
			break;
		}
	case ENUM_PHYSICALS:
		Serialise_vkEnumeratePhysicalDevices(GetMainSerialiser(), NULL, NULL, NULL);
		break;
	case CREATE_DEVICE:
		Serialise_vkCreateDevice(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case GET_DEVICE_QUEUE:
		Serialise_vkGetDeviceQueue(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0, NULL);
		break;

	case ALLOC_MEM:
		Serialise_vkAllocMemory(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case UNMAP_MEM:
		Serialise_vkUnmapMemory(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case FLUSH_MEM:
		Serialise_vkFlushMappedMemoryRanges(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL);
		break;
	case FREE_MEM:
		RDCERR("vkFreeMemory should not be serialised directly");
		break;
	case CREATE_CMD_POOL:
		Serialise_vkCreateCommandPool(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_CMD_BUFFER:
		RDCERR("vkCreateCommandBuffer should not be serialised directly");
		break;
	case CREATE_FRAMEBUFFER:
		Serialise_vkCreateFramebuffer(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_RENDERPASS:
		Serialise_vkCreateRenderPass(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_DESCRIPTOR_POOL:
		Serialise_vkCreateDescriptorPool(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_DESCRIPTOR_SET_LAYOUT:
		Serialise_vkCreateDescriptorSetLayout(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_BUFFER:
		Serialise_vkCreateBuffer(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_BUFFER_VIEW:
		Serialise_vkCreateBufferView(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_IMAGE:
		Serialise_vkCreateImage(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_IMAGE_VIEW:
		Serialise_vkCreateImageView(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_SAMPLER:
		Serialise_vkCreateSampler(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_SHADER:
		Serialise_vkCreateShader(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_SHADER_MODULE:
		Serialise_vkCreateShaderModule(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_PIPE_LAYOUT:
		Serialise_vkCreatePipelineLayout(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_PIPE_CACHE:
		Serialise_vkCreatePipelineCache(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_GRAPHICS_PIPE:
		Serialise_vkCreateGraphicsPipelines(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL, NULL);
		break;
	case CREATE_COMPUTE_PIPE:
		Serialise_vkCreateComputePipelines(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL, NULL);
		break;
	case GET_SWAPCHAIN_IMAGE:
		Serialise_vkGetSwapchainImagesKHR(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, NULL, NULL);
		break;

	case CREATE_SEMAPHORE:
		Serialise_vkCreateSemaphore(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case CREATE_FENCE:
		Serialise_vkCreateFence(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case GET_FENCE_STATUS:
		Serialise_vkGetFenceStatus(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case RESET_FENCE:
		Serialise_vkResetFences(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL);
		break;
	case WAIT_FENCES:
		Serialise_vkWaitForFences(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL, VK_FALSE, 0);
		break;
		
	case CREATE_EVENT:
		Serialise_vkCreateEvent(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;
	case GET_EVENT_STATUS:
		Serialise_vkGetEventStatus(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case SET_EVENT:
		Serialise_vkSetEvent(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case RESET_EVENT:
		Serialise_vkResetEvent(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;

	case CREATE_QUERY_POOL:
		Serialise_vkCreateQueryPool(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;

	case ALLOC_DESC_SET:
		Serialise_vkAllocDescriptorSets(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_DESCRIPTOR_SET_USAGE_MAX_ENUM, 0, NULL, NULL);
		break;
	case UPDATE_DESC_SET:
		Serialise_vkUpdateDescriptorSets(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL, 0, NULL);
		break;

	case RESET_CMD_BUFFER:
		Serialise_vkResetCommandBuffer(GetMainSerialiser(), VK_NULL_HANDLE, 0);
		break;
	case BEGIN_CMD_BUFFER:
		Serialise_vkBeginCommandBuffer(GetMainSerialiser(), VK_NULL_HANDLE, NULL);
		break;
	case END_CMD_BUFFER:
		Serialise_vkEndCommandBuffer(GetMainSerialiser(), VK_NULL_HANDLE);
		break;

	case QUEUE_SIGNAL_SEMAPHORE:
		Serialise_vkQueueSignalSemaphore(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case QUEUE_WAIT_SEMAPHORE:
		Serialise_vkQueueWaitSemaphore(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE);
		break;
	case QUEUE_WAIT_IDLE:
		Serialise_vkQueueWaitIdle(GetMainSerialiser(), VK_NULL_HANDLE);
		break;
	case DEVICE_WAIT_IDLE:
		Serialise_vkDeviceWaitIdle(GetMainSerialiser(), VK_NULL_HANDLE);
		break;

	case QUEUE_SUBMIT:
		Serialise_vkQueueSubmit(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL, VK_NULL_HANDLE);
		break;
	case BIND_BUFFER_MEM:
		Serialise_vkBindBufferMemory(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
		break;
	case BIND_IMAGE_MEM:
		Serialise_vkBindImageMemory(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
		break;

	case BEGIN_RENDERPASS:
		Serialise_vkCmdBeginRenderPass(GetMainSerialiser(), VK_NULL_HANDLE, NULL, VK_RENDER_PASS_CONTENTS_MAX_ENUM);
		break;
	case NEXT_SUBPASS:
		Serialise_vkCmdNextSubpass(GetMainSerialiser(), VK_NULL_HANDLE, VK_RENDER_PASS_CONTENTS_MAX_ENUM);
		break;
	case EXEC_CMDS:
		Serialise_vkCmdExecuteCommands(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL);
		break;
	case END_RENDERPASS:
		Serialise_vkCmdEndRenderPass(GetMainSerialiser(), VK_NULL_HANDLE);
		break;

	case BIND_PIPELINE:
		Serialise_vkCmdBindPipeline(GetMainSerialiser(), VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_MAX_ENUM, VK_NULL_HANDLE);
		break;
	case SET_VP:
		Serialise_vkCmdSetViewport(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL);
		break;
	case SET_SCISSOR:
		Serialise_vkCmdSetScissor(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL);
		break;
	case SET_LINE_WIDTH:
		Serialise_vkCmdSetLineWidth(GetMainSerialiser(), VK_NULL_HANDLE, 0);
		break;
	case SET_DEPTH_BIAS:
		Serialise_vkCmdSetDepthBias(GetMainSerialiser(), VK_NULL_HANDLE, 0.0f, 0.0f, 0.0f);
		break;
	case SET_BLEND_CONST:
		Serialise_vkCmdSetBlendConstants(GetMainSerialiser(), VK_NULL_HANDLE, NULL);
		break;
	case SET_DEPTH_BOUNDS:
		Serialise_vkCmdSetDepthBounds(GetMainSerialiser(), VK_NULL_HANDLE, 0.0f, 0.0f);
		break;
	case SET_STENCIL_COMP_MASK:
		Serialise_vkCmdSetStencilCompareMask(GetMainSerialiser(), VK_NULL_HANDLE, VK_STENCIL_FACE_NONE, 0);
		break;
	case SET_STENCIL_WRITE_MASK:
		Serialise_vkCmdSetStencilWriteMask(GetMainSerialiser(), VK_NULL_HANDLE, VK_STENCIL_FACE_NONE, 0);
		break;
	case SET_STENCIL_REF:
		Serialise_vkCmdSetStencilReference(GetMainSerialiser(), VK_NULL_HANDLE, VK_STENCIL_FACE_NONE, 0);
		break;
	case BIND_DESCRIPTOR_SET:
		Serialise_vkCmdBindDescriptorSets(GetMainSerialiser(), VK_NULL_HANDLE, VK_PIPELINE_BIND_POINT_MAX_ENUM, VK_NULL_HANDLE, 0, 0, NULL, 0, NULL);
		break;
	case BIND_INDEX_BUFFER:
		Serialise_vkCmdBindIndexBuffer(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, VK_INDEX_TYPE_MAX_ENUM);
		break;
	case BIND_VERTEX_BUFFERS:
		Serialise_vkCmdBindVertexBuffers(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0, NULL, NULL);
		break;
	case COPY_BUF2IMG:
		Serialise_vkCmdCopyBufferToImage(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
		break;
	case COPY_IMG2BUF:
		Serialise_vkCmdCopyImageToBuffer(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE, 0, NULL);
		break;
	case COPY_IMG:
		Serialise_vkCmdCopyImage(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
		break;
	case BLIT_IMG:
		Serialise_vkCmdBlitImage(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL, VK_TEX_FILTER_MAX_ENUM);
		break;
	case RESOLVE_IMG:
		Serialise_vkCmdResolveImage(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, 0, NULL);
		break;
	case COPY_BUF:
		Serialise_vkCmdCopyBuffer(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, 0, NULL);
		break;
	case UPDATE_BUF:
		Serialise_vkCmdUpdateBuffer(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, NULL);
		break;
	case FILL_BUF:
		Serialise_vkCmdFillBuffer(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
		break;
	case PUSH_CONST:
		Serialise_vkCmdPushConstants(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_SHADER_STAGE_ALL, 0, 0, NULL);
		break;
	case CLEAR_COLOR:
		Serialise_vkCmdClearColorImage(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, NULL, 0, NULL);
		break;
	case CLEAR_DEPTHSTENCIL:
		Serialise_vkCmdClearDepthStencilImage(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_MAX_ENUM, NULL, 0, NULL);
		break;
	case CLEAR_COLOR_ATTACH:
		Serialise_vkCmdClearColorAttachment(GetMainSerialiser(), VK_NULL_HANDLE, 0, VK_IMAGE_LAYOUT_MAX_ENUM, NULL, 0, NULL);
		break;
	case CLEAR_DEPTHSTENCIL_ATTACH:
		Serialise_vkCmdClearDepthStencilAttachment(GetMainSerialiser(), VK_NULL_HANDLE, 0, VK_IMAGE_LAYOUT_MAX_ENUM, NULL, 0, NULL);
		break;
	case PIPELINE_BARRIER:
		Serialise_vkCmdPipelineBarrier(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0, VK_FALSE, 0, NULL);
		break;
	case WRITE_TIMESTAMP:
		Serialise_vkCmdWriteTimestamp(GetMainSerialiser(), VK_NULL_HANDLE, VK_TIMESTAMP_TYPE_MAX_ENUM, VK_NULL_HANDLE, 0);
		break;
	case COPY_QUERY_RESULTS:
		Serialise_vkCmdCopyQueryPoolResults(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, VK_NULL_HANDLE, 0, 0, VK_QUERY_RESULT_DEFAULT);
		break;
	case BEGIN_QUERY:
		Serialise_vkCmdBeginQuery(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);
		break;
	case END_QUERY:
		Serialise_vkCmdEndQuery(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
		break;
	case RESET_QUERY_POOL:
		Serialise_vkCmdResetQueryPool(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0);
		break;
		
	case CMD_SET_EVENT:
		Serialise_vkCmdSetEvent(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_PIPELINE_STAGE_ALL_GPU_COMMANDS);
		break;
	case CMD_RESET_EVENT:
		Serialise_vkCmdResetEvent(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, VK_PIPELINE_STAGE_ALL_GPU_COMMANDS);
		break;
	case CMD_WAIT_EVENTS:
		Serialise_vkCmdWaitEvents(GetMainSerialiser(), VK_NULL_HANDLE, 0, NULL, VK_PIPELINE_STAGE_ALL_GPU_COMMANDS, VK_PIPELINE_STAGE_ALL_GPU_COMMANDS, 0, NULL);
		break;

	case DRAW:
		Serialise_vkCmdDraw(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0, 0, 0);
		break;
	case DRAW_INDIRECT:
		Serialise_vkCmdDrawIndirect(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
		break;
	case DRAW_INDEXED:
		Serialise_vkCmdDrawIndexed(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0, 0, 0, 0);
		break;
	case DRAW_INDEXED_INDIRECT:
		Serialise_vkCmdDrawIndexedIndirect(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, 0, 0);
		break;
	case DISPATCH:
		Serialise_vkCmdDispatch(GetMainSerialiser(), VK_NULL_HANDLE, 0, 0, 0);
		break;
	case DISPATCH_INDIRECT:
		Serialise_vkCmdDispatchIndirect(GetMainSerialiser(), VK_NULL_HANDLE, VK_NULL_HANDLE, 0);
		break;

	case BEGIN_EVENT:
		Serialise_vkCmdDbgMarkerBegin(GetMainSerialiser(), VK_NULL_HANDLE, NULL);
		break;
	case SET_MARKER:
		RDCFATAL("No such function vkCmdDbgMarker");
		break;
	case END_EVENT:
		Serialise_vkCmdDbgMarkerEnd(GetMainSerialiser(), VK_NULL_HANDLE);
		break;

	case CREATE_SWAP_BUFFER:
		Serialise_vkCreateSwapchainKHR(GetMainSerialiser(), VK_NULL_HANDLE, NULL, NULL);
		break;

	case CAPTURE_SCOPE:
		Serialise_CaptureScope(offset);
		break;
	case CONTEXT_CAPTURE_FOOTER:
		{
			Serialiser *localSerialiser = GetMainSerialiser();

			SERIALISE_ELEMENT(ResourceId, bbid, ResourceId());

			bool HasCallstack = false;
			localSerialiser->Serialise("HasCallstack", HasCallstack);	

			if(HasCallstack)
			{
				size_t numLevels = 0;
				uint64_t *stack = NULL;

				localSerialiser->SerialisePODArray("callstack", stack, numLevels);

				localSerialiser->SetCallstack(stack, numLevels);

				SAFE_DELETE_ARRAY(stack);
			}

			if(m_State == READING)
			{
				AddEvent(CONTEXT_CAPTURE_FOOTER, "vkQueuePresentKHR()");

				FetchDrawcall draw;
				draw.name = "vkQueuePresentKHR()";
				draw.flags |= eDraw_Present;

				draw.copyDestination = bbid;

				AddDrawcall(draw, true);
			}
		}
		break;
	default:
		// ignore system chunks
		if((int)context == (int)INITIAL_CONTENTS)
			Serialise_InitialState(NULL);
		else if((int)context < (int)FIRST_CHUNK_ID)
			m_pSerialiser->SkipCurrentChunk();
		else
			RDCERR("Unrecognised Chunk type %d", context);
		break;
	}
}

void WrappedVulkan::ReplayLog(uint32_t frameID, uint32_t startEventID, uint32_t endEventID, ReplayLogType replayType)
{
	RDCASSERT(frameID < (uint32_t)m_FrameRecord.size());

	uint64_t offs = m_FrameRecord[frameID].frameInfo.fileOffset;

	m_pSerialiser->SetOffset(offs);

	bool partial = true;

	if(startEventID == 0 && (replayType == eReplay_WithoutDraw || replayType == eReplay_Full))
	{
		startEventID = m_FrameRecord[frameID].frameInfo.firstEvent;
		partial = false;
	}
	
	VulkanChunkType header = (VulkanChunkType)m_pSerialiser->PushContext(NULL, NULL, 1, false);

	RDCASSERT(header == CAPTURE_SCOPE);

	m_pSerialiser->SkipCurrentChunk();

	m_pSerialiser->PopContext(header);
	
	if(!partial)
	{
		GetResourceManager()->ApplyInitialContents();

		SubmitCmds();
		FlushQ();

		GetResourceManager()->ReleaseInFrameResources();
	}
	
	{
		if(!partial)
		{
			m_PartialReplayData.renderPassActive = false;
			RDCASSERT(m_PartialReplayData.resultPartialCmdBuffer == VK_NULL_HANDLE);
			m_PartialReplayData.partialParent = ResourceId();
			m_PartialReplayData.baseEvent = 0;
			m_PartialReplayData.state = PartialReplayData::StateVector();
		}

		if(replayType == eReplay_Full)
		{
			ContextReplayLog(EXECUTING, startEventID, endEventID, partial);
		}
		else if(replayType == eReplay_WithoutDraw)
		{
			ContextReplayLog(EXECUTING, startEventID, RDCMAX(1U,endEventID)-1, partial);
		}
		else if(replayType == eReplay_OnlyDraw)
		{
			VkCmdBuffer cmd = m_PartialReplayData.singleDrawCmdBuffer = GetNextCmd();
			
			VkCmdBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_CMD_BUFFER_BEGIN_INFO, NULL, VK_CMD_BUFFER_OPTIMIZE_SMALL_BATCH_BIT | VK_CMD_BUFFER_OPTIMIZE_ONE_TIME_SUBMIT_BIT };

			VkResult vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
			RDCASSERT(vkr == VK_SUCCESS);

			// if a render pass was active, begin it and set up the partial replay state
			if(m_PartialReplayData.renderPassActive)
			{
				auto &s = m_PartialReplayData.state;

				RDCASSERT(s.renderPass != ResourceId());

				// clear values don't matter as we're using the load renderpass here, that
				// has all load ops set to load (as we're doing a partial replay - can't
				// just clear the targets that are partially written to).

				VkClearValue empty[16] = {0};

				RDCASSERT(ARRAY_COUNT(empty) >= m_CreationInfo.m_RenderPass[s.renderPass].attachments.size());

				VkRenderPassBeginInfo rpbegin = {
					VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL,
					Unwrap(m_CreationInfo.m_RenderPass[s.renderPass].loadRP),
					Unwrap(GetResourceManager()->GetCurrentHandle<VkFramebuffer>(s.framebuffer)),
					s.renderArea,
					(uint32_t)m_CreationInfo.m_RenderPass[s.renderPass].attachments.size(), empty,
				};
				ObjDisp(cmd)->CmdBeginRenderPass(Unwrap(cmd), &rpbegin, VK_RENDER_PASS_CONTENTS_INLINE);

				if(s.graphics.pipeline != ResourceId())
				{
					ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(GetResourceManager()->GetCurrentHandle<VkPipeline>(s.graphics.pipeline)));

					ResourceId pipeLayoutId = m_CreationInfo.m_Pipeline[s.graphics.pipeline].layout;
					VkPipelineLayout layout = GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeLayoutId);

					const vector<ResourceId> &descSetLayouts = m_CreationInfo.m_PipelineLayout[pipeLayoutId].descSetLayouts;

					// only iterate over the desc sets that this layout actually uses, not all that were bound
					for(size_t i=0; i < descSetLayouts.size(); i++)
					{
						const DescSetLayout &descLayout = m_CreationInfo.m_DescSetLayout[ descSetLayouts[i] ];

						if(i < s.graphics.descSets.size() && s.graphics.descSets[i] != ResourceId())
						{
							// if there are dynamic buffers, pass along the offsets
							ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(layout), (uint32_t)i,
									1, UnwrapPtr(GetResourceManager()->GetCurrentHandle<VkDescriptorSet>(s.graphics.descSets[i])),
									descLayout.dynamicCount, descLayout.dynamicCount == 0 ? NULL : &s.graphics.offsets[i][0]);
						}
						else
						{
							RDCWARN("Descriptor set is not bound but pipeline layout expects one");
						}
					}
				}

				if(s.compute.pipeline != ResourceId())
				{
					ObjDisp(cmd)->CmdBindPipeline(Unwrap(cmd), VK_PIPELINE_BIND_POINT_COMPUTE, Unwrap(GetResourceManager()->GetCurrentHandle<VkPipeline>(s.compute.pipeline)));

					ResourceId pipeLayoutId = m_CreationInfo.m_Pipeline[s.compute.pipeline].layout;
					VkPipelineLayout layout = GetResourceManager()->GetCurrentHandle<VkPipelineLayout>(pipeLayoutId);

					const vector<ResourceId> &descSetLayouts = m_CreationInfo.m_PipelineLayout[pipeLayoutId].descSetLayouts;

					for(size_t i=0; i < descSetLayouts.size(); i++)
					{
						const DescSetLayout &descLayout = m_CreationInfo.m_DescSetLayout[ descSetLayouts[i] ];

						if(s.compute.descSets[i] != ResourceId())
						{
							ObjDisp(cmd)->CmdBindDescriptorSets(Unwrap(cmd), VK_PIPELINE_BIND_POINT_GRAPHICS, Unwrap(layout), (uint32_t)i,
									1, UnwrapPtr(GetResourceManager()->GetCurrentHandle<VkDescriptorSet>(s.compute.descSets[i])),
									descLayout.dynamicCount, descLayout.dynamicCount == 0 ? NULL : &s.compute.offsets[i][0]);
						}
					}
				}
				
				if(!s.views.empty())
					ObjDisp(cmd)->CmdSetViewport(Unwrap(cmd), (uint32_t)s.views.size(), &s.views[0]);
				if(!s.scissors.empty())
					ObjDisp(cmd)->CmdSetScissor(Unwrap(cmd), (uint32_t)s.scissors.size(), &s.scissors[0]);

				ObjDisp(cmd)->CmdSetBlendConstants(Unwrap(cmd), s.blendConst);
				ObjDisp(cmd)->CmdSetDepthBounds(Unwrap(cmd), s.mindepth, s.maxdepth);
				ObjDisp(cmd)->CmdSetLineWidth(Unwrap(cmd), s.lineWidth);
				ObjDisp(cmd)->CmdSetDepthBias(Unwrap(cmd), s.bias.depth, s.bias.biasclamp, s.bias.slope);
				
				ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, s.back.ref);
				ObjDisp(cmd)->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, s.back.compare);
				ObjDisp(cmd)->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_BACK_BIT, s.back.write);
				
				ObjDisp(cmd)->CmdSetStencilReference(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, s.front.ref);
				ObjDisp(cmd)->CmdSetStencilCompareMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, s.front.compare);
				ObjDisp(cmd)->CmdSetStencilWriteMask(Unwrap(cmd), VK_STENCIL_FACE_FRONT_BIT, s.front.write);

				if(s.ibuffer.buf != ResourceId())
					ObjDisp(cmd)->CmdBindIndexBuffer(Unwrap(cmd), Unwrap(GetResourceManager()->GetCurrentHandle<VkBuffer>(s.ibuffer.buf)), s.ibuffer.offs, s.ibuffer.bytewidth == 4 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);

				for(size_t i=0; i < s.vbuffers.size(); i++)
					ObjDisp(cmd)->CmdBindVertexBuffers(Unwrap(cmd), (uint32_t)i, 1, UnwrapPtr(GetResourceManager()->GetCurrentHandle<VkBuffer>(s.vbuffers[i].buf)), &s.vbuffers[i].offs);
			}

			ContextReplayLog(EXECUTING, endEventID, endEventID, partial);

			if(m_PartialReplayData.renderPassActive)
				ObjDisp(cmd)->CmdEndRenderPass(Unwrap(cmd));
			
			ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));

			SubmitCmds();

			m_PartialReplayData.singleDrawCmdBuffer = VK_NULL_HANDLE;
		}
		else
			RDCFATAL("Unexpected replay type");
	}
}

void WrappedVulkan::DebugCallback(
				VkFlags             msgFlags,
				VkDbgObjectType     objType,
				uint64_t            srcObject,
				size_t              location,
				int32_t             msgCode,
				const char*         pLayerPrefix,
				const char*         pMsg)
{
	RDCWARN("debug message:\n%s", pMsg);
}

void WrappedVulkan::AddDrawcall(FetchDrawcall d, bool hasEvents)
{
	m_AddedDrawcall = true;

	FetchDrawcall draw = d;
	draw.eventID = m_LastCmdBufferID != ResourceId() ? m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID : m_RootEventID;
	draw.drawcallID = m_LastCmdBufferID != ResourceId() ? m_BakedCmdBufferInfo[m_LastCmdBufferID].drawCount : m_RootDrawcallID;

	for(int i=0; i < 8; i++)
		draw.outputs[i] = ResourceId();

	draw.depthOut = ResourceId();

	ResourceId pipe = m_PartialReplayData.state.graphics.pipeline;
	if(pipe != ResourceId())
		draw.topology = MakePrimitiveTopology(m_CreationInfo.m_Pipeline[pipe].topology, m_CreationInfo.m_Pipeline[pipe].patchControlPoints);
	else
		draw.topology = eTopology_Unknown;

	draw.indexByteWidth = m_PartialReplayData.state.ibuffer.bytewidth;

	if(m_LastCmdBufferID != ResourceId())
		m_BakedCmdBufferInfo[m_LastCmdBufferID].drawCount++;
	else
		m_RootDrawcallID++;

	if(hasEvents)
	{
		vector<FetchAPIEvent> &srcEvents = m_LastCmdBufferID != ResourceId() ? m_BakedCmdBufferInfo[m_LastCmdBufferID].curEvents : m_RootEvents;

		draw.events = srcEvents; srcEvents.clear();
	}

	//AddUsage(draw);
	
	// should have at least the root drawcall here, push this drawcall
	// onto the back's children list.
	if(!GetDrawcallStack().empty())
	{
		DrawcallTreeNode node(draw);
		node.children.insert(node.children.begin(), draw.children.elems, draw.children.elems+draw.children.count);
		GetDrawcallStack().back()->children.push_back(node);
	}
	else
		RDCERR("Somehow lost drawcall stack!");
}

void WrappedVulkan::AddEvent(VulkanChunkType type, string description)
{
	FetchAPIEvent apievent;

	apievent.context = ResourceId();
	apievent.fileOffset = m_CurChunkOffset;
	apievent.eventID = m_LastCmdBufferID != ResourceId() ? m_BakedCmdBufferInfo[m_LastCmdBufferID].curEventID : m_RootEventID;

	apievent.eventDesc = description;

	Callstack::Stackwalk *stack = m_pSerialiser->GetLastCallstack();
	if(stack)
	{
		create_array(apievent.callstack, stack->NumLevels());
		memcpy(apievent.callstack.elems, stack->GetAddrs(), sizeof(uint64_t)*stack->NumLevels());
	}

	if(m_LastCmdBufferID != ResourceId())
	{
		m_BakedCmdBufferInfo[m_LastCmdBufferID].curEvents.push_back(apievent);
	}
	else
	{
		m_RootEvents.push_back(apievent);
		m_Events.push_back(apievent);
	}
}

FetchAPIEvent WrappedVulkan::GetEvent(uint32_t eventID)
{
	for(size_t i=m_Events.size()-1; i > 0; i--)
	{
		if(m_Events[i].eventID <= eventID)
			return m_Events[i];
	}

	return m_Events[0];
}
