/*
===========================================================================
DX12 Renderer Main Entry Points
===========================================================================
*/
#include "dx12_local.h"

using Microsoft::WRL::ComPtr;

// Global renderer API
dx12RendererAPI_t dx12RendererAPI = {0};

// Frame timing
static LARGE_INTEGER g_frequency = {0};
static LARGE_INTEGER g_lastTime = {0};
static LARGE_INTEGER g_currentTime = {0};
static float g_deltaTime = 0.0f;

// Input state
#define MAX_KEYS 512
static BOOL g_keyStates[MAX_KEYS] = {0};
static BOOL g_keyStatesPrev[MAX_KEYS] = {0};
static int g_mouseX = 0, g_mouseY = 0;
static int g_mouseDeltaX = 0, g_mouseDeltaY = 0;
static BOOL g_mouseButtons[3] = {0};
static BOOL g_mouseButtonsPrev[3] = {0};
static int g_mouseWheel = 0;

// Renderer state
static qboolean g_rendererInitialized = qfalse;
static int g_frameCount = 0;

// Initialize renderer
qboolean DX12_InitRenderer(void) {
    if (g_rendererInitialized) {
        return qtrue;
    }
    
    dx12Device_t* dev = DX12_GetDevice();
    dx12Swapchain_t* swapchain = DX12_GetSwapchain();
    
    if (!dev || !swapchain) {
        Com_Printf("DX12_InitRenderer: Device or swapchain not initialized\n");
        return qfalse;
    }
    
    // Initialize timing
    QueryPerformanceFrequency(&g_frequency);
    QueryPerformanceCounter(&g_lastTime);
    
    // Initialize renderer API function pointers
    dx12RendererAPI.Init = DX12_InitRenderer;
    dx12RendererAPI.Shutdown = DX12_ShutdownRenderer;
    dx12RendererAPI.BeginFrame = DX12_BeginFrame;
    dx12RendererAPI.EndFrame = DX12_EndFrame;
    dx12RendererAPI.DrawIndexed = DX12_DrawIndexed;
    dx12RendererAPI.Draw = DX12_Draw;
    dx12RendererAPI.CreateBuffer = DX12_CreateBuffer;
    dx12RendererAPI.CreateTexture = DX12_CreateTexture;
    dx12RendererAPI.UpdateBuffer = DX12_UpdateBuffer;
    dx12RendererAPI.UpdateTexture = DX12_UpdateTexture;
    dx12RendererAPI.CreatePipeline = DX12_CreateGraphicsPipeline;
    dx12RendererAPI.BindPipeline = DX12_BindPipeline;
    dx12RendererAPI.BindDescriptorSet = DX12_SetDescriptorTable;
    dx12RendererAPI.CreateAccelerationStructure = (qhandle_t(*)(qboolean, int))DX12_CreateBottomLevelAS;
    dx12RendererAPI.BuildAccelerationStructure = (void(*)(qhandle_t, const void*))DX12_UpdateTopLevelAS;
    dx12RendererAPI.CreateRayTracingPipeline = (qhandle_t(*)(const char*, const char*, const char*, int))DX12_CreateRayTracingPipeline;
    dx12RendererAPI.TraceRays = (void(*)(int, int, int))DX12_DispatchRays;
    dx12RendererAPI.SetDebugName = DX12_SetDebugName;
    
    // Create default pipeline for rasterization
    DX12_CreateDefaultPipeline();
    
    g_rendererInitialized = qtrue;
    Com_Printf("DX12 Renderer initialized\n");
    return qtrue;
}

void DX12_ShutdownRenderer(void) {
    if (!g_rendererInitialized) {
        return;
    }
    
    DX12_CleanupPipelines();
    DX12_CleanupRayTracingPipelines();
    DX12_CleanupAccelerationStructures();
    
    g_rendererInitialized = qfalse;
    Com_Printf("DX12 Renderer shut down\n");
}

void DX12_BeginFrame(void) {
    dx12Device_t* dev = DX12_GetDevice();
    dx12Swapchain_t* swapchain = DX12_GetSwapchain();
    
    if (!dev || !swapchain) return;
    
    // Calculate delta time
    QueryPerformanceCounter(&g_currentTime);
    g_deltaTime = (float)(g_currentTime.QuadPart - g_lastTime.QuadPart) / (float)g_frequency.QuadPart;
    g_lastTime = g_currentTime;
    
    // Clamp delta time
    if (g_deltaTime > 0.1f) g_deltaTime = 0.1f;
    
    // Update input state
    memcpy(g_keyStatesPrev, g_keyStates, sizeof(g_keyStates));
    memcpy(g_mouseButtonsPrev, g_mouseButtons, sizeof(g_mouseButtons));
    g_mouseDeltaX = 0;
    g_mouseDeltaY = 0;
    g_mouseWheel = 0;
    
    // Reset command allocator and list for this frame
    dx12Device_t* devPtr = DX12_GetDevice();
    int frameIndex = devPtr->currentFrameIndex;
    devPtr->frames[frameIndex].allocator->Reset();
    devPtr->frames[frameIndex].cmdList->Reset(devPtr->frames[frameIndex].allocator.Get(), nullptr);
    
    // Set descriptor heaps
    ID3D12DescriptorHeap* heaps[] = {
        g_heaps[0].heap.Get(), // CBV/SRV/UAV
        g_heaps[1].heap.Get()  // Sampler
    };
    DX12_GetCommandList()->SetDescriptorHeaps(2, heaps);
    
    g_frameCount++;
}

void DX12_EndFrame(void) {
    dx12Device_t* dev = DX12_GetDevice();
    dx12Swapchain_t* swapchain = DX12_GetSwapchain();
    
    if (!dev || !swapchain) return;
    
    ID3D12GraphicsCommandList* cmdList = DX12_GetCommandList();
    
    // Flush any pending barriers
    DX12_FlushBarriers(cmdList);
    
    // Transition back buffer to present
    DX12_ResourceBarrier(cmdList, swapchain->backBuffers[swapchain->currentBuffer].Get(),
                         D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    DX12_FlushBarriers(cmdList);
    
    // Close and execute command list
    cmdList->Close();
    ID3D12CommandList* cmdLists[] = {cmdList};
    dev->graphicsQueue->ExecuteCommandLists(1, cmdLists);
    
    // Present
    DX12_Present();
    
    // Wait for frame fence and move to next frame
    DX12_WaitForGPU();
    DX12_NextFrame();
}

void DX12_RenderFrame(void) {
    DX12_BeginFrame();
    
    // Engine would call rendering commands here
    // For now, just clear the screen
    dx12Swapchain_t* swapchain = DX12_GetSwapchain();
    if (swapchain) {
        ID3D12GraphicsCommandList* cmdList = DX12_GetCommandList();
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = DX12_GetCurrentRTV();
        
        const float clearColor[] = {0.1f, 0.1f, 0.15f, 1.0f};
        cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    }
    
    DX12_EndFrame();
}

// Drawing functions
void DX12_DrawIndexed(int numIndices, int baseIndex, int baseVertex) {
    ID3D12GraphicsCommandList* cmdList = DX12_GetCommandList();
    cmdList->DrawIndexedInstanced(numIndices, 1, baseIndex, baseVertex, 0);
}

void DX12_Draw(int numVertices, int baseVertex) {
    ID3D12GraphicsCommandList* cmdList = DX12_GetCommandList();
    cmdList->DrawInstanced(numVertices, 1, baseVertex, 0);
}

// Resource creation
qhandle_t DX12_CreateBuffer(size_t size, qboolean dynamic, const void* data) {
    dx12Device_t* dev = DX12_GetDevice();
    ID3D12Resource* buffer = nullptr;
    
    D3D12_HEAP_TYPE heapType = dynamic ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_STATES initialState = dynamic ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON;
    
    DX12_CreateBuffer(size, heapType, initialState, &buffer, data);
    
    return (qhandle_t)(uintptr_t)buffer;
}

qhandle_t DX12_CreateTexture(int width, int height, int format, const void* data) {
    // Simplified - would need proper format mapping
    return 0;
}

void DX12_UpdateBuffer(qhandle_t handle, const void* data, size_t size, size_t offset) {
    ID3D12Resource* buffer = (ID3D12Resource*)(uintptr_t)handle;
    if (!buffer) return;
    
    void* mapped = nullptr;
    buffer->Map(0, nullptr, (void**)&mapped);
    memcpy((BYTE*)mapped + offset, data, size);
    buffer->Unmap(0, nullptr);
}

void DX12_UpdateTexture(qhandle_t handle, const void* data, int width, int height, int stride) {
    // Would implement texture upload
}

// Pipeline functions (implemented in dx12_pipeline.c)
extern qhandle_t DX12_CreateGraphicsPipeline(const char* vsSource, const char* fsSource, const D3D12_INPUT_ELEMENT_DESC* inputLayout, UINT numInputElements, D3D12_PRIMITIVE_TOPOLOGY_TYPE topology);
extern void DX12_BindPipeline(qhandle_t handle);
extern void DX12_SetDescriptorTable(UINT rootParamIndex, D3D12_GPU_DESCRIPTOR_HANDLE handle);
extern void DX12_CreateDefaultPipeline(void);
extern void DX12_CleanupPipelines(void);
extern void DX12_SetDebugName(qhandle_t handle, const char* name);

// Debug name
void DX12_SetDebugName(qhandle_t handle, const char* name) {
    ID3D12Resource* resource = (ID3D12Resource*)(uintptr_t)handle;
    if (resource) {
        wchar_t wname[256];
        size_t converted;
        mbstowcs_s(&converted, wname, name, 256);
        resource->SetName(wname);
    }
}

// Input functions (called from Win32 WndProc)
void DX12_KeyDown(int key) {
    if (key >= 0 && key < MAX_KEYS) {
        g_keyStates[key] = TRUE;
    }
}

void DX12_KeyUp(int key) {
    if (key >= 0 && key < MAX_KEYS) {
        g_keyStates[key] = FALSE;
    }
}

void DX12_MouseButtonDown(int button) {
    if (button >= 0 && button < 3) {
        g_mouseButtons[button] = TRUE;
    }
}

void DX12_MouseButtonUp(int button) {
    if (button >= 0 && button < 3) {
        g_mouseButtons[button] = FALSE;
    }
}

void DX12_MouseMove(int x, int y) {
    g_mouseDeltaX = x - g_mouseX;
    g_mouseDeltaY = y - g_mouseY;
    g_mouseX = x;
    g_mouseY = y;
}

void DX12_MouseWheel(int delta) {
    g_mouseWheel += delta;
}

// Input query functions (for engine)
qboolean DX12_GetKey(int key) {
    if (key >= 0 && key < MAX_KEYS) {
        return g_keyStates[key];
    }
    return qfalse;
}

qboolean DX12_GetKeyDown(int key) {
    if (key >= 0 && key < MAX_KEYS) {
        return g_keyStates[key] && !g_keyStatesPrev[key];
    }
    return qfalse;
}

qboolean DX12_GetKeyUp(int key) {
    if (key >= 0 && key < MAX_KEYS) {
        return !g_keyStates[key] && g_keyStatesPrev[key];
    }
    return qfalse;
}

qboolean DX12_GetMouseButton(int button) {
    if (button >= 0 && button < 3) {
        return g_mouseButtons[button];
    }
    return qfalse;
}

void DX12_GetMouseDelta(int* dx, int* dy) {
    *dx = g_mouseDeltaX;
    *dy = g_mouseDeltaY;
}

int DX12_GetMouseWheel(void) {
    return g_mouseWheel;
}

float DX12_GetDeltaTime(void) {
    return g_deltaTime;
}