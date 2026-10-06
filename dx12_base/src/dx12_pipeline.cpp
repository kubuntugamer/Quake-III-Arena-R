// dx12_pipeline.cpp
#include "dx12_pipeline.h"

#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace dx12base {

bool CompileShader(const std::string& hlsl, const char* entryPoint, const char* target,
                   std::vector<BYTE>& outBlob, std::string& outErrors) {
    ComPtr<ID3DBlob> code;
    ComPtr<ID3DBlob> errors;

    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;

    const HRESULT hr = D3DCompile(hlsl.data(), hlsl.size(), nullptr, nullptr, nullptr,
                                  entryPoint, target, flags, 0, &code, &errors);

    if (errors) {
        outErrors.assign(static_cast<const char*>(errors->GetBufferPointer()),
                         errors->GetBufferSize());
    }

    if (FAILED(hr)) {
        return false;
    }

    // GetBufferPointer returns void*, so cast before doing pointer arithmetic.
    const auto* bytes = static_cast<const BYTE*>(code->GetBufferPointer());
    const SIZE_T size = code->GetBufferSize();

    outBlob.assign(bytes, bytes + size);
    return true;
}

bool CreateRootSignature(D3DDevice& device,
                         const D3D12_ROOT_PARAMETER* params, UINT numParams,
                         D3D12_ROOT_SIGNATURE_DESC& outDesc) {
    outDesc.NumParameters = numParams;
    outDesc.pParameters = params;
    outDesc.NumStaticSamplers = 0;
    outDesc.pStaticSamplers = nullptr;
    outDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                    // Lets HLSL index descriptor heaps directly, so no
                    // descriptor tables are needed in the shader.
                    D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
                    D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

    (void)device;
    return true;
}

bool TexturePipeline::Create(D3DDevice& device,
                             const std::string& vertexHlsl,
                             const std::string& pixelHlsl) {
    std::vector<BYTE> vertexBytecode;
    std::vector<BYTE> pixelBytecode;
    std::string errors;

    if (!CompileShader(vertexHlsl, "main", "vs_5_1", vertexBytecode, errors)) {
        OutputDebugStringA(errors.c_str());
        return false;
    }
    if (!CompileShader(pixelHlsl, "main", "ps_5_1", pixelBytecode, errors)) {
        OutputDebugStringA(errors.c_str());
        return false;
    }

    // Root parameters.
    //   0: transform constants, visible to the vertex stage, bound as a CBV.
    //   1: texture, visible to the pixel stage, bound as an SRV table.
    //   2: sampler, visible to the pixel stage, bound as a sampler table.
    D3D12_DESCRIPTOR_RANGE textureRange{};
    textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange.NumDescriptors = 1;
    textureRange.BaseShaderRegister = kTextureRegister;
    textureRange.RegisterSpace = kRegisterSpace;
    textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE samplerRange{};
    samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    samplerRange.NumDescriptors = 1;
    samplerRange.BaseShaderRegister = kSamplerRegister;
    samplerRange.RegisterSpace = kRegisterSpace;
    samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3]{};

    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = kTransformRegister;
    rootParameters[0].Descriptor.RegisterSpace = kRegisterSpace;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &textureRange;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &samplerRange;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    if (!CreateRootSignature(device, rootParameters, 3, rootDesc)) {
        return false;
    }

    ComPtr<ID3DBlob> serializedRoot;
    ComPtr<ID3DBlob> rootError;
    if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                          &serializedRoot, &rootError))) {
        if (rootError) {
            OutputDebugStringA(static_cast<const char*>(rootError->GetBufferPointer()));
        }
        return false;
    }

    if (FAILED(device.Device()->CreateRootSignature(
            0, serializedRoot->GetBufferPointer(), serializedRoot->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature_)))) {
        return false;
    }

    // Input layout must match the Vertex struct exactly: POSITION then TEXCOORD.
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,     0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = rootSignature_.Get();
    pso.VS = { vertexBytecode.data(), vertexBytecode.size() };
    pso.PS = { pixelBytecode.data(), pixelBytecode.size() };

    pso.BlendState = {};
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    pso.SampleMask = UINT_MAX;

    pso.RasterizerState = {};
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;

    pso.DepthStencilState = {};
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

    pso.InputLayout = { inputLayout, _countof(inputLayout) };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count = 1;

    if (FAILED(device.Device()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipeline_)))) {
        return false;
    }

    return true;
}

void TexturePipeline::Destroy() {
    pipeline_.Reset();
    rootSignature_.Reset();
}

}  // namespace dx12base