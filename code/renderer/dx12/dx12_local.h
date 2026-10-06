/*
===========================================================================
DX12 Renderer - Local Definitions
===========================================================================
*/
#ifndef __DX12_LOCAL_H__
#define __DX12_LOCAL_H__

#include "../../game/q_shared.h"
#include "../tr_local.h"

// Windows/DX12 headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <wrl/client.h>

// For COM smart pointers
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

// Maximum frames in flight
#define DX12_MAX_FRAMES_IN_FLIGHT 3

// Descriptor heap sizes
#define DX12_CBV_SRV_UAV_HEAP_SIZE 1024
#define DX12_SAMPLER_HEAP_SIZE 256
#define DX12_RTV_HEAP_SIZE 64
#define DX12_DSV_HEAP_SIZE 16

// Resource state tracking
typedef enum {
    DX12_STATE_COMMON,
    DX12_STATE_VERTEX_AND_CONSTANT_BUFFER,
    DX12_STATE_INDEX_BUFFER,
    DX12_STATE_RENDER_TARGET,
    DX12_STATE_UNORDERED_ACCESS,
    DX12_STATE_DEPTH_WRITE,
    DX12_STATE_DEPTH_READ,
    DX12_STATE_NON_PIXEL_SHADER_RESOURCE,
    DX12_STATE_PIXEL_SHADER_RESOURCE,
    DX12_STATE_STREAM_OUT,
    DX12_STATE_INDIRECT_ARGUMENT,
    DX12_STATE_COPY_DEST,
    DX12_STATE_COPY_SOURCE,
    DX12_STATE_RESOLVE_DEST,
    DX12_STATE_RESOLVE_SOURCE,
    DX12_STATE_GENERIC_READ,
    DX12_STATE_PRESENT,
    DX12_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
    DX12_STATE_SHADING_RATE_SOURCE,
} Dx12ResourceState;

// Forward declarations
typedef struct dx12Device_s dx12Device_t;
typedef struct dx12Swapchain_s dx12Swapchain_t;
typedef struct dx12CommandContext_s dx12CommandContext_t;
typedef struct dx12Resource_s dx12Resource_t;
typedef struct dx12Pipeline_s dx12Pipeline_t;
typedef struct dx12RayTracingPipeline_s dx12RayTracingPipeline_t;
typedef struct dx12AccelerationStructure_s dx12AccelerationStructure_t;
typedef struct dx12DescriptorHeap_s dx12DescriptorHeap_t;
typedef struct dx12RootSignature_s dx12RootSignature_t;

// Function pointers for renderer API (matching Vulkan renderer interface)
typedef struct {
    // Init/Shutdown
    qboolean (*Init)(void);
    void (*Shutdown)(void);

    // Frame
    void (*BeginFrame)(void);
    void (*EndFrame)(void);

    // Drawing
    void (*DrawIndexed)(int numIndices, int baseIndex, int baseVertex);
    void (*Draw)(int numVertices, int baseVertex);

    // Resources
    qhandle_t (*CreateBuffer)(size_t size, qboolean dynamic, const void* data);
    qhandle_t (*CreateTexture)(int width, int height, int format, const void* data);
    void (*UpdateBuffer)(qhandle_t handle, const void* data, size_t size, size_t offset);
    void (*UpdateTexture)(qhandle_t handle, const void* data, int width, int height, int stride);

    // Pipeline
    qhandle_t (*CreatePipeline)(const char* vsSource, const char* fsSource, qboolean rt);
    void (*BindPipeline)(qhandle_t handle);

    // Descriptors
    void (*BindDescriptorSet)(int set, qhandle_t handle);

    // Ray Tracing
    qhandle_t (*CreateAccelerationStructure)(qboolean topLevel, int numGeometries);
    void (*BuildAccelerationStructure)(qhandle_t handle, const void* geometryData);
    qhandle_t (*CreateRayTracingPipeline)(const char* rgSource, const char* chSource, const char* msSource, int numMaterials);
    void (*TraceRays)(int width, int height, int depth);

    // Debug
    void (*SetDebugName)(qhandle_t handle, const char* name);
    
    // Input
    void (*KeyDown)(int key);
    void (*KeyUp)(int key);
    void (*MouseButtonDown)(int button);
    void (*MouseButtonUp)(int button);
    void (*MouseMove)(int x, int y);
    void (*MouseWheel)(int delta);
    
    qboolean (*GetKey)(int key);
    qboolean (*GetKeyDown)(int key);
    qboolean (*GetKeyUp)(int key);
    qboolean (*GetMouseButton)(int button);
    void (*GetMouseDelta)(int* dx, int* dy);
    int (*GetMouseWheel)(void);
    
    float (*GetDeltaTime)(void);
} dx12RendererAPI_t;

// Global API instance
extern dx12RendererAPI_t dx12RendererAPI;

// Internal functions
dx12Device_t* DX12_GetDevice(void);
dx12Swapchain_t* DX12_GetSwapchain(void);
dx12CommandContext_t* DX12_GetCommandContext(int frameIndex);

// Acceleration structures
dx12AccelerationStructure_t* DX12_CreateBottomLevelAS(UINT numGeometries, const D3D12_RAYTRACING_GEOMETRY_DESC* geometries);
dx12AccelerationStructure_t* DX12_CreateTopLevelAS(UINT numInstances, const D3D12_RAYTRACING_INSTANCE_DESC* instances);
void DX12_UpdateTopLevelAS(dx12AccelerationStructure_t* as, UINT numInstances, const D3D12_RAYTRACING_INSTANCE_DESC* instances);
D3D12_GPU_VIRTUAL_ADDRESS DX12_GetAccelerationStructureGPUVA(dx12AccelerationStructure_t* as);
void DX12_CleanupAccelerationStructures(void);

// Ray tracing pipeline
dx12RayTracingPipeline_t* DX12_CreateRayTracingPipeline(
    const char* rayGenSource, const char* missSource, const char* closestHitSource,
    const char* anyHitSource, const char* intersectionSource,
    UINT numRayGen, UINT numMiss, UINT numHitGroups,
    const D3D12_ROOT_SIGNATURE* globalRootSig, const D3D12_ROOT_SIGNATURE* localRootSig);
void DX12_DispatchRays(dx12RayTracingPipeline_t* pipeline, UINT width, UINT height, UINT depth);
void DX12_CleanupRayTracingPipelines(void);

// Input functions
void DX12_KeyDown(int key);
void DX12_KeyUp(int key);
void DX12_MouseButtonDown(int button);
void DX12_MouseButtonUp(int button);
void DX12_MouseMove(int x, int y);
void DX12_MouseWheel(int delta);

qboolean DX12_GetKey(int key);
qboolean DX12_GetKeyDown(int key);
qboolean DX12_GetKeyUp(int key);
qboolean DX12_GetMouseButton(int button);
void DX12_GetMouseDelta(int* dx, int* dy);
int DX12_GetMouseWheel(void);

float DX12_GetDeltaTime(void);

#endif // __DX12_LOCAL_H__