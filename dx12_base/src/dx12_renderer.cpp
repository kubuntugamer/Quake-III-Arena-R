// dx12_renderer.cpp
#include "dx12_renderer.h"

#include <cmath>
#include <cstring>

namespace dx12base {

namespace {

// Rotation about the Z axis, written straight into the constants struct.
void BuildTransform(TexturePipeline::Constants& out, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    // Row-major, matching the HLSL mul(matrix, float4) in the vertex shader.
    out.transform[0]  =  c; out.transform[1]  =  s; out.transform[2]  = 0.0f; out.transform[3]  = 0.0f;
    out.transform[4]  = -s; out.transform[5]  =  c; out.transform[6]  = 0.0f; out.transform[7]  = 0.0f;
    out.transform[8]  = 0.0f; out.transform[9]  = 0.0f; out.transform[10] = 1.0f; out.transform[11] = 0.0f;
    out.transform[12] = 0.0f; out.transform[13] = 0.0f; out.transform[14] = 0.0f; out.transform[15] = 1.0f;
}

}  // namespace

bool Renderer::Initialize(D3DDevice& device, Swapchain& swapchain,
                          const std::string& vertexHlsl, const std::string& pixelHlsl) {
    // The swapchain supplies the render targets; the pipeline and the
    // per-frame resources are all this function owns.
    (void)swapchain;

    if (!pipeline_.Create(device, vertexHlsl, pixelHlsl)) {
        return false;
    }

    // One texel of opaque white, so the triangle is visible with no image file.
    // Staged in an upload heap; the copy to the default heap happens in
    // CreateTextureDescriptors once an SRV slot exists to point at.
    const UINT32 white = 0xFFFFFFFFu;

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeof(white);
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(device.Device()->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&whiteTexture_)))) {
        return false;
    }
    whiteTexture_->SetName(L"White Texture Data");

    void* mapped = nullptr;
    if (FAILED(whiteTexture_->Map(0, nullptr, &mapped))) {
        return false;
    }
    *static_cast<UINT32*>(mapped) = white;
    whiteTexture_->Unmap(0, nullptr);

    if (!CreateFrameResources(device)) {
        return false;
    }

    ready_ = true;
    return true;
}

bool Renderer::CreateFrameResources(D3DDevice& device) {
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeof(TexturePipeline::Constants);
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    for (FrameResources& frame : frames_) {
        if (FAILED(device.Device()->CreateCommittedResource(
                &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(&frame.transformBuffer)))) {
            return false;
        }
        frame.transformBuffer->SetName(L"Transform Constants");

        // Persistently mapped: the CPU pointer stays valid for the
        // lifetime of the resource, so each frame just memcpys into its own.
        frame.transformBuffer->Map(0, nullptr, &frame.transformCpu);

        // The GPU-visible address is separate and is what the command list
        // binds. It comes from the resource, not from the mapped pointer.
        frame.transformGpu = frame.transformBuffer->GetGPUVirtualAddress();
    }

    return CreateTextureDescriptors(device);
}

bool Renderer::CreateTextureDescriptors(D3DDevice& device) {
    // Intentionally empty. This is the extension point: a Quake port writes the
    // SRV for the texture atlas here, plus one sampler descriptor. The heap is
    // already allocated in D3DDevice, and because the root signature is built
    // with HEAP_DIRECTLY_INDEXED, the shader binds by [space][index] with no
    // descriptor tables to thread through.
    (void)device;
    return true;
}

void Renderer::RenderFrame(D3DDevice& device, Swapchain& swapchain, float timeSeconds) {
    if (!ready_) {
        return;
    }

    // A swapchain that failed to resize reports zero-sized render targets.
    // Recording a barrier against those would submit a null resource to the
    // GPU, so bail out and let the main loop call us again next frame.
    ID3D12Resource* backBuffer = swapchain.BackBuffer();
    if (!backBuffer || backBuffer->GetDesc().Width == 0) {
        return;
    }

    FrameContext& frame = device.CurrentFrame();
    FrameResources& resources = frames_[device.CurrentFrameIndex()];

    // Reset the allocator and reopen the command list for this frame.
    frame.allocator->Reset();
    frame.cmdList->Reset(frame.allocator.Get(), nullptr);

    ID3D12GraphicsCommandList* cmdList = frame.cmdList.Get();

    // Descriptor heaps must be set before any descriptor is referenced.
    ID3D12DescriptorHeap* heaps[] = {
        device.CbvSrvUavHeap(),
        device.SamplerHeap(),
    };
    cmdList->SetDescriptorHeaps(_countof(heaps), heaps);

    // The back buffer starts each frame in PRESENT state. Move it to
    // RENDER_TARGET before writing.
    D3D12_RESOURCE_BARRIER toTarget{};
    toTarget.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toTarget.Transition.pResource = backBuffer;
    toTarget.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    toTarget.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toTarget.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &toTarget);

    // Clear to a dark blue so an empty frame is distinguishable from a
    // crashed frame.
    const FLOAT clearColor[] = {0.05f, 0.06f, 0.10f, 1.0f};
    cmdList->ClearRenderTargetView(swapchain.BackBufferRtv(), clearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(swapchain.DepthRtv(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Viewport and scissor must cover the whole target.
    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(swapchain.Width());
    viewport.Height = static_cast<float>(swapchain.Height());
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    cmdList->RSSetViewports(1, &viewport);

    D3D12_RECT scissor{};
    scissor.right = static_cast<LONG>(swapchain.Width());
    scissor.bottom = static_cast<LONG>(swapchain.Height());
    cmdList->RSSetScissorRects(1, &scissor);

    // Spin the triangle so there is visible motion, proving the loop runs.
    // Uploaded into this frame's own slice of the constant buffer, because the
    // GPU may still be reading the other frames' copies.
    BuildTransform(constants_, timeSeconds * 0.75f);
    std::memcpy(resources.transformCpu, &constants_, sizeof(constants_));

    cmdList->SetGraphicsRootSignature(pipeline_.Signature());
    cmdList->SetPipelineState(pipeline_.State());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // The transform is a real root CBV, so it is read from the mapped upload
    // buffer at the address the GPU sees.
    cmdList->SetGraphicsRootConstantBufferView(0, resources.transformGpu);

    // A Quake port binds the BSP vertex buffer here:
    //   cmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
    // and the texture atlas plus sampler:
    //   cmdList->SetGraphicsRootDescriptorTable(1, textureGpuHandle);
    //   cmdList->SetGraphicsRootDescriptorTable(2, samplerGpuHandle);
    // followed by one DrawIndexedInstanced per surface.

    // Move the back buffer back to PRESENT for this frame's Present call.
    D3D12_RESOURCE_BARRIER toPresent = toTarget;
    toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cmdList->ResourceBarrier(1, &toPresent);

    if (FAILED(cmdList->Close())) {
        return;
    }

    ID3D12CommandList* lists[] = { cmdList };
    device.GraphicsQueue()->ExecuteCommandLists(1, lists);

    swapchain.Present(device, /*vsync=*/true);
}

bool Renderer::OnResize(D3DDevice& device) {
    // Nothing here depends on back buffer size yet. A real port recreates
    // any render targets derived from the swapchain.
    (void)device;
    return true;
}

void Renderer::Shutdown() {
    for (FrameResources& frame : frames_) {
        frame.transformBuffer.Reset();
    }
    whiteTexture_.Reset();
    pipeline_.Destroy();
    ready_ = false;
}

}  // namespace dx12base