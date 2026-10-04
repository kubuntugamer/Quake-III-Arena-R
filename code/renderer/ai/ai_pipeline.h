/*
===========================================================================
AI Pipeline for Quake III Arena Vulkan Renderer
===========================================================================
*/
#ifndef __AI_PIPELINE_H
#define __AI_PIPELINE_H

#include "../tr_local.h"

// AI Pipeline types
typedef enum {
    AI_PIPELINE_DENOISER = 0,
    AI_PIPELINE_TSR,
    AI_PIPELINE_AS_PREDICTOR,
    AI_PIPELINE_MATERIAL,
    AI_PIPELINE_COUNT
} aiPipelineType_t;

// AI Pipeline state
typedef struct {
    VkPipeline        pipeline;
    VkPipelineLayout  layout;
    VkDescriptorSetLayout descriptorLayout;
    VkDescriptorSet   descriptorSet;
    VkBuffer          weightBuffer;
    VkDeviceMemory    weightMemory;
    uint32_t          weightSize;
    qboolean          initialized;
} aiPipeline_t;

// Global AI state
extern aiPipeline_t aiPipelines[AI_PIPELINE_COUNT];
extern qboolean aiEnabled;

// CVars
extern cvar_t *rt_aiDenoiser;
extern cvar_t *rt_aiTSR;
extern cvar_t *rt_aiASPredict;
extern cvar_t *rt_aiMaterial;

// Function declarations
void AI_InitPipelines();
void AI_DestroyPipelines();
void AI_LoadWeights();
void AI_DispatchDenoiser(VkCommandBuffer cmdBuf, uint32_t frameIndex);
void AI_DispatchTSR(VkCommandBuffer cmdBuf, uint32_t frameIndex);
void AI_DispatchASPredictor(VkCommandBuffer cmdBuf, uint32_t frameIndex);
void AI_DispatchMaterial(VkCommandBuffer cmdBuf, uint32_t frameIndex);
void AI_SetupDescriptors(uint32_t frameIndex);

#endif // __AI_PIPELINE_H