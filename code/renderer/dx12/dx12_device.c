/*
===========================================================================
DX12 Device Initialization
===========================================================================
*/
#include "dx12_local.h"

using Microsoft::WRL::ComPtr;

// Device structure
struct dx12Device_s {
    ComPtr<ID3D12Device8> device;
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter4> adapter;
    
    // Feature support
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7;
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5;
    D3D12_RAYTRACING_TIER rtTier;
    BOOL rtSupported;
    
    // Command queues
    ComPtr<ID3D12CommandQueue> graphicsQueue;
    ComPtr<ID3D12CommandQueue> computeQueue;
    ComPtr<ID3D12CommandQueue> copyQueue;
    
    // Descriptor heaps
    dx12DescriptorHeap_t* cbvSrvUavHeap;
    dx12DescriptorHeap_t* samplerHeap;
    dx12DescriptorHeap_t* rtvHeap;
    dx12DescriptorHeap_t* dsvHeap;
    
    // Frame resources
    struct {
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList6> cmdList;
        UINT64 fenceValue;
        ComPtr<ID3D12Fence> fence;
        HANDLE fenceEvent;
    } frames[DX12_MAX_FRAMES_IN_FLIGHT];
    
    UINT64 currentFenceValue;
    int currentFrameIndex;
    
    // Info
    D3D12_FEATURE_DATA_ARCHITECTURE arch;
    wchar_t adapterDesc[128];
};

static dx12Device_t g_device = {};

// Helper: Check HRESULT
#define DX12_CHECK(hr, msg) \
    do { \
        HRESULT _hr = (hr); \
        if (FAILED(_hr)) { \
            Com_Printf("DX12 ERROR: %s (0x%08X)\n", msg, _hr); \
            return qfalse; \
        } \
    } while (0)

// Helper: Create command queue
static qboolean DX12_CreateCommandQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandQueue** queue) {
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;
    return SUCCEEDED(device->CreateCommandQueue(&desc, IID_PPV_ARGS(queue)));
}

// Helper: Create fence
static qboolean DX12_CreateFence(ID3D12Device* device, UINT64 initialValue, ID3D12Fence** fence) {
    return SUCCEEDED(device->CreateFence(initialValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence)));
}

// Initialize DX12 device
qboolean DX12_InitDevice(HWND hwnd) {
    // Enable debug layer in debug builds
    #ifdef _DEBUG
    {
        ComPtr<ID3D12Debug6> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
            debugController->SetEnableGPUBasedValidation(TRUE);
            debugController->SetEnableSynchronizedCommandQueueValidation(TRUE);
            Com_Printf("DX12: Debug layer enabled\n");
        }
    }
    #endif

    // Create DXGI factory
    UINT factoryFlags = 0;
    #ifdef _DEBUG
    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    #endif
    DX12_CHECK(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&g_device.factory)), "CreateDXGIFactory2");

    // Select adapter (prefer discrete GPU with RT support)
    ComPtr<IDXGIAdapter1> adapter1;
    SIZE_T maxVRAM = 0;
    for (UINT i = 0; g_device.factory->EnumAdapters1(i, &adapter1) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter1->GetDesc1(&desc);
        
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        
        ComPtr<ID3D12Device> testDevice;
        if (SUCCEEDED(D3D12CreateDevice(adapter1.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&testDevice)))) {
            // Check VRAM
            DXGI_QUERY_VIDEO_MEMORY_INFO memInfo = {};
            adapter1->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo);
            
            if (memInfo.Budget > maxVRAM) {
                maxVRAM = memInfo.Budget;
                g_device.adapter = adapter1;
                wcscpy_s(g_device.adapterDesc, desc.Description);
            }
        }
        adapter1.Reset();
    }

    if (!g_device.adapter) {
        Com_Printf("DX12: No suitable adapter found\n");
        return qfalse;
    }

    Com_Printf("DX12: Selected adapter: %S (VRAM: %zu MB)\n", g_device.adapterDesc, maxVRAM / (1024*1024));

    // Create device
    DX12_CHECK(D3D12CreateDevice(g_device.adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&g_device.device)), "D3D12CreateDevice");

    // Check feature support
    g_device.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &g_device.options7, sizeof(g_device.options7));
    g_device.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &g_device.options5, sizeof(g_device.options5));
    g_device.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &g_device.options5, sizeof(g_device.options5));
    g_device.device->CheckFeatureSupport(D3D12_FEATURE_RAYTRACING, &g_device.rtTier, sizeof(g_device.rtTier));
    
    g_device.rtSupported = (g_device.rtTier >= D3D12_RAYTRACING_TIER_1_0);
    g_device.device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &g_device.arch, sizeof(g_device.arch));

    Com_Printf("DX12: Ray tracing tier: %d, Supported: %s\n", g_device.rtTier, g_device.rtSupported ? "YES" : "NO");
    Com_Printf("DX12: Mesh shaders: %s, Sampler feedback: %s\n", 
        g_device.options7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1 ? "YES" : "NO",
        g_device.options5.SamplerFeedbackTier >= D3D12_SAMPLER_FEEDBACK_TIER_1 ? "YES" : "NO");

    // Create command queues
    DX12_CHECK(DX12_CreateCommandQueue(g_device.device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, &g_device.graphicsQueue), "Graphics queue");
    DX12_CHECK(DX12_CreateCommandQueue(g_device.device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE, &g_device.computeQueue), "Compute queue");
    DX12_CHECK(DX12_CreateCommandQueue(g_device.device.Get(), D3D12_COMMAND_LIST_TYPE_COPY, &g_device.copyQueue), "Copy queue");

    g_device.graphicsQueue->SetName(L"Graphics Queue");
    g_device.computeQueue->SetName(L"Compute Queue");
    g_device.copyQueue->SetName(L"Copy Queue");

    // Create frame resources
    for (int i = 0; i < DX12_MAX_FRAMES_IN_FLIGHT; ++i) {
        DX12_CHECK(g_device.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_device.frames[i].allocator)), "Command allocator");
        g_device.frames[i].allocator->SetName(L"Frame Allocator");
        
        DX12_CHECK(g_device.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_device.frames[i].allocator.Get(), nullptr, IID_PPV_ARGS(&g_device.frames[i].cmdList)), "Command list");
        g_device.frames[i].cmdList->SetName(L"Frame Command List");
        g_device.frames[i].cmdList->Close(); // Start closed
        
        DX12_CHECK(DX12_CreateFence(g_device.device.Get(), 0, &g_device.frames[i].fence), "Fence");
        g_device.frames[i].fence->SetName(L"Frame Fence");
        
        g_device.frames[i].fenceValue = 0;
        g_device.frames[i].fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    g_device.currentFenceValue = 1;
    g_device.currentFrameIndex = 0;

    // Create descriptor heaps
    DX12_InitDescriptorHeaps();

    Com_Printf("DX12: Device initialized successfully\n");
    return qtrue;
}

// Shutdown
void DX12_ShutdownDevice(void) {
    // Wait for all frames
    for (int i = 0; i < DX12_MAX_FRAMES_IN_FLIGHT; ++i) {
        if (g_device.frames[i].fence) {
            g_device.graphicsQueue->Signal(g_device.frames[i].fence.Get(), g_device.currentFenceValue + i + 1);
            if (g_device.frames[i].fence->GetCompletedValue() < g_device.currentFenceValue + i + 1) {
                g_device.frames[i].fence->SetEventOnCompletion(g_device.currentFenceValue + i + 1, g_device.frames[i].fenceEvent);
                WaitForSingleObject(g_device.frames[i].fenceEvent, INFINITE);
            }
            CloseHandle(g_device.frames[i].fenceEvent);
        }
    }

    g_device = {};
    Com_Printf("DX12: Device shut down\n");
}

// Get current frame resources
dx12Device_t* DX12_GetDevice(void) {
    return &g_device;
}

// Wait for GPU
void DX12_WaitForGPU(void) {
    dx12Device_t* dev = &g_device;
    UINT64 fenceValue = dev->currentFenceValue++;
    dev->graphicsQueue->Signal(dev->frames[dev->currentFrameIndex].fence.Get(), fenceValue);
    if (dev->frames[dev->currentFrameIndex].fence->GetCompletedValue() < fenceValue) {
        dev->frames[dev->currentFrameIndex].fence->SetEventOnCompletion(fenceValue, dev->frames[dev->currentFrameIndex].fenceEvent);
        WaitForSingleObject(dev->frames[dev->currentFrameIndex].fenceEvent, INFINITE);
    }
}

// Move to next frame
void DX12_NextFrame(void) {
    dx12Device_t* dev = &g_device;
    dev->currentFrameIndex = (dev->currentFrameIndex + 1) % DX12_MAX_FRAMES_IN_FLIGHT;
    
    // Wait for this frame's fence
    UINT64 fenceValue = dev->frames[dev->currentFrameIndex].fenceValue;
    if (fenceValue > 0) {
        if (dev->frames[dev->currentFrameIndex].fence->GetCompletedValue() < fenceValue) {
            dev->frames[dev->currentFrameIndex].fence->SetEventOnCompletion(fenceValue, dev->frames[dev->currentFrameIndex].fenceEvent);
            WaitForSingleObject(dev->frames[dev->currentFrameIndex].fenceEvent, INFINITE);
        }
    }
    
    // Reset allocator and command list
    dev->frames[dev->currentFrameIndex].allocator->Reset();
    dev->frames[dev->currentFrameIndex].cmdList->Reset(dev->frames[dev->currentFrameIndex].allocator.Get(), nullptr);
}

// Get current command list
ID3D12GraphicsCommandList6* DX12_GetCommandList(void) {
    return g_device.frames[g_device.currentFrameIndex].cmdList.Get();
}

// Get current graphics queue
ID3D12CommandQueue* DX12_GetGraphicsQueue(void) {
    return g_device.graphicsQueue.Get();
}

// Check RT support
qboolean DX12_IsRayTracingSupported(void) {
    return g_device.rtSupported;
}

// Get device for resource creation
ID3D12Device8* DX12_GetD3DDevice(void) {
    return g_device.device.Get();
}