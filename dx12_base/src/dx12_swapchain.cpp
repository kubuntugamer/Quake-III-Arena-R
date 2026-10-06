// dx12_swapchain.cpp
#include "dx12_swapchain.h"

#pragma comment(lib, "dxgi.lib")

namespace dx12base {

bool Swapchain::Create(HWND window, UINT width, UINT height, D3DDevice& device) {
    if (width == 0 || height == 0) {
        return false;
    }

    IDXGIFactory4* swapFactory = device.Factory();
    if (!swapFactory) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kFramesInFlight;
    desc.Scaling = DXGI_SCALING_NONE;
    // Flip model requires FLIP_DISCARD on Windows 10 1809+ and gives us
    // hardware free-wait with DXGI_PRESENT_ALLOW_TEARING.
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    ComPtr<IDXGISwapChain1> swapchain1;
    if (FAILED(swapFactory->CreateSwapChainForHwnd(
            device.GraphicsQueue(), window, &desc, nullptr, nullptr, &swapchain1))) {
        return false;
    }

    if (FAILED(swapchain1.As(&swapchain_))) {
        return false;
    }

    // Block the automatic Alt+Enter toggle; we handle fullscreen ourselves.
    swapFactory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);

    width_ = width;
    height_ = height;
    format_ = desc.Format;
    depthFormat_ = DXGI_FORMAT_D32_FLOAT;
    // IDXGISwapChain4 has no GetBufferCount, so remember what we asked for.
    bufferCount_ = desc.BufferCount;

    return CreateRenderTargets(device);
}

bool Swapchain::CreateRenderTargets(D3DDevice& device) {
    backBuffers_.resize(bufferCount_);
    backBufferRtvs_.resize(bufferCount_);

    for (UINT i = 0; i < bufferCount_; ++i) {
        if (FAILED(swapchain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i])))) {
            return false;
        }

        const wchar_t* name = L"Back Buffer";
        backBuffers_[i]->SetName(name);

        backBufferRtvs_[i] = device.AllocateRtv(1);
        device.Device()->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, backBufferRtvs_[i]);
    }

    // Depth buffer: created once per swapchain, recreated on resize.
    D3D12_RESOURCE_DESC depthDesc{};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = width_;
    depthDesc.Height = height_;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = depthFormat_;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clear{};
    clear.Format = depthFormat_;
    clear.DepthStencil.Depth = 1.0f;
    clear.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;

    if (FAILED(device.Device()->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
            IID_PPV_ARGS(&depthBuffer_)))) {
        return false;
    }

    depthBuffer_->SetName(L"Depth Buffer");
    depthRtv_ = device.AllocateDsv(1);
    device.Device()->CreateDepthStencilView(depthBuffer_.Get(), nullptr, depthRtv_);

    return true;
}

bool Swapchain::Resize(UINT width, UINT height, D3DDevice& device) {
    if (width == 0 || height == 0 || (width == width_ && height == height_)) {
        return false;
    }

    // The GPU may still be reading these resources. Wait before releasing them.
    device.WaitForGpu();

    backBuffers_.clear();
    backBufferRtvs_.clear();
    depthBuffer_.Reset();

    if (FAILED(swapchain_->ResizeBuffers(0, width, height, format_, 0))) {
        return false;
    }

    width_ = width;
    height_ = height;

    return CreateRenderTargets(device);
}

void Swapchain::Present(D3DDevice& device, bool vsync) {
    // Device is not needed here yet: Present blocks on the queue itself. A
    // port that handles DXGI_ERROR_DEVICE_REMOVED will use it to rebuild.
    (void)device;

    const UINT syncInterval = vsync ? 1 : 0;

    const HRESULT hr = swapchain_->Present(syncInterval, DXGI_PRESENT_ALLOW_TEARING);

    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        // The GPU was reset by the driver or another fullscreen app. Swallowing
        // this lets the caller's loop keep running; a real sample would rebuild
        // the device here.
        return;
    }
}

void Swapchain::Destroy() {
    backBuffers_.clear();
    backBufferRtvs_.clear();
    depthBuffer_.Reset();
    swapchain_.Reset();
}

ID3D12Resource* Swapchain::BackBuffer() const {
    if (backBuffers_.empty()) {
        return nullptr;
    }
    const UINT index = swapchain_->GetCurrentBackBufferIndex();
    return index < backBuffers_.size() ? backBuffers_[index].Get() : nullptr;
}

D3D12_CPU_DESCRIPTOR_HANDLE Swapchain::BackBufferRtv() const {
    if (backBufferRtvs_.empty()) {
        return {0};
    }
    const UINT index = swapchain_->GetCurrentBackBufferIndex();
    return index < backBufferRtvs_.size() ? backBufferRtvs_[index] : D3D12_CPU_DESCRIPTOR_HANDLE{0};
}

D3D12_CPU_DESCRIPTOR_HANDLE Swapchain::DepthRtv() const {
    return depthRtv_;
}

float Swapchain::AspectRatio() const {
    return height_ ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f;
}

}  // namespace dx12base