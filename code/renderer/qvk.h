
/*
** QVK.H
*/

#ifndef __QVK_H__
#define __QVK_H__

#if defined( _WIN32 )

#pragma warning (disable: 4201)
#pragma warning (disable: 4214)
#pragma warning (disable: 4514)
#pragma warning (disable: 4032)
#pragma warning (disable: 4201)
#pragma warning (disable: 4214)
#include <windows.h>

#define VK_USE_PLATFORM_WIN32_KHR 
#define VK_NO_PROTOTYPES 
#include <vulkan/vulkan.h>

#elif defined(__APPLE__)

#define VK_USE_PLATFORM_MACOS_MVK
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#elif defined( __linux__ )

#include <X11/Xlib.h>
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#else

#include <gl.h>

#endif

#include "tr_local.h"

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef WINAPI
#define WINAPI
#endif


extern PFN_vkGetInstanceProcAddr						vkGetInstanceProcAddr;

/*
** GLOBAL
*/
extern PFN_vkCreateInstance							vkCreateInstance;
extern PFN_vkEnumerateInstanceExtensionProperties		vkEnumerateInstanceExtensionProperties;
extern PFN_vkEnumerateInstanceLayerProperties			vkEnumerateInstanceLayerProperties;

/*
** INSTANCE
*/
extern PFN_vkDestroyInstance                           vkDestroyInstance;

extern PFN_vkEnumeratePhysicalDevices					vkEnumeratePhysicalDevices;
extern PFN_vkEnumerateDeviceExtensionProperties		vkEnumerateDeviceExtensionProperties;
extern PFN_vkGetPhysicalDeviceMemoryProperties			vkGetPhysicalDeviceMemoryProperties;

/* Surface */
#if defined( _WIN32 )
extern PFN_vkCreateWin32SurfaceKHR						vkCreateWin32SurfaceKHR;
#elif defined(__APPLE__)
extern PFN_vkCreateMacOSSurfaceMVK                     vkCreateMacOSSurfaceMVK;
#elif defined( __linux__ )
extern PFN_vkCreateXlibSurfaceKHR				vkCreateXlibSurfaceKHR;
extern PFN_vkCreateWaylandSurfaceKHR			vkCreateWaylandSurfaceKHR;
extern PFN_vkCreateWaylandSurfaceKHR			vkCreateWaylandSurfaceKHR;
#endif
extern PFN_vkDestroySurfaceKHR                         vkDestroySurfaceKHR;

/* Physical Device */
extern PFN_vkGetPhysicalDeviceFeatures					vkGetPhysicalDeviceFeatures;
extern PFN_vkGetPhysicalDeviceProperties				vkGetPhysicalDeviceProperties;
extern PFN_vkGetPhysicalDeviceProperties2				vkGetPhysicalDeviceProperties2;
extern PFN_vkGetPhysicalDeviceSurfaceSupportKHR		vkGetPhysicalDeviceSurfaceSupportKHR;
extern PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceSupportKHR		vkGetPhysicalDeviceSurfaceSupportKHR;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties	vkGetPhysicalDeviceQueueFamilyProperties;
extern PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	vkGetPhysicalDeviceSurfacePresentModesKHR;
extern PFN_vkGetPhysicalDeviceSurfaceFormatsKHR		vkGetPhysicalDeviceSurfaceFormatsKHR;

/* Device */
extern PFN_vkGetDeviceProcAddr							vkGetDeviceProcAddr;
extern PFN_vkCreateDevice								vkCreateDevice;

/* Debug */
#ifndef NDEBUG
extern PFN_vkCreateDebugUtilsMessengerEXT              vkCreateDebugUtilsMessengerEXT;
extern PFN_vkDestroyDebugUtilsMessengerEXT             vkDestroyDebugUtilsMessengerEXT;
#endif

/*
** DEVICE
*/
extern PFN_vkGetDeviceQueue							vkGetDeviceQueue;
extern PFN_vkCreateCommandPool							vkCreateCommandPool;
extern PFN_vkCreateSwapchainKHR						vkCreateSwapchainKHR;
extern PFN_vkGetSwapchainImagesKHR						vkGetSwapchainImagesKHR;
extern PFN_vkCreateImageView							vkCreateImageView;
extern PFN_vkCreateSampler								vkCreateSampler;
extern PFN_vkCreateRenderPass							vkCreateRenderPass;
extern PFN_vkCreateFramebuffer							vkCreateFramebuffer;
extern PFN_vkAllocateCommandBuffers					vkAllocateCommandBuffers;
extern PFN_vkCreateSemaphore							vkCreateSemaphore;
extern PFN_vkCreateFence								vkCreateFence;

extern PFN_vkWaitForFences								vkWaitForFences;
extern PFN_vkResetFences								vkResetFences;
extern PFN_vkAcquireNextImageKHR						vkAcquireNextImageKHR;
extern PFN_vkFreeCommandBuffers						vkFreeCommandBuffers;
extern PFN_vkBeginCommandBuffer						vkBeginCommandBuffer;

extern PFN_vkEndCommandBuffer							vkEndCommandBuffer;
extern PFN_vkQueueSubmit								vkQueueSubmit;
extern PFN_vkQueueWaitIdle								vkQueueWaitIdle;
extern PFN_vkQueuePresentKHR							vkQueuePresentKHR;

extern PFN_vkCmdBeginRenderPass						vkCmdBeginRenderPass;
extern PFN_vkCmdSetViewport							vkCmdSetViewport;
extern PFN_vkCmdSetScissor								vkCmdSetScissor;
extern PFN_vkCmdEndRenderPass							vkCmdEndRenderPass;
extern PFN_vkCmdBindVertexBuffers                      vkCmdBindVertexBuffers;
extern PFN_vkCmdBindIndexBuffer                        vkCmdBindIndexBuffer;
extern PFN_vkCmdPushConstants                          vkCmdPushConstants;
extern PFN_vkCmdClearAttachments                       vkCmdClearAttachments;
extern PFN_vkCmdClearColorImage						vkCmdClearColorImage;

extern PFN_vkCreateImage								vkCreateImage;
extern PFN_vkGetImageMemoryRequirements				vkGetImageMemoryRequirements;
extern PFN_vkGetBufferMemoryRequirements				vkGetBufferMemoryRequirements;
extern PFN_vkGetBufferMemoryRequirements2				vkGetBufferMemoryRequirements2;
extern PFN_vkGetImageSubresourceLayout					vkGetImageSubresourceLayout;

extern PFN_vkCreateBuffer								vkCreateBuffer;
extern PFN_vkAllocateMemory							vkAllocateMemory;
extern PFN_vkBindBufferMemory							vkBindBufferMemory;
extern PFN_vkBindImageMemory							vkBindImageMemory;
extern PFN_vkMapMemory									vkMapMemory;
extern PFN_vkUnmapMemory								vkUnmapMemory;

extern PFN_vkDestroyBuffer								vkDestroyBuffer;
extern PFN_vkFreeMemory								vkFreeMemory;

extern PFN_vkAllocateCommandBuffers					vkAllocateCommandBuffers;
extern PFN_vkBeginCommandBuffer						vkBeginCommandBuffer;
extern PFN_vkEndCommandBuffer							vkEndCommandBuffer;
extern PFN_vkFreeCommandBuffers						vkFreeCommandBuffers;

extern PFN_vkCmdPipelineBarrier						vkCmdPipelineBarrier;
extern PFN_vkCmdCopyBufferToImage						vkCmdCopyBufferToImage;
extern PFN_vkCmdCopyBuffer							vkCmdCopyBuffer;
extern PFN_vkCmdBindPipeline                           vkCmdBindPipeline;
extern PFN_vkCmdBindDescriptorSets                     vkCmdBindDescriptorSets;
extern PFN_vkCmdBindVertexBuffers                      vkCmdBindVertexBuffers;
extern PFN_vkCmdDraw                                   vkCmdDraw;
extern PFN_vkCmdDrawIndexed                            vkCmdDrawIndexed;
extern PFN_vkCmdPushConstants                          vkCmdPushConstants;
extern PFN_vkCmdClearAttachments                       vkCmdClearAttachments;
extern PFN_vkCmdSetDepthBias                           vkCmdSetDepthBias;
extern PFN_vkCmdSetBlendConstants                      vkCmdSetBlendConstants;
extern PFN_vkCmdCopyImage								vkCmdCopyImage;
extern PFN_vkCmdDispatch								vkCmdDispatch;
extern PFN_vkCmdWriteTimestamp							vkCmdWriteTimestamp;
extern PFN_vkCmdBeginDebugUtilsLabelEXT				vkCmdBeginDebugUtilsLabelEXT;
extern PFN_vkCmdEndDebugUtilsLabelEXT					vkCmdEndDebugUtilsLabelEXT;

extern PFN_vkCreateQueryPool							vkCreateQueryPool;
extern PFN_vkGetQueryPoolResults						vkGetQueryPoolResults;
extern PFN_vkCmdResetQueryPool							vkCmdResetQueryPool;
extern PFN_vkDestroyQueryPool							vkDestroyQueryPool;

extern PFN_vkCreatePipelineCache                       vkCreatePipelineCache;
extern PFN_vkCreatePipelineLayout                      vkCreatePipelineLayout;
extern PFN_vkCreateGraphicsPipelines                   vkCreateGraphicsPipelines;
extern PFN_vkCreateComputePipelines					vkCreateComputePipelines;

extern PFN_vkCreateShaderModule                        vkCreateShaderModule;
extern PFN_vkCreateDescriptorSetLayout                 vkCreateDescriptorSetLayout;
extern PFN_vkCreateDescriptorPool                      vkCreateDescriptorPool;
extern PFN_vkUpdateDescriptorSets                      vkUpdateDescriptorSets;

extern PFN_vkDestroySampler                            vkDestroySampler;
extern PFN_vkDestroyImage                              vkDestroyImage;
extern PFN_vkDestroyImageView                          vkDestroyImageView;
extern PFN_vkDestroyFramebuffer						vkDestroyFramebuffer;
extern PFN_vkFreeDescriptorSets                        vkFreeDescriptorSets;
extern PFN_vkDestroyDescriptorSetLayout                vkDestroyDescriptorSetLayout;
extern PFN_vkDestroyDescriptorPool                     vkDestroyDescriptorPool;
extern PFN_vkDestroyRenderPass							vkDestroyRenderPass;
extern PFN_vkDestroySwapchainKHR						vkDestroySwapchainKHR;
extern PFN_vkDestroySemaphore							vkDestroySemaphore;
extern PFN_vkDestroyFence								vkDestroyFence;
extern PFN_vkDestroyCommandPool 						vkDestroyCommandPool;
extern PFN_vkDestroyDevice								vkDestroyDevice;

extern PFN_vkDeviceWaitIdle							vkDeviceWaitIdle;
extern PFN_vkWaitForFences								vkWaitForFences;
extern PFN_vkGetFenceStatus                            vkGetFenceStatus;
extern PFN_vkAllocateDescriptorSets					vkAllocateDescriptorSets;

extern PFN_vkDestroyShaderModule						vkDestroyShaderModule;
extern PFN_vkDestroyPipeline							vkDestroyPipeline;
extern PFN_vkDestroyPipelineLayout						vkDestroyPipelineLayout;
extern PFN_vkDestroyPipelineCache						vkDestroyPipelineCache;

/*
** KHR Ray Tracing
*/
extern PFN_vkCreateAccelerationStructureKHR				vkCreateAccelerationStructureKHR;
extern PFN_vkDestroyAccelerationStructureKHR				vkDestroyAccelerationStructureKHR;
extern PFN_vkGetAccelerationStructureBuildSizesKHR		vkGetAccelerationStructureBuildSizesKHR;
extern PFN_vkGetAccelerationStructureDeviceAddressKHR	vkGetAccelerationStructureDeviceAddressKHR;
extern PFN_vkCmdBuildAccelerationStructuresKHR			vkCmdBuildAccelerationStructuresKHR;
extern PFN_vkCreateRayTracingPipelinesKHR				vkCreateRayTracingPipelinesKHR;
extern PFN_vkGetRayTracingShaderGroupHandlesKHR			vkGetRayTracingShaderGroupHandlesKHR;
extern PFN_vkCmdTraceRaysKHR								vkCmdTraceRaysKHR;
extern PFN_vkGetBufferDeviceAddress						vkGetBufferDeviceAddress;
extern PFN_vkWaitForPresentKHR							vkWaitForPresentKHR;
extern PFN_vkAntiLagUpdateAMD							vkAntiLagUpdateAMD;
extern PFN_vkSetLatencySleepModeNV						vkSetLatencySleepModeNV;
extern PFN_vkLatencySleepNV							vkLatencySleepNV;
extern PFN_vkSetLatencyMarkerNV						vkSetLatencyMarkerNV;
extern PFN_vkCmdSetCheckpointNV						vkCmdSetCheckpointNV;
extern PFN_vkGetQueueCheckpointDataNV					vkGetQueueCheckpointDataNV;

#endif
