#include "../tr_local.h"

#define max(a,b) (((a)>(b))?(a):(b))

// KHR placement/scratch alignment (conservative; AS buffers have slack)
#define VK_AS_ALIGN 256

static VkDeviceSize VK_AlignUp(VkDeviceSize v, VkDeviceSize a) {
	return (v + a - 1) & ~(a - 1);
}

static VkDeviceSize VK_ScratchAlignment(void) {
	VkDeviceSize a = vk.accelProperties.minAccelerationStructureScratchOffsetAlignment;
	return a > VK_AS_ALIGN ? a : VK_AS_ALIGN;
}

static VkDeviceAddress VK_ScratchAddress(void) {
	return VK_GetBufferDeviceAddress(vk_d.scratchBuffer.buffer) + VK_AlignUp(vk_d.scratchBufferOffset, VK_ScratchAlignment());
}

static void VK_ScratchConsume(VkDeviceSize size) {
	vk_d.scratchBufferOffset = VK_AlignUp(vk_d.scratchBufferOffset, VK_ScratchAlignment()) + size;
}

static void VK_FillBlasBuildInfo(vkbottomAS_t* bas, VkAccelerationStructureBuildGeometryInfoKHR* buildInfo,
		VkBuildAccelerationStructureFlagsKHR flag, VkAccelerationStructureKHR dst) {
	buildInfo->sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo->pNext = NULL;
	buildInfo->type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo->flags = flag;
	buildInfo->mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo->srcAccelerationStructure = VK_NULL_HANDLE;
	buildInfo->dstAccelerationStructure = dst;
	buildInfo->geometryCount = 1;
	buildInfo->pGeometries = &bas->geometries;
	buildInfo->ppGeometries = NULL;
	buildInfo->scratchData.deviceAddress = 0;
}

static void VK_BlasMemoryBarrier(VkCommandBuffer commandBuffer) {
	VkMemoryBarrier memoryBarrier = { 0 };
	memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memoryBarrier, 0, 0, 0, 0);
}

static VkDeviceAddress VK_CreateKHRas(VkBuffer buffer, VkDeviceSize* offset, VkDeviceSize size, VkAccelerationStructureTypeKHR type, VkAccelerationStructureKHR* as) {
	VkDeviceSize asOffset = VK_AlignUp(offset != NULL ? *offset : 0, VK_AS_ALIGN);
	VkAccelerationStructureCreateInfoKHR ci = { 0 };
	ci.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	ci.buffer = buffer;
	ci.offset = asOffset;
	ci.size = size;
	ci.type = type;
	VK_CHECK(vkCreateAccelerationStructureKHR(vk.device, &ci, NULL, as), "failed to create Acceleration Structure KHR");
	if (offset != NULL) {
		*offset = asOffset + size;
	}
	VkAccelerationStructureDeviceAddressInfoKHR addrInfo = { 0 };
	addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addrInfo.accelerationStructure = *as;
	return vkGetAccelerationStructureDeviceAddressKHR(vk.device, &addrInfo);
}

void VK_CreateBottomAS(VkCommandBuffer commandBuffer,
						vkbottomAS_t* bas, vkbuffer_t *bottomASBuffer,
						VkDeviceSize* offset, VkBuildAccelerationStructureFlagsKHR flag) {
	uint32_t maxPrim = bas->indexCount / 3;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { 0 };
	VK_FillBlasBuildInfo(bas, &buildInfo, flag, VK_NULL_HANDLE);

	VkAccelerationStructureBuildSizesInfoKHR sizes = { 0 };
	sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxPrim, &sizes);

	if (sizes.accelerationStructureSize > bottomASBuffer->allocSize - (offset != NULL ? VK_AlignUp(*offset, VK_AS_ALIGN) : bas->offset)) {
		ri.Error(ERR_FATAL, "Vulkan: Bottom Level Buffer to small!");
	}
	if (sizes.buildScratchSize > vk_d.scratchBuffer.allocSize - VK_AlignUp(vk_d.scratchBufferOffset, VK_ScratchAlignment())) {
		ri.Error(ERR_FATAL, "Vulkan: Scratch Buffer to small!");
	}

	// create (updates bas->offset when the caller suballocates, else reuses it)
	VkDeviceSize asOffset = VK_AlignUp(offset != NULL ? *offset : bas->offset, VK_AS_ALIGN);
	bas->offset = asOffset;
	bas->handle = VK_CreateKHRas(bottomASBuffer->buffer, &asOffset, sizes.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, &bas->accelerationStructure);
	if (offset != NULL) {
		*offset = asOffset;
	}

	// build
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.dstAccelerationStructure = bas->accelerationStructure;
	buildInfo.scratchData.deviceAddress = VK_ScratchAddress();

	VkAccelerationStructureBuildRangeInfoKHR range = { 0 };
	range.primitiveCount = maxPrim;
	range.primitiveOffset = 0;
	range.firstVertex = 0;
	range.transformOffset = 0;
	const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

	vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &pRange);
	VK_ScratchConsume(sizes.buildScratchSize);
	VK_BlasMemoryBarrier(commandBuffer);
}
void VK_UpdateBottomAS(VkCommandBuffer commandBuffer,
						vkbottomAS_t* oldBas, vkbottomAS_t* newBas,
						vkbuffer_t* bottomASBuffer, VkDeviceSize* offset, VkBuildAccelerationStructureFlagsKHR flag) {

	uint32_t maxPrim = newBas->indexCount / 3;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { 0 };
	VK_FillBlasBuildInfo(newBas, &buildInfo, flag, VK_NULL_HANDLE);

	VkAccelerationStructureBuildSizesInfoKHR sizes = { 0 };
	sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxPrim, &sizes);

	// if old and new are the same, update in place; else create new as
	if (oldBas == newBas) {
		if (sizes.updateScratchSize > vk_d.scratchBuffer.allocSize - VK_AlignUp(vk_d.scratchBufferOffset, VK_ScratchAlignment())) {
			ri.Error(ERR_FATAL, "Vulkan: Scratch Buffer to small!");
		}
	}
	else {
		if (sizes.accelerationStructureSize > bottomASBuffer->allocSize - VK_AlignUp(*offset, VK_AS_ALIGN)) {
			ri.Error(ERR_FATAL, "Vulkan: Bottom Level Buffer to small!");
		}
		if (sizes.updateScratchSize > vk_d.scratchBuffer.allocSize - VK_AlignUp(vk_d.scratchBufferOffset, VK_ScratchAlignment())) {
			ri.Error(ERR_FATAL, "Vulkan: Scratch Buffer to small!");
		}
		VkDeviceSize asOffset = VK_AlignUp(*offset, VK_AS_ALIGN);
		newBas->offset = asOffset;
		newBas->handle = VK_CreateKHRas(bottomASBuffer->buffer, &asOffset, sizes.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, &newBas->accelerationStructure);
		*offset = asOffset;
	}

	// build (update)
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
	buildInfo.srcAccelerationStructure = oldBas->accelerationStructure;
	buildInfo.dstAccelerationStructure = newBas->accelerationStructure;
	buildInfo.scratchData.deviceAddress = VK_ScratchAddress();

	VkAccelerationStructureBuildRangeInfoKHR range = { 0 };
	range.primitiveCount = maxPrim;
	range.primitiveOffset = 0;
	range.firstVertex = 0;
	range.transformOffset = 0;
	const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

	vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &pRange);
	VK_ScratchConsume(sizes.updateScratchSize);

	VK_BlasMemoryBarrier(commandBuffer);
}

// destroy and create new AS
void VK_RecreateBottomAS(VkCommandBuffer commandBuffer, vkbottomAS_t* bas, vkbuffer_t* bottomASBuffer, VkBuildAccelerationStructureFlagsKHR flag) {
	vkDestroyAccelerationStructureKHR(vk.device, bas->accelerationStructure, NULL);
	bas->accelerationStructure = VK_NULL_HANDLE;
	bas->handle = 0;
	VK_CreateBottomAS(commandBuffer, bas, bottomASBuffer, NULL, flag);
}

static void VK_FillTlasBuildInfo(VkAccelerationStructureGeometryKHR* geom, VkBuffer instanceBuffer,
		VkAccelerationStructureBuildGeometryInfoKHR* buildInfo, VkBuildAccelerationStructureFlagsKHR flag) {
	VkAccelerationStructureGeometryInstancesDataKHR* instances = &geom->geometry.instances;
	instances->sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instances->pNext = NULL;
	instances->arrayOfPointers = VK_FALSE;
	instances->data.deviceAddress = VK_GetBufferDeviceAddress(instanceBuffer);

	geom->sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geom->pNext = NULL;
	geom->geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geom->flags = 0;

	buildInfo->sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo->pNext = NULL;
	buildInfo->type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo->flags = flag;
	buildInfo->mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo->srcAccelerationStructure = VK_NULL_HANDLE;
	buildInfo->dstAccelerationStructure = VK_NULL_HANDLE;
	buildInfo->geometryCount = 1;
	buildInfo->pGeometries = geom;
	buildInfo->ppGeometries = NULL;
	buildInfo->scratchData.deviceAddress = 0;
}

void VK_MakeTopAS(VkCommandBuffer commandBuffer,
					vktopAS_t* topAS, vkbuffer_t* topASBuffer,
					vkbottomAS_t* basList, uint32_t basCount, vkbuffer_t* instanceBuffer,
					VkBuildAccelerationStructureFlagsKHR flag) {
	VkAccelerationStructureGeometryKHR geom = { 0 };
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { 0 };
	VK_FillTlasBuildInfo(&geom, instanceBuffer->buffer, &buildInfo, flag);

	VkAccelerationStructureBuildSizesInfoKHR sizes = { 0 };
	sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &basCount, &sizes);

	if (sizes.accelerationStructureSize > topASBuffer->allocSize) {
		ri.Error(ERR_FATAL, "Vulkan: Top Level Buffer to small!");
	}
	if (sizes.buildScratchSize > vk_d.scratchBuffer.allocSize - VK_AlignUp(vk_d.scratchBufferOffset, VK_ScratchAlignment())) {
		ri.Error(ERR_FATAL, "Vulkan: Scratch Buffer to small!");
	}

	VkDeviceSize asOffset = 0;
	topAS->handle = VK_CreateKHRas(topASBuffer->buffer, &asOffset, sizes.accelerationStructureSize, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, &topAS->accelerationStructure);

	// build
	VK_FillTlasBuildInfo(&geom, instanceBuffer->buffer, &buildInfo, flag);
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.dstAccelerationStructure = topAS->accelerationStructure;
	buildInfo.scratchData.deviceAddress = VK_ScratchAddress();

	VkAccelerationStructureBuildRangeInfoKHR range = { 0 };
	range.primitiveCount = basCount;
	range.primitiveOffset = 0;
	range.firstVertex = 0;
	range.transformOffset = 0;
	const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

	VK_BlasMemoryBarrier(commandBuffer);
	vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &pRange);
	VK_ScratchConsume(sizes.buildScratchSize);
	VK_BlasMemoryBarrier(commandBuffer);
}

void VK_UpdateTopAS(VkCommandBuffer commandBuffer,
	vktopAS_t* topASold, vktopAS_t* topASnew, vkbuffer_t* topASBuffer,
	vkbottomAS_t* basList, uint32_t basCount, vkbuffer_t* instanceBuffer,
	VkBuildAccelerationStructureFlagsKHR flag) {

	VkAccelerationStructureGeometryKHR geom = { 0 };
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { 0 };
	VK_FillTlasBuildInfo(&geom, instanceBuffer->buffer, &buildInfo, flag);

	VkAccelerationStructureBuildSizesInfoKHR sizes = { 0 };
	sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &basCount, &sizes);

	if (sizes.accelerationStructureSize > topASBuffer->allocSize) {
		ri.Error(ERR_FATAL, "Vulkan: Top Level Buffer to small!");
	}
	if (sizes.updateScratchSize > vk_d.scratchBuffer.allocSize - VK_AlignUp(vk_d.scratchBufferOffset, VK_ScratchAlignment())) {
		ri.Error(ERR_FATAL, "Vulkan: Scratch Buffer to small!");
	}

	// build (update)
	VK_FillTlasBuildInfo(&geom, instanceBuffer->buffer, &buildInfo, flag);
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
	buildInfo.srcAccelerationStructure = topASold->accelerationStructure;
	buildInfo.dstAccelerationStructure = topASnew->accelerationStructure;
	buildInfo.scratchData.deviceAddress = VK_ScratchAddress();

	VkAccelerationStructureBuildRangeInfoKHR range = { 0 };
	range.primitiveCount = basCount;
	range.primitiveOffset = 0;
	range.firstVertex = 0;
	range.transformOffset = 0;
	const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

	VK_BlasMemoryBarrier(commandBuffer);
	vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &pRange);
	VK_ScratchConsume(sizes.updateScratchSize);
	VK_BlasMemoryBarrier(commandBuffer);
}

void VK_DestroyTopAccelerationStructure(vktopAS_t* as) {
	if (as->accelerationStructure != VK_NULL_HANDLE) {
		vkDestroyAccelerationStructureKHR(vk.device, as->accelerationStructure, NULL);
		as->accelerationStructure = VK_NULL_HANDLE;
	}
	memset(as, 0, sizeof(vktopAS_t));
}

void VK_DestroyBottomAccelerationStructure(vkbottomAS_t* as) {
	if (as->accelerationStructure != VK_NULL_HANDLE) {
		vkDestroyAccelerationStructureKHR(vk.device, as->accelerationStructure, NULL);
		as->accelerationStructure = VK_NULL_HANDLE;
	}
	memset(as, 0, sizeof(vkbottomAS_t));
}

void VK_DestroyAllAccelerationStructures() {
	for (int i = 0; i < vk.swapchain.imageCount; i++) {
		VK_DestroyTopAccelerationStructure(&vk_d.topAS[i]);
	}
	for (int i = 0; i < vk_d.bottomASCount; ++i) {
		VK_DestroyBottomAccelerationStructure(&vk_d.bottomASList[i]);
	}
	vk_d.bottomASCount = 0;
}
