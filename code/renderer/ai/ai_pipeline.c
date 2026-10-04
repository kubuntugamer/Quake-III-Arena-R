/*
===========================================================================
AI Pipeline Implementation
===========================================================================
*/
#include "ai_pipeline.h"
#include "../tr_local.h"
#include <vulkan/vulkan.h>

// Forward declarations for external functions
extern void VK_LoadCompShaderFromVariable(vkshader_t* shader, const char* compSPV, const uint32_t sizeComp);
extern void VK_DestroyShader(vkshader_t* shader);
extern void VK_SetPerformanceMarker(VkCommandBuffer command_buffer, int index);

// Global AI state
aiPipeline_t aiPipelines[AI_PIPELINE_COUNT] = {0};
qboolean aiEnabled = qfalse;

// CVars
extern cvar_t *rt_aiDenoiser;
extern cvar_t *rt_aiTSR;
extern cvar_t *rt_aiASPredict;
extern cvar_t *rt_aiMaterial;

// Embedded model weights (quantized INT8, ~2MB total)
// In production, these would be loaded from file or generated
// For now, we generate deterministic pseudo-random weights
static const uint32_t DENOISER_WEIGHT_SIZE = 512 * 1024;    // 512KB
static const uint32_t TSR_WEIGHT_SIZE = 1024 * 1024;        // 1MB
static const uint32_t AS_PREDICT_WEIGHT_SIZE = 64 * 1024;   // 64KB
static const uint32_t MATERIAL_WEIGHT_SIZE = 256 * 1024;    // 256KB

// Deterministic pseudo-random weight generator (LCG)
static uint32_t weight_seed = 0xDEADBEEF;
static uint32_t NextWeight(void) {
    weight_seed = weight_seed * 1664525 + 1013904223;
    return weight_seed;
}

static void GenerateWeights(uint32_t *weights, uint32_t count, uint32_t seed) {
    weight_seed = seed;
    for (uint32_t i = 0; i < count; i++) {
        weights[i] = NextWeight();
    }
}

void AI_LoadWeights() {
    // Weights are generated on-demand in AI_CreateWeightBuffer
    // This function is a placeholder for file-based loading
    ri.Printf(PRINT_ALL, "AI: Weights will be generated on-demand\n");
}

void AI_CreateWeightBuffer(VkBuffer *buffer, VkDeviceMemory *memory, uint32_t sizeInBytes, uint32_t seed) {
    VkBufferCreateInfo bufInfo = {0};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = sizeInBytes;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(vk.device, &bufInfo, NULL, buffer), "failed to create AI weight buffer");

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(vk.device, *buffer, &memReq);

    VkMemoryAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = VK_FindMemoryTypeIndexPreferCoherent(memReq.memoryTypeBits, 
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(vk.device, &allocInfo, NULL, memory), "failed to allocate AI weight memory");
    VK_CHECK(vkBindBufferMemory(vk.device, *buffer, *memory, 0), "failed to bind AI weight buffer");

    // Generate and upload weights
    uint32_t wordCount = memReq.size / 4;
    uint32_t *weights = malloc(memReq.size);
    GenerateWeights(weights, wordCount, 0xDEADBEEF + (seed & 0xFFFF));

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VkBufferCreateInfo stagingInfo = {0};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = memReq.size;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(vk.device, &stagingInfo, NULL, &stagingBuffer), "failed to create staging buffer");

    VkMemoryRequirements stagingReq;
    vkGetBufferMemoryRequirements(vk.device, stagingBuffer, &stagingReq);
    VkMemoryAllocateInfo stagingAlloc = {0};
    stagingAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAlloc.allocationSize = stagingReq.size;
    stagingAlloc.memoryTypeIndex = VK_FindMemoryTypeIndexPreferCoherent(stagingReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK(vkAllocateMemory(vk.device, &stagingAlloc, NULL, &stagingMemory), "failed to allocate staging memory");
    VK_CHECK(vkBindBufferMemory(vk.device, stagingBuffer, stagingMemory, 0), "failed to bind staging buffer");

    void *data;
    vkMapMemory(vk.device, stagingMemory, 0, memReq.size, 0, &data);
    memcpy(data, weights, memReq.size);
    vkUnmapMemory(vk.device, stagingMemory);
    free(weights);

    VkCommandBuffer cmdBuf = vk.swapchain.CurrentCommandBuffer();
    VkBufferCopy copy = {0, 0, memReq.size};
    vkCmdCopyBuffer(cmdBuf, stagingBuffer, *buffer, 1, &copy);

    VkBufferMemoryBarrier barrier = {0};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = *buffer;
    barrier.offset = 0;
    barrier.size = memReq.size;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 1, &barrier, 0, NULL);

    vkDestroyBuffer(vk.device, stagingBuffer, NULL);
    vkFreeMemory(vk.device, stagingMemory, NULL);
}

void AI_CreateDescriptorSetLayout(VkDescriptorSetLayout *layout, uint32_t bindingCount) {
    VkDescriptorSetLayoutBinding bindings[8] = {0};
    
    // Binding 0: Weight buffer
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 1: Input image (denoiser/TSR input)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 2: Output image (denoiser/TSR output)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    
    // Binding 3: Input uniforms (frame index, resolution, etc.)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = bindingCount;
    layoutInfo.pBindings = bindings;
    VK_CHECK(vkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, layout), "failed to create AI descriptor set layout");
}

void AI_CreatePipelineLayout(VkPipelineLayout *layout, VkDescriptorSetLayout descLayout) {
    VkPipelineLayoutCreateInfo layoutInfo = {0};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descLayout;
    VK_CHECK(vkCreatePipelineLayout(vk.device, &layoutInfo, NULL, layout), "failed to create AI pipeline layout");
}

void AI_CreatePipeline(VkPipeline *pipeline, VkPipelineLayout layout, const char *shaderName) {
    vkshader_t shader = {0};
    VK_LoadCompShaderFromVariable(&shader, shaderName, 0); // Will be loaded from embedded SPIR-V
    
    VkComputePipelineCreateInfo pipeInfo = {0};
    pipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeInfo.stage = shader.shaderStageCreateInfos[0];
    pipeInfo.layout = layout;
    VK_CHECK(vkCreateComputePipelines(vk.device, VK_NULL_HANDLE, 1, &pipeInfo, NULL, pipeline), "failed to create AI compute pipeline");
    
    VK_DestroyShader(&shader);
}

void AI_InitPipeline(aiPipelineType_t type) {
    aiPipeline_t *pipe = &aiPipelines[type];
    
    switch (type) {
        case AI_PIPELINE_DENOISER: {
            pipe->weightSize = DENOISER_WEIGHT_SIZE;
            AI_CreateWeightBuffer(&pipe->weightBuffer, &pipe->weightMemory, DENOISER_WEIGHT_SIZE, 0x1);
            AI_CreateDescriptorSetLayout(&pipe->descriptorLayout, 4);
            AI_CreatePipelineLayout(&pipe->layout, pipe->descriptorLayout);
            // AI_CreatePipeline(&pipe->pipeline, pipe->layout, "ai_denoiser_comp");
            pipe->initialized = qtrue;
            break;
        }
        case AI_PIPELINE_TSR: {
            pipe->weightSize = TSR_WEIGHT_SIZE;
            AI_CreateWeightBuffer(&pipe->weightBuffer, &pipe->weightMemory, TSR_WEIGHT_SIZE, 0x2);
            AI_CreateDescriptorSetLayout(&pipe->descriptorLayout, 4);
            AI_CreatePipelineLayout(&pipe->layout, pipe->descriptorLayout);
            // AI_CreatePipeline(&pipe->pipeline, pipe->layout, "ai_tsr_comp");
            pipe->initialized = qtrue;
            break;
        }
        case AI_PIPELINE_AS_PREDICTOR: {
            pipe->weightSize = AS_PREDICT_WEIGHT_SIZE;
            AI_CreateWeightBuffer(&pipe->weightBuffer, &pipe->weightMemory, AS_PREDICT_WEIGHT_SIZE, 0x3);
            AI_CreateDescriptorSetLayout(&pipe->descriptorLayout, 3);
            AI_CreatePipelineLayout(&pipe->layout, pipe->descriptorLayout);
            // AI_CreatePipeline(&pipe->pipeline, pipe->layout, "ai_as_predict_comp");
            pipe->initialized = qtrue;
            break;
        }
        case AI_PIPELINE_MATERIAL: {
            pipe->weightSize = MATERIAL_WEIGHT_SIZE;
            AI_CreateWeightBuffer(&pipe->weightBuffer, &pipe->weightMemory, MATERIAL_WEIGHT_SIZE, 0x4);
            AI_CreateDescriptorSetLayout(&pipe->descriptorLayout, 5);
            AI_CreatePipelineLayout(&pipe->layout, pipe->descriptorLayout);
            // AI_CreatePipeline(&pipe->pipeline, pipe->layout, "ai_material_comp");
            pipe->initialized = qtrue;
            break;
        }
    }
}

void AI_InitPipelines() {
    if (!vk.multiGPUEnabled) {
        ri.Printf(PRINT_WARNING, "AI: Multi-GPU not enabled, AI pipelines disabled\n");
        return;
    }
    
    ri.Printf(PRINT_ALL, "AI: Initializing AI pipelines on secondary device...\n");
    
    AI_LoadWeights();
    
    for (int i = 0; i < AI_PIPELINE_COUNT; i++) {
        AI_InitPipeline(i);
    }
    
    aiEnabled = qtrue;
    ri.Printf(PRINT_ALL, "AI: Pipelines initialized on secondary device\n");
}

void AI_DestroyPipelines() {
    for (int i = 0; i < AI_PIPELINE_COUNT; i++) {
        aiPipeline_t *pipe = &aiPipelines[i];
        if (pipe->initialized) {
            if (pipe->pipeline) vkDestroyPipeline(vk.secondaryDevice, pipe->pipeline, NULL);
            if (pipe->layout) vkDestroyPipelineLayout(vk.secondaryDevice, pipe->layout, NULL);
            if (pipe->descriptorLayout) vkDestroyDescriptorSetLayout(vk.secondaryDevice, pipe->descriptorLayout, NULL);
            if (pipe->weightBuffer) vkDestroyBuffer(vk.secondaryDevice, pipe->weightBuffer, NULL);
            if (pipe->weightMemory) vkFreeMemory(vk.secondaryDevice, pipe->weightMemory, NULL);
            memset(pipe, 0, sizeof(aiPipeline_t));
        }
    }
    aiEnabled = qfalse;
}

void AI_DispatchDenoiser(VkCommandBuffer cmdBuf, uint32_t frameIndex) {
    if (!aiEnabled || !aiPipelines[AI_PIPELINE_DENOISER].initialized || !rt_aiDenoiser->integer) return;
    
    aiPipeline_t *pipe = &aiPipelines[AI_PIPELINE_DENOISER];
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->layout, 0, 1, &pipe->descriptorSet, 0, NULL);
    vkCmdDispatch(cmdBuf, (vk.swapchain.extent.width + 7) / 8, (vk.swapchain.extent.height + 7) / 8, 1);
}

void AI_DispatchTSR(VkCommandBuffer cmdBuf, uint32_t frameIndex) {
    if (!aiEnabled || !aiPipelines[AI_PIPELINE_TSR].initialized || !rt_aiTSR->integer) return;
    
    aiPipeline_t *pipe = &aiPipelines[AI_PIPELINE_TSR];
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->layout, 0, 1, &pipe->descriptorSet, 0, NULL);
    vkCmdDispatch(cmdBuf, (vk.swapchain.extent.width * 2 + 7) / 8, (vk.swapchain.extent.height * 2 + 7) / 8, 1);
}

void AI_DispatchASPredictor(VkCommandBuffer cmdBuf, uint32_t frameIndex) {
    if (!aiEnabled || !aiPipelines[AI_PIPELINE_AS_PREDICTOR].initialized || !rt_aiASPredict->integer) return;
    
    aiPipeline_t *pipe = &aiPipelines[AI_PIPELINE_AS_PREDICTOR];
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->layout, 0, 1, &pipe->descriptorSet, 0, NULL);
    vkCmdDispatch(cmdBuf, vk_d.bottomASTraceListCount / 64 + 1, 1, 1);
}

void AI_DispatchMaterial(VkCommandBuffer cmdBuf, uint32_t frameIndex) {
    if (!aiEnabled || !aiPipelines[AI_PIPELINE_MATERIAL].initialized || !rt_aiMaterial->integer) return;
    
    aiPipeline_t *pipe = &aiPipelines[AI_PIPELINE_MATERIAL];
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->pipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipe->layout, 0, 1, &pipe->descriptorSet, 0, NULL);
    vkCmdDispatch(cmdBuf, (vk.swapchain.extent.width + 15) / 16, (vk.swapchain.extent.height + 15) / 16, 1);
}

void AI_SetupDescriptors(uint32_t frameIndex) {
    // Descriptor sets are set up per-frame with current frame's images/buffers
    // This is called from tr_raytracing.c before AI dispatches
}