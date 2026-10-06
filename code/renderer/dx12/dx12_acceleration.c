/*
===========================================================================
DX12 Acceleration Structures (BLAS/TLAS) for Ray Tracing
===========================================================================
*/
#include "dx12_local.h"

using Microsoft::WRL::ComPtr;

struct dx12AccelerationStructure_s {
    ComPtr<ID3D12Resource> buffer;
    ComPtr<ID3D12Resource> scratchBuffer;
    D3D12_GPU_VIRTUAL_ADDRESS gpuVA;
    UINT64 size;
    BOOL isTopLevel;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs;
};

static std::vector<dx12AccelerationStructure_s*> g_accelerationStructures;

dx12AccelerationStructure_s* DX12_CreateBottomLevelAS(UINT numGeometries, const D3D12_RAYTRACING_GEOMETRY_DESC* geometries) {
    dx12Device_t* dev = DX12_GetDevice();
    
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = numGeometries;
    inputs.pGeometryDescs = geometries;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE | 
                   D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;
    
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
    dev->device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
    
    // Align sizes
    UINT64 scratchSize = align_up(prebuildInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
    UINT64 resultSize = align_up(prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
    
    dx12AccelerationStructure_s* as = new dx12AccelerationStructure_s();
    as->isTopLevel = FALSE;
    as->inputs = inputs;
    as->size = resultSize;
    
    // Create result buffer
    DX12_CreateBuffer(resultSize, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, &as->buffer, nullptr);
    as->buffer->SetName(L"BLAS Buffer");
    as->gpuVA = as->buffer->GetGPUVirtualAddress();
    
    // Create scratch buffer
    DX12_CreateBuffer(scratchSize, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &as->scratchBuffer, nullptr);
    as->scratchBuffer->SetName(L"BLAS Scratch");
    
    // Build AS
    ID3D12GraphicsCommandList4* cmdList4 = nullptr;
    DX12_GetCommandList()->QueryInterface(IID_PPV_ARGS(&cmdList4));
    
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs = inputs;
    buildDesc.DestAccelerationStructureData = as->gpuVA;
    buildDesc.ScratchAccelerationStructureData = as->scratchBuffer->GetGPUVirtualAddress();
    
    cmdList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    
    // UAV barrier before compaction
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = as->buffer.Get();
    DX12_GetCommandList()->ResourceBarrier(1, &uavBarrier);
    
    // Query compacted size
    ComPtr<ID3D12Resource> compactedSizeBuffer;
    DX12_CreateBuffer(sizeof(UINT64), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, &compactedSizeBuffer, nullptr);
    
    cmdList4->EmitRaytracingAccelerationStructurePostbuildInfo(
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_COMPACTED_SIZE,
        0, nullptr, compactedSizeBuffer.Get());
    
    DX12_FlushBarriers(DX12_GetCommandList());
    DX12_ExecuteCommandList();
    DX12_WaitForGPU();
    
    // Read compacted size
    UINT64* compactedSize = nullptr;
    compactedSizeBuffer->Map(0, nullptr, (void**)&compactedSize);
    UINT64 compactedSizeValue = *compactedSize;
    compactedSizeBuffer->Unmap(0, nullptr);
    
    // Create compacted buffer
    ComPtr<ID3D12Resource> compactedBuffer;
    DX12_CreateBuffer(compactedSizeValue, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, &compactedBuffer, nullptr);
    compactedBuffer->SetName(L"BLAS Compacted");
    
    // Copy to compacted
    cmdList4->CopyRaytracingAccelerationStructure(compactedBuffer->GetGPUVirtualAddress(), as->gpuVA, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT);
    
    DX12_FlushBarriers(DX12_GetCommandList());
    DX12_ExecuteCommandList();
    DX12_WaitForGPU();
    
    // Swap buffers
    as->buffer = compactedBuffer;
    as->gpuVA = as->buffer->GetGPUVirtualAddress();
    as->size = compactedSizeValue;
    as->scratchBuffer.Reset();
    
    g_accelerationStructures.push_back(as);
    return as;
}

dx12AccelerationStructure_s* DX12_CreateTopLevelAS(UINT numInstances, const D3D12_RAYTRACING_INSTANCE_DESC* instances) {
    dx12Device_t* dev = DX12_GetDevice();
    
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = numInstances;
    inputs.pInstanceDescs = instances;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE | 
                   D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
    dev->device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);
    
    UINT64 scratchSize = align_up(prebuildInfo.ScratchDataSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
    UINT64 resultSize = align_up(prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
    
    dx12AccelerationStructure_s* as = new dx12AccelerationStructure_s();
    as->isTopLevel = TRUE;
    as->inputs = inputs;
    as->size = resultSize;
    
    DX12_CreateBuffer(resultSize, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, &as->buffer, nullptr);
    as->buffer->SetName(L"TLAS Buffer");
    as->gpuVA = as->buffer->GetGPUVirtualAddress();
    
    DX12_CreateBuffer(scratchSize, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &as->scratchBuffer, nullptr);
    as->scratchBuffer->SetName(L"TLAS Scratch");
    
    ID3D12GraphicsCommandList4* cmdList4 = nullptr;
    DX12_GetCommandList()->QueryInterface(IID_PPV_ARGS(&cmdList4));
    
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs = inputs;
    buildDesc.DestAccelerationStructureData = as->gpuVA;
    buildDesc.ScratchAccelerationStructureData = as->scratchBuffer->GetGPUVirtualAddress();
    
    cmdList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    
    DX12_FlushBarriers(DX12_GetCommandList());
    DX12_ExecuteCommandList();
    DX12_WaitForGPU();
    
    g_accelerationStructures.push_back(as);
    return as;
}

void DX12_UpdateTopLevelAS(dx12AccelerationStructure_s* as, UINT numInstances, const D3D12_RAYTRACING_INSTANCE_DESC* instances) {
    if (!as->isTopLevel) return;
    
    as->inputs.NumDescs = numInstances;
    as->inputs.pInstanceDescs = instances;
    
    ID3D12GraphicsCommandList4* cmdList4 = nullptr;
    DX12_GetCommandList()->QueryInterface(IID_PPV_ARGS(&cmdList4));
    
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.Inputs = as->inputs;
    buildDesc.DestAccelerationStructureData = as->gpuVA;
    buildDesc.ScratchAccelerationStructureData = as->scratchBuffer->GetGPUVirtualAddress();
    buildDesc.SourceAccelerationStructureData = as->gpuVA;
    buildDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
    
    ID3D12GraphicsCommandList* cmdList = DX12_GetCommandList();
    cmdList4->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);
    
    DX12_FlushBarriers(cmdList);
}

D3D12_GPU_VIRTUAL_ADDRESS DX12_GetAccelerationStructureGPUVA(dx12AccelerationStructure_s* as) {
    return as->gpuVA;
}

void DX12_CleanupAccelerationStructures(void) {
    for (auto* as : g_accelerationStructures) {
        delete as;
    }
    g_accelerationStructures.clear();
}