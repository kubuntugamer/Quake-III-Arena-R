// dx12_pipeline.h - shader compilation and pipeline state objects
#pragma once

#include "dx12_device.h"

#include <string>
#include <vector>

namespace dx12base {

// Compiles HLSL to DXIL using the Windows SDK's d3dcompiler, which every
// Windows install ships with. No external toolchain required.
bool CompileShader(const std::string& hlsl, const char* entryPoint, const char* target,
                   std::vector<BYTE>& outBlob, std::string& outErrors);

// Builds and owns a root signature. Uses the flag that lets shaders index
// descriptor heaps directly with [space][index], which keeps shaders simple.
bool CreateRootSignature(D3DDevice& device,
                         const D3D12_ROOT_PARAMETER* params, UINT numParams,
                         D3D12_ROOT_SIGNATURE_DESC& outDesc);

// The graphics pipeline used by the sample: one shader that reads a
// constant buffer holding a per-object transform plus a texture and sampler.
struct TexturePipeline {
    struct Vertex {
        float position[3];
        float uv[2];
    };
    struct Constants {
        float transform[16];
    };

    bool Create(D3DDevice& device, const std::string& vertexHlsl, const std::string& pixelHlsl);
    void Destroy();

    ID3D12PipelineState* State() const { return pipeline_.Get(); }
    ID3D12RootSignature* Signature() const { return rootSignature_.Get(); }

    // Registers for the descriptor table set up in Create(). Kept public so the
    // renderer can bind without duplicating the constants.
    static constexpr UINT kTransformRegister = 0;   // b0
    static constexpr UINT kTextureRegister = 0;    // t0
    static constexpr UINT kSamplerRegister = 0;    // s0
    static constexpr UINT kRegisterSpace = 0;

private:
    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> pipeline_;
};

}  // namespace dx12base