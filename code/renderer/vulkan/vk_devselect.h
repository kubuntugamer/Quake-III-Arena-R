/*
===========================================================================
GPU selection policy - pure logic, deliberately free of any Vulkan types.

Device *queries* live in vk_setup.c. The decisions about which device drives
graphics and which one, if any, does raytracing compute live here, so they can
be exercised without a Vulkan loader. See tools/vkdevselect_test.c.

The rule this file exists to enforce: a GPU that cannot present is still a
valid candidate for the compute slot. On a muxless hybrid laptop the display
is owned by the discrete GPU, so the on-board iGPU has no queue family with
surface support. Gating the compute slot on present support silently dropped
those devices, which is the opposite of what they are wanted for.
===========================================================================
*/
#ifndef VK_DEVSELECT_H
#define VK_DEVSELECT_H

#include <stdint.h>

// Mirrors VkPhysicalDeviceType without including vulkan.h, so this header
// compiles in a bare test harness.
typedef enum {
    VKDEVTYPE_OTHER = 0,
    VKDEVTYPE_INTEGRATED = 1,
    VKDEVTYPE_DISCRETE = 2,
    VKDEVTYPE_VIRTUAL = 3,
    VKDEVTYPE_CPU = 4,
} vkdevtype_t;

// A candidate GPU, reduced to the facts the selection policy cares about.
// Filled in by vk_setup.c from the real VkPhysicalDevice queries.
typedef struct {
    const char *name;          // for logging only
    vkdevtype_t type;
    uint64_t deviceLocalMemory; // summed device-local heap bytes
    int hasComputeQueue;        // VK_QUEUE_COMPUTE_BIT present
    int hasGraphicsQueue;       // VK_QUEUE_GRAPHICS_BIT present
    int canPresent;             // a queue family reports surface support
    int swapchainAdequate;      // at least one surface format and present mode
    int requiredExts;           // required non-RT extensions available
    int rayTracingExts;         // VK_KHR_acceleration_structure + ray_tracing_pipeline
    int externalMemoryFd;       // VK_KHR_external_memory_fd
    int externalSemaphoreFd;    // VK_KHR_external_semaphore_fd
    int externalFenceFd;        // VK_KHR_external_fence_fd
} vkdevcand_t;

// How aggressively to use a second GPU.
typedef enum {
    VK_MULTIGPU_AUTO = 0,  // use one if the policy accepts it
    VK_MULTIGPU_OFF = 1,   // never
    VK_MULTIGPU_STRICT = 2 // only if the second device can also present
} vkmultigpu_t;

// Outcome of a selection pass. Indices are into the candidate array, or
// VK_DEVSELECT_NONE when nothing was accepted.
#define VK_DEVSELECT_NONE (-1)

typedef struct {
    int primary;    // must be able to present
    int secondary;  // compute only, may be headless
} vkdevselect_t;

// True when the candidate can drive the display.
int VK_DevCanPresent(const vkdevcand_t *c);

// True when the candidate can run raytracing compute. Deliberately does not
// consult present support: a headless iGPU is exactly the interesting case.
int VK_DevCanCompute(const vkdevcand_t *c);

// Higher is better. forCompute changes the weighting: the compute slot
// prefers raytracing parts and deliberately ignores memory buckets, because
// integrated GPUs report shared system memory and would otherwise lose to a
// discrete card that cannot do the job.
int VK_DevRank(const vkdevcand_t *c, int forCompute);

// Picks the graphics device and the optional compute device.
//
//   - the primary is the best-ranked candidate that can present
//   - the secondary is the best-ranked *other* candidate that can compute
//   - a CPU (software) device is never chosen for either slot
//
// Returns 0 on success, non-zero if no device can present.
int VK_DevSelect(const vkdevcand_t *cands, int count, vkmultigpu_t policy,
                 vkdevselect_t *out);

#endif // VK_DEVSELECT_H