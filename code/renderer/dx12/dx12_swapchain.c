/*
===========================================================================
DX12 Swapchain
===========================================================================
*/
#include "dx12_local.h"

using Microsoft::WRL::ComPtr;

struct dx12Swapchain_s {
    ComPtr<IDXGISwapChain4> swapchain;
    ComPtr<ID3D12Resource> backBuffers[DX12_MAX_FRAMES_IN_FLIGHT];
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[DX12_MAX_FRAMES_IN_FLIGHT];
    UINT width, height;
    DXGI_FORMAT format;
    UINT bufferCount;
    UINT currentBuffer;
    BOOL fullscreen;
    BOOL vsync;
    HWND hwnd;
};

static dx12Swapchain_t g_swapchain = {};

qboolean DX12_CreateSwapchain(HWND hwnd, int width, int height, BOOL fullscreen, BOOL vsync) {
    dx12Device_t* dev = DX12_GetDevice();
    
    g_swapchain.hwnd = hwnd;
    g_swapchain.width = width;
    g_swapchain.height = height;
    g_swapchain.fullscreen = fullscreen;
    g_swapchain.vsync = vsync;
    g_swapchain.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    g_swapchain.bufferCount = DX12_MAX_FRAMES_IN_FLIGHT;
    g_swapchain.currentBuffer = 0;

    // Release old swapchain
    for (int i = 0; i < DX12_MAX_FRAMES_IN_FLIGHT; ++i) {
        g_swapchain.backBuffers[i].Reset();
    }
    g_swapchain.swapchain.Reset();

    // Create swapchain
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = g_swapchain.format;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = g_swapchain.bufferCount;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = fullscreen ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0;

    ComPtr<IDXGISwapChain1> swapchain1;
    DX12_CHECK(dev->factory->CreateSwapChainForHwnd(
        dev->graphicsQueue.Get(), hwnd, &desc, nullptr, nullptr, &swapchain1), "CreateSwapChainForHwnd");
    
    DX12_CHECK(swapchain1.As(&g_swapchain.swapchain), "Query swapchain4");

    // Disable Alt+Enter fullscreen toggle
    dev->factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    // Create RTVs for back buffers
    dx12Device_t* devPtr = DX12_GetDevice();
    for (UINT i = 0; i < g_swapchain.bufferCount; ++i) {
        DX12_CHECK(g_swapchain.swapchain->GetBuffer(i, IID_PPV_ARGS(&g_swapchain.backBuffers[i])), "GetBuffer");
        
        wchar_t name[64];
        swprintf_s(name, L"BackBuffer %d", i);
        g_swapchain.backBuffers[i]->SetName(name);
        
        g_swapchain.rtvHandles[i] = DX12_AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, nullptr);
        devPtr->device->CreateRenderTargetView(g_swapchain.backBuffers[i].Get(), nullptr, g_swapchain.rtvHandles[i]);
    }

    g_swapchain.currentBuffer = g_swapchain.swapchain->GetCurrentBackBufferIndex();

    Com_Printf("DX12: Swapchain created %dx%d %s %s\n", width, height, fullscreen ? "fullscreen" : "windowed", vsync ? "vsync" : "no-vsync");
    return qtrue;
}

void DX12_ResizeSwapchain(int width, int height) {
    if (width == g_swapchain.width && height == g_swapchain.height) return;
    
    dx12Device_t* dev = DX12_GetDevice();
    DX12_WaitForGPU();
    
    // Release back buffers
    for (int i = 0; i < DX12_MAX_FRAMES_IN_FLIGHT; ++i) {
        g_swapchain.backBuffers[i].Reset();
    }
    
    DX12_CHECK(g_swapchain.swapchain->ResizeBuffers(0, width, height, g_swapchain.format, 0), "ResizeBuffers");
    
    g_swapchain.width = width;
    g_swapchain.height = height;
    g_swapchain.currentBuffer = g_swapchain.swapchain->GetCurrentBackBufferIndex();
    
    // Recreate RTVs
    dx12Device_t* devPtr = DX12_GetDevice();
    for (UINT i = 0; i < g_swapchain.bufferCount; ++i) {
        DX12_CHECK(g_swapchain.swapchain->GetBuffer(i, IID_PPV_ARGS(&g_swapchain.backBuffers[i])), "GetBuffer after resize");
        
        wchar_t name[64];
        swprintf_s(name, L"BackBuffer %d", i);
        g_swapchain.backBuffers[i]->SetName(name);
        
        g_swapchain.rtvHandles[i] = DX12_AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, nullptr);
        devPtr->device->CreateRenderTargetView(g_swapchain.backBuffers[i].Get(), nullptr, g_swapchain.rtvHandles[i]);
    }
    
    Com_Printf("DX12: Swapchain resized to %dx%d\n", width, height);
}

void DX12_Present(void) {
    UINT syncInterval = g_swapchain.vsync ? 1 : 0;
    UINT flags = g_swapchain.vsync ? 0 : DXGI_PRESENT_ALLOW_TEARING;
    
    HRESULT hr = g_swapchain.swapchain->Present(syncInterval, flags);
    
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        Com_Printf("DX12: Device removed/reset, need to recreate\n");
        // TODO: Handle device loss
    } else if (FAILED(hr)) {
        Com_Printf("DX12: Present failed: 0x%08X\n", hr);
    }
    
    g_swapchain.currentBuffer = g_swapchain.swapchain->GetCurrentBackBufferIndex();
}

void DX12_SetFullscreen(BOOL fullscreen) {
    if (g_swapchain.fullscreen != fullscreen) {
        g_swapchain.swapchain->SetFullscreenState(fullscreen, nullptr);
        g_swapchain.fullscreen = fullscreen;
    }
}

void DX12_SetVSync(BOOL vsync) {
    g_swapchain.vsync = vsync;
}

// Get current back buffer RTV
D3D12_CPU_DESCRIPTOR_HANDLE DX12_GetCurrentRTV(void) {
    return g_swapchain.rtvHandles[g_swapchain.currentBuffer];
}

ID3D12Resource* DX12_GetCurrentBackBuffer(void) {
    return g_swapchain.backBuffers[g_swapchain.currentBuffer].Get();
}

UINT DX12_GetSwapchainWidth(void) { return g_swapchain.width; }
UINT DX12_GetSwapchainHeight(void) { return g_swapchain.height; }