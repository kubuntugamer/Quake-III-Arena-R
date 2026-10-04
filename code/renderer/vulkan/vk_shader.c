#include "../tr_local.h"

// SHADER HEADER
#include "../../../shader/header/clearAttachment.vert.h"
#include "../../../shader/header/clearAttachment.frag.h"

#include "../../../shader/header/fullscreenRect.vert.h"
#include "../../../shader/header/fullscreenRect.frag.h"

#include "../../../shader/header/texture.vert.h"
#include "../../../shader/header/texture.frag.h"

#include "../../../shader/header/asvgf_rng.comp.h"
#include "../../../shader/header/asvgf_forward.comp.h"
#include "../../../shader/header/asvgf_grad.comp.h"
#include "../../../shader/header/asvgf_grad_atrous.comp.h"
#include "../../../shader/header/asvgf_temporal.comp.h"
#include "../../../shader/header/asvgf_atrous.comp.h"
#include "../../../shader/header/asvgf_atrous_lf.comp.h"
#include "../../../shader/header/asvgf_taa.comp.h"
#include "../../../shader/header/compositing.comp.h"
#include "../../../shader/header/maxmipmap.comp.h"
#include "../../../shader/header/tonemapping.comp.h"
#include "../../../shader/header/fsr_rcas.comp.h"
#include "../../../shader/header/nrd_pack.comp.h"

// RTX
#include "../../../shader/header/primary_rays.rgen.h"

#include "../../../shader/header/reflect_rays.rgen.h"
#include "../../../shader/header/reflect_rays.rmiss.h"
#include "../../../shader/header/reflect_rays.rahit.h"
#include "../../../shader/header/reflect_rays.rchit.h"

#include "../../../shader/header/direct_illumination.rgen.h"
#include "../../../shader/header/indirect_illumination.rgen.h"

#include "../../../shader/header/rt_miss.rmiss.h"
#include "../../../shader/header/rt_closesthit.rchit.h"
#include "../../../shader/header/rt_anyhit.rahit.h"

#include "../../../shader/header/rt_shadow.rmiss.h"
#include "../../../shader/header/rt_shadow.rchit.h"
#include "../../../shader/header/rt_shadow.rahit.h"

static vkshader_t *texture;
static vkshader_t *clearAttachment;
static vkshader_t *fullscreenRect;
static vkshader_t* asvgfRngCompShader;
static vkshader_t* asvgfFwdCompShader;
static vkshader_t* asvgfGradCompShader;
static vkshader_t* asvgfGradAtrousCompShader;
static vkshader_t* asvgfTemporalCompShader;
static vkshader_t* asvgfAtrousCompShader;
static vkshader_t* asvgfAtrousLFCompShader;
static vkshader_t* asvgfTaaCompShader;
static vkshader_t* compositingCompShader;
static vkshader_t* maxmipmapCompShader;
static vkshader_t* tonemappingCompShader;
static vkshader_t* fsrRcasCompShader;
static vkshader_t* nrdPackCompShader;
// rtx
static vkshader_t* rayTracingAny;
static vkshader_t* primaryRays;
static vkshader_t* reflectRays;
static vkshader_t* directIllumination;
static vkshader_t* indirectIllumination;

void VK_CreateShaderModule(VkShaderModule *handle, const char *code, size_t size);
void VK_LoadVertFragShadersFromFile(vkshader_t *shader, const char *vertexSPV, const char *fragmentSPV);
void VK_LoadVertFragShadersFromVariable(vkshader_t* shader, const char* vertexSPV, const uint32_t sizeVert, const char* fragmentSPV, const uint32_t sizeFrag);
void VK_LoadCompShaderFromVariable(vkshader_t* shader, const char* compSPV, const uint32_t sizeComp);
void VK_LoadRayTracingShadersFromVariable(vkshader_t* shader, const char* rgenSPV, const uint32_t sizeRGEN,
	const char* rmissSPV, const uint32_t sizeRMISS,
	const char* rhitSPV, const uint32_t sizeRHIT);

void VK_SingleTextureShader(vkshader_t *shader){
    if (texture == NULL) {
		texture = malloc(sizeof(vkshader_t));
		//VK_LoadVertFragShadersFromFile(singleTexture, "../../shader/spv/singleTexture.vert.spv", "../../shader/spv/singleTexture.frag.spv");
		VK_LoadVertFragShadersFromVariable(texture, (const char *)textureVert, sizeof(textureVert), (const char *)textureFrag, sizeof(textureFrag));
    }
    Com_Memcpy(shader, texture, sizeof(vkshader_t));
}

void VK_ClearAttachmentShader(vkshader_t* shader) {
    if (clearAttachment == NULL) {
        clearAttachment = malloc(sizeof(vkshader_t));
        //VK_LoadVertFragShadersFromFile(clearAttachment, "../../shader/spv/clearAttachment.vert.spv", "../../shader/spv/clearAttachment.frag.spv");
		VK_LoadVertFragShadersFromVariable(clearAttachment, (const char *)clearAttachmentVert, sizeof(clearAttachmentVert), (const char *)clearAttachmentFrag, sizeof(clearAttachmentFrag));
    }
    Com_Memcpy(shader, clearAttachment, sizeof(vkshader_t));
}

void VK_FullscreenRectShader(vkshader_t* shader) {
	if (fullscreenRect == NULL) {
		fullscreenRect = malloc(sizeof(vkshader_t));
		//VK_LoadVertFragShadersFromFile(fullscreenRect, "../../shader/spv/fullscreenRect.vert.spv", "../../shader/spv/fullscreenRect.frag.spv");
		VK_LoadVertFragShadersFromVariable(fullscreenRect, (const char *)fullscreenRectVert, sizeof(fullscreenRectVert), (const char *)fullscreenRectFrag, sizeof(fullscreenRectFrag));
	}
	Com_Memcpy(shader, fullscreenRect, sizeof(vkshader_t));
}

void VK_AsvgfRngCompShader(vkshader_t* shader) {
	if (asvgfRngCompShader == NULL) {
		asvgfRngCompShader = malloc(sizeof(vkshader_t));
		VK_LoadCompShaderFromVariable(asvgfRngCompShader, (const char *)asvgf_rngComp, sizeof(asvgf_rngComp));
	}
	Com_Memcpy(shader, asvgfRngCompShader, sizeof(vkshader_t));
}

void VK_AsvgfFwdCompShader(vkshader_t* shader) {
	if (asvgfFwdCompShader == NULL) {
		asvgfFwdCompShader = malloc(sizeof(vkshader_t));
		VK_LoadCompShaderFromVariable(asvgfFwdCompShader, (const char *)asvgf_forwardComp, sizeof(asvgf_forwardComp));
	}
	Com_Memcpy(shader, asvgfFwdCompShader, sizeof(vkshader_t));
}
void VK_AsvgfGradCompShader(vkshader_t* shader) {
	if (asvgfGradCompShader == NULL) {
		asvgfGradCompShader = malloc(sizeof(vkshader_t));
		VK_LoadCompShaderFromVariable(asvgfGradCompShader, (const char *)asvgf_gradComp, sizeof(asvgf_gradComp));
	}
	Com_Memcpy(shader, asvgfGradCompShader, sizeof(vkshader_t));
}
void VK_AsvgfGradAtrousCompShader(vkshader_t* shader) {
	if (asvgfGradAtrousCompShader == NULL) {
		asvgfGradAtrousCompShader = malloc(sizeof(vkshader_t));
		VK_LoadCompShaderFromVariable(asvgfGradAtrousCompShader, (const char *)asvgf_grad_atrousComp, sizeof(asvgf_grad_atrousComp));
	}
	Com_Memcpy(shader, asvgfGradAtrousCompShader, sizeof(vkshader_t));
}
void VK_AsvgfTemporalCompShader(vkshader_t* shader) {
	if (asvgfTemporalCompShader == NULL) {
		asvgfTemporalCompShader = malloc(sizeof(vkshader_t));
		VK_LoadCompShaderFromVariable(asvgfTemporalCompShader, (const char *)asvgf_temporalComp, sizeof(asvgf_temporalComp));
	}
	Com_Memcpy(shader, asvgfTemporalCompShader, sizeof(vkshader_t));
}

void VK_AsvgfTaaCompShader(vkshader_t* shader) {
	if (asvgfTaaCompShader == NULL) {
		asvgfTaaCompShader = malloc(sizeof(vkshader_t));
		VK_LoadCompShaderFromVariable(asvgfTaaCompShader, (const char *)asvgf_taaComp, sizeof(asvgf_taaComp));
	}
	Com_Memcpy(shader, asvgfTaaCompShader, sizeof(vkshader_t));
}
void VK_AsvgfAtrousCompShader(vkshader_t* shader) {
	if (asvgfAtrousCompShader == NULL) {
		asvgfAtrousCompShader = malloc(sizeof(vkshader_t));
		VK_LoadCompShaderFromVariable(asvgfAtrousCompShader, (const char *)asvgf_atrousComp, sizeof(asvgf_atrousComp));
	}
	Com_Memcpy(shader, asvgfAtrousCompShader, sizeof(vkshader_t));
}

void VK_AsvgfAtrousLFCompShader(vkshader_t* shader) {
	if (asvgfAtrousLFCompShader == NULL) {
		asvgfAtrousLFCompShader = malloc(sizeof(vkshader_t));
		VK_LoadCompShaderFromVariable(asvgfAtrousLFCompShader, (const char *)asvgf_atrous_lfComp, sizeof(asvgf_atrous_lfComp));
	}
	Com_Memcpy(shader, asvgfAtrousLFCompShader, sizeof(vkshader_t));
}

void VK_CompositingCompShader(vkshader_t* shader) {
	if (compositingCompShader == NULL) {
		compositingCompShader = malloc(sizeof(vkshader_t));
		VK_LoadCompShaderFromVariable(compositingCompShader, (const char *)compositingComp, sizeof(compositingComp));
	}
	Com_Memcpy(shader, compositingCompShader, sizeof(vkshader_t));
}

void VK_MaxMipMapCompShader(vkshader_t* shader) {
	if (maxmipmapCompShader == NULL) {
		maxmipmapCompShader = malloc(sizeof(vkshader_t));
		VK_LoadCompShaderFromVariable(maxmipmapCompShader, (const char *)maxmipmapComp, sizeof(maxmipmapComp));
	}
	Com_Memcpy(shader, maxmipmapCompShader, sizeof(vkshader_t));
}

void VK_TonemappingCompShader(vkshader_t* shader) {
	if (tonemappingCompShader == NULL) {
		tonemappingCompShader = malloc(sizeof(vkshader_t));
		VK_LoadCompShaderFromVariable(tonemappingCompShader, (const char *)tonemappingComp, sizeof(tonemappingComp));
	}
	Com_Memcpy(shader, tonemappingCompShader, sizeof(vkshader_t));
}

void VK_FsrRcasCompShader(vkshader_t* shader) {
	if (fsrRcasCompShader == NULL) {
		fsrRcasCompShader = malloc(sizeof(vkshader_t));
		VK_LoadCompShaderFromVariable(fsrRcasCompShader, (const char *)fsr_rcasComp, sizeof(fsr_rcasComp));
	}
	Com_Memcpy(shader, fsrRcasCompShader, sizeof(vkshader_t));
}

void VK_NrdPackCompShader(vkshader_t* shader) {
	if (nrdPackCompShader == NULL) {
		nrdPackCompShader = malloc(sizeof(vkshader_t));
		VK_LoadCompShaderFromVariable(nrdPackCompShader, (const char *)nrd_packComp, sizeof(nrd_packComp));
	}
	Com_Memcpy(shader, nrdPackCompShader, sizeof(vkshader_t));
}

// rtx
void VK_DestroyShader(vkshader_t* shader) {
	for (int i = 0; i < shader->size; ++i) {
		vkDestroyShaderModule(vk.device, shader->modules[i], NULL);
	}
	free(shader->modules);
	free(shader->flags);
	free(shader->shaderStageCreateInfos);
	if(shader->shaderGroupCreateInfos != NULL) free(shader->shaderGroupCreateInfos);
	memset(shader, 0, sizeof(vkshader_t));
}

void VK_DestroyAllShaders() {
	if (texture != NULL) {
		VK_DestroyShader(texture);
		free(texture);
		texture = NULL;
	}
	if (clearAttachment != NULL) {
		VK_DestroyShader(clearAttachment);
		free(clearAttachment);
		clearAttachment = NULL;
	}
	if (fullscreenRect != NULL) {
		VK_DestroyShader(fullscreenRect);
		free(fullscreenRect);
		fullscreenRect = NULL;
	}
	// compute
	if (asvgfRngCompShader != NULL) {
		VK_DestroyShader(asvgfRngCompShader);
		free(asvgfRngCompShader);
		asvgfRngCompShader = NULL;
	}
	if (asvgfFwdCompShader != NULL) {
		VK_DestroyShader(asvgfFwdCompShader);
		free(asvgfFwdCompShader);
		asvgfFwdCompShader = NULL;
	}
	if (asvgfGradCompShader != NULL) {
		VK_DestroyShader(asvgfGradCompShader);
		free(asvgfGradCompShader);
		asvgfGradCompShader = NULL;
	}
	if (asvgfGradAtrousCompShader != NULL) {
		VK_DestroyShader(asvgfGradAtrousCompShader);
		free(asvgfGradAtrousCompShader);
		asvgfGradAtrousCompShader = NULL;
	}
	if (asvgfTemporalCompShader != NULL) {
		VK_DestroyShader(asvgfTemporalCompShader);
		free(asvgfTemporalCompShader);
		asvgfTemporalCompShader = NULL;
	}
	if (asvgfTaaCompShader != NULL) {
		VK_DestroyShader(asvgfTaaCompShader);
		free(asvgfTaaCompShader);
		asvgfTaaCompShader = NULL;
	}
	if (asvgfAtrousCompShader != NULL) {
		VK_DestroyShader(asvgfAtrousCompShader);
		free(asvgfAtrousCompShader);
		asvgfAtrousCompShader = NULL;
	}
	if (asvgfAtrousLFCompShader != NULL) {
		VK_DestroyShader(asvgfAtrousLFCompShader);
		free(asvgfAtrousLFCompShader);
		asvgfAtrousLFCompShader = NULL;
	}
	if (compositingCompShader != NULL) {
		VK_DestroyShader(compositingCompShader);
		free(compositingCompShader);
		compositingCompShader = NULL;
	}
	if (maxmipmapCompShader != NULL) {
		VK_DestroyShader(maxmipmapCompShader);
		free(maxmipmapCompShader);
		maxmipmapCompShader = NULL;
	}
	if (tonemappingCompShader != NULL) {
		VK_DestroyShader(tonemappingCompShader);
		free(tonemappingCompShader);
		tonemappingCompShader = NULL;
	}
	if (fsrRcasCompShader != NULL) {
		VK_DestroyShader(fsrRcasCompShader);
		free(fsrRcasCompShader);
		fsrRcasCompShader = NULL;
	}
	if (nrdPackCompShader != NULL) {
		VK_DestroyShader(nrdPackCompShader);
		free(nrdPackCompShader);
		nrdPackCompShader = NULL;
	}
	// RTX
	if (rayTracingAny != NULL) {
		VK_DestroyShader(rayTracingAny);
		free(rayTracingAny);
		rayTracingAny = NULL;
	}

	if (primaryRays != NULL) {
		VK_DestroyShader(primaryRays);
		free(primaryRays);
		primaryRays = NULL;
	}
	if (reflectRays != NULL) {
		VK_DestroyShader(reflectRays);
		free(reflectRays);
		reflectRays = NULL;
	}
	if (directIllumination != NULL) {
		VK_DestroyShader(directIllumination);
		free(directIllumination);
		directIllumination = NULL;
	}
	if (indirectIllumination != NULL) {
		VK_DestroyShader(indirectIllumination);
		free(indirectIllumination);
		indirectIllumination = NULL;
	}
}

void VK_CreateShaderModule(VkShaderModule *handle, const char *code, size_t size)
{
    VkShaderModuleCreateInfo createInfo = { 0 };
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = size;
    createInfo.pCode = (const uint32_t*)code;
    
    VK_CHECK(vkCreateShaderModule(vk.device, &createInfo, NULL, handle), "failed to create Shader Module!");
}

// Vertex and Fragment Shader
void VK_LoadVertFragShadersFromVariable(vkshader_t* shader, const char* vertexSPV, const uint32_t sizeVert, const char* fragmentSPV, const uint32_t sizeFrag) {
	shader->size = 2;
	shader->modules = malloc(shader->size * sizeof(VkShaderModule));
	shader->flags = malloc(shader->size * sizeof(VkShaderStageFlagBits));
	shader->shaderStageCreateInfos = calloc(shader->size, sizeof(VkPipelineShaderStageCreateInfo));
	shader->shaderGroupCreateInfos = NULL;

	shader->flags[0] = VK_SHADER_STAGE_VERTEX_BIT;
	shader->flags[1] = VK_SHADER_STAGE_FRAGMENT_BIT;

	VK_CreateShaderModule(&shader->modules[0], vertexSPV, sizeVert);
	VK_CreateShaderModule(&shader->modules[1], fragmentSPV, sizeFrag);

	shader->shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shader->shaderStageCreateInfos[0].module = shader->modules[0];
	shader->shaderStageCreateInfos[0].pName = "main";

	shader->shaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shader->shaderStageCreateInfos[1].module = shader->modules[1];
	shader->shaderStageCreateInfos[1].pName = "main";
}

void VK_LoadCompShaderFromVariable(vkshader_t* shader, const char* compSPV, const uint32_t sizeComp) {
	shader->size = 1;
	shader->modules = malloc(shader->size * sizeof(VkShaderModule));
	shader->flags = malloc(shader->size * sizeof(VkShaderStageFlagBits));
	shader->shaderStageCreateInfos = calloc(shader->size, sizeof(VkPipelineShaderStageCreateInfo));
	shader->shaderGroupCreateInfos = NULL;

	shader->flags[0] = VK_SHADER_STAGE_COMPUTE_BIT;

	VK_CreateShaderModule(&shader->modules[0], compSPV, sizeComp);

	shader->shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[0].stage = VK_SHADER_STAGE_COMPUTE_BIT;
	shader->shaderStageCreateInfos[0].module = shader->modules[0];
	shader->shaderStageCreateInfos[0].pName = "main";
}

void VK_LoadVertFragShadersFromFile(vkshader_t *shader, const char *vertexSPV, const char *fragmentSPV){
    shader->size = 2;
    shader->modules = malloc(shader->size * sizeof(VkShaderModule));
    shader->flags = malloc(shader->size * sizeof(VkShaderStageFlagBits));
    shader->shaderStageCreateInfos = calloc(shader->size, sizeof(VkPipelineShaderStageCreateInfo));
    
    shader->flags[0] = VK_SHADER_STAGE_VERTEX_BIT;
    shader->flags[1] = VK_SHADER_STAGE_FRAGMENT_BIT;
    
    unsigned char vertSPV[2000] = {0};
    unsigned char fragSPV[2000] = {0};
    size_t vertSize;
    size_t fragSize;
    
    FILE *fp;
    fp = fopen(vertexSPV, "rb");
    if(!fp) ri.Error(ERR_FATAL, "Vulkan: could not open Shader %s", vertexSPV);
    
    fseek (fp, 0, SEEK_END);
	vertSize = ftell(fp);
    fseek (fp, 0, SEEK_SET);
    fread(vertSPV,sizeof(vertSPV),1,fp);
    fclose(fp);
    
    
    fp = fopen(fragmentSPV, "rb");
    if(!fp) ri.Error(ERR_FATAL, "Vulkan: could not open Shader %s", fragmentSPV);
    
    fseek (fp, 0, SEEK_END);
	fragSize = ftell(fp);
    fseek (fp, 0, SEEK_SET);
    fread(fragSPV,sizeof(fragSPV),1,fp);
    fclose(fp);
    
    VK_CreateShaderModule(&shader->modules[0], vertSPV, vertSize);
    VK_CreateShaderModule(&shader->modules[1], fragSPV, fragSize);
    
    shader->shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader->shaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader->shaderStageCreateInfos[0].module = shader->modules[0];
    shader->shaderStageCreateInfos[0].pName = "main";
    
    shader->shaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader->shaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader->shaderStageCreateInfos[1].module = shader->modules[1];
    shader->shaderStageCreateInfos[1].pName = "main";
}

// RTX
void VK_LoadRayTracingShadersFromVariable(vkshader_t* shader, const char* rgenSPV, const uint32_t sizeRGEN, 
																const char* rmissSPV, const uint32_t sizeRMISS,
																const char* rhitSPV, const uint32_t sizeRHIT) {
	shader->size = 3;
	shader->modules = calloc(shader->size, sizeof(VkShaderModule));
	shader->flags = calloc(shader->size, sizeof(VkShaderStageFlagBits));
	shader->shaderStageCreateInfos = calloc(shader->size, sizeof(VkPipelineShaderStageCreateInfo));
	shader->shaderGroupCreateInfos = NULL;

	shader->flags[0] = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shader->flags[1] = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->flags[2] = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	VK_CreateShaderModule(&shader->modules[0], rgenSPV, sizeRGEN);
	VK_CreateShaderModule(&shader->modules[1], rmissSPV, sizeRMISS);
	VK_CreateShaderModule(&shader->modules[2], rhitSPV, sizeRHIT);

	shader->shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shader->shaderStageCreateInfos[0].module = shader->modules[0];
	shader->shaderStageCreateInfos[0].pName = "main";

	shader->shaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->shaderStageCreateInfos[1].module = shader->modules[1];
	shader->shaderStageCreateInfos[1].pName = "main";

	shader->shaderStageCreateInfos[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[2].module = shader->modules[2];
	shader->shaderStageCreateInfos[2].pName = "main";

}
void VK_LoadRayTracingShadersWithAnyFromVariable(vkshader_t* shader, const char* rgenSPV, const uint32_t sizeRGEN,
	const char* rmissSPV, const uint32_t sizeRMISS,
	const char* rhitSPV, const uint32_t sizeRHIT,
	const char* rahitSPV, const uint32_t sizeRAHIT,
	// shadow
	const char* rmissShadowSPV, const uint32_t sizeRMISSShadow,
	const char* rchitShadowSPV, const uint32_t sizeRCHITShadow,
	const char* rahitShadowSPV, const uint32_t sizeRAHITShadow) {
	shader->size = 7;
	shader->modules = calloc(shader->size, sizeof(VkShaderModule));
	shader->flags = calloc(shader->size, sizeof(VkShaderStageFlagBits));
	shader->shaderStageCreateInfos = calloc(shader->size, sizeof(VkPipelineShaderStageCreateInfo));
	shader->shaderGroupCreateInfos = NULL;

	shader->flags[0] = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shader->flags[1] = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->flags[2] = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->flags[3] = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	// SHADOW
	shader->flags[4] = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->flags[5] = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->flags[6] = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

	VK_CreateShaderModule(&shader->modules[0], rgenSPV, sizeRGEN);
	VK_CreateShaderModule(&shader->modules[1], rmissSPV, sizeRMISS);
	VK_CreateShaderModule(&shader->modules[2], rhitSPV, sizeRHIT);
	VK_CreateShaderModule(&shader->modules[3], rahitSPV, sizeRAHIT);
	// SHADOW
	VK_CreateShaderModule(&shader->modules[4], rmissShadowSPV, sizeRMISSShadow);
	VK_CreateShaderModule(&shader->modules[5], rchitShadowSPV, sizeRCHITShadow);
	VK_CreateShaderModule(&shader->modules[6], rahitShadowSPV, sizeRAHITShadow);

	shader->shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shader->shaderStageCreateInfos[0].module = shader->modules[0];
	shader->shaderStageCreateInfos[0].pName = "main";

	shader->shaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->shaderStageCreateInfos[1].module = shader->modules[1];
	shader->shaderStageCreateInfos[1].pName = "main";

	shader->shaderStageCreateInfos[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[2].module = shader->modules[2];
	shader->shaderStageCreateInfos[2].pName = "main";

	shader->shaderStageCreateInfos[3].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[3].stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[3].module = shader->modules[3];
	shader->shaderStageCreateInfos[3].pName = "main";

	shader->shaderStageCreateInfos[4].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[4].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->shaderStageCreateInfos[4].module = shader->modules[4];
	shader->shaderStageCreateInfos[4].pName = "main";

	shader->shaderStageCreateInfos[5].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[5].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[5].module = shader->modules[5];
	shader->shaderStageCreateInfos[5].pName = "main";

	shader->shaderStageCreateInfos[6].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[6].stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[6].module = shader->modules[6];
	shader->shaderStageCreateInfos[6].pName = "main";

	VkRayTracingShaderGroupCreateInfoKHR groups[] = {
		[SBT_RGEN_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = 0,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RMISS_PATH_TRACER] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = 1,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RCHIT_OPAQUE] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = 2,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RAHIT_PARTICLE] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = 3,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		// SHADOW
		[SBT_RMISS_SHADOW_RAY] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = 4,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RCHIT_SHADOW_RAY] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = 5,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RAHIT_SHADOW_RAY] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = 6,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		}
	};
	shader->shaderGroupSize = sizeof(groups) / sizeof(VkRayTracingShaderGroupCreateInfoKHR);
	shader->shaderGroupCreateInfos = calloc(shader->shaderGroupSize, sizeof(VkRayTracingShaderGroupCreateInfoKHR));
	memcpy(shader->shaderGroupCreateInfos, &groups[0], sizeof(groups));
}

void VK_LoadPrimaryRaysShadersFromVariable(vkshader_t* shader) {
	shader->size = 4;
	shader->modules = calloc(shader->size, sizeof(VkShaderModule));
	shader->flags = calloc(shader->size, sizeof(VkShaderStageFlagBits));
	shader->shaderStageCreateInfos = calloc(shader->size, sizeof(VkPipelineShaderStageCreateInfo));

	shader->flags[0] = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shader->flags[1] = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->flags[2] = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->flags[3] = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

	VK_CreateShaderModule(&shader->modules[0], (const char *)primary_raysRGen, sizeof(primary_raysRGen));
	VK_CreateShaderModule(&shader->modules[1], (const char *)rt_missRMiss, sizeof(rt_missRMiss));
	VK_CreateShaderModule(&shader->modules[2], (const char *)rt_closesthitRCHit, sizeof(rt_closesthitRCHit));
	VK_CreateShaderModule(&shader->modules[3], (const char *)rt_anyhitRAHit, sizeof(rt_anyhitRAHit));
	
	shader->shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shader->shaderStageCreateInfos[0].module = shader->modules[0];
	shader->shaderStageCreateInfos[0].pName = "main";

	shader->shaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->shaderStageCreateInfos[1].module = shader->modules[1];
	shader->shaderStageCreateInfos[1].pName = "main";

	shader->shaderStageCreateInfos[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[2].module = shader->modules[2];
	shader->shaderStageCreateInfos[2].pName = "main";

	shader->shaderStageCreateInfos[3].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[3].stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[3].module = shader->modules[3];
	shader->shaderStageCreateInfos[3].pName = "main";

	VkRayTracingShaderGroupCreateInfoKHR groups[] = {
		[SBT_RGEN_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = 0,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RMISS_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = 1,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RCHIT_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = 2,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RAHIT_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = 3,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		}
	};
	shader->shaderGroupSize = sizeof(groups) / sizeof(VkRayTracingShaderGroupCreateInfoKHR);
	shader->shaderGroupCreateInfos = calloc(shader->shaderGroupSize, sizeof(VkRayTracingShaderGroupCreateInfoKHR));
	memcpy(shader->shaderGroupCreateInfos, &groups[0], sizeof(groups));
}

void VK_RTX_PrimaryRayShader(vkshader_t* shader) {
	if (primaryRays == NULL) {
		primaryRays = malloc(sizeof(vkshader_t));
		VK_LoadPrimaryRaysShadersFromVariable(primaryRays);
	}
	Com_Memcpy(shader, primaryRays, sizeof(vkshader_t));
}

void VK_LoadReflectRaysShadersFromVariable(vkshader_t* shader) {
	shader->size = 7;
	shader->modules = calloc(shader->size, sizeof(VkShaderModule));
	shader->flags = calloc(shader->size, sizeof(VkShaderStageFlagBits));
	shader->shaderStageCreateInfos = calloc(shader->size, sizeof(VkPipelineShaderStageCreateInfo));

	shader->flags[0] = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shader->flags[1] = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->flags[2] = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->flags[3] = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	// SHADOW
	shader->flags[4] = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->flags[5] = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->flags[6] = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

	VK_CreateShaderModule(&shader->modules[0], (const char *)reflect_raysRGen, sizeof(reflect_raysRGen));
	VK_CreateShaderModule(&shader->modules[1], (const char *)reflect_raysRMiss, sizeof(reflect_raysRMiss));
	VK_CreateShaderModule(&shader->modules[2], (const char *)reflect_raysRCHit, sizeof(reflect_raysRCHit));
	VK_CreateShaderModule(&shader->modules[3], (const char *)reflect_raysRAHit, sizeof(reflect_raysRAHit));
	// SHADOW
	VK_CreateShaderModule(&shader->modules[4], (const char *)rt_shadowRMiss, sizeof(rt_shadowRMiss));
	VK_CreateShaderModule(&shader->modules[5], (const char *)rt_shadowRCHit, sizeof(rt_shadowRCHit));
	VK_CreateShaderModule(&shader->modules[6], (const char *)rt_shadowRAHit, sizeof(rt_shadowRAHit));

	shader->shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shader->shaderStageCreateInfos[0].module = shader->modules[0];
	shader->shaderStageCreateInfos[0].pName = "main";

	shader->shaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->shaderStageCreateInfos[1].module = shader->modules[1];
	shader->shaderStageCreateInfos[1].pName = "main";

	shader->shaderStageCreateInfos[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[2].module = shader->modules[2];
	shader->shaderStageCreateInfos[2].pName = "main";

	shader->shaderStageCreateInfos[3].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[3].stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[3].module = shader->modules[3];
	shader->shaderStageCreateInfos[3].pName = "main";

	shader->shaderStageCreateInfos[4].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[4].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->shaderStageCreateInfos[4].module = shader->modules[4];
	shader->shaderStageCreateInfos[4].pName = "main";

	shader->shaderStageCreateInfos[5].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[5].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[5].module = shader->modules[5];
	shader->shaderStageCreateInfos[5].pName = "main";

	shader->shaderStageCreateInfos[6].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[6].stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[6].module = shader->modules[6];
	shader->shaderStageCreateInfos[6].pName = "main";

	VkRayTracingShaderGroupCreateInfoKHR groups[] = {
		[SBT_RGEN_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = 0,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RMISS_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = 1,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RCHIT_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = 2,
			.anyHitShader = 3,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RAHIT_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = 2,
			.anyHitShader = 3,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		// SHADOW
		[SBT_RMISS_SHADOW_RAY] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = 4,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RCHIT_SHADOW_RAY] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = 5,
			.anyHitShader = 6,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RAHIT_SHADOW_RAY] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = 5,
			.anyHitShader = 6,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		}
	};
	shader->shaderGroupSize = sizeof(groups) / sizeof(VkRayTracingShaderGroupCreateInfoKHR);
	shader->shaderGroupCreateInfos = calloc(shader->shaderGroupSize, sizeof(VkRayTracingShaderGroupCreateInfoKHR));
	memcpy(shader->shaderGroupCreateInfos, &groups[0], sizeof(groups));
}

void VK_RTX_ReflectRaysShader(vkshader_t* shader) {
	if (reflectRays == NULL) {
		reflectRays = malloc(sizeof(vkshader_t));
		VK_LoadReflectRaysShadersFromVariable(reflectRays);
	}
	Com_Memcpy(shader, reflectRays, sizeof(vkshader_t));
}

void VK_LoadDirectIlluminationShadersFromVariable(vkshader_t* shader) {
	shader->size = 7;
	shader->modules = calloc(shader->size, sizeof(VkShaderModule));
	shader->flags = calloc(shader->size, sizeof(VkShaderStageFlagBits));
	shader->shaderStageCreateInfos = calloc(shader->size, sizeof(VkPipelineShaderStageCreateInfo));

	shader->flags[0] = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shader->flags[1] = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->flags[2] = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->flags[3] = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	// SHADOW
	shader->flags[4] = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->flags[5] = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->flags[6] = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

	VK_CreateShaderModule(&shader->modules[0], (const char *)direct_illuminationRGen, sizeof(direct_illuminationRGen));
	VK_CreateShaderModule(&shader->modules[1], (const char *)rt_missRMiss, sizeof(rt_missRMiss));
	VK_CreateShaderModule(&shader->modules[2], (const char *)rt_closesthitRCHit, sizeof(rt_closesthitRCHit));
	VK_CreateShaderModule(&shader->modules[3], (const char *)rt_anyhitRAHit, sizeof(rt_anyhitRAHit));
	// SHADOW
	VK_CreateShaderModule(&shader->modules[4], (const char *)rt_shadowRMiss, sizeof(rt_shadowRMiss));
	VK_CreateShaderModule(&shader->modules[5], (const char *)rt_shadowRCHit, sizeof(rt_shadowRCHit));
	VK_CreateShaderModule(&shader->modules[6], (const char *)rt_shadowRAHit, sizeof(rt_shadowRAHit));

	shader->shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shader->shaderStageCreateInfos[0].module = shader->modules[0];
	shader->shaderStageCreateInfos[0].pName = "main";

	shader->shaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->shaderStageCreateInfos[1].module = shader->modules[1];
	shader->shaderStageCreateInfos[1].pName = "main";

	shader->shaderStageCreateInfos[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[2].module = shader->modules[2];
	shader->shaderStageCreateInfos[2].pName = "main";

	shader->shaderStageCreateInfos[3].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[3].stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[3].module = shader->modules[3];
	shader->shaderStageCreateInfos[3].pName = "main";

	shader->shaderStageCreateInfos[4].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[4].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->shaderStageCreateInfos[4].module = shader->modules[4];
	shader->shaderStageCreateInfos[4].pName = "main";

	shader->shaderStageCreateInfos[5].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[5].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[5].module = shader->modules[5];
	shader->shaderStageCreateInfos[5].pName = "main";

	shader->shaderStageCreateInfos[6].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[6].stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[6].module = shader->modules[6];
	shader->shaderStageCreateInfos[6].pName = "main";

	VkRayTracingShaderGroupCreateInfoKHR groups[] = {
		[SBT_RGEN_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = 0,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RMISS_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = 1,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RCHIT_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = 2,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RAHIT_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = 3,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		// SHADOW
		[SBT_RMISS_SHADOW_RAY] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = 4,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RCHIT_SHADOW_RAY] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = 5,
			.anyHitShader = 6,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RAHIT_SHADOW_RAY] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = 5,
			.anyHitShader = 6,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		}
	};
	shader->shaderGroupSize = sizeof(groups) / sizeof(VkRayTracingShaderGroupCreateInfoKHR);
	shader->shaderGroupCreateInfos = calloc(shader->shaderGroupSize, sizeof(VkRayTracingShaderGroupCreateInfoKHR));
	memcpy(shader->shaderGroupCreateInfos, &groups[0], sizeof(groups));
}

void VK_RTX_DirectIlluminationShader(vkshader_t* shader) {
	if (directIllumination == NULL) {
		directIllumination = malloc(sizeof(vkshader_t));
		VK_LoadDirectIlluminationShadersFromVariable(directIllumination);
	}
	Com_Memcpy(shader, directIllumination, sizeof(vkshader_t));
}

void VK_LoadIndirectIlluminationShadersFromVariable(vkshader_t* shader) {
	shader->size = 7;
	shader->modules = calloc(shader->size, sizeof(VkShaderModule));
	shader->flags = calloc(shader->size, sizeof(VkShaderStageFlagBits));
	shader->shaderStageCreateInfos = calloc(shader->size, sizeof(VkPipelineShaderStageCreateInfo));

	shader->flags[0] = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shader->flags[1] = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->flags[2] = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->flags[3] = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	// SHADOW
	shader->flags[4] = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->flags[5] = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->flags[6] = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

	VK_CreateShaderModule(&shader->modules[0], (const char *)indirect_illuminationRGen, sizeof(indirect_illuminationRGen));
	VK_CreateShaderModule(&shader->modules[1], (const char *)rt_missRMiss, sizeof(rt_missRMiss));
	VK_CreateShaderModule(&shader->modules[2], (const char *)rt_closesthitRCHit, sizeof(rt_closesthitRCHit));
	VK_CreateShaderModule(&shader->modules[3], (const char *)rt_anyhitRAHit, sizeof(rt_anyhitRAHit));
	// SHADOW
	VK_CreateShaderModule(&shader->modules[4], (const char *)rt_shadowRMiss, sizeof(rt_shadowRMiss));
	VK_CreateShaderModule(&shader->modules[5], (const char *)rt_shadowRCHit, sizeof(rt_shadowRCHit));
	VK_CreateShaderModule(&shader->modules[6], (const char *)rt_shadowRAHit, sizeof(rt_shadowRAHit));

	shader->shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	shader->shaderStageCreateInfos[0].module = shader->modules[0];
	shader->shaderStageCreateInfos[0].pName = "main";

	shader->shaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->shaderStageCreateInfos[1].module = shader->modules[1];
	shader->shaderStageCreateInfos[1].pName = "main";

	shader->shaderStageCreateInfos[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[2].module = shader->modules[2];
	shader->shaderStageCreateInfos[2].pName = "main";

	shader->shaderStageCreateInfos[3].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[3].stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[3].module = shader->modules[3];
	shader->shaderStageCreateInfos[3].pName = "main";

	shader->shaderStageCreateInfos[4].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[4].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	shader->shaderStageCreateInfos[4].module = shader->modules[4];
	shader->shaderStageCreateInfos[4].pName = "main";

	shader->shaderStageCreateInfos[5].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[5].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[5].module = shader->modules[5];
	shader->shaderStageCreateInfos[5].pName = "main";

	shader->shaderStageCreateInfos[6].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader->shaderStageCreateInfos[6].stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
	shader->shaderStageCreateInfos[6].module = shader->modules[6];
	shader->shaderStageCreateInfos[6].pName = "main";

	VkRayTracingShaderGroupCreateInfoKHR groups[] = {
		[SBT_RGEN_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = 0,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RMISS_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = 1,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RCHIT_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = 2,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RAHIT_PRIMARY_RAYS] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = 3,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		// SHADOW
		[SBT_RMISS_SHADOW_RAY] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
			.generalShader = 4,
			.closestHitShader = VK_SHADER_UNUSED_KHR,
			.anyHitShader = VK_SHADER_UNUSED_KHR,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RCHIT_SHADOW_RAY] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = 5,
			.anyHitShader = 6,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		},
		[SBT_RAHIT_SHADOW_RAY] = {
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
			.generalShader = VK_SHADER_UNUSED_KHR,
			.closestHitShader = 5,
			.anyHitShader = 6,
			.intersectionShader = VK_SHADER_UNUSED_KHR
		}
	};
	shader->shaderGroupSize = sizeof(groups) / sizeof(VkRayTracingShaderGroupCreateInfoKHR);
	shader->shaderGroupCreateInfos = calloc(shader->shaderGroupSize, sizeof(VkRayTracingShaderGroupCreateInfoKHR));
	memcpy(shader->shaderGroupCreateInfos, &groups[0], sizeof(groups));
}

void VK_RTX_IndirectIlluminationShader(vkshader_t* shader) {
	if (indirectIllumination == NULL) {
		indirectIllumination = malloc(sizeof(vkshader_t));
		VK_LoadIndirectIlluminationShadersFromVariable(indirectIllumination);
	}
	Com_Memcpy(shader, indirectIllumination, sizeof(vkshader_t));
}