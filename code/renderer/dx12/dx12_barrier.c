/*
===========================================================================
DX12 Resource Barrier Tracking
===========================================================================
*/
#include "dx12_local.h"

using Microsoft::WRL::ComPtr;

struct dx12Resource_s {
    ComPtr<ID3D12Resource> resource;
    D3D12_RESOURCE_STATES currentState;
    D3D12_RESOURCE_DESC desc;
    wchar_t name[64];
};

#define MAX_PENDING_BARRIERS 64

static D3D12_RESOURCE_BARRIER g_pendingBarriers[MAX_PENDING_BARRIERS];
static int g_numPendingBarriers = 0;

void DX12_ResourceBarrier(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) {
    if (stateBefore == stateAfter) return;
    
    if (g_numPendingBarriers >= MAX_PENDING_BARRIERS) {
        cmdList->ResourceBarrier(g_numPendingBarriers, g_pendingBarriers);
        g_numPendingBarriers = 0;
    }
    
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    g_pendingBarriers[g_numPendingBarriers++] = barrier;
}

void DX12_FlushBarriers(ID3D12GraphicsCommandList* cmdList) {
    if (g_numPendingBarriers > 0) {
        cmdList->ResourceBarrier(g_numPendingBarriers, g_pendingBarriers);
        g_numPendingBarriers = 0;
    }
}

void DX12_UAVBarrier(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* resource) {
    if (g_numPendingBarriers >= MAX_PENDING_BARRIERS) {
        cmdList->ResourceBarrier(g_numPendingBarriers, g_pendingBarriers);
        g_numPendingBarriers = 0;
    }
    
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = resource;
    
    g_pendingBarriers[g_numPendingBarriers++] = barrier;
}

void DX12_AliasingBarrier(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* before, ID3D12Resource* after) {
    if (g_numPendingBarriers >= MAX_PENDING_BARRIERS) {
        cmdList->ResourceBarrier(g_numPendingBarriers, g_pendingBarriers);
        g_numPendingBarriers = 0;
    }
    
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Aliasing.pResourceBefore = before;
    barrier.Aliasing.pResourceAfter = after;
    
    g_pendingBarriers[g_numPendingBarriers++] = barrier;
}

// Resource creation helpers
qboolean DX12_CreateBuffer(UINT64 size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState, ID3D12Resource** buffer, const void* initialData) {
    dx12Device_t* dev = DX12_GetDevice();
    
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = heapType;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;
    
    DX12_CHECK(dev->device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, initialState, nullptr, IID_PPV_ARGS(buffer)), "CreateBuffer");
    
    if (initialData) {
        void* mapped;
        (*buffer)->Map(0, nullptr, &mapped);
        memcpy(mapped, initialData, (size_t)size);
        (*buffer)->Unmap(0, nullptr);
    }
    
    return qtrue;
}

qboolean DX12_CreateTexture2D(UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initialState, ID3D12Resource** texture, const D3D12_CLEAR_VALUE* clearValue) {
    dx12Device_t* dev = DX12_GetDevice();
    
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = flags;
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;
    
    DX12_CHECK(dev->device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, initialState, clearValue, IID_PPV_ARGS(texture)), "CreateTexture2D");
    
    return qtrue;
}