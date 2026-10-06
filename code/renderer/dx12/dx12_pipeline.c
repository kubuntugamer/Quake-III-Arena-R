/*
===========================================================================
DX12 Pipeline State Objects (Raster)
===========================================================================
*/
#include "dx12_local.h"

using Microsoft::WRL::ComPtr;

struct dx12RootSignature_s {
    ComPtr<ID3D12RootSignature> rootSig;
    UINT numRootParams;
};

struct dx12Pipeline_s {
    ComPtr<ID3D12PipelineState> pso;
    dx12RootSignature_s* rootSig;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType;
};

static std::vector<dx12Pipeline_s*> g_pipelines;
static std::vector<dx12RootSignature_s*> g_rootSignatures;

static ComPtr<IDxcUtils> g_dxcUtils;
static ComPtr<IDxcCompiler3> g_dxcCompiler;
static ComPtr<IDxcIncludeHandler> g_includeHandler;

static void DX12_InitDXC(void) {
    if (!g_dxcUtils) {
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&g_dxcUtils));
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&g_dxcCompiler));
        g_dxcUtils->CreateDefaultIncludeHandler(&g_includeHandler);
    }
}

ComPtr<IDxcBlob> DX12_CompileShader(const char* source, const char* entryPoint, const char* target, const char* defines) {
    DX12_InitDXC();
    
    ComPtr<IDxcBlobEncoding> sourceBlob;
    g_dxcUtils->CreateBlob(source, (UINT32)strlen(source), CP_UTF8, &sourceBlob);
    
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
    HRESULT hr = g_dxcCompiler->Compile(
        sourceBlob.Get(),
        args.data(),
        (UINT32)args.size() - 1,
        g_includeHandler.Get(),
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

dx12RootSignature_s* DX12_CreateRootSignature(const D3D12_ROOT_PARAMETER* params, UINT numParams, const D3D12_STATIC_SAMPLER_DESC* samplers, UINT numSamplers) {
    dx12Device_t* dev = DX12_GetDevice();
    
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = numParams;
    desc.pParameters = params;
    desc.NumStaticSamplers = numSamplers;
    desc.pStaticSamplers = samplers;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                 D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
                 D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;
    
    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error);
    if (FAILED(hr)) {
        if (error) Com_Printf("Root signature error: %s\n", (char*)error->GetBufferPointer());
        return nullptr;
    }
    
    dx12RootSignature_s* rootSig = new dx12RootSignature_s();
    DX12_CHECK(dev->device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&rootSig->rootSig)), "CreateRootSignature");
    rootSig->numRootParams = numParams;
    rootSig->rootSig->SetName(L"RootSignature");
    
    static std::vector<dx12RootSignature_s*> g_rootSignatures;
    g_rootSignatures.push_back(rootSig);
    return rootSig;
}

qhandle_t DX12_CreateGraphicsPipeline(const char* vsSource, const char* fsSource, const D3D12_INPUT_ELEMENT_DESC* inputLayout, UINT numInputElements, D3D12_PRIMITIVE_TOPOLOGY_TYPE topology) {
    dx12Device_t* dev = DX12_GetDevice();
    
    D3D12_ROOT_PARAMETER rootParams[3] = {};
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace = 0;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    D3D12_DESCRIPTOR_RANGE range = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND};
    rootParams[1].DescriptorTable.pDescriptorRanges = &range;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    
    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    D3D12_DESCRIPTOR_RANGE samplerRange = {D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND};
    rootParams[2].DescriptorTable.pDescriptorRanges = &samplerRange;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_ANISOTROPIC;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 16;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    
    dx12RootSignature_s* rootSig = DX12_CreateRootSignature(rootParams, 3, &sampler, 1);
    if (!rootSig) return 0;
    
    ComPtr<IDxcBlob> vsBlob = DX12_CompileShader(vsSource, "main", "vs_6_5", nullptr);
    ComPtr<IDxcBlob> fsBlob = DX12_CompileShader(fsSource, "main", "ps_6_5", nullptr);
    
    if (!vsBlob || !fsBlob) return 0;
    
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSig->rootSig.Get();
    psoDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
    psoDesc.PS = {fsBlob->GetBufferPointer(), fsBlob->GetBufferSize()};
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = topology;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.InputLayout = {inputLayout, numInputElements};
    psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    
    struct dx12Pipeline_s {
        ComPtr<ID3D12PipelineState> pso;
        struct dx12RootSignature_s* rootSig;
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType;
    };
    
    static std::vector<struct dx12Pipeline_s*> g_pipelines;
    
    struct dx12Pipeline_s* pipeline = new struct dx12Pipeline_s();
    pipeline->rootSig = (dx12RootSignature_s*)rootSig;
    pipeline->topologyType = topology;
    
    DX12_CHECK(dev->device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline->pso)), "CreateGraphicsPipelineState");
    pipeline->pso->SetName(L"Graphics PSO");
    
    static std::vector<struct dx12Pipeline_s*> g_pipelines;
    g_pipelines.push_back(pipeline);
    return (qhandle_t)(g_pipelines.size() - 1);
}

void DX12_BindPipeline(qhandle_t handle) {
    static std::vector<struct dx12Pipeline_s*> g_pipelines;
    if (handle == 0 || handle > g_pipelines.size()) return;
    
    struct dx12Pipeline_s* pipeline = g_pipelines[handle - 1];
    ID3D12GraphicsCommandList* cmdList = DX12_GetCommandList();
    
    cmdList->SetPipelineState(pipeline->pso.Get());
    cmdList->SetGraphicsRootSignature(pipeline->rootSig->rootSig.Get());
    cmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void DX12_SetRootConstants(UINT rootParamIndex, UINT num32BitValues, const void* data) {
    ID3D12GraphicsCommandList* cmdList = DX12_GetCommandList();
    cmdList->SetGraphicsRoot32BitConstants(rootParamIndex, num32BitValues, data, 0);
}

void DX12_SetDescriptorTable(UINT rootParamIndex, D3D12_GPU_DESCRIPTOR_HANDLE handle) {
    ID3D12GraphicsCommandList* cmdList = DX12_GetCommandList();
    cmdList->SetGraphicsRootDescriptorTable(rootParamIndex, handle);
}

void DX12_CleanupPipelines(void) {
    static std::vector<struct dx12Pipeline_s*> g_pipelines;
    static std::vector<struct dx12RootSignature_s*> g_rootSignatures;
    
    for (auto* p : g_pipelines) delete p;
    for (auto* r : g_rootSignatures) delete r;
    g_pipelines.clear();
    g_rootSignatures.clear();
}