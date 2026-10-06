// dx12_device.h - DX12 device, command queues, and descriptor heaps
#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <string>

namespace dx12base {

using Microsoft::WRL::ComPtr;

// Number of frames we can have in flight before waiting on the GPU.
inline constexpr UINT kFramesInFlight = 2;

// Per-frame resources: allocator, command list, fence, fence value.
struct FrameContext {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> cmdList;
    ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = nullptr;
    UINT64 fenceValue = 0;

    void Reset() {
        allocator.Reset();
        cmdList.Reset();
        fence.Reset();
        if (fenceEvent) {
            CloseHandle(fenceEvent);
            fenceEvent = nullptr;
        }
        fenceValue = 0;
    }
};

// Owns the device, the command queues and all descriptor heaps.
// Everything else in the sample takes a D3DDevice& and asks it for heaps.
class D3DDevice {
public:
    // Picks the best available adapter, creates a device at feature level
    // 12.1 and sets up the three standard command queues.
    bool Initialize(HWND window);

    void Shutdown();

    // Blocks until the GPU has finished every command submitted so far.
    void WaitForGpu();

    // Returns the per-frame context for the frame currently being recorded.
    // Cycles through kFramesInFlight internally.
    FrameContext& CurrentFrame() { return frames_[frameIndex_]; }
    void AdvanceFrame();

    UINT CurrentFrameIndex() const { return frameIndex_; }

    ID3D12Device*               Device() const { return device_.Get(); }
    // Non-const because recording mutates the list through the allocator.
    ID3D12GraphicsCommandList*  CommandList() { return CurrentFrame().cmdList.Get(); }
    ID3D12CommandQueue*         GraphicsQueue() const { return graphicsQueue_.Get(); }

    // Swapchain creation has to go through the factory, not the device.
    IDXGIFactory4*              Factory() const { return factory_.Get(); }

    // Descriptor heaps. Sized once at startup; the sample never grows past them.
    ID3D12DescriptorHeap* CbvSrvUavHeap() const { return cbvSrvUavHeap_.Get(); }
    ID3D12DescriptorHeap* SamplerHeap() const { return samplerHeap_.Get(); }
    ID3D12DescriptorHeap* RtvHeap() const { return rtvHeap_.Get(); }
    ID3D12DescriptorHeap* DsvHeap() const { return dsvHeap_.Get(); }

    static constexpr UINT kMaxDescriptors = 1024;

    // Simple bump allocator over RtvHeap, used for swapchain and offscreen targets.
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateRtv(UINT count);
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateDsv(UINT count);

    // Reports whether the chosen adapter exposes ray tracing at tier 1.0+.
    bool RayTracingTier1() const { return d3d12Options5_.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0; }

    const std::wstring& AdapterName() const { return adapterName_; }

private:
    bool CreateCommandQueues();
    bool CreateDescriptorHeaps();
    bool CreateFences();

    // Dumps the highest feature level the adapter supports so the README
    // can claim an honest minimum requirement.
    static D3D_FEATURE_LEVEL FindHighestFeatureLevel(IDXGIAdapter1* adapter);

    ComPtr<IDXGIFactory6>          factory_;
    ComPtr<IDXGIAdapter1>          adapter_;
    ComPtr<ID3D12Device>           device_;
    ComPtr<ID3D12CommandQueue>     graphicsQueue_;
    ComPtr<ID3D12CommandQueue>     copyQueue_;

    ComPtr<ID3D12DescriptorHeap>   cbvSrvUavHeap_;
    ComPtr<ID3D12DescriptorHeap>   samplerHeap_;
    ComPtr<ID3D12DescriptorHeap>   rtvHeap_;
    ComPtr<ID3D12DescriptorHeap>   dsvHeap_;

    FrameContext frames_[kFramesInFlight];
    UINT frameIndex_ = 0;
    UINT64 nextFenceValue_ = 1;

    UINT  rtvCursor_ = 0;
    UINT  dsvCursor_ = 0;

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 d3d12Options5_{};
    std::wstring adapterName_;
    SIZE_T bestBudget_ = 0;

    static constexpr UINT kMaxRtvDescriptors = 64;
    static constexpr UINT kMaxDsvDescriptors = 8;
};

}  // namespace dx12base