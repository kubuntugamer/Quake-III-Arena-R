
#include "../tr_local.h"


/*
** vkGetInstanceProcAddr needs to be set by platform specific implementation.
** Store through void* so no function-pointer cast warnings/errors (GCC 14+,
** MSVC) regardless of the concrete PFN type.
*/
#define VK_LOAD_FN( func, expr ) do { void *vk_load_p = (void *)(expr); memcpy(&(func), &vk_load_p, sizeof(vk_load_p)); if(!(func)) return qfalse; } while(0)
#define VK_GLOBAL_LEVEL_FUNCTION( func, a ) VK_LOAD_FN( func, vkGetInstanceProcAddr( NULL, a ) )
#define VK_INSTANCE_LEVEL_FUNCTION( func, a ) VK_LOAD_FN( func, vkGetInstanceProcAddr( vk.instance , a ) )
#define VK_DEVICE_LEVEL_FUNCTION( func, a ) VK_LOAD_FN( func, vkGetDeviceProcAddr( vk.device , a ) )



/* QVK function pointer storage (defined once; declared extern in qvk.h) */
PFN_vkGetInstanceProcAddr						vkGetInstanceProcAddr;
PFN_vkCreateInstance							vkCreateInstance;
PFN_vkEnumerateInstanceExtensionProperties		vkEnumerateInstanceExtensionProperties;
PFN_vkEnumerateInstanceLayerProperties			vkEnumerateInstanceLayerProperties;
PFN_vkDestroyInstance                           vkDestroyInstance;
PFN_vkEnumeratePhysicalDevices					vkEnumeratePhysicalDevices;
PFN_vkEnumerateDeviceExtensionProperties		vkEnumerateDeviceExtensionProperties;
PFN_vkGetPhysicalDeviceMemoryProperties			vkGetPhysicalDeviceMemoryProperties;
#if defined( _WIN32 )
PFN_vkCreateWin32SurfaceKHR						vkCreateWin32SurfaceKHR;
#elif defined(__APPLE__)
PFN_vkCreateMacOSSurfaceMVK                     vkCreateMacOSSurfaceMVK;
#elif defined( __linux__ )
PFN_vkCreateXlibSurfaceKHR				vkCreateXlibSurfaceKHR;
PFN_vkCreateWaylandSurfaceKHR			vkCreateWaylandSurfaceKHR;
#endif
PFN_vkDestroySurfaceKHR                         vkDestroySurfaceKHR;

extern qboolean VK_UsingWayland;
PFN_vkGetPhysicalDeviceFeatures					vkGetPhysicalDeviceFeatures;
PFN_vkGetPhysicalDeviceProperties				vkGetPhysicalDeviceProperties;
PFN_vkGetPhysicalDeviceProperties2				vkGetPhysicalDeviceProperties2;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR		vkGetPhysicalDeviceSurfaceSupportKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR		vkGetPhysicalDeviceSurfaceSupportKHR;
PFN_vkGetPhysicalDeviceQueueFamilyProperties	vkGetPhysicalDeviceQueueFamilyProperties;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	vkGetPhysicalDeviceSurfacePresentModesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR		vkGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetDeviceProcAddr							vkGetDeviceProcAddr;
PFN_vkCreateDevice								vkCreateDevice;
PFN_vkCreateDebugUtilsMessengerEXT              vkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT             vkDestroyDebugUtilsMessengerEXT;
PFN_vkGetDeviceQueue							vkGetDeviceQueue;
PFN_vkCreateCommandPool							vkCreateCommandPool;
PFN_vkCreateSwapchainKHR						vkCreateSwapchainKHR;
PFN_vkGetSwapchainImagesKHR						vkGetSwapchainImagesKHR;
PFN_vkCreateImageView							vkCreateImageView;
PFN_vkCreateSampler								vkCreateSampler;
PFN_vkCreateRenderPass							vkCreateRenderPass;
PFN_vkCreateFramebuffer							vkCreateFramebuffer;
PFN_vkAllocateCommandBuffers					vkAllocateCommandBuffers;
PFN_vkCreateSemaphore							vkCreateSemaphore;
PFN_vkCreateFence								vkCreateFence;
PFN_vkWaitForFences								vkWaitForFences;
PFN_vkResetFences								vkResetFences;
PFN_vkAcquireNextImageKHR						vkAcquireNextImageKHR;
PFN_vkFreeCommandBuffers						vkFreeCommandBuffers;
PFN_vkBeginCommandBuffer						vkBeginCommandBuffer;
PFN_vkEndCommandBuffer							vkEndCommandBuffer;
PFN_vkQueueSubmit								vkQueueSubmit;
PFN_vkQueueWaitIdle								vkQueueWaitIdle;
PFN_vkQueuePresentKHR							vkQueuePresentKHR;
PFN_vkCmdBeginRenderPass						vkCmdBeginRenderPass;
PFN_vkCmdSetViewport							vkCmdSetViewport;
PFN_vkCmdSetScissor								vkCmdSetScissor;
PFN_vkCmdEndRenderPass							vkCmdEndRenderPass;
PFN_vkCmdBindVertexBuffers                      vkCmdBindVertexBuffers;
PFN_vkCmdBindIndexBuffer                        vkCmdBindIndexBuffer;
PFN_vkCmdPushConstants                          vkCmdPushConstants;
PFN_vkCmdClearAttachments                       vkCmdClearAttachments;
PFN_vkCmdClearColorImage						vkCmdClearColorImage;
PFN_vkCreateImage								vkCreateImage;
PFN_vkGetImageMemoryRequirements				vkGetImageMemoryRequirements;
PFN_vkGetBufferMemoryRequirements				vkGetBufferMemoryRequirements;
PFN_vkGetBufferMemoryRequirements2				vkGetBufferMemoryRequirements2;
PFN_vkGetImageSubresourceLayout					vkGetImageSubresourceLayout;
PFN_vkCreateBuffer								vkCreateBuffer;
PFN_vkAllocateMemory							vkAllocateMemory;
PFN_vkBindBufferMemory							vkBindBufferMemory;
PFN_vkBindImageMemory							vkBindImageMemory;
PFN_vkMapMemory									vkMapMemory;
PFN_vkUnmapMemory								vkUnmapMemory;
PFN_vkDestroyBuffer								vkDestroyBuffer;
PFN_vkFreeMemory								vkFreeMemory;
PFN_vkAllocateCommandBuffers					vkAllocateCommandBuffers;
PFN_vkBeginCommandBuffer						vkBeginCommandBuffer;
PFN_vkEndCommandBuffer							vkEndCommandBuffer;
PFN_vkFreeCommandBuffers						vkFreeCommandBuffers;
PFN_vkCmdPipelineBarrier						vkCmdPipelineBarrier;
PFN_vkCmdCopyBuffer							vkCmdCopyBuffer;
PFN_vkCmdCopyBufferToImage						vkCmdCopyBufferToImage;
PFN_vkCmdBindPipeline                           vkCmdBindPipeline;
PFN_vkCmdBindDescriptorSets                     vkCmdBindDescriptorSets;
PFN_vkCmdBindVertexBuffers                      vkCmdBindVertexBuffers;
PFN_vkCmdDraw                                   vkCmdDraw;
PFN_vkCmdDrawIndexed                            vkCmdDrawIndexed;
PFN_vkCmdPushConstants                          vkCmdPushConstants;
PFN_vkCmdClearAttachments                       vkCmdClearAttachments;
PFN_vkCmdSetDepthBias                           vkCmdSetDepthBias;
PFN_vkCmdSetBlendConstants                      vkCmdSetBlendConstants;
PFN_vkCmdCopyImage								vkCmdCopyImage;
PFN_vkCmdDispatch								vkCmdDispatch;
PFN_vkCmdWriteTimestamp							vkCmdWriteTimestamp;
PFN_vkCmdBeginDebugUtilsLabelEXT				vkCmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT					vkCmdEndDebugUtilsLabelEXT;
PFN_vkCreateQueryPool							vkCreateQueryPool;
PFN_vkGetQueryPoolResults						vkGetQueryPoolResults;
PFN_vkCmdResetQueryPool							vkCmdResetQueryPool;
PFN_vkDestroyQueryPool							vkDestroyQueryPool;
PFN_vkCreatePipelineCache                       vkCreatePipelineCache;
PFN_vkCreatePipelineLayout                      vkCreatePipelineLayout;
PFN_vkCreateGraphicsPipelines                   vkCreateGraphicsPipelines;
PFN_vkCreateComputePipelines					vkCreateComputePipelines;
PFN_vkCreateShaderModule                        vkCreateShaderModule;
PFN_vkCreateDescriptorSetLayout                 vkCreateDescriptorSetLayout;
PFN_vkCreateDescriptorPool                      vkCreateDescriptorPool;
PFN_vkUpdateDescriptorSets                      vkUpdateDescriptorSets;
PFN_vkDestroySampler                            vkDestroySampler;
PFN_vkDestroyImage                              vkDestroyImage;
PFN_vkDestroyImageView                          vkDestroyImageView;
PFN_vkDestroyFramebuffer						vkDestroyFramebuffer;
PFN_vkFreeDescriptorSets                        vkFreeDescriptorSets;
PFN_vkDestroyDescriptorSetLayout                vkDestroyDescriptorSetLayout;
PFN_vkDestroyDescriptorPool                     vkDestroyDescriptorPool;
PFN_vkDestroyRenderPass							vkDestroyRenderPass;
PFN_vkDestroySwapchainKHR						vkDestroySwapchainKHR;
PFN_vkDestroySemaphore							vkDestroySemaphore;
PFN_vkDestroyFence								vkDestroyFence;
PFN_vkDestroyCommandPool 						vkDestroyCommandPool;
PFN_vkDestroyDevice								vkDestroyDevice;
PFN_vkDeviceWaitIdle							vkDeviceWaitIdle;
PFN_vkWaitForFences								vkWaitForFences;
PFN_vkGetFenceStatus                            vkGetFenceStatus;
PFN_vkAllocateDescriptorSets					vkAllocateDescriptorSets;
PFN_vkDestroyShaderModule						vkDestroyShaderModule;
PFN_vkDestroyPipeline							vkDestroyPipeline;
PFN_vkDestroyPipelineLayout						vkDestroyPipelineLayout;
PFN_vkDestroyPipelineCache						vkDestroyPipelineCache;
PFN_vkCreateAccelerationStructureKHR				vkCreateAccelerationStructureKHR;
PFN_vkDestroyAccelerationStructureKHR				vkDestroyAccelerationStructureKHR;
PFN_vkGetAccelerationStructureBuildSizesKHR		vkGetAccelerationStructureBuildSizesKHR;
PFN_vkGetAccelerationStructureDeviceAddressKHR	vkGetAccelerationStructureDeviceAddressKHR;
PFN_vkCmdBuildAccelerationStructuresKHR			vkCmdBuildAccelerationStructuresKHR;
PFN_vkCreateRayTracingPipelinesKHR				vkCreateRayTracingPipelinesKHR;
PFN_vkGetRayTracingShaderGroupHandlesKHR			vkGetRayTracingShaderGroupHandlesKHR;
PFN_vkCmdTraceRaysKHR								vkCmdTraceRaysKHR;
PFN_vkGetBufferDeviceAddress						vkGetBufferDeviceAddress;
PFN_vkWaitForPresentKHR							vkWaitForPresentKHR;
PFN_vkAntiLagUpdateAMD							vkAntiLagUpdateAMD;
PFN_vkSetLatencySleepModeNV						vkSetLatencySleepModeNV;
PFN_vkLatencySleepNV							vkLatencySleepNV;
PFN_vkSetLatencyMarkerNV						vkSetLatencyMarkerNV;
PFN_vkCmdSetCheckpointNV						vkCmdSetCheckpointNV;
PFN_vkGetQueueCheckpointDataNV					vkGetQueueCheckpointDataNV;

qboolean VK_LoadGlobalFunctions(void)
{
	VK_GLOBAL_LEVEL_FUNCTION(vkCreateInstance, "vkCreateInstance");
	VK_GLOBAL_LEVEL_FUNCTION(vkEnumerateInstanceExtensionProperties, "vkEnumerateInstanceExtensionProperties");
	VK_GLOBAL_LEVEL_FUNCTION(vkEnumerateInstanceLayerProperties, "vkEnumerateInstanceLayerProperties");
    
    return qtrue;
}

qboolean VK_LoadInstanceFunctions(void)
{
    VK_INSTANCE_LEVEL_FUNCTION(vkDestroyInstance, "vkDestroyInstance");
    
	VK_INSTANCE_LEVEL_FUNCTION(vkEnumeratePhysicalDevices, "vkEnumeratePhysicalDevices");
	VK_INSTANCE_LEVEL_FUNCTION(vkEnumerateDeviceExtensionProperties, "vkEnumerateDeviceExtensionProperties");
	VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceMemoryProperties, "vkGetPhysicalDeviceMemoryProperties");

	/* Surface */
#if defined( _WIN32 )
    VK_INSTANCE_LEVEL_FUNCTION(vkCreateWin32SurfaceKHR, "vkCreateWin32SurfaceKHR");
#elif defined(__APPLE__)
    VK_INSTANCE_LEVEL_FUNCTION(vkCreateMacOSSurfaceMVK, "vkCreateMacOSSurfaceMVK");
#elif defined( __linux__ )
    if ( VK_UsingWayland ) {
        VK_INSTANCE_LEVEL_FUNCTION(vkCreateWaylandSurfaceKHR, "vkCreateWaylandSurfaceKHR");
    } else {
        VK_INSTANCE_LEVEL_FUNCTION(vkCreateXlibSurfaceKHR, "vkCreateXlibSurfaceKHR");
    }
#endif
    VK_INSTANCE_LEVEL_FUNCTION(vkDestroySurfaceKHR, "vkDestroySurfaceKHR");
	
	/* Physical Device */
	VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceFeatures, "vkGetPhysicalDeviceFeatures");
	VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceProperties, "vkGetPhysicalDeviceProperties");
	VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceProperties2, "vkGetPhysicalDeviceProperties2");
	VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR, "vkGetPhysicalDeviceSurfaceSupportKHR");
	VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
	VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR, "vkGetPhysicalDeviceSurfaceSupportKHR");
	VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties, "vkGetPhysicalDeviceQueueFamilyProperties");
	VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR, "vkGetPhysicalDeviceSurfacePresentModesKHR");
	VK_INSTANCE_LEVEL_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR, "vkGetPhysicalDeviceSurfaceFormatsKHR");

	/* Device */
	VK_INSTANCE_LEVEL_FUNCTION(vkGetDeviceProcAddr, "vkGetDeviceProcAddr");
	VK_INSTANCE_LEVEL_FUNCTION(vkCreateDevice, "vkCreateDevice");
    
    /* Debug */
#ifndef NDEBUG
    VK_INSTANCE_LEVEL_FUNCTION(vkCreateDebugUtilsMessengerEXT, "vkCreateDebugUtilsMessengerEXT");
    VK_INSTANCE_LEVEL_FUNCTION(vkDestroyDebugUtilsMessengerEXT, "vkDestroyDebugUtilsMessengerEXT");
#endif

	return qtrue;
}

qboolean VK_LoadDeviceFunctions(void)
{
	VK_DEVICE_LEVEL_FUNCTION(vkGetDeviceQueue, "vkGetDeviceQueue");
	VK_DEVICE_LEVEL_FUNCTION(vkCreateCommandPool, "vkCreateCommandPool");
	VK_DEVICE_LEVEL_FUNCTION(vkCreateSwapchainKHR, "vkCreateSwapchainKHR");
	VK_DEVICE_LEVEL_FUNCTION(vkGetSwapchainImagesKHR, "vkGetSwapchainImagesKHR");
	VK_DEVICE_LEVEL_FUNCTION(vkCreateImageView, "vkCreateImageView");
	VK_DEVICE_LEVEL_FUNCTION(vkCreateSampler, "vkCreateSampler");
	VK_DEVICE_LEVEL_FUNCTION(vkCreateRenderPass, "vkCreateRenderPass");
	VK_DEVICE_LEVEL_FUNCTION(vkCreateFramebuffer, "vkCreateFramebuffer");
	VK_DEVICE_LEVEL_FUNCTION(vkAllocateCommandBuffers, "vkAllocateCommandBuffers");
	VK_DEVICE_LEVEL_FUNCTION(vkCreateSemaphore, "vkCreateSemaphore");
	VK_DEVICE_LEVEL_FUNCTION(vkCreateFence, "vkCreateFence");

	VK_DEVICE_LEVEL_FUNCTION(vkWaitForFences, "vkWaitForFences");
	VK_DEVICE_LEVEL_FUNCTION(vkResetFences, "vkResetFences");
	VK_DEVICE_LEVEL_FUNCTION(vkAcquireNextImageKHR, "vkAcquireNextImageKHR");
	VK_DEVICE_LEVEL_FUNCTION(vkFreeCommandBuffers, "vkFreeCommandBuffers");
	VK_DEVICE_LEVEL_FUNCTION(vkBeginCommandBuffer, "vkBeginCommandBuffer");

	VK_DEVICE_LEVEL_FUNCTION(vkEndCommandBuffer, "vkEndCommandBuffer");
	VK_DEVICE_LEVEL_FUNCTION(vkQueueSubmit, "vkQueueSubmit");
	VK_DEVICE_LEVEL_FUNCTION(vkQueueWaitIdle, "vkQueueWaitIdle");
	VK_DEVICE_LEVEL_FUNCTION(vkQueuePresentKHR, "vkQueuePresentKHR");

	VK_DEVICE_LEVEL_FUNCTION(vkCmdBeginRenderPass, "vkCmdBeginRenderPass");
	VK_DEVICE_LEVEL_FUNCTION(vkCmdSetViewport, "vkCmdSetViewport");
	VK_DEVICE_LEVEL_FUNCTION(vkCmdSetScissor, "vkCmdSetScissor");
	VK_DEVICE_LEVEL_FUNCTION(vkCmdEndRenderPass, "vkCmdEndRenderPass");
    VK_DEVICE_LEVEL_FUNCTION(vkCmdBindVertexBuffers, "vkCmdBindVertexBuffers");
    VK_DEVICE_LEVEL_FUNCTION(vkCmdBindIndexBuffer, "vkCmdBindIndexBuffer");
    VK_DEVICE_LEVEL_FUNCTION(vkCmdPushConstants, "vkCmdPushConstants");
    VK_DEVICE_LEVEL_FUNCTION(vkCmdClearAttachments, "vkCmdClearAttachments");
	VK_DEVICE_LEVEL_FUNCTION(vkCmdClearColorImage, "vkCmdClearColorImage");
    VK_DEVICE_LEVEL_FUNCTION(vkCmdPushConstants, "vkCmdPushConstants");

	VK_DEVICE_LEVEL_FUNCTION(vkCreateImage, "vkCreateImage");
	VK_DEVICE_LEVEL_FUNCTION(vkGetImageMemoryRequirements, "vkGetImageMemoryRequirements");
	VK_DEVICE_LEVEL_FUNCTION(vkGetBufferMemoryRequirements, "vkGetBufferMemoryRequirements");
	VK_DEVICE_LEVEL_FUNCTION(vkGetBufferMemoryRequirements2, "vkGetBufferMemoryRequirements2");
	VK_DEVICE_LEVEL_FUNCTION(vkGetImageSubresourceLayout, "vkGetImageSubresourceLayout");

	VK_DEVICE_LEVEL_FUNCTION(vkCreateBuffer, "vkCreateBuffer");
	VK_DEVICE_LEVEL_FUNCTION(vkAllocateMemory, "vkAllocateMemory");
	VK_DEVICE_LEVEL_FUNCTION(vkBindBufferMemory, "vkBindBufferMemory");
	VK_DEVICE_LEVEL_FUNCTION(vkBindImageMemory, "vkBindImageMemory");
	VK_DEVICE_LEVEL_FUNCTION(vkMapMemory, "vkMapMemory");
	VK_DEVICE_LEVEL_FUNCTION(vkUnmapMemory, "vkUnmapMemory");

	VK_DEVICE_LEVEL_FUNCTION(vkDestroyBuffer, "vkDestroyBuffer");
	VK_DEVICE_LEVEL_FUNCTION(vkFreeMemory, "vkFreeMemory");

	VK_DEVICE_LEVEL_FUNCTION(vkAllocateCommandBuffers, "vkAllocateCommandBuffers");
	VK_DEVICE_LEVEL_FUNCTION(vkBeginCommandBuffer, "vkBeginCommandBuffer");
	VK_DEVICE_LEVEL_FUNCTION(vkEndCommandBuffer, "vkEndCommandBuffer");
	VK_DEVICE_LEVEL_FUNCTION(vkFreeCommandBuffers, "vkFreeCommandBuffers");

	VK_DEVICE_LEVEL_FUNCTION(vkCmdPipelineBarrier, "vkCmdPipelineBarrier");
	VK_DEVICE_LEVEL_FUNCTION(vkCmdCopyBuffer, "vkCmdCopyBuffer");
	VK_DEVICE_LEVEL_FUNCTION(vkCmdCopyBufferToImage, "vkCmdCopyBufferToImage");
    VK_DEVICE_LEVEL_FUNCTION(vkCmdBindPipeline, "vkCmdBindPipeline");
    VK_DEVICE_LEVEL_FUNCTION(vkCmdBindDescriptorSets, "vkCmdBindDescriptorSets");
    VK_DEVICE_LEVEL_FUNCTION(vkCmdBindVertexBuffers, "vkCmdBindVertexBuffers");
    VK_DEVICE_LEVEL_FUNCTION(vkCmdDraw, "vkCmdDraw");
    VK_DEVICE_LEVEL_FUNCTION(vkCmdDrawIndexed, "vkCmdDrawIndexed");
    VK_DEVICE_LEVEL_FUNCTION(vkCmdClearAttachments, "vkCmdClearAttachments");
    VK_DEVICE_LEVEL_FUNCTION(vkCmdSetDepthBias, "vkCmdSetDepthBias");
    VK_DEVICE_LEVEL_FUNCTION(vkCmdSetBlendConstants, "vkCmdSetBlendConstants");
	VK_DEVICE_LEVEL_FUNCTION(vkCmdCopyImage, "vkCmdCopyImage");
	VK_DEVICE_LEVEL_FUNCTION(vkCmdDispatch, "vkCmdDispatch");
	VK_DEVICE_LEVEL_FUNCTION(vkCmdWriteTimestamp, "vkCmdWriteTimestamp");
#ifndef NDEBUG
	VK_DEVICE_LEVEL_FUNCTION(vkCmdBeginDebugUtilsLabelEXT, "vkCmdBeginDebugUtilsLabelEXT");
	VK_DEVICE_LEVEL_FUNCTION(vkCmdEndDebugUtilsLabelEXT, "vkCmdEndDebugUtilsLabelEXT");
#endif

	VK_DEVICE_LEVEL_FUNCTION(vkCreateQueryPool, "vkCreateQueryPool");
	VK_DEVICE_LEVEL_FUNCTION(vkGetQueryPoolResults, "vkGetQueryPoolResults");
	VK_DEVICE_LEVEL_FUNCTION(vkCmdResetQueryPool, "vkCmdResetQueryPool");
	VK_DEVICE_LEVEL_FUNCTION(vkDestroyQueryPool, "vkDestroyQueryPool");
    VK_DEVICE_LEVEL_FUNCTION(vkCreatePipelineCache, "vkCreatePipelineCache");
    VK_DEVICE_LEVEL_FUNCTION(vkCreatePipelineLayout, "vkCreatePipelineLayout");
    VK_DEVICE_LEVEL_FUNCTION(vkCreateGraphicsPipelines, "vkCreateGraphicsPipelines");
	VK_DEVICE_LEVEL_FUNCTION(vkCreateComputePipelines, "vkCreateComputePipelines");
    
    VK_DEVICE_LEVEL_FUNCTION(vkCreateShaderModule, "vkCreateShaderModule");
    VK_DEVICE_LEVEL_FUNCTION(vkCreateDescriptorSetLayout, "vkCreateDescriptorSetLayout");
    VK_DEVICE_LEVEL_FUNCTION(vkCreateDescriptorPool, "vkCreateDescriptorPool");
    VK_DEVICE_LEVEL_FUNCTION(vkUpdateDescriptorSets, "vkUpdateDescriptorSets");
    
    VK_DEVICE_LEVEL_FUNCTION(vkDestroySampler, "vkDestroySampler");
    VK_DEVICE_LEVEL_FUNCTION(vkDestroyImage, "vkDestroyImage");
    VK_DEVICE_LEVEL_FUNCTION(vkDestroyImageView, "vkDestroyImageView");
	VK_DEVICE_LEVEL_FUNCTION(vkDestroyFramebuffer, "vkDestroyFramebuffer");
    VK_DEVICE_LEVEL_FUNCTION(vkFreeDescriptorSets, "vkFreeDescriptorSets");
    VK_DEVICE_LEVEL_FUNCTION(vkDestroyDescriptorSetLayout, "vkDestroyDescriptorSetLayout");
    VK_DEVICE_LEVEL_FUNCTION(vkDestroyDescriptorPool, "vkDestroyDescriptorPool");
	VK_DEVICE_LEVEL_FUNCTION(vkDestroyRenderPass, "vkDestroyRenderPass");
	VK_DEVICE_LEVEL_FUNCTION(vkDestroySwapchainKHR, "vkDestroySwapchainKHR");
	VK_DEVICE_LEVEL_FUNCTION(vkDestroySemaphore, "vkDestroySemaphore");
	VK_DEVICE_LEVEL_FUNCTION(vkDestroyFence, "vkDestroyFence");
	VK_DEVICE_LEVEL_FUNCTION(vkDestroyCommandPool, "vkDestroyCommandPool");
	VK_DEVICE_LEVEL_FUNCTION(vkDestroyDevice, "vkDestroyDevice");

	VK_DEVICE_LEVEL_FUNCTION(vkDeviceWaitIdle, "vkDeviceWaitIdle");
	VK_DEVICE_LEVEL_FUNCTION(vkWaitForFences, "vkWaitForFences");
    VK_DEVICE_LEVEL_FUNCTION(vkGetFenceStatus, "vkGetFenceStatus");
	VK_DEVICE_LEVEL_FUNCTION(vkAllocateDescriptorSets, "vkAllocateDescriptorSets");

	VK_DEVICE_LEVEL_FUNCTION(vkDestroyShaderModule, "vkDestroyShaderModule");
	VK_DEVICE_LEVEL_FUNCTION(vkDestroyPipeline, "vkDestroyPipeline");
	VK_DEVICE_LEVEL_FUNCTION(vkDestroyPipelineLayout, "vkDestroyPipelineLayout");
	VK_DEVICE_LEVEL_FUNCTION(vkDestroyPipelineCache, "vkDestroyPipelineCache");

	/*
	** KHR Ray Tracing
	*/
	VK_DEVICE_LEVEL_FUNCTION(vkCreateAccelerationStructureKHR, "vkCreateAccelerationStructureKHR");
	VK_DEVICE_LEVEL_FUNCTION(vkDestroyAccelerationStructureKHR, "vkDestroyAccelerationStructureKHR");
	VK_DEVICE_LEVEL_FUNCTION(vkGetAccelerationStructureBuildSizesKHR, "vkGetAccelerationStructureBuildSizesKHR");
	VK_DEVICE_LEVEL_FUNCTION(vkGetAccelerationStructureDeviceAddressKHR, "vkGetAccelerationStructureDeviceAddressKHR");
	VK_DEVICE_LEVEL_FUNCTION(vkCmdBuildAccelerationStructuresKHR, "vkCmdBuildAccelerationStructuresKHR");
	VK_DEVICE_LEVEL_FUNCTION(vkCreateRayTracingPipelinesKHR, "vkCreateRayTracingPipelinesKHR");
	VK_DEVICE_LEVEL_FUNCTION(vkGetRayTracingShaderGroupHandlesKHR, "vkGetRayTracingShaderGroupHandlesKHR");
	VK_DEVICE_LEVEL_FUNCTION(vkCmdTraceRaysKHR, "vkCmdTraceRaysKHR");
	VK_DEVICE_LEVEL_FUNCTION(vkGetBufferDeviceAddress, "vkGetBufferDeviceAddress");
	/* present_wait is optional: resolve tolerantly (pacing checks the flag + pointer) */
	*(void**)&vkWaitForPresentKHR = (void*)vkGetDeviceProcAddr(vk.device, "vkWaitForPresentKHR");
	if (vkWaitForPresentKHR == NULL) {
		vk.presentWait = qfalse;
	}

	/* Vendor-optional entry points: resolve tolerantly, never fail the device.
	   Use sites check both the extension flag and the pointer. */
	*(void**)&vkAntiLagUpdateAMD = (void*)vkGetDeviceProcAddr(vk.device, "vkAntiLagUpdateAMD");
	*(void**)&vkSetLatencySleepModeNV = (void*)vkGetDeviceProcAddr(vk.device, "vkSetLatencySleepModeNV");
	*(void**)&vkLatencySleepNV = (void*)vkGetDeviceProcAddr(vk.device, "vkLatencySleepNV");
	*(void**)&vkSetLatencyMarkerNV = (void*)vkGetDeviceProcAddr(vk.device, "vkSetLatencyMarkerNV");
	*(void**)&vkCmdSetCheckpointNV = (void*)vkGetDeviceProcAddr(vk.device, "vkCmdSetCheckpointNV");
	*(void**)&vkGetQueueCheckpointDataNV = (void*)vkGetDeviceProcAddr(vk.device, "vkGetQueueCheckpointDataNV");

    return qtrue;
}
