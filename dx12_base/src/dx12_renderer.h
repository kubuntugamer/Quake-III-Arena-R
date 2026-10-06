// dx12_renderer.h - per-frame resources and the draw path
#pragma once

#include "dx12_device.h"
#include "dx12_pipeline.h"
#include "dx12_swapchain.h"

namespace dx12base {

// Resources that must be duplicated per frame in flight because the GPU may
// still be reading last frame's copy while we write this frame's.
struct FrameResources {
    ComPtr<ID3D12Resource> transformBuffer;

    // transformCpu is where the CPU writes each frame. transformGpu is the
    // address the command list needs, which is a distinct value from the
    // mapped pointer and cannot be derived from it by casting.
    void* transformCpu = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS transformGpu = 0;
};

// Owns the frame resources, the pipeline and the texture/sampler descriptors,
// and issues the per-frame draw calls.
class Renderer {
public:
    bool Initialize(D3DDevice& device, Swapchain& swapchain,
                    const std::string& vertexHlsl, const std::string& pixelHlsl);
    void Shutdown();

    // Reallocates anything that depends on the back buffer size.
    bool OnResize(D3DDevice& device);

    void RenderFrame(D3DDevice& device, Swapchain& swapchain, float timeSeconds);

private:
    bool CreateFrameResources(D3DDevice& device);
    bool CreateTextureDescriptors(D3DDevice& device);

    FrameResources frames_[kFramesInFlight];

    // 4x4 identity, uploaded to the transform buffer each frame.
    TexturePipeline::Constants constants_{};

    // 1x1 white texture so the sample has something to sample without
    // shipping an image file.
    ComPtr<ID3D12Resource> whiteTexture_;

    TexturePipeline pipeline_;
    bool ready_ = false;
};

}  // namespace dx12base