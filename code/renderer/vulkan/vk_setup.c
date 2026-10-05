
#include "../tr_local.h"

vkinstance_t vk;
vkdata_t     vk_d;

// Needed to present anything at all.
static const char* requiredDeviceExtensions[] = {
#if defined( _WIN32 ) || defined( __linux__ )
		VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
#endif
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Needed only by the raytracing path. Requiring these unconditionally meant a
// GPU without RT support was rejected as a candidate even when running pure
// rasterization, so r_vertexLight 0 plus r_fsrScale could not run on it.
static const char* rayTracingDeviceExtensions[] = {
#if defined( _WIN32 ) || defined( __linux__ )
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
#endif
};

// Is the raytracing renderer actually being used? Single source of truth for the
// setup-time gating of RT extensions, features and entry points.
qboolean VK_RayTracingActive( void )
{
	return ( r_vertexLight != NULL && r_vertexLight->integer == 2 );
}


// Enabled when the driver offers them (queried per physical device, never fatal).
// AMD vendor set: only anti_lag/coherent/core-props are actively used; the
// rest are enabled when present so future shader work can rely on them.
static const char* optionalDeviceExtensions[] = {
#if defined( _WIN32 ) || defined( __linux__ )
		VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME,
		VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
		VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
		VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
		VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME,
		VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME,
		VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
		VK_KHR_PRESENT_ID_EXTENSION_NAME,
		VK_KHR_PRESENT_WAIT_EXTENSION_NAME,
		VK_NV_LOW_LATENCY_2_EXTENSION_NAME,
		VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME,
		VK_AMD_ANTI_LAG_EXTENSION_NAME,
		VK_AMD_BUFFER_MARKER_EXTENSION_NAME,
		VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME,
		VK_AMD_DISPLAY_NATIVE_HDR_EXTENSION_NAME,
		VK_AMD_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
		VK_AMD_GCN_SHADER_EXTENSION_NAME,
		VK_AMD_GPU_SHADER_HALF_FLOAT_EXTENSION_NAME,
		VK_AMD_GPU_SHADER_INT16_EXTENSION_NAME,
		VK_AMD_MEMORY_OVERALLOCATION_BEHAVIOR_EXTENSION_NAME,
		VK_AMD_MIXED_ATTACHMENT_SAMPLES_EXTENSION_NAME,
		VK_AMD_NEGATIVE_VIEWPORT_HEIGHT_EXTENSION_NAME,
		VK_AMD_PIPELINE_COMPILER_CONTROL_EXTENSION_NAME,
		VK_AMD_RASTERIZATION_ORDER_EXTENSION_NAME,
		VK_AMD_SHADER_BALLOT_EXTENSION_NAME,
		VK_AMD_SHADER_CORE_PROPERTIES_EXTENSION_NAME,
		VK_AMD_SHADER_CORE_PROPERTIES_2_EXTENSION_NAME,
		VK_AMD_SHADER_EARLY_AND_LATE_FRAGMENT_TESTS_EXTENSION_NAME,
		VK_AMD_SHADER_EXPLICIT_VERTEX_PARAMETER_EXTENSION_NAME,
		VK_AMD_SHADER_FRAGMENT_MASK_EXTENSION_NAME,
		VK_AMD_SHADER_IMAGE_LOAD_STORE_LOD_EXTENSION_NAME,
		VK_AMD_SHADER_INFO_EXTENSION_NAME,
		VK_AMD_SHADER_TRINARY_MINMAX_EXTENSION_NAME,
		VK_AMD_TEXTURE_GATHER_BIAS_LOD_EXTENSION_NAME,
#endif
};

#define VK_MAX_ENABLED_DEVICE_EXTENSIONS 48
static const char* enabledDeviceExtensions[VK_MAX_ENABLED_DEVICE_EXTENSIONS];
static uint32_t enabledDeviceExtensionCount = 0;

static const char* validationLayers[] = {
		"VK_LAYER_KHRONOS_validation"
};

/* Wayland backend was chosen by the unix layer at window-create time. */
qboolean VK_UsingWayland = qfalse;
void    *VK_WaylandDisplay = NULL;
void    *VK_WaylandSurface = NULL;

static const char* instanceExtensions[] = {
#ifndef NDEBUG
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
		VK_KHR_SURFACE_EXTENSION_NAME,
#if defined( _WIN32 )
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
#elif defined(__APPLE__)
		VK_MVK_MACOS_SURFACE_EXTENSION_NAME
#elif defined( __linux__ )
		VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
#endif
};

/*
 ==============================================================================
 
 Vulkan Setup
 
 ==============================================================================
 */

// Function Declaration
static void VK_CreateInstance();
static void VK_CreateSurface(void* p1, void* p2);
static void VK_PickPhysicalDevice();
static void VK_CreateLogicalDevice();
static void VK_CreateCommandPool();
static void VK_SetupDebugCallback();
static void VK_SetupQueryPool();

// Helper
static qboolean VK_IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
static qboolean VK_CheckValidationLayerSupport();
static void VK_FillEnabledDeviceExtensions(VkPhysicalDevice device);

void VK_Setup(void* p1, void* p2) {
    if (!VK_LoadGlobalFunctions()) return;
    VK_CreateInstance();
    if (!VK_LoadInstanceFunctions()) return;
    VK_CreateSurface(p1, p2);
    VK_PickPhysicalDevice();
    VK_CreateLogicalDevice();
    if (!VK_LoadDeviceFunctions()) return;
    VK_CreateCommandPool();
#ifndef NDEBUG
    VK_SetupDebugCallback();
#endif
    
    // Create Swapchain
	VK_SetupSwapchain();
	// For Performance Marker
	VK_SetupQueryPool();
}

void VK_Destroy() {
	vkDeviceWaitIdle(vk.device);

	VK_DestroySwapchain();

	vkDestroyQueryPool(vk.device, vk.queryPool, NULL);
	vkDestroyCommandPool(vk.device, vk.commandPool, NULL);
	vkDestroyDevice(vk.device, NULL);
	vkDestroySurfaceKHR(vk.instance, vk.surface, NULL);

#ifndef NDEBUG
	vkDestroyDebugUtilsMessengerEXT(vk.instance, vk.callback, NULL);
#endif

	vkDestroyInstance(vk.instance, NULL);

	Com_Memset(&vk, 0, sizeof(vk));
}

static void VK_CreateInstance() {
	// check extensions availability
	uint32_t count = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &count, NULL);
	VkExtensionProperties *extension_properties = malloc(count * sizeof(VkExtensionProperties));
	vkEnumerateInstanceExtensionProperties(NULL, &count, &extension_properties[0]);

	// Build the instance extension list; use the Wayland surface extension
	// when a Wayland window was created instead of the Xlib one.
	int extCount = sizeof(instanceExtensions) / sizeof(instanceExtensions[0]);
	const char *exts[64];
	for (int i = 0; i < extCount; i++) exts[i] = instanceExtensions[i];
#ifdef __linux__
	if ( VK_UsingWayland ) {
		int found = -1;
		for (int i = 0; i < extCount; i++) {
			if (!strcmp(exts[i], VK_KHR_XLIB_SURFACE_EXTENSION_NAME)) { found = i; break; }
		}
		if (found >= 0) exts[found] = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
	}
#endif

	for (int i = 0; i < extCount; i++) {
		qboolean supported = qfalse;
		for (int j = 0; j < count; j++) {
			if (!strcmp(extension_properties[j].extensionName, exts[i])) {
				supported = qtrue;
				break;
			}
		}
		if (!supported) ri.Error(ERR_FATAL, "Vulkan: required instance extension is not available: %s", exts[i]);
	}
	free(extension_properties);

#ifndef NDEBUG
	if (!VK_CheckValidationLayerSupport()) {
		ri.Error(ERR_FATAL, "Vulkan: validation layers requested, but not available!");
	}
#endif

	// create instance
	VkApplicationInfo vk_app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Quake 3",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "q3pt",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_2,
	};

	VkInstanceCreateInfo desc = { 0 };
	desc.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.pApplicationInfo = &vk_app_info;
	desc.enabledLayerCount = 0;
	desc.ppEnabledLayerNames = NULL;
	desc.enabledExtensionCount = extCount;
	desc.ppEnabledExtensionNames = exts;
#ifndef NDEBUG
	desc.enabledLayerCount = (uint32_t)(sizeof(validationLayers) / sizeof(validationLayers[0]));
	desc.ppEnabledLayerNames = &validationLayers[0];
#endif
	VK_CHECK(vkCreateInstance(&desc, NULL, &vk.instance), "failed to create Instance!");
}


/*
** VK_CreateSurface
**
** win:		(HINSTANCE, HWND)
** macOS:	(NSView, NULL)
** linux:	(Display*, Window)
*/
static void VK_CreateSurface(void* p1, void* p2) {
#ifdef WIN32
	VkWin32SurfaceCreateInfoKHR desc = {0};
	desc.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.hinstance = p1;
	desc.hwnd = p2;
	VK_CHECK(vkCreateWin32SurfaceKHR(vk.instance, &desc, NULL, &vk.surface), "failed to create Win32 Surface!");
#elif defined(__APPLE__)
    VkMacOSSurfaceCreateInfoMVK desc = {0};
    desc.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.pView = p1;
    VK_CHECK(vkCreateMacOSSurfaceMVK(vk.instance, &desc, NULL, &vk.surface), "failed to create MacOS Surface!");
#elif defined( __linux__ )
	if ( VK_UsingWayland ) {
		VkWaylandSurfaceCreateInfoKHR desc = {0};
		desc.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.display = (struct wl_display*)VK_WaylandDisplay;
		desc.surface = (struct wl_surface*)VK_WaylandSurface;
		VK_CHECK(vkCreateWaylandSurfaceKHR(vk.instance, &desc, NULL, &vk.surface), "failed to create Wayland Surface!");
	} else {
		VkXlibSurfaceCreateInfoKHR desc = {0};
		desc.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
		desc.pNext = NULL;
		desc.flags = 0;
		desc.dpy = (Display*)p1;
		desc.window = (Window)(uintptr_t)p2;
		VK_CHECK(vkCreateXlibSurfaceKHR(vk.instance, &desc, NULL, &vk.surface), "failed to create Xlib Surface!");
	}
#endif
}

// Total device-local VRAM. Not a member of VkPhysicalDeviceProperties, and some
// drivers split it across several heaps, so sum every heap flagged device-local.
static uint64_t VK_DeviceLocalMemorySize( VkPhysicalDevice device )
{
	VkPhysicalDeviceMemoryProperties mem;
	vkGetPhysicalDeviceMemoryProperties(device, &mem);

	uint64_t total = 0;
	for (uint32_t i = 0; i < mem.memoryHeapCount; i++) {
		if (mem.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
			total += mem.memoryHeaps[i].size;
		}
	}
	return total;
}

// How good a device is for driving graphics + presentation. Vendor neutral on
// purpose: Vulkan's own deviceType is the signal, so an Intel Arc dGPU, an AMD
// dGPU and an NVIDIA dGPU all get identical treatment and no vendor is special
// cased. Device-local memory breaks ties, because how much VRAM is left over
// for acceleration structures is what actually decides whether the raytracing
// path fits.
static int VK_DeviceRank( VkPhysicalDevice device, const VkPhysicalDeviceProperties *props )
{
	int typeRank;
	switch ( props->deviceType ) {
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   typeRank = 3; break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: typeRank = 2; break;
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    typeRank = 1; break;
		default:                                     typeRank = 0; break;	// CPU / unknown
	}

	// Bucketed rather than compared raw: integrated parts commonly report 0 or a
	// shared-memory figure, and all we need is to separate tiny from roomy.
	uint64_t mem = VK_DeviceLocalMemorySize(device);
	int memRank = ( mem > ( 1ull << 32 ) ) ? 2 : ( mem > 0 ? 1 : 0 );

	return typeRank * 4 + memRank;
}

static void VK_PickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(vk.instance, &deviceCount, NULL);
	if (deviceCount == 0) {
		ri.Error(ERR_FATAL, "Vulkan: failed to find GPUs with Vulkan support!");
	}

	// Sized from the enumeration instead of a fixed 10: with a few software ICDs
	// installed alongside real hardware the loader can legitimately report more.
	VkPhysicalDevice *devices = (VkPhysicalDevice *)malloc(deviceCount * sizeof(VkPhysicalDevice));
	if (devices == NULL) {
		ri.Error(ERR_FATAL, "Vulkan: out of memory enumerating GPUs!");
	}
	if (vkEnumeratePhysicalDevices(vk.instance, &deviceCount, devices) != VK_SUCCESS) {
		free(devices);
		ri.Error(ERR_FATAL, "Vulkan: failed to enumerate GPUs!");
	}

	// Keep the two best devices that can actually present to our surface. Any
	// further device is ignored - including software rasterisers, which the
	// loader happily reports alongside real hardware.
	VkPhysicalDevice best = VK_NULL_HANDLE, next = VK_NULL_HANDLE;
	int bestRank = -1, nextRank = -1;

	for (uint32_t i = 0; i < deviceCount; i++) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(devices[i], &props);

		if (!VK_IsDeviceSuitable(devices[i], vk.surface)) {
			continue;
		}

		int rank = VK_DeviceRank(devices[i], &props);
		if (rank > bestRank) {
			next = best; nextRank = bestRank;	// old winner slides into the compute slot
			best = devices[i]; bestRank = rank;
		} else if (rank > nextRank) {
			next = devices[i]; nextRank = rank;
		}
	}

	free(devices);

	// The second GPU only serves raytracing compute, so there is no point
	// enumerating for one when RT is off.
	if (!VK_RayTracingActive()) {
		next = VK_NULL_HANDLE;
	}

	// Never hand raytracing a CPU device - a software Vulkan target is far slower
	// than simply staying single-GPU.
	if (next != VK_NULL_HANDLE) {
		VkPhysicalDeviceProperties np;
		vkGetPhysicalDeviceProperties(next, &np);
		if (np.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
			next = VK_NULL_HANDLE;
		}
	}

	vk.physicalDevice = best;
	vk.secondaryPhysicalDevice = next;

	if (vk.physicalDevice != VK_NULL_HANDLE) {
		// VK_IsDeviceSuitable() caches the queue family indices of whichever
		// device it last accepted, which during the loop above is not necessarily
		// the one we settled on. Re-query so the indices belong to the device we
		// are about to create a logical device on.
		VK_IsDeviceSuitable(vk.physicalDevice, vk.surface);
	}

	if (vk.physicalDevice != VK_NULL_HANDLE) {
		VkPhysicalDeviceProperties chosen;
		vkGetPhysicalDeviceProperties(vk.physicalDevice, &chosen);
		if (vk.secondaryPhysicalDevice != VK_NULL_HANDLE) {
			VkPhysicalDeviceProperties second;
			vkGetPhysicalDeviceProperties(vk.secondaryPhysicalDevice, &second);
			ri.Printf(PRINT_ALL, "Vulkan: graphics on %s, raytracing compute on %s\n",
				chosen.deviceName, second.deviceName);
		} else {
			ri.Printf(PRINT_ALL, "Vulkan: graphics on %s (no second GPU for compute)\n",
				chosen.deviceName);
		}
	}

	if (vk.physicalDevice == VK_NULL_HANDLE) {
#if defined( _WIN32 )
		MessageBoxEx(NULL, "Vulkan: failed to find a suitable GPU!",
			"Error", MB_OK | MB_ICONEXCLAMATION | MB_DEFBUTTON2 | MB_TOPMOST | MB_SETFOREGROUND,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT));
#endif
		ri.Error(ERR_FATAL, "Vulkan: failed to find a suitable GPU!");
	}

	{
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(vk.physicalDevice, NULL, &extensionCount, NULL);
		VkExtensionProperties *extensions = malloc(extensionCount * sizeof(VkExtensionProperties));
		vkEnumerateDeviceExtensionProperties(vk.physicalDevice, NULL, &extensionCount, &extensions[0]);
		//std::cout << "LogicalDeviceExtensions..." << std::endl;

		//for (const auto& extension : extensions) {
		//	std::cout << "\t" << extension.extensionName << std::endl;
		//}
		free(extensions);
	}
	// device properties
	vkGetPhysicalDeviceProperties(vk.physicalDevice, &vk.deviceProperties);

	// final enabled extension set (required + available optional)
	VK_FillEnabledDeviceExtensions(vk.physicalDevice);

	// rtx properties (KHR)
	vk.accelProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
	vk.accelProperties.pNext = NULL;
	vk.amdCoreProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CORE_PROPERTIES_2_AMD;
	vk.amdCoreProperties.pNext = NULL;
	vk.rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	vk.rayTracingProperties.pNext = NULL;
	vk.deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	vk.deviceProperties2.pNext = &vk.amdCoreProperties;

	// Querying the RT property structs for a device without the RT extensions is
	// invalid usage, so leave them out of the chain when RT is off.
	if (VK_RayTracingActive()) {
		vk.amdCoreProperties.pNext = &vk.accelProperties;
		vk.accelProperties.pNext = &vk.rayTracingProperties;
	}

	vkGetPhysicalDeviceProperties2(vk.physicalDevice, &vk.deviceProperties2);
	vk.amdComputeUnits = vk.amdCoreProperties.activeComputeUnitCount;

	// device limits
	VkPhysicalDeviceLimits limits = vk.deviceProperties.limits;

	//std::cout << "GPU in use..." << std::endl;
	//std::cout << "\t" << devProperties.deviceName << std::endl;

}

static void VK_CreateLogicalDevice()
{
	VkDeviceQueueCreateInfo queueCreateInfos[2] = {0};

	// if graphic and present indices are the same, only one queue is needed
	uint32_t queueCreateInfosCount = vk.queryFamilyIndices.graphicsFamily == vk.queryFamilyIndices.presentFamily ? 1 : 2;
	float queuePriority = 1.0f;
	queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfos[0].queueFamilyIndex = vk.queryFamilyIndices.graphicsFamily;
	queueCreateInfos[0].queueCount = 1;
	queueCreateInfos[0].pQueuePriorities = &queuePriority;

	if (vk.queryFamilyIndices.graphicsFamily != vk.queryFamilyIndices.presentFamily) {
		queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[1].queueFamilyIndex = vk.queryFamilyIndices.presentFamily;
		queueCreateInfos[1].queueCount = 1;
		queueCreateInfos[1].pQueuePriorities = &queuePriority;
	}

	// activate features (1.0 bits gated on actual device support)
	VkPhysicalDeviceFeatures supportedFeatures = { 0 };
	vkGetPhysicalDeviceFeatures(vk.physicalDevice, &supportedFeatures);
	vk.anisotropy = supportedFeatures.samplerAnisotropy;
// Build feature chain for device creation (reusable for primary and secondary)
	VkPhysicalDeviceFeatures deviceFeatures = { 0 };
	deviceFeatures.fillModeNonSolid = qtrue;
	deviceFeatures.multiDrawIndirect = qfalse;
	deviceFeatures.drawIndirectFirstInstance = qfalse;
	deviceFeatures.shaderClipDistance = qtrue;
	deviceFeatures.samplerAnisotropy = vk.anisotropy;

	VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeatures = { 0 };
	indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
	indexingFeatures.runtimeDescriptorArray = qtrue;
	indexingFeatures.descriptorBindingVariableDescriptorCount = qtrue;
	indexingFeatures.descriptorBindingPartiallyBound = qtrue;

	VkPhysicalDeviceVulkan12Features vulkan12Features = { 0 };
	vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	vulkan12Features.bufferDeviceAddress = qtrue;
	vulkan12Features.pNext = &indexingFeatures;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = { 0 };
	accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	accelerationStructureFeatures.accelerationStructure = qtrue;
	accelerationStructureFeatures.pNext = &vulkan12Features;

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = { 0 };
	rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	rayTracingPipelineFeatures.rayTracingPipeline = qtrue;
	rayTracingPipelineFeatures.pNext = &accelerationStructureFeatures;
	// Present-id/wait features also chain onward, so splice the RT feature
	// structs in only when RT is on. Naming a feature struct for an extension the
	// device lacks is invalid usage and fails vkCreateDevice.
	VkPhysicalDevicePresentIdFeaturesKHR presentIdFeatures = { 0 };
	presentIdFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR;
	presentIdFeatures.presentId = vk.presentId;
	presentIdFeatures.pNext = VK_RayTracingActive() ? (VkPhysicalDeviceFeatures2 *)&rayTracingPipelineFeatures
	                                                : (VkPhysicalDeviceFeatures2 *)&vulkan12Features;

	VkPhysicalDevicePresentWaitFeaturesKHR presentWaitFeatures = { 0 };
	presentWaitFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR;
	presentWaitFeatures.presentWait = vk.presentWait;
	presentWaitFeatures.pNext = &presentIdFeatures;

	VkPhysicalDeviceFeatures2 device_features = { 0 };
	device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
	device_features.pNext = &presentWaitFeatures;

	VkDeviceCreateInfo primaryDesc = { 0 };
	primaryDesc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	primaryDesc.pNext = &device_features;
	primaryDesc.queueCreateInfoCount = queueCreateInfosCount;
	primaryDesc.pQueueCreateInfos = &queueCreateInfos[0];
	primaryDesc.pEnabledFeatures = &deviceFeatures;
	primaryDesc.enabledExtensionCount = enabledDeviceExtensionCount;
	primaryDesc.ppEnabledExtensionNames = &enabledDeviceExtensions[0];

	VK_CHECK(vkCreateDevice(vk.physicalDevice, &primaryDesc, NULL, &vk.device), "failed to create logical device!")

	// Create a second logical device for raytracing compute, if a second
	// suitable GPU was found. Any vendor will do.
	if (vk.secondaryPhysicalDevice != VK_NULL_HANDLE) {
		
		// Find compute queue family on secondary device
		uint32_t secQueueCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(vk.secondaryPhysicalDevice, &secQueueCount, NULL);
		VkQueueFamilyProperties* secQueueProps = malloc(secQueueCount * sizeof(VkQueueFamilyProperties));
		vkGetPhysicalDeviceQueueFamilyProperties(vk.secondaryPhysicalDevice, &secQueueCount, secQueueProps);
		
		int secComputeFamily = -1;
		for (uint32_t i = 0; i < secQueueCount; i++) {
			if ((secQueueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && !(secQueueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
				secComputeFamily = i;
				break;
			}
			// Fallback: any compute queue
			if (secComputeFamily == -1 && (secQueueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
				secComputeFamily = i;
			}
		}
		free(secQueueProps);
		
		if (secComputeFamily >= 0) {
			VkDeviceQueueCreateInfo secQueueInfo = {0};
			secQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			secQueueInfo.queueFamilyIndex = secComputeFamily;
			secQueueInfo.queueCount = 1;
			float queuePriority = 1.0f;
			secQueueInfo.pQueuePriorities = &queuePriority;
			
			// Reuse the same feature chain as primary device
			VkDeviceCreateInfo secDesc = {0};
			secDesc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			secDesc.pNext = &device_features;  // reuse the same feature chain
			secDesc.queueCreateInfoCount = 1;
			secDesc.pQueueCreateInfos = &secQueueInfo;
			secDesc.pEnabledFeatures = NULL;
			secDesc.enabledExtensionCount = enabledDeviceExtensionCount;
			secDesc.ppEnabledExtensionNames = &enabledDeviceExtensions[0];
			
			VkResult res = vkCreateDevice(vk.secondaryPhysicalDevice, &secDesc, NULL, &vk.secondaryDevice);
			if (res == VK_SUCCESS) {
				vk.secondaryComputeFamily = secComputeFamily;
				vkGetDeviceQueue(vk.secondaryDevice, secComputeFamily, 0, &vk.secondaryComputeQueue);
				vkGetPhysicalDeviceProperties(vk.secondaryPhysicalDevice, &vk.secondaryDeviceProperties);
				vk.multiGPUEnabled = qtrue;
				ri.Printf(PRINT_ALL, "Multi-GPU: Secondary device %s (compute queue family %d) enabled\n", 
					vk.secondaryDeviceProperties.deviceName, secComputeFamily);
			} else {
				ri.Printf(PRINT_WARNING, "Failed to create secondary device: %d\n", res);
			}
		}
	}

}
static void VK_CreateCommandPool() {
	vkGetDeviceQueue(vk.device, vk.queryFamilyIndices.graphicsFamily, 0, &vk.graphicsQueue);
	vkGetDeviceQueue(vk.device, vk.queryFamilyIndices.presentFamily, 0, &vk.presentQueue);

	VkCommandPoolCreateInfo poolInfo = { 0 };
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = vk.queryFamilyIndices.graphicsFamily;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;//VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	VK_CHECK(vkCreateCommandPool(vk.device, &poolInfo, NULL, &vk.commandPool), "failed to create command pool!");
}

static void VK_SetupQueryPool() {
	VkQueryPoolCreateInfo createInfo = {
		VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
		NULL,
		0,
		VK_QUERY_TYPE_TIMESTAMP,
		VK_MAX_SWAPCHAIN_SIZE * PROFILER_IN_FLIGHT,
		0
	};
	VK_CHECK(vkCreateQueryPool(vk.device, &createInfo, NULL, &vk.queryPool), "failed to create queryPool!");
}


/*
==============================================================================

Vulkan Debug Function

==============================================================================
*/
static qboolean VK_CheckValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, NULL);
	VkLayerProperties* availableLayers = malloc(layerCount * sizeof(VkLayerProperties));
	vkEnumerateInstanceLayerProperties(&layerCount, &availableLayers[0]);

	for (int i = 0; i < (sizeof(validationLayers) / sizeof(validationLayers[0])); i++) {
		qboolean layerFound = qfalse;
		for (int j = 0; j < layerCount; j++) {
			if (!strcmp(availableLayers[j].layerName, validationLayers[i])) {
				layerFound = qtrue;
				break;
			}
		}
		if (!layerFound) {
			return qfalse;
		}
	}

	free(availableLayers);
	return qtrue;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL VK_DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	ri.Printf(PRINT_WARNING, "Vulkan Validation Layer: %s\n", pCallbackData->pMessage);

	if (pCallbackData->cmdBufLabelCount)
	{
		ri.Printf(PRINT_WARNING, "~~~");
		for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; ++i)
		{
			VkDebugUtilsLabelEXT* label = &pCallbackData->pCmdBufLabels[i];
			ri.Printf(PRINT_WARNING, "%s ~", label->pLabelName);
		}
		ri.Printf(PRINT_WARNING, "\n");
	}

	if (pCallbackData->objectCount)
	{
		for (uint32_t i = 0; i < pCallbackData->objectCount; ++i)
		{
			VkDebugUtilsObjectNameInfoEXT* obj = &pCallbackData->pObjects[i];
			ri.Printf(PRINT_WARNING, "--- %s %i\n", obj->pObjectName, (int32_t)obj->objectType);\
		}
	}

	return VK_FALSE;
}

void VK_SetupDebugCallback() {
	VkDebugUtilsMessengerCreateInfoEXT createInfo = { 0 };
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = &VK_DebugCallback;

#ifndef NDEBUG
	VK_CHECK(vkCreateDebugUtilsMessengerEXT(vk.instance, &createInfo, NULL, &vk.callback), "failed to set up debug callback!");
#endif
}

/*
==============================================================================

Vulkan Helper Function (Query Functions etc)

==============================================================================
*/

// -- Device Query -- Start
static vkqueueFamilyIndices_t VK_FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {

	vkqueueFamilyIndices_t indices;
	indices.graphicsFamily = -1;
	indices.presentFamily = -1;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);
	VkQueueFamilyProperties *queueFamilies = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, &queueFamilies[0]);

	int i = 0;
	for (int j = 0; j < queueFamilyCount; j++) {
		VkQueueFamilyProperties queueFamily = queueFamilies[j];
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}

		VkBool32 presentSupport = qfalse;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		if (queueFamily.queueCount > 0 && presentSupport) {
			indices.presentFamily = i;
		}

		if (indices.graphicsFamily >= 0 &&
			indices.presentFamily  >= 0) {
			break;
		}

		i++;
	}

	free(queueFamilies);
	return indices;
}

static qboolean VK_HasDeviceExtension(VkPhysicalDevice device, const char* name) {
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);
	VkExtensionProperties* availableExtensions = malloc(extensionCount * sizeof(VkExtensionProperties));
	vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, &availableExtensions[0]);
	qboolean supported = qfalse;
	for (uint32_t j = 0; j < extensionCount; j++) {
		if (!strcmp(availableExtensions[j].extensionName, name)) {
			supported = qtrue;
			break;
		}
	}
	free(availableExtensions);
	return supported;
}

static qboolean VK_CheckDeviceExtensionSupport(VkPhysicalDevice device) {
	for (int i = 0; i < (int)(sizeof(requiredDeviceExtensions) / sizeof(requiredDeviceExtensions[0])); i++) {
		if (!VK_HasDeviceExtension(device, requiredDeviceExtensions[i])) {
			return qfalse;
		}
	}

	// RT extensions are only a requirement when the RT renderer is in use.
	if (VK_RayTracingActive()) {
		for (int i = 0; i < (int)(sizeof(rayTracingDeviceExtensions) / sizeof(rayTracingDeviceExtensions[0])); i++) {
			if (!VK_HasDeviceExtension(device, rayTracingDeviceExtensions[i])) {
				return qfalse;
			}
		}
	}
	return qtrue;
}

// Fill the final enabled extension list for the picked device: everything
// required plus whichever optional extensions the driver offers.
static void VK_FillEnabledDeviceExtensions(VkPhysicalDevice device) {
	enabledDeviceExtensionCount = 0;
	for (int i = 0; i < (int)(sizeof(requiredDeviceExtensions) / sizeof(requiredDeviceExtensions[0])); i++) {
		enabledDeviceExtensions[enabledDeviceExtensionCount++] = requiredDeviceExtensions[i];
	}
	// Only enable RT extensions when RT is in use - enabling an extension the
	// device does not support makes vkCreateDevice fail.
	if (VK_RayTracingActive()) {
		for (int i = 0; i < (int)(sizeof(rayTracingDeviceExtensions) / sizeof(rayTracingDeviceExtensions[0])); i++) {
			enabledDeviceExtensions[enabledDeviceExtensionCount++] = rayTracingDeviceExtensions[i];
		}
	}
	vk.rtMaintenance1 = qfalse;
	vk.rtPositionFetch = qfalse;
	vk.swapchainMaintenance1 = qfalse;
	vk.presentId = qfalse;
	vk.presentWait = qfalse;
	for (int i = 0; i < (int)(sizeof(optionalDeviceExtensions) / sizeof(optionalDeviceExtensions[0])); i++) {
		if (enabledDeviceExtensionCount >= VK_MAX_ENABLED_DEVICE_EXTENSIONS) {
			break;
		}
		if (VK_HasDeviceExtension(device, optionalDeviceExtensions[i])) {
			enabledDeviceExtensions[enabledDeviceExtensionCount++] = optionalDeviceExtensions[i];
			if (!strcmp(optionalDeviceExtensions[i], VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME)) {
				vk.rtMaintenance1 = qtrue;
			}
			if (!strcmp(optionalDeviceExtensions[i], VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME)) {
				vk.rtPositionFetch = qtrue;
			}
			if (!strcmp(optionalDeviceExtensions[i], VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)) {
				vk.swapchainMaintenance1 = qtrue;
			}
			if (!strcmp(optionalDeviceExtensions[i], VK_KHR_PRESENT_ID_EXTENSION_NAME)) {
				vk.presentId = qtrue;
			}
			if (!strcmp(optionalDeviceExtensions[i], VK_KHR_PRESENT_WAIT_EXTENSION_NAME)) {
				vk.presentWait = qtrue;
			}
			if (!strcmp(optionalDeviceExtensions[i], VK_AMD_ANTI_LAG_EXTENSION_NAME)) {
				vk.antiLag = qtrue;
			}
			if (!strcmp(optionalDeviceExtensions[i], VK_NV_LOW_LATENCY_2_EXTENSION_NAME)) {
				vk.reflex = qtrue;
			}
			if (!strcmp(optionalDeviceExtensions[i], VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME)) {
				vk.diagnosticCheckpoints = qtrue;
			}
			ri.Printf(PRINT_ALL, "...enabling optional device extension %s\n", optionalDeviceExtensions[i]);
		}
	}
}

qboolean VK_IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
	vkqueueFamilyIndices_t queueFamilyIndices = VK_FindQueueFamilies(device, surface);

	qboolean extensionsSupported = VK_CheckDeviceExtensionSupport(device);

	qboolean swapChainAdequate = qfalse;
	if (extensionsSupported) {
		swapChainSupportDetails_t swapChainSupport = querySwapChainSupport(device, surface);
		swapChainAdequate = swapChainSupport.formatCount && swapChainSupport.presentModeCount;
	}

	if (queueFamilyIndices.graphicsFamily >= 0 &&
		queueFamilyIndices.presentFamily >= 0 &&
		extensionsSupported &&
		swapChainAdequate)
	{
		Com_Memcpy(&vk.queryFamilyIndices, &queueFamilyIndices, sizeof(vkqueueFamilyIndices_t));
		return qtrue;
	}
	else return qfalse;

	return	vk.queryFamilyIndices.graphicsFamily >= 0 &&
		vk.queryFamilyIndices.presentFamily >= 0 && extensionsSupported && swapChainAdequate;;
}
// -- Device Query -- End


swapChainSupportDetails_t querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
	swapChainSupportDetails_t details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formatCount, NULL);

	if (details.formatCount != 0) {
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formatCount, &details.formats[0]);
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.presentModeCount, NULL);

	if (details.presentModeCount != 0) {
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.presentModeCount, &details.presentModes[0]);
	}

	return details;
}

char* VK_ErrorString(VkResult errorCode)
{
	switch (errorCode)
	{
#define STR(r) case VK_ ##r: return #r
		STR(SUCCESS);
		STR(NOT_READY);
		STR(TIMEOUT);
		STR(EVENT_SET);
		STR(EVENT_RESET);
		STR(INCOMPLETE);
		STR(ERROR_OUT_OF_HOST_MEMORY);
		STR(ERROR_OUT_OF_DEVICE_MEMORY);
		STR(ERROR_INITIALIZATION_FAILED);
		STR(ERROR_DEVICE_LOST);
		STR(ERROR_MEMORY_MAP_FAILED);
		STR(ERROR_LAYER_NOT_PRESENT);
		STR(ERROR_EXTENSION_NOT_PRESENT);
		STR(ERROR_FEATURE_NOT_PRESENT);
		STR(ERROR_INCOMPATIBLE_DRIVER);
		STR(ERROR_TOO_MANY_OBJECTS);
		STR(ERROR_FORMAT_NOT_SUPPORTED);
		STR(ERROR_FRAGMENTED_POOL);
		STR(ERROR_OUT_OF_POOL_MEMORY);
		STR(ERROR_INVALID_EXTERNAL_HANDLE);
		STR(ERROR_SURFACE_LOST_KHR);
		STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
		STR(SUBOPTIMAL_KHR);
		STR(ERROR_OUT_OF_DATE_KHR);
		STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
		STR(ERROR_VALIDATION_FAILED_EXT);
		STR(ERROR_INVALID_SHADER_NV);
		STR(ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
		STR(ERROR_FRAGMENTATION_EXT);
		STR(ERROR_NOT_PERMITTED_EXT);
		STR(ERROR_INVALID_DEVICE_ADDRESS_EXT);
		STR(ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
#undef STR
	default:
		return "UNKNOWN_ERROR";
	}
}



