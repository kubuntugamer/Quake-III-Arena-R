// dx12_swapchain.h - swapchain and the render targets derived from it
#pragma once

#include "dx12_device.h"

#include <vector>

namespace dx12base {

// Wraps IDXGISwapChain4 plus the RTVs for its back buffers and a matching
// depth buffer. Handles resize by tearing down and rebuilding both.
class Swapchain {
public:
    bool Create(HWND window, UINT width, UINT height, D3DDevice& device);
    void Destroy();

    // Rebuilds at a new size. Returns false if the caller should stop.
    bool Resize(UINT width, UINT height, D3DDevice& device);

    void Present(D3DDevice& device, bool vsync);

    ID3D12Resource* BackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE BackBufferRtv() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthRtv() const;

    UINT Width() const { return width_; }
    UINT Height() const { return height_; }
    UINT CurrentBackBufferIndex() const { return swapchain_ ? swapchain_->GetCurrentBackBufferIndex() : 0; }
    DXGI_FORMAT BackBufferFormat() const { return format_; }
    DXGI_FORMAT DepthFormat() const { return depthFormat_; }
    float AspectRatio() const;

private:
    bool CreateRenderTargets(D3DDevice& device);

    ComPtr<IDXGISwapChain4> swapchain_;
    std::vector<ComPtr<ID3D12Resource>> backBuffers_;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> backBufferRtvs_;

    ComPtr<ID3D12Resource> depthBuffer_;
    D3D12_CPU_DESCRIPTOR_HANDLE depthRtv_{};

    UINT width_ = 0;
    UINT height_ = 0;
    UINT bufferCount_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT depthFormat_ = DXGI_FORMAT_UNKNOWN;
};

}  // namespace dx12base