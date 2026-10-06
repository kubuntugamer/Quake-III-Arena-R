// main.cpp - entry point for the DX12 foundation sample
#include "dx12_device.h"
#include "dx12_pipeline.h"
#include "dx12_renderer.h"
#include "dx12_swapchain.h"
#include "win32_window.h"

#include <Windows.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

// Minimal shader pair so the sample runs without a build step that copies HLSL
// files next to the executable. A real port loads these from disk; see
// dx12_pipeline.h's CompileShader for the path that does.
constexpr char kTriangleVertexShader[] = R"(
cbuffer TransformConstants : register(b0) {
    float4x4 transform;
};

struct VertexInput {
    float3 position : POSITION;
    float2 uv       : TEXCOORD;
};

struct VertexOutput {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

VertexOutput main(VertexInput input) {
    VertexOutput output;
    output.position = mul(transform, float4(input.position, 1.0f));
    output.uv = input.uv;
    return output;
}
)";

constexpr char kTrianglePixelShader[] = R"(
Texture2D    texture0 : register(t0);
SamplerState  sampler0 : register(s0);

struct PixelInput {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD;
};

float4 main(PixelInput input) : SV_TARGET {
    return texture0.Sample(sampler0, input.uv);
}
)";

void PrintUsage(const char* exe) {
    std::printf(
        "DX12 Base Sample\n"
        "\n"
        "Usage: %s [options]\n"
        "\n"
        "  --width N        Initial window width  (default 1280)\n"
        "  --height N       Initial window height (default 720)\n"
        "  --fullscreen     Start fullscreen\n"
        "  --vsync          Present with vsync (default)\n"
        "  --novsync        Present without vsync\n"
        "  --info           Print adapter details and exit\n",
        exe);
}

bool Matches(const char* arg, const char* name) {
    return std::strcmp(arg, name) == 0;
}

}  // namespace

// Plain main() rather than wWinMain: the sample prints adapter details and
// shader errors to stdout, so a console subsystem is what we want. That also
// means the CRT looks for main, not wWinMain.
int main(int argc, char** argv) {
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    bool vsync = true;
    bool infoOnly = false;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (Matches(arg, "--width") && i + 1 < argc) {
            width = std::atoi(argv[++i]);
        } else if (Matches(arg, "--height") && i + 1 < argc) {
            height = std::atoi(argv[++i]);
        } else if (Matches(arg, "--fullscreen")) {
            fullscreen = true;
        } else if (Matches(arg, "--novsync")) {
            vsync = false;
        } else if (Matches(arg, "--vsync")) {
            vsync = true;
        } else if (Matches(arg, "--info")) {
            infoOnly = true;
        } else if (Matches(arg, "--help") || Matches(arg, "-h")) {
            PrintUsage("dx12_base.exe");
            return 0;
        }
    }

    dx12base::D3DDevice device;
    if (!device.Initialize(nullptr)) {
        std::printf("Failed to create a DX12 device.\n"
                    "Requirement: Windows 10 1809 or newer, a GPU with feature level 12_0 or above.\n");
        MessageBoxW(nullptr, L"DX12 device creation failed.\n"
                              L"Requires Windows 10 1809+ and a DX12 capable GPU.",
                     L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    std::printf("Adapter: %ls\n", device.AdapterName().c_str());
    std::printf("Ray tracing tier 1.0: %s\n", device.RayTracingTier1() ? "yes" : "no");

    if (infoOnly) {
        device.Shutdown();
        return 0;
    }

    dx12base::Win32Window window;
    if (!window.Create(L"DX12 Base", width, height, fullscreen)) {
        std::printf("Failed to create the window.\n");
        device.Shutdown();
        return 1;
    }

    dx12base::Swapchain swapchain;
    if (!swapchain.Create(window.Handle(), width, height, device)) {
        std::printf("Failed to create the swapchain.\n");
        window.Destroy();
        device.Shutdown();
        return 1;
    }

    dx12base::Renderer renderer;
    if (!renderer.Initialize(device, swapchain, kTriangleVertexShader, kTrianglePixelShader)) {
        std::printf("Failed to initialise the renderer (shader compile or PSO creation failed).\n");
        swapchain.Destroy();
        window.Destroy();
        device.Shutdown();
        return 1;
    }

    using Clock = std::chrono::high_resolution_clock;
    const auto start = Clock::now();

    bool running = true;
    while (running) {
        running = window.PumpMessages();

        if (window.WasResized()) {
            window.ClearResized();

            int clientWidth = 0;
            int clientHeight = 0;
            window.GetClientSize(clientWidth, clientHeight);

            if (clientWidth > 0 && clientHeight > 0) {
                swapchain.Resize(static_cast<UINT>(clientWidth),
                                static_cast<UINT>(clientHeight),
                                device);
                renderer.OnResize(device);
            }
        }

        if (window.IsFullscreen() == false && swapchain.Width() == 0) {
            break;
        }

        const auto now = Clock::now();
        const float elapsedSeconds =
            std::chrono::duration<float>(now - start).count();

        // Wait for the GPU to catch up before reusing this frame's allocator.
        device.WaitForGpu();

        renderer.RenderFrame(device, swapchain, elapsedSeconds);
        device.AdvanceFrame();
    }

    device.WaitForGpu();

    renderer.Shutdown();
    swapchain.Destroy();
    window.Destroy();
    device.Shutdown();

    std::printf("Clean shutdown.\n");
    return 0;
}