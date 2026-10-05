#include "../tr_local.h"

/*
** Render scale upscale for the rasterization path (FSR style).
**
** With r_fsrScale set to something below 1.0 the whole frame is rendered into
** an offscreen color/depth target at a reduced resolution and then blitted up
** into the swapchain image with linear filtering.
**
** The engine keeps working in window resolution everywhere: viewports and
** scissors stay in window pixels and are scaled on their way to the GPU, so
** every draw (3D, sprites, UI, text) lands correctly inside the smaller
** framebuffer. The upscale itself is the hardware blit, which is the same idea
** as the spatial upscale stage of FSR1 (EASU) with a bilinear reconstruction
** filter instead of the FSR filter. That keeps this path shader free and works
** without any of the ray tracing resources.
**
** Cvars:
**   r_fsrScale  internal render scale, 0 disables (default 0)
**   restart/vid_restart required to change it, resources are built at startup
**
** The ray tracing path brings its own upscale (rt_fsr / rt_fsrSharpness), so
** this stays off while r_vertexLight is 2.
*/

typedef struct {
	qboolean active;
	qboolean created;
	float scale;
	VkExtent2D extent;
	vkframebuffer_t target;
} vkfsr_t;

static vkfsr_t fsr;

qboolean VK_FSRActive(void) {
	return fsr.active;
}

VkRenderPass VK_FSRRenderPass(void) {
	return fsr.active ? fsr.target.renderPass : VK_NULL_HANDLE;
}

VkFramebuffer VK_FSRFramebuffer(void) {
	return fsr.active ? fsr.target.framebuffer : VK_NULL_HANDLE;
}

VkExtent2D VK_FSRExtent(void) {
	return fsr.extent;
}

qboolean VK_InitFSR(void) {
	float scale = 0.0f;

	// rebuild from scratch, VK_SetupSwapchain also runs on present mode changes
	VK_FSRShutdown();

	if (r_fsrScale) {
		// r_fsrScale is latched, so a command line "+set r_fsrScale x" is only
		// parked in latchedString by Cvar_Set. The render target is built here,
		// at swapchain creation, which is already a restart, so honor it.
		const char *requested = r_fsrScale->latchedString ? r_fsrScale->latchedString
														 : r_fsrScale->string;
		scale = (float)atof(requested);
	}

	// the ray tracing path has its own upscale, keep out of its way
	if (r_vertexLight && r_vertexLight->integer == 2) {
		if (scale > 0.0f && scale < 1.0f) {
			ri.Printf(PRINT_ALL, "r_fsrScale: ignored, ray tracing path owns the upscale\n");
		}
		return qfalse;
	}

	if (scale <= 0.0f || scale >= 1.0f) {
		return qfalse;
	}

	uint32_t width = (uint32_t)(vk.swapchain.extent.width * scale);
	uint32_t height = (uint32_t)(vk.swapchain.extent.height * scale);
	if (width < 320 || height < 200) {
		ri.Printf(PRINT_ALL, "r_fsrScale %.2f ignored, would render at %ix%i\n",
				  scale, width, height);
		return qfalse;
	}

	fsr.extent = (VkExtent2D){ width, height };
	fsr.scale = scale;

	// same format as the swapchain so the upscale blit stays format compatible
	VK_CreateFramebuffer(&fsr.target, fsr.extent, vk.swapchain.imageFormat);
	fsr.created = qtrue;
	fsr.active = qtrue;

	ri.Printf(PRINT_ALL, "r_fsrScale: %.2f (%ix%i -> %ix%i, bilinear upscale)\n",
			  scale,
			  vk.swapchain.extent.width, vk.swapchain.extent.height,
			  width, height);

	return qtrue;
}

void VK_FSRShutdown(void) {
	if (!fsr.created) {
		memset(&fsr, 0, sizeof(vkfsr_t));
		return;
	}

	vkDeviceWaitIdle(vk.device);

	if (fsr.target.framebuffer != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(vk.device, fsr.target.framebuffer, NULL);
	}
	if (fsr.target.renderPass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(vk.device, fsr.target.renderPass, NULL);
	}
	VK_DestroyImage(&fsr.target.image);
	VK_DestroyImage(&fsr.target.depth);

	memset(&fsr, 0, sizeof(vkfsr_t));
}

/*
** Window space viewport/scissor -> render target space. No-op when the
** feature is off so the normal path stays untouched.
*/
void VK_FSRScaleViewport(VkViewport* viewport, VkRect2D* scissor) {
	float scale = fsr.active ? fsr.scale : 1.0f;

	if (scale == 1.0f) {
		return;
	}

	if (viewport) {
		viewport->x *= scale;
		viewport->y *= scale;
		viewport->width *= scale;
		viewport->height *= scale;
		if (viewport->width < 1.0f) viewport->width = 1.0f;
		if (viewport->height < 1.0f) viewport->height = 1.0f;
	}

	if (scissor) {
		scissor->offset.x = (int32_t)(scissor->offset.x * scale);
		scissor->offset.y = (int32_t)(scissor->offset.y * scale);
		scissor->extent.width = (uint32_t)(scissor->extent.width * scale);
		scissor->extent.height = (uint32_t)(scissor->extent.height * scale);
		if (scissor->extent.width < 1) scissor->extent.width = 1;
		if (scissor->extent.height < 1) scissor->extent.height = 1;
	}
}

/*
** Upscale the finished low resolution image into the swapchain image and leave
** it in PRESENT_SRC_KHR. Call after vkCmdEndRenderPass of the low resolution
** pass.
*/
void VK_FSRUpscale(VkImage swapchainImage) {
	if (!fsr.active || swapchainImage == VK_NULL_HANDLE) {
		return;
	}

	VkCommandBuffer cmd = vk.swapchain.commandBuffers[vk.swapchain.currentImage];

	VkImageMemoryBarrier barrier = { 0 };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.subresourceRange = (VkImageSubresourceRange){
		VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
	};

	// acquire the swapchain image for writing, the blit overwrites all of it
	barrier.image = swapchainImage;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 0, 0, NULL, 0, NULL, 1, &barrier);

	// the render pass leaves the low resolution color image in GENERAL
	VkImageBlit blit = { 0 };
	blit.srcSubresource = (VkImageSubresourceLayers){ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	blit.dstSubresource = blit.srcSubresource;
	blit.srcOffsets[0] = (VkOffset3D){ 0, 0, 0 };
	blit.dstOffsets[0] = (VkOffset3D){ 0, 0, 0 };
	blit.srcOffsets[1] = (VkOffset3D){ (int32_t)fsr.extent.width, (int32_t)fsr.extent.height, 1 };
	blit.dstOffsets[1] = (VkOffset3D){ (int32_t)vk.swapchain.extent.width, (int32_t)vk.swapchain.extent.height, 1 };
	vkCmdBlitImage(cmd, fsr.target.image.handle, VK_IMAGE_LAYOUT_GENERAL,
				   swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				   1, &blit, VK_FILTER_LINEAR);

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = 0;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 0, 0, NULL, 0, NULL, 1, &barrier);
}