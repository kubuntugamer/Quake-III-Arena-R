// dx12_device.cpp
#include "dx12_device.h"

#include <cassert>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace dx12base {

namespace {

// Lets the debug layer and GPU-based validation talk to the console when the
// sample runs from a debugger or a CI job.
void EnableDebugLayer() {
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }
#else
    (void)0;
#endif
}

}  // namespace

D3D_FEATURE_LEVEL D3DDevice::FindHighestFeatureLevel(IDXGIAdapter1* adapter) {
    static constexpr D3D_FEATURE_LEVEL kLevels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    for (D3D_FEATURE_LEVEL level : kLevels) {
        if (SUCCEEDED(D3D12CreateDevice(adapter, level, __uuidof(ID3D12Device), nullptr))) {
            return level;
        }
    }
    return D3D_FEATURE_LEVEL_11_0;
}

bool D3DDevice::Initialize(HWND window) {
    (void)window;
    EnableDebugLayer();

    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory_)))) {
        return false;
    }

    // Prefer the adapter with the most dedicated VRAM. On hybrid laptops the
    // discrete part is usually the one the user wants this running on.
    ComPtr<IDXGIAdapter1> candidate;

    for (UINT i = 0; factory_->EnumAdapters1(i, &candidate) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        candidate->GetDesc1(&desc);

        // Skip the software rasteriser; it cannot run this sample meaningfully.
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            candidate.Reset();
            continue;
        }

        if (FindHighestFeatureLevel(candidate.Get()) < D3D_FEATURE_LEVEL_11_0) {
            candidate.Reset();
            continue;
        }

        DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};

// QueryVideoMemoryInfo only exists on IDXGIAdapter3 and later, so ask for
        // the newer interface just for this query.
        ComPtr<IDXGIAdapter4> adapter4;
        if (SUCCEEDED(candidate.As(&adapter4))) {
            adapter4->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo);
        } else {
            // Without the query we cannot compare budgets, so keep the first
            // capable adapter we find rather than guessing.
            memInfo.Budget = (bestBudget_ == 0) ? 1 : bestBudget_;
        }

        if (memInfo.Budget > bestBudget_) {
            bestBudget_ = memInfo.Budget;
            adapterName_ = desc.Description;
            adapter_ = candidate;
        }
        candidate.Reset();
    }

    if (!adapter_) {
        return false;
    }

    const D3D_FEATURE_LEVEL level = FindHighestFeatureLevel(adapter_.Get());
    if (FAILED(D3D12CreateDevice(adapter_.Get(), level, IID_PPV_ARGS(&device_)))) {
        return false;
    }

    // Ask for RT tier information so the README and any future RT path know
    // whether they are running on capable hardware.
    device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &d3d12Options5_, sizeof(d3d12Options5_));

    return CreateCommandQueues() && CreateDescriptorHeaps() && CreateFences();
}

bool D3DDevice::CreateCommandQueues() {
    auto makeQueue = [this](D3D12_COMMAND_LIST_TYPE type, const wchar_t* name) {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Type = type;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

        ComPtr<ID3D12CommandQueue> queue;
        if (FAILED(device_->CreateCommandQueue(&desc, IID_PPV_ARGS(&queue)))) {
            return false;
        }
        queue->SetName(name);

        if (type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            graphicsQueue_ = queue;
        } else {
            copyQueue_ = queue;
        }
        return true;
    };

    return makeQueue(D3D12_COMMAND_LIST_TYPE_DIRECT, L"Graphics Queue") &&
           makeQueue(D3D12_COMMAND_LIST_TYPE_COPY, L"Copy Queue");
}

bool D3DDevice::CreateDescriptorHeaps() {
    // CBV / SRV / UAV: shader visible so shaders can index them directly.
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = kMaxDescriptors;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&cbvSrvUavHeap_)))) {
            return false;
        }
        cbvSrvUavHeap_->SetName(L"CBV/SRV/UAV Heap");
    }

    // Samplers: shader visible, much smaller than the CBV/SRV/UAV heap.
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        desc.NumDescriptors = 256;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&samplerHeap_)))) {
            return false;
        }
        samplerHeap_->SetName(L"Sampler Heap");
    }

    // RTV and DSV are never bound to shaders, so no shader-visible flag.
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = kMaxRtvDescriptors;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (FAILED(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvHeap_)))) {
            return false;
        }
        rtvHeap_->SetName(L"RTV Heap");
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.NumDescriptors = kMaxDsvDescriptors;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (FAILED(device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&dsvHeap_)))) {
            return false;
        }
        dsvHeap_->SetName(L"DSV Heap");
    }

    return true;
}

bool D3DDevice::CreateFences() {
    for (FrameContext& frame : frames_) {
        if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                   IID_PPV_ARGS(&frame.allocator)))) {
            return false;
        }
        frame.allocator->SetName(L"Per Frame Allocator");

        if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              frame.allocator.Get(), nullptr,
                                              IID_PPV_ARGS(&frame.cmdList)))) {
            return false;
        }
        frame.cmdList->SetName(L"Per Frame Command List");

        // A freshly created command list is in the recording state. Close it so
        // the first BeginFrame can Reset() it cleanly.
        frame.cmdList->Close();

        if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                        IID_PPV_ARGS(&frame.fence)))) {
            return false;
        }
        frame.fence->SetName(L"Per Frame Fence");

        // An auto-reset event is signalled when the fence reaches the value we
        // ask for. Created up front because SetEventOnCompletion is cheap but
        // CreateEvent is not.
        frame.fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!frame.fenceEvent) {
            return false;
        }
    }
    return true;
}

void D3DDevice::WaitForGpu() {
    const UINT64 fenceValue = nextFenceValue_++;
    FrameContext& frame = CurrentFrame();

    if (FAILED(graphicsQueue_->Signal(frame.fence.Get(), fenceValue))) {
        return;
    }

    if (frame.fence->GetCompletedValue() < fenceValue) {
        if (FAILED(frame.fence->SetEventOnCompletion(fenceValue, frame.fenceEvent))) {
            return;
        }
        WaitForSingleObject(frame.fenceEvent, INFINITE);
    }
}

void D3DDevice::AdvanceFrame() {
    frameIndex_ = (frameIndex_ + 1) % kFramesInFlight;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DDevice::AllocateRtv(UINT count) {
    if (rtvCursor_ + count > kMaxRtvDescriptors) {
        return {0};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    const UINT stride = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    handle.ptr += static_cast<SIZE_T>(rtvCursor_) * stride;
    rtvCursor_ += count;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DDevice::AllocateDsv(UINT count) {
    if (dsvCursor_ + count > kMaxDsvDescriptors) {
        return {0};
    }

    D3D12_CPU_DESCRIPTOR_HANDLE handle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
    const UINT stride = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    handle.ptr += static_cast<SIZE_T>(dsvCursor_) * stride;
    dsvCursor_ += count;
    return handle;
}

void D3DDevice::Shutdown() {
    WaitForGpu();

    for (FrameContext& frame : frames_) {
        frame.Reset();
    }

    rtvHeap_.Reset();
    dsvHeap_.Reset();
    samplerHeap_.Reset();
    cbvSrvUavHeap_.Reset();

    graphicsQueue_.Reset();
    copyQueue_.Reset();
    device_.Reset();
    adapter_.Reset();
    factory_.Reset();
}

}  // namespace dx12base