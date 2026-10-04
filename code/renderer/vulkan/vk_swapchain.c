#include "../tr_local.h"
#include <stdlib.h>

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

/*
 ==============================================================================

 SwapChain Setup

 ==============================================================================
 */

 // Function Declaration
static void VK_CreateSwapChain();
static void VK_CreateImageViews();
static void VK_CreateDepthStencil();
static void VK_CreateRenderPass();
static void VK_CreateFramebuffers();
static void VK_CreateCommandBuffers();
static void VK_CreateSyncObjects();

// Helper
static VkSurfaceFormatKHR chooseSwapSurfaceFormat(VkSurfaceFormatKHR* availableFormats, uint32_t availableFormatsCount);
static VkPresentModeKHR chooseSwapPresentMode(VkPresentModeKHR* availablePresentModes, uint32_t availablePresentModesCount);
static const char *VK_PresentModeName(VkPresentModeKHR mode);

static VkFramebuffer VK_CurrentFramebuffer();
static VkCommandBuffer VK_CurrentCommandBuffer();

void VK_SetupSwapchain()
{
	VK_CreateSwapChain();
	VK_CreateImageViews();
	VK_CreateDepthStencil();
	VK_CreateRenderPass();
	VK_CreateFramebuffers();

	VK_CreateCommandBuffers();
	VK_CreateSyncObjects();

	vk.swapchain.CurrentCommandBuffer = VK_CurrentCommandBuffer;
	vk.swapchain.CurrentFramebuffer = VK_CurrentFramebuffer;

	ri.Printf(PRINT_ALL, "...using present mode %s\n", VK_PresentModeName(vk.swapchain.presentMode));
}

// Idle + rebuild the swapchain (same window). Enough when only the present
// mode changed; extent-sized renderer resources are untouched. Returns qfalse
// if the surface extent changed (caller must vid_restart for a full rebuild).
qboolean VK_RecreateSwapchain(void)
{
	vkDeviceWaitIdle(vk.device);
	VK_DestroySwapchain();
	VK_SetupSwapchain();
	if (vk.swapchain.extent.width != (uint32_t)glConfig.vidWidth ||
		vk.swapchain.extent.height != (uint32_t)glConfig.vidHeight) {
		return qfalse;
	}
	return qtrue;
}

void VK_DestroySwapchain() {
	vkWaitForFences(vk.device, vk.swapchain.imageCount, vk.swapchain.inFlightFences, VK_TRUE, 10);

	for (int i = 0; i < vk.swapchain.imageCount; ++i) {
		vkDestroyFramebuffer(vk.device, vk.swapchain.framebuffers[i], NULL);
	}
	free(vk.swapchain.framebuffers);

	for (int i = 0; i < vk.swapchain.imageCount; ++i) {
		vkDestroyImageView(vk.device, vk.swapchain.imageViews[i], NULL);
	}
	free(vk.swapchain.imageViews);

	/*for (int i = 0; i < vk.swapchain.imageCount; ++i) {
		vkDestroyImage(vk.device, vk.swapchain.images[i], NULL);
	}
	*/

	vkDestroyRenderPass(vk.device, vk.swapchain.renderpass, NULL);

	vkDestroyImageView(vk.device, vk.swapchain.depthImageView, NULL);
	vkDestroyImage(vk.device, vk.swapchain.depthImage, NULL);
	vkFreeMemory(vk.device, vk.swapchain.depthImageMemory, NULL);

	vkDestroySwapchainKHR(vk.device, vk.swapchain.handle, NULL);
	free(vk.swapchain.images);

	vkFreeCommandBuffers(vk.device, vk.commandPool, vk.swapchain.imageCount, vk.swapchain.commandBuffers);
	free(vk.swapchain.commandBuffers);

	for (int i = 0; i < vk.swapchain.imageCount; ++i) {
		vkDestroySemaphore(vk.device, vk.swapchain.imageAvailableSemaphores[i], NULL);
		vkDestroySemaphore(vk.device, vk.swapchain.renderFinishedSemaphores[i], NULL);
		vkDestroyFence(vk.device, vk.swapchain.inFlightFences[i], NULL);
	}
	free(vk.swapchain.imageAvailableSemaphores);
	free(vk.swapchain.renderFinishedSemaphores);
	free(vk.swapchain.inFlightFences);

	memset(&vk.swapchain, 0, sizeof(vk.swapchain));
}

static void VK_CreateSwapChain() {
	vk.swapchain.depthStencilFormat = VK_FORMAT_D24_UNORM_S8_UINT;

	swapChainSupportDetails_t swapChainSupport = querySwapChainSupport(vk.physicalDevice, vk.surface);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(&swapChainSupport.formats[0], swapChainSupport.formatCount);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(&swapChainSupport.presentModes[0], swapChainSupport.presentModeCount);

	// cache the surface mode list for per-present resolution (live switching)
	vk.swapchain.supportedModeCount = swapChainSupport.presentModeCount;
	if (vk.swapchain.supportedModeCount > 8) {
		vk.swapchain.supportedModeCount = 8;
	}
	for (uint32_t i = 0; i < vk.swapchain.supportedModeCount; i++) {
		vk.swapchain.supportedModes[i] = swapChainSupport.presentModes[i];
	}
	vk.swapchain.presentId = 0;
	
	// set extent
	vk.swapchain.extent = swapChainSupport.capabilities.currentExtent;
	vk.swapchain.extent.width = max(swapChainSupport.capabilities.minImageExtent.width, min(swapChainSupport.capabilities.maxImageExtent.width, vk.swapchain.extent.width));
	vk.swapchain.extent.height = max(swapChainSupport.capabilities.minImageExtent.height, min(swapChainSupport.capabilities.maxImageExtent.height, vk.swapchain.extent.height));

	vk.swapchain.imageFormat = surfaceFormat.format;

	uint32_t imageCount = min(swapChainSupport.capabilities.maxImageCount, VK_MAX_SWAPCHAIN_SIZE);

	VkSwapchainCreateInfoKHR createInfo = {0};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = vk.surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = vk.swapchain.extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	createInfo.clipped = VK_TRUE;

	uint32_t queueFamilyIndices[] = {	vk.queryFamilyIndices.graphicsFamily, 
										vk.queryFamilyIndices.presentFamily };

	if (vk.queryFamilyIndices.graphicsFamily != vk.queryFamilyIndices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	vk.swapchain.presentMode = presentMode;
	// maintenance1: pre-authorize every mode we might switch to live
	VkPresentModeKHR allowedModes[8];
	uint32_t allowedModeCount = 0;
	{
		VkPresentModeKHR candidates[] = {
			VK_PRESENT_MODE_FIFO_KHR,
			VK_PRESENT_MODE_MAILBOX_KHR,
			VK_PRESENT_MODE_IMMEDIATE_KHR,
#ifdef VK_PRESENT_MODE_FIFO_LATEST_READY_KHR
			VK_PRESENT_MODE_FIFO_LATEST_READY_KHR,
#endif
		};
		for (int i = 0; i < (int)(sizeof(candidates) / sizeof(candidates[0])); i++) {
			for (uint32_t j = 0; j < swapChainSupport.presentModeCount; j++) {
				if (swapChainSupport.presentModes[j] == candidates[i]) {
					allowedModes[allowedModeCount++] = candidates[i];
					break;
				}
			}
		}
	}
	VkSwapchainPresentModesCreateInfoKHR presentModesInfo = { 0 };
	if (vk.swapchainMaintenance1 && allowedModeCount > 0) {
		presentModesInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_KHR;
		presentModesInfo.presentModeCount = allowedModeCount;
		presentModesInfo.pPresentModes = &allowedModes[0];
		createInfo.pNext = &presentModesInfo;
	}
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	VK_CHECK(vkCreateSwapchainKHR(vk.device, &createInfo, NULL, &vk.swapchain.handle), "failed to create Swapchain!");

	vkGetSwapchainImagesKHR(vk.device, vk.swapchain.handle, &vk.swapchain.imageCount, NULL);
	vk.swapchain.images = malloc(vk.swapchain.imageCount * sizeof(VkImage));
	vkGetSwapchainImagesKHR(vk.device, vk.swapchain.handle, &imageCount, &vk.swapchain.images[0]);

}

static void VK_CreateImageViews() {
	vk.swapchain.imageViews = malloc(vk.swapchain.imageCount * sizeof(VkImageView));

	for (size_t i = 0; i < vk.swapchain.imageCount; i++) {
		VkImageViewCreateInfo createInfo = { 0 };
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = vk.swapchain.images[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = vk.swapchain.imageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		VK_CHECK(vkCreateImageView(vk.device, &createInfo, NULL, &vk.swapchain.imageViews[i]), "failed to create ImageView for Swapchain!");
	}
}

static void VK_CreateDepthStencil()
{
	// Buffer
	VkImageCreateInfo imageInfo = {0};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = vk.swapchain.extent.width;
	imageInfo.extent.height = vk.swapchain.extent.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = vk.swapchain.depthStencilFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CHECK(vkCreateImage(vk.device, &imageInfo, NULL, &vk.swapchain.depthImage), "failed to create DepthStencil Image for Swapchain!");	
	VK_CreateImageMemory(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vk.swapchain.depthImage, &vk.swapchain.depthImageMemory);

	// Image View
	VkImageViewCreateInfo viewInfo = {0};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = vk.swapchain.depthImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = vk.swapchain.depthStencilFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VK_CHECK(vkCreateImageView(vk.device, &viewInfo, NULL, &vk.swapchain.depthImageView), "failed to create DepthStencil image view for Swapchain!");
	
}

static void VK_CreateRenderPass()
{
	VkAttachmentDescription colorAttachment = {0};
	colorAttachment.format = vk.swapchain.imageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {0};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment = {0};
	depthAttachment.format = vk.swapchain.depthStencilFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {0};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {0};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	if (vk.swapchain.depthImageView) subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency[2] = {0};
	dependency[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency[0].dstSubpass = 0;
	dependency[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency[0].srcAccessMask = 0;
	dependency[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	dependency[1].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency[1].dstSubpass = 0;
	dependency[1].srcStageMask = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
	dependency[1].srcAccessMask = 0;
	dependency[1].dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
	dependency[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[2];
	attachments[0] = colorAttachment;
	attachments[1] = depthAttachment;

	VkRenderPassCreateInfo renderPassInfo = {0};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = (uint32_t) (vk.swapchain.depthImageView ? 2 : 1);
	renderPassInfo.pAttachments = &attachments[0];
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = dependency;

	VK_CHECK(vkCreateRenderPass(vk.device, &renderPassInfo, NULL, &vk.swapchain.renderpass), "failed to create RenderPass for Swapchain!");
}

static void VK_CreateFramebuffers()
{
	vk.swapchain.framebuffers = malloc(vk.swapchain.imageCount * sizeof(VkFramebuffer));

	for (size_t i = 0; i < vk.swapchain.imageCount; i++) {
		VkImageView attachments[2];
		attachments[0] = vk.swapchain.imageViews[i];
		attachments[1] = vk.swapchain.depthImageView;

		VkFramebufferCreateInfo framebufferInfo = {0};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = vk.swapchain.renderpass;
		framebufferInfo.attachmentCount = (uint32_t) (vk.swapchain.depthImageView ? 2 : 1);
		framebufferInfo.pAttachments = &attachments[0];
		framebufferInfo.width = vk.swapchain.extent.width;
		framebufferInfo.height = vk.swapchain.extent.height;
		framebufferInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(vk.device, &framebufferInfo, NULL, &vk.swapchain.framebuffers[i]), "failed to create Framebuffer for Swapchain!");
	}
}

static void VK_CreateCommandBuffers()
{
	vk.swapchain.commandBuffers = malloc(vk.swapchain.imageCount * sizeof(VkCommandBuffer));

	VkCommandBufferAllocateInfo allocInfo = { 0 };
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = vk.commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = vk.swapchain.imageCount;

	VK_CHECK(vkAllocateCommandBuffers(vk.device, &allocInfo, vk.swapchain.commandBuffers), "failed to allocate command buffers!");
}

static void VK_CreateSyncObjects()
{
	vk.swapchain.imageAvailableSemaphores = malloc(vk.swapchain.imageCount * sizeof(VkSemaphore));
	vk.swapchain.renderFinishedSemaphores = malloc(vk.swapchain.imageCount * sizeof(VkSemaphore));
	vk.swapchain.inFlightFences = malloc(vk.swapchain.imageCount * sizeof(VkFence));

	VkSemaphoreCreateInfo semaphoreInfo = { 0 };
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = { 0 };
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < vk.swapchain.imageCount; i++) {
		VK_CHECK(vkCreateSemaphore(vk.device, &semaphoreInfo, NULL, &vk.swapchain.imageAvailableSemaphores[i]), "failed to create Semaphore!");
		VK_CHECK(vkCreateSemaphore(vk.device, &semaphoreInfo, NULL, &vk.swapchain.renderFinishedSemaphores[i]), "failed to create Semaphore!");
		VK_CHECK(vkCreateFence(vk.device, &fenceInfo, NULL, &vk.swapchain.inFlightFences[i]), "failed to create Fence!");
	}

}

void record_buffer_memory_barrier(VkCommandBuffer cb, VkBuffer buffer,
	VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
	VkAccessFlags src_access, VkAccessFlags dst_access) {

	VkBufferMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;;
	barrier.pNext = NULL;
	barrier.srcAccessMask = src_access;
	barrier.dstAccessMask = dst_access;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer = buffer;
	barrier.offset = 0;
	barrier.size = VK_WHOLE_SIZE;

	vkCmdPipelineBarrier(cb, src_stages, dst_stages, 0, 0, NULL, 1, &barrier, 0, NULL);
}

void VK_BeginFrame()
{
	VkResult res;
	if (vk.swapchain.frameStarted) return;

	// live present-mode switch (r_presentMode is intentionally not latched)
	if (r_presentMode != NULL && r_presentMode->modified) {
		r_presentMode->modified = qfalse;
		if (vk.swapchainMaintenance1) {
			ri.Printf(PRINT_ALL, "...present mode takes effect on next present (no swapchain rebuild)\n");
		} else {
			ri.Printf(PRINT_ALL, "...recreating swapchain for r_presentMode %d\n", r_presentMode->integer);
			if (!VK_RecreateSwapchain()) {
				ri.Printf(PRINT_WARNING, "Vulkan: surface extent changed, run vid_restart\n");
				ri.Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");
				return;
			}
		}
	}

	// save current image as last and acquire next
	vk.swapchain.lastImage = vk.swapchain.currentImage;
	res = vkAcquireNextImageKHR(vk.device, vk.swapchain.handle, UINT64_MAX, vk.swapchain.imageAvailableSemaphores[vk.swapchain.currentFrame], VK_NULL_HANDLE, &vk.swapchain.currentImage);
	if (res == VK_ERROR_OUT_OF_DATE_KHR) {
		if (!VK_RecreateSwapchain()) {
			ri.Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");
			return;
		}
		res = vkAcquireNextImageKHR(vk.device, vk.swapchain.handle, UINT64_MAX, vk.swapchain.imageAvailableSemaphores[vk.swapchain.currentFrame], VK_NULL_HANDLE, &vk.swapchain.currentImage);
	}
	if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR && res != VK_TIMEOUT) {
		ri.Printf(PRINT_WARNING, "Vulkan: vkAcquireNextImageKHR failed (%s), skipping frame\n", VK_ErrorString(res));
		return;
	}

	vk.swapchain.frameStarted = qtrue;

    clock_t start = clock();
	// we want to wait until the commands frm the current image and next image have completed execution in order to avoid race condition when reading the prev buffers
    vkWaitForFences(vk.device, 1, &vk.swapchain.inFlightFences[vk.swapchain.currentImage], VK_TRUE, UINT64_MAX);
	vkWaitForFences(vk.device, 1, &vk.swapchain.inFlightFences[(vk.swapchain.currentImage + 1) % vk.swapchain.imageCount], VK_TRUE, UINT64_MAX);
    vkResetFences(vk.device, 1, &vk.swapchain.inFlightFences[vk.swapchain.currentImage]);
    clock_t end = clock();
    float seconds = (float)(end - start) / CLOCKS_PER_SEC;
    //Com_Printf("new pipe %f\n", seconds);
    
    
    
//    vkFreeCommandBuffers(vk.device, vk.commandPool, 1, &vk.swapchain.commandBuffers[vk.swapchain.currentImage]);
//    VkCommandBufferAllocateInfo cmdBufInfo = {
//        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, vk.commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
//    VkResult err = vkAllocateCommandBuffers(vk.device, &cmdBufInfo, &vk.swapchain.commandBuffers[vk.swapchain.currentImage]);

	VkCommandBufferBeginInfo beginInfo = { 0 };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;//VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

	VK_CHECK(vkBeginCommandBuffer(vk.swapchain.commandBuffers[vk.swapchain.currentImage], &beginInfo), "failed to begin recording command buffer!");

	// Ensure visibility of geometry buffers writes.
//    record_buffer_memory_barrier(vk.swapchain.commandBuffers[vk.swapchain.currentImage], vk_d.vertexbuffer.buffer,
//        VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
//        VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
//
//    record_buffer_memory_barrier(vk.swapchain.commandBuffers[vk.swapchain.currentImage], vk_d.indexbuffer.buffer,
//        VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
//        VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_INDEX_READ_BIT);
}

void VK_EndFrame()
{
	if (!vk.swapchain.frameStarted) return;
	vk.swapchain.frameStarted = qfalse;

	VK_CHECK(vkEndCommandBuffer(vk.swapchain.commandBuffers[vk.swapchain.currentImage]), "failed to end commandbuffer!");
	//vkResetFences(this->device, 1, &inFlightFences[currentFrame]);

	//int last = (vk.swapchain.currentFrame + (vk.swapchain.imageCount - 1)) % vk.swapchain.imageCount;
	//int next = (vk.swapchain.currentFrame + 1) % vk.swapchain.imageCount;
	VkSemaphore waitSemaphores[] = { vk.swapchain.imageAvailableSemaphores[vk.swapchain.currentFrame]};
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSemaphore signalSemaphores[] = { vk.swapchain.renderFinishedSemaphores[vk.swapchain.currentFrame] };
	//(vk.swapchain.currentFrame + 1) % vk.swapchain.imageCount
	VkSubmitInfo submitInfo = { 0 };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = sizeof(waitSemaphores) / sizeof(VkSemaphore);
	submitInfo.pWaitSemaphores = &waitSemaphores[0];
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vk.swapchain.commandBuffers[vk.swapchain.currentImage];
	submitInfo.signalSemaphoreCount = sizeof(signalSemaphores) / sizeof(VkSemaphore);
	submitInfo.pSignalSemaphores = &signalSemaphores[0];

	VK_CHECK(vkQueueSubmit(vk.graphicsQueue, 1, &submitInfo, vk.swapchain.inFlightFences[vk.swapchain.currentImage]), "failed to submit draw command buffer!");
	//VK_CHECK(vkQueueWaitIdle(vk.graphicsQueue), "failed to wait for Queue execution!");

	VkSwapchainKHR swapChains[] = { vk.swapchain.handle };

	VkPresentInfoKHR presentInfo = { 0 };
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &vk.swapchain.currentImage;

	// maintenance1: switch present mode live (must be in the create-time list)
	VkSwapchainPresentModeInfoKHR presentModeInfo = { 0 };
	VkPresentModeKHR desiredMode = vk.swapchain.presentMode;
	if (vk.swapchainMaintenance1 && vk.swapchain.supportedModeCount > 0) {
		desiredMode = chooseSwapPresentMode(&vk.swapchain.supportedModes[0], vk.swapchain.supportedModeCount);
		presentModeInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODE_INFO_KHR;
		presentModeInfo.swapchainCount = 1;
		presentModeInfo.pPresentModes = &desiredMode;
		presentInfo.pNext = &presentModeInfo;
		vk.swapchain.presentMode = desiredMode;
	}
	// present_id: tag every present for pacing/debug
	VkPresentIdKHR presentIdInfo = { 0 };
	if (vk.presentId) {
		vk.swapchain.presentId++;
		presentIdInfo.sType = VK_STRUCTURE_TYPE_PRESENT_ID_KHR;
		presentIdInfo.swapchainCount = 1;
		presentIdInfo.pPresentIds = &vk.swapchain.presentId;
		if (presentInfo.pNext == &presentModeInfo) {
			presentModeInfo.pNext = &presentIdInfo;
		} else {
			presentInfo.pNext = &presentIdInfo;
		}
	}

	VkResult res = vkQueuePresentKHR(vk.presentQueue, &presentInfo);
	if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
		// window resized or surface changed: rebuild swapchain; a full
		// vid_restart follows automatically if the extent itself changed
		if (!VK_RecreateSwapchain()) {
			ri.Printf(PRINT_WARNING, "Vulkan: surface extent changed, running vid_restart\n");
			ri.Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n");
		}
	} else {
		VK_CHECK(res, "failed to Queue Present!");
		// present_wait pacing: in non-FIFO modes cap queue-ahead by waiting
		// for the previous present (bounded: never hangs the frame loop)
		if (res == VK_SUCCESS && vk.presentWait && vk.presentId && vkWaitForPresentKHR != NULL &&
			vk.swapchain.presentMode != VK_PRESENT_MODE_FIFO_KHR && vk.swapchain.presentId > 1) {
			VkResult wres = vkWaitForPresentKHR(vk.device, vk.swapchain.handle, vk.swapchain.presentId - 1, 50000000);
			if (wres == VK_TIMEOUT) {
				ri.Printf(PRINT_DEVELOPER, "Vulkan: present wait timed out, proceeding\n");
			} else if (wres != VK_SUCCESS) {
				vk.presentWait = qfalse; // driver disagrees: stop pacing, keep presenting
				ri.Printf(PRINT_WARNING, "Vulkan: present wait failed (%s), pacing disabled\n", VK_ErrorString(wres));
			}
		}
	}

	vk.swapchain.lastFrame = vk.swapchain.currentFrame;
	vk.swapchain.currentFrame = (vk.swapchain.currentFrame + 1) % vk.swapchain.imageCount;
}

/*
==============================================================================

SwapChain Helper Function

==============================================================================
*/
static VkFramebuffer VK_CurrentFramebuffer()
{
	return vk.swapchain.framebuffers[vk.swapchain.currentImage];
}

static VkCommandBuffer VK_CurrentCommandBuffer()
{
	return vk.swapchain.commandBuffers[vk.swapchain.currentImage];
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(VkSurfaceFormatKHR *availableFormats, uint32_t availableFormatsCount) {

	if (availableFormatsCount == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
		return (VkSurfaceFormatKHR) {	.format = VK_FORMAT_B8G8R8A8_UNORM,
										.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
									};
	}

	for (int i = 0; i < availableFormatsCount; i++) {
		if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM && availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			return availableFormats[i];
		}
	}

	return availableFormats[0];
}

static const char *VK_PresentModeName(VkPresentModeKHR mode) {
	switch (mode) {
	case VK_PRESENT_MODE_FIFO_KHR: return "FIFO (vsync)";
	case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "FIFO_RELAXED (vsync, tear if late)";
	case VK_PRESENT_MODE_MAILBOX_KHR: return "MAILBOX (low-latency vsync)";
	case VK_PRESENT_MODE_IMMEDIATE_KHR: return "IMMEDIATE (tearing)";
#ifdef VK_PRESENT_MODE_FIFO_LATEST_READY_KHR
	case VK_PRESENT_MODE_FIFO_LATEST_READY_KHR: return "FIFO_LATEST_READY (low-latency vsync)";
#endif
	default: return "unknown";
	}
}

static VkPresentModeKHR chooseSwapPresentMode(VkPresentModeKHR *availablePresentModes, uint32_t availablePresentModesCount) {
	// r_presentMode: 0 FIFO, 1 MAILBOX (default), 2 IMMEDIATE, 3 FIFO_LATEST_READY
	int want = (r_presentMode != NULL) ? r_presentMode->integer : 1;
	VkPresentModeKHR preference[4];
	int prefCount = 0;
	switch (want) {
	case 0:
		preference[prefCount++] = VK_PRESENT_MODE_FIFO_KHR;
		break;
	case 2:
		preference[prefCount++] = VK_PRESENT_MODE_IMMEDIATE_KHR;
		preference[prefCount++] = VK_PRESENT_MODE_MAILBOX_KHR;
		preference[prefCount++] = VK_PRESENT_MODE_FIFO_KHR;
		break;
	case 3:
#ifdef VK_PRESENT_MODE_FIFO_LATEST_READY_KHR
		preference[prefCount++] = VK_PRESENT_MODE_FIFO_LATEST_READY_KHR;
#endif
		preference[prefCount++] = VK_PRESENT_MODE_MAILBOX_KHR;
		preference[prefCount++] = VK_PRESENT_MODE_FIFO_KHR;
		break;
	case 1:
	default:
		preference[prefCount++] = VK_PRESENT_MODE_MAILBOX_KHR;
		preference[prefCount++] = VK_PRESENT_MODE_FIFO_KHR;
		break;
	}

	for (int p = 0; p < prefCount; p++) {
		for (int i = 0; i < (int)availablePresentModesCount; i++) {
			if (availablePresentModes[i] == preference[p]) {
				return availablePresentModes[i];
			}
		}
	}

	// FIFO is guaranteed to be supported
	return VK_PRESENT_MODE_FIFO_KHR;
}
