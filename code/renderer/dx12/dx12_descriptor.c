/*
===========================================================================
DX12 Descriptor Heaps
===========================================================================
*/
#include "dx12_local.h"

using Microsoft::WRL::ComPtr;

struct dx12DescriptorHeap_s {
    ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_DESCRIPTOR_HEAP_TYPE type;
    UINT descriptorSize;
    UINT numDescriptors;
    UINT currentIndex;
    std::vector<UINT> freeIndices; // Simple free list
};

static dx12DescriptorHeap_t g_heaps[4]; // CBV_SRV_UAV, SAMPLER, RTV, DSV

qboolean DX12_InitDescriptorHeaps(void) {
    dx12Device_t* dev = DX12_GetDevice();
    
    // CBV/SRV/UAV heap (shader visible)
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = DX12_CBV_SRV_UAV_HEAP_SIZE;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    desc.NodeMask = 0;
    DX12_CHECK(dev->device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_heaps[0].heap)), "CBV/SRV/UAV heap");
    g_heaps[0].type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    g_heaps[0].descriptorSize = dev->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    g_heaps[0].numDescriptors = DX12_CBV_SRV_UAV_HEAP_SIZE;
    g_heaps[0].currentIndex = 0;
    g_heaps[0].heap->SetName(L"CBV/SRV/UAV Heap");

    // Sampler heap (shader visible)
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    desc.NumDescriptors = DX12_SAMPLER_HEAP_SIZE;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    DX12_CHECK(dev->device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_heaps[1].heap)), "Sampler heap");
    g_heaps[1].type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    g_heaps[1].descriptorSize = dev->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    g_heaps[1].numDescriptors = DX12_SAMPLER_HEAP_SIZE;
    g_heaps[1].currentIndex = 0;
    g_heaps[1].heap->SetName(L"Sampler Heap");

    // RTV heap (not shader visible)
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    desc.NumDescriptors = DX12_RTV_HEAP_SIZE;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    DX12_CHECK(dev->device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_heaps[2].heap)), "RTV heap");
    g_heaps[2].type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    g_heaps[2].descriptorSize = dev->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    g_heaps[2].numDescriptors = DX12_RTV_HEAP_SIZE;
    g_heaps[2].currentIndex = 0;
    g_heaps[2].heap->SetName(L"RTV Heap");

    // DSV heap (not shader visible)
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    desc.NumDescriptors = DX12_DSV_HEAP_SIZE;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    DX12_CHECK(dev->device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_heaps[3].heap)), "DSV heap");
    g_heaps[3].type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    g_heaps[3].descriptorSize = dev->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    g_heaps[3].numDescriptors = DX12_DSV_HEAP_SIZE;
    g_heaps[3].currentIndex = 0;
    g_heaps[3].heap->SetName(L"DSV Heap");

    Com_Printf("DX12: Descriptor heaps created\n");
    return qtrue;
}

void DX12_ShutdownDescriptorHeaps(void) {
    for (int i = 0; i < 4; ++i) {
        g_heaps[i].heap.Reset();
    }
}

// Allocate descriptor(s)
D3D12_CPU_DESCRIPTOR_HANDLE DX12_AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT count, D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle) {
    int heapIndex = -1;
    switch (type) {
        case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: heapIndex = 0; break;
        case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER: heapIndex = 1; break;
        case D3D12_DESCRIPTOR_HEAP_TYPE_RTV: heapIndex = 2; break;
        case D3D12_DESCRIPTOR_HEAP_TYPE_DSV: heapIndex = 3; break;
    }
    
    dx12DescriptorHeap_t* heap = &g_heaps[heapIndex];
    
    // Simple linear allocation (could use free list for reuse)
    if (heap->currentIndex + count > heap->numDescriptors) {
        Com_Printf("DX12: Descriptor heap exhausted for type %d\n", type);
        return {0};
    }
    
    D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (size_t)heap->currentIndex * heap->descriptorSize;
    
    if (gpuHandle && (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)) {
        gpuHandle->ptr = heap->heap->GetGPUDescriptorHandleForHeapStart().ptr + (size_t)heap->currentIndex * heap->descriptorSize;
    } else if (gpuHandle) {
        gpuHandle->ptr = 0;
    }
    
    heap->currentIndex += count;
    return handle;
}

// Get heap for binding
ID3D12DescriptorHeap* DX12_GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) {
    int heapIndex = -1;
    switch (type) {
        case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: return g_heaps[0].heap.Get();
        case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER: return g_heaps[1].heap.Get();
        case D3D12_DESCRIPTOR_HEAP_TYPE_RTV: return g_heaps[2].heap.Get();
        case D3D12_DESCRIPTOR_HEAP_TYPE_DSV: return g_heaps[3].heap.Get();
    }
    return nullptr;
}

// Get GPU handle for shader visible heap
D3D12_GPU_DESCRIPTOR_HANDLE DX12_GetGPUDescriptorHandle(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT index) {
    int heapIndex = -1;
    switch (type) {
        case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV: heapIndex = 0; break;
        case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER: heapIndex = 1; break;
        default: return {0};
    }
    D3D12_GPU_DESCRIPTOR_HANDLE handle = g_heaps[heapIndex].heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += (size_t)index * g_heaps[heapIndex].descriptorSize;
    return handle;
}