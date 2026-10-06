/*
===========================================================================
DX12 Ray Tracing Pipeline (DXR) and Shader Binding Table
===========================================================================
*/
#include "dx12_local.h"

using Microsoft::WRL::ComPtr;

struct dx12RayTracingPipeline_s {
    ComPtr<ID3D12StateObject> stateObject;
    ComPtr<ID3D12Resource> shaderBindingTable;
    D3D12_GPU_VIRTUAL_ADDRESS sbtRayGen;
    D3D12_GPU_VIRTUAL_ADDRESS sbtMiss;
    D3D12_GPU_VIRTUAL_ADDRESS sbtHitGroup;
    UINT sbtRecordSize;
    UINT numRayGen;
    UINT numMiss;
    UINT numHitGroups;
};

static std::vector<dx12RayTracingPipeline_s*> g_rtPipelines;

static const wchar_t* RT_EXPORT_RAYGEN = L"RayGen";
static const wchar_t* RT_EXPORT_MISS = L"Miss";
static const wchar_t* RT_EXPORT_CLOSEST_HIT = L"ClosestHit";
static const wchar_t* RT_EXPORT_ANY_HIT = L"AnyHit";
static const wchar_t* RT_EXPORT_INTERSECTION = L"Intersection";

static void DX12_InitDXC(void) {
    static ComPtr<IDxcUtils> dxcUtils;
    static ComPtr<IDxcCompiler3> dxcCompiler;
    static ComPtr<IDxcIncludeHandler> includeHandler;
    
    if (!dxcUtils) {
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
        dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
    }
}

ComPtr<IDxcBlob> DX12_CompileShader(const char* source, const char* entryPoint, const char* target, const char* defines) {
    DX12_InitDXC();
    
    static ComPtr<IDxcUtils> dxcUtils;
    static ComPtr<IDxcCompiler3> dxcCompiler;
    static ComPtr<IDxcIncludeHandler> includeHandler;
    
    if (!dxcUtils) {
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
        dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
    }
    
    ComPtr<IDxcBlobEncoding> sourceBlob;
    dxcUtils->CreateBlob(source, (UINT32)strlen(source), CP_UTF8, &sourceBlob);
    
    std::vector<LPCWSTR> args;
    args.push_back(L"-E");
    
    wchar_t entryW[64];
    size_t converted;
    mbstowcs_s(&converted, entryW, entryPoint, 64);
    args.push_back(entryW);
    
    args.push_back(L"-T");
    wchar_t targetW[32];
    mbstowcs_s(&converted, targetW, target, 32);
    args.push_back(targetW);
    
    args.push_back(L"-Zi");
    args.push_back(L"-Qembed_debug");
    args.push_back(L"-O3");
    
    if (defines && strlen(defines) > 0) {
        args.push_back(L"-D");
        wchar_t defineW[256];
        mbstowcs_s(&converted, defineW, defines, 256);
        args.push_back(defineW);
    }
    
    args.push_back(nullptr);
    
    ComPtr<IDxcResult> result;
    HRESULT hr = dxcCompiler->Compile(
        sourceBlob.Get(),
        args.data(),
        (UINT32)args.size() - 1,
        nullptr, // no include handler for now
        IID_PPV_ARGS(&result)
    );
    
    if (FAILED(hr)) {
        ComPtr<IDxcBlobUtf8> errors;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        if (errors) {
            Com_Printf("DX12 Shader Error: %s\n", (char*)errors->GetBufferPointer());
        }
        return nullptr;
    }
    
    ComPtr<IDxcBlob> blob;
    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr);
    return blob;
}

dx12RayTracingPipeline_s* DX12_CreateRayTracingPipeline(
    const char* rayGenSource, const char* missSource, const char* closestHitSource,
    const char* anyHitSource, const char* intersectionSource,
    UINT numRayGen, UINT numMiss, UINT numHitGroups,
    const D3D12_ROOT_SIGNATURE* globalRootSig, const D3D12_ROOT_SIGNATURE* localRootSig) {
    
    dx12Device_t* dev = DX12_GetDevice();
    
    // Compile shaders
    ComPtr<IDxcBlob> rayGenBlob = DX12_CompileShader(rayGenSource, "main", "lib_6_5", nullptr);
    ComPtr<IDxcBlob> missBlob = missSource ? DX12_CompileShader(missSource, "main", "lib_6_5", nullptr) : nullptr;
    ComPtr<IDxcBlob> chBlob = closestHitSource ? DX12_CompileShader(closestHitSource, "main", "lib_6_5", nullptr) : nullptr;
    ComPtr<IDxcBlob> ahBlob = anyHitSource ? DX12_CompileShader(anyHitSource, "main", "lib_6_5", nullptr) : nullptr;
    ComPtr<IDxcBlob> isBlob = intersectionSource ? DX12_CompileShader(intersectionSource, "main", "lib_6_5", nullptr) : nullptr;
    
    if (!rayGenBlob) return nullptr;
    
    // Create DXIL library subobjects
    std::vector<D3D12_STATE_SUBOBJECT> subobjects;
    
    // DXIL library
    D3D12_DXIL_LIBRARY_DESC libDesc = {};
    libDesc.DXILLibrary = {rayGenBlob->GetBufferPointer(), rayGenBlob->GetBufferSize()};
    
    // Export associations
    D3D12_EXPORT_DESC exports[5] = {};
    UINT numExports = 0;
    
    exports[numExports++] = {RT_EXPORT_RAYGEN, D3D12_EXPORT_FLAG_NONE};
    if (missBlob) exports[numExports++] = {RT_EXPORT_MISS, D3D12_EXPORT_FLAG_NONE};
    if (chBlob) exports[numExports++] = {RT_EXPORT_CLOSEST_HIT, D3D12_EXPORT_FLAG_NONE};
    if (ahBlob) exports[numExports++] = {RT_EXPORT_ANY_HIT, D3D12_EXPORT_FLAG_NONE};
    if (isBlob) exports[numExports++] = {RT_EXPORT_INTERSECTION, D3D12_EXPORT_FLAG_NONE};
    
    libDesc.NumExports = numExports;
    libDesc.pExports = exports;
    
    D3D12_STATE_SUBOBJECT libSubobject = {};
    libSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    libSubobject.pDesc = &libDesc;
    subobjects.push_back(libSubobject);
    
    // Hit groups
    D3D12_HIT_GROUP_DESC hitGroups[4] = {};
    UINT numHitGroups = 0;
    
    if (chBlob || ahBlob || isBlob) {
        hitGroups[numHitGroups].HitGroupExport = RT_EXPORT_CLOSEST_HIT;
        hitGroups[numHitGroups].Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
        hitGroups[numHitGroups].AnyHitShaderImport = ahBlob ? RT_EXPORT_ANY_HIT : nullptr;
        hitGroups[numHitGroups].ClosestHitShaderImport = chBlob ? RT_EXPORT_CLOSEST_HIT : nullptr;
        hitGroups[numHitGroups].IntersectionShaderImport = isBlob ? RT_EXPORT_INTERSECTION : nullptr;
        numHitGroups++;
    }
    
    D3D12_STATE_SUBOBJECT hgSubobject = {};
    hgSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    hgSubobject.pDesc = &hitGroups[0];
    subobjects.push_back(hgSubobject);
    
    // Root signatures
    D3D12_STATE_SUBOBJECT globalRootSigSubobject = {};
    globalRootSigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    globalRootSigSubobject.pDesc = globalRootSig;
    subobjects.push_back(globalRootSigSubobject);
    
    D3D12_STATE_SUBOBJECT localRootSigSubobject = {};
    localRootSigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
    localRootSigSubobject.pDesc = localRootSig;
    subobjects.push_back(localRootSigSubobject);
    
    // Shader config
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
    shaderConfig.MaxPayloadSizeInBytes = 32; // sizeof(RayPayload)
    shaderConfig.MaxAttributeSizeInBytes = 8; // float2 barycentrics
    
    D3D12_STATE_SUBOBJECT shaderConfigSubobject = {};
    shaderConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    shaderConfigSubobject.pDesc = &shaderConfig;
    subobjects.push_back(shaderConfigSubobject);
    
    // Pipeline config
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
    pipelineConfig.MaxTraceRecursionDepth = 4;
    
    D3D12_STATE_SUBOBJECT pipelineConfigSubobject = {};
    pipelineConfigSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    pipelineConfigSubobject.pDesc = &pipelineConfig;
    subobjects.push_back(pipelineConfigSubobject);
    
    // Create state object
    D3D12_STATE_OBJECT_DESC stateObjectDesc = {};
    stateObjectDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    stateObjectDesc.NumSubobjects = (UINT)subobjects.size();
    stateObjectDesc.pSubobjects = subobjects.data();
    
    dx12RayTracingPipeline_s* pipeline = new dx12RayTracingPipeline_s();
    pipeline->numRayGen = numRayGen;
    pipeline->numMiss = numMiss;
    pipeline->numHitGroups = numHitGroups;
    
    DX12_CHECK(dev->device->CreateStateObject(&stateObjectDesc, IID_PPV_ARGS(&pipeline->stateObject)), "CreateRayTracingPipeline");
    
    // Create Shader Binding Table
    DX12_CreateShaderBindingTable(pipeline);
    
    return pipeline;
}

void DX12_CreateShaderBindingTable(dx12RayTracingPipeline_s* pipeline) {
    dx12Device_t* dev = DX12_GetDevice();
    
    // Get shader identifiers
    ComPtr<ID3D12StateObjectProperties> stateObjectProps;
    pipeline->stateObject->QueryInterface(IID_PPV_ARGS(&stateObjectProps));
    
    const UINT identifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    const UINT numRecords = pipeline->numRayGen + pipeline->numMiss + pipeline->numHitGroups;
    pipeline->sbtRecordSize = align_up(identifierSize + 32, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT); // identifier + root arguments
    
    UINT64 sbtSize = (UINT64)pipeline->sbtRecordSize * numRecords;
    
    // Create SBT buffer
    DX12_CreateBuffer(sbtSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, &pipeline->shaderBindingTable, nullptr);
    
    // Map and fill SBT
    BYTE* mapped = nullptr;
    pipeline->shaderBindingTable->Map(0, nullptr, (void**)&mapped);
    
    BYTE* ptr = mapped;
    
    // RayGen records
    for (UINT i = 0; i < pipeline->numRayGen; ++i) {
        void* identifier = stateObjectProps->GetShaderIdentifier(RT_EXPORT_RAYGEN);
        memcpy(ptr, identifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        // Root arguments would go here (32 bytes)
        ptr += pipeline->sbtRecordSize;
    }
    pipeline->sbtRayGen = pipeline->shaderBindingTable->GetGPUVirtualAddress();
    
    // Miss records
    for (UINT i = 0; i < pipeline->numMiss; ++i) {
        void* identifier = stateObjectProps->GetShaderIdentifier(RT_EXPORT_MISS);
        memcpy(ptr, identifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        ptr += pipeline->sbtRecordSize;
    }
    pipeline->sbtMiss = pipeline->shaderBindingTable->GetGPUVirtualAddress() + pipeline->sbtRecordSize * pipeline->numRayGen;
    
    // Hit group records
    for (UINT i = 0; i < pipeline->numHitGroups; ++i) {
        void* identifier = stateObjectProps->GetShaderIdentifier(RT_EXPORT_CLOSEST_HIT);
        memcpy(ptr, identifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        // Hit group root arguments (material data) would go here
        ptr += pipeline->sbtRecordSize;
    }
    pipeline->sbtHitGroup = pipeline->shaderBindingTable->GetGPUVirtualAddress() + pipeline->sbtRecordSize * (pipeline->numRayGen + pipeline->numMiss);
    
    pipeline->shaderBindingTable->Unmap(0, nullptr);
}

void DX12_DispatchRays(dx12RayTracingPipeline_s* pipeline, UINT width, UINT height, UINT depth) {
    ID3D12GraphicsCommandList4* cmdList4 = nullptr;
    DX12_GetCommandList()->QueryInterface(IID_PPV_ARGS(&cmdList4));
    
    D3D12_DISPATCH_RAYS_DESC desc = {};
    desc.RayGenerationShaderRecord.StartAddress = pipeline->sbtRayGen;
    desc.RayGenerationShaderRecord.SizeInBytes = pipeline->sbtRecordSize;
    
    desc.MissShaderTable.StartAddress = pipeline->sbtMiss;
    desc.MissShaderTable.SizeInBytes = pipeline->sbtRecordSize;
    desc.MissShaderTable.StrideInBytes = pipeline->sbtRecordSize;
    
    desc.HitGroupTable.StartAddress = pipeline->sbtHitGroup;
    desc.HitGroupTable.SizeInBytes = pipeline->sbtRecordSize;
    desc.HitGroupTable.StrideInBytes = pipeline->sbtRecordSize;
    
    desc.Width = width;
    desc.Height = height;
    desc.Depth = depth;
    
    cmdList4->DispatchRays(&desc);
}

void DX12_CleanupRayTracingPipelines(void) {
    for (auto* p : g_rtPipelines) {
        delete p;
    }
    g_rtPipelines.clear();
}