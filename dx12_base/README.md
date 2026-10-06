# DX12 Base

A standalone DirectX 12 **skeleton**: device creation, swapchain, graphics
pipeline and a frame loop, laid out the way a game renderer wants them. No
Quake III code is referenced, so it builds and runs on any Windows machine
with a DX12-capable GPU and nothing else installed.

It exists to be extended, not to be learned from. See
[What this is not](#what-this-is-not) before you rely on it.

## Licence

GPL-2.0-or-later. `LICENSE` is the verbatim GPL v2 text; the "or, at your
option, any later version" grant is stated here, matching the header notice in
id Software's 2005 Quake III Arena release that this project descends from.

If you port this into a renderer, that renderer inherits GPL terms. That is
already true of anything derived from Quake III Arena source, so it is not a
new constraint, but it is worth knowing before you start.

## What it actually does

Opens a window, brings up DX12, clears to a dark blue, presents. That is the
whole visual output.

What is real and complete behind that:

- Adapter selection across all GPUs, preferring the one with the most
  dedicated VRAM, rejecting the software rasteriser
- Device, graphics and copy command queues
- All four descriptor heap types
- Flip-model swapchain with back buffer RTVs and a matching depth buffer
- Two frames in flight, each with its own allocator, command list, fence and
  constant buffer
- HLSL compilation, root signature, graphics pipeline state object
- The `PRESENT -> RENDER_TARGET -> PRESENT` barrier pair around the clear

## What this is not

**It draws no geometry.** No vertex buffer, no index buffer, no
`DrawInstanced`, no `DrawIndexedInstanced`. The pipeline and heaps are bound
but nothing is submitted. The call sites are marked in `dx12_renderer.cpp`.

**It is not a DX12 tutorial.** Everything interesting about DX12 is its
explicitness, and none of that explicitness is demonstrated:

| Not demonstrated | Why it matters |
|------------------|----------------|
| Resource transitions beyond the back buffer | The single thing DX12 makes you do by hand |
| Texture upload (upload heap → copy → default heap) | Where everyone stalls the first time |
| Writing SRV and sampler descriptors | Heaps are allocated but never written to |
| MSAA | Hardcoded to 1 sample in three places; depth buffer and RTV array must match, and `ResolveSubresource` is required |
| sRGB output | Swapchain defaults to `UNORM`, not `UNORM_SRGB`, so gamma is wrong until you fix it |
| Debug layer messages | The layer is enabled, then everything goes to `OutputDebugString` and is invisible without a debugger attached |
| Device-lost recovery | `Present` returns `DEVICE_REMOVED` and the code returns; a real app rebuilds the device |
| Shaders loaded from disk | HLSL is a string literal in `main.cpp` |
| PIX markers | `SetMarker` is free performance data and is absent |
| Fast frame pacing | `WaitForGpu` every frame is correct but serialises CPU and GPU |

If you are trying to learn DX12, use Microsoft's
[DirectX-Graphics-Samples](https://github.com/microsoft/DirectX-Graphics-Samples).
`D3D12HelloTriangle` and `D3D12Raytracing` cover the ground this skips.

## What is verified, and what is not

| | |
|---|---|
| Compiles with MSVC, Debug and Release | Verified by CI |
| Adapter selection, device creation, ray tracing tier query | Verified by CI, ran on WARP |
| Window opens, swapchain presents, clear lands on screen | **Not verified.** CI has no GPU, so nothing past `Present()` has ever executed. |

Treat the window and present path as unproven until someone runs the binary on
real hardware. If it misbehaves there, the likely suspects are the
`D3D12_RESOURCE_STATE_PRESENT` barrier in `dx12_renderer.cpp` and the
`DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` requirement on Windows 10 1809.

Fourteen compile errors in the first three commits were found by CI, not by
review. That is the base rate for hand-written code nobody has compiled; the
untested runtime path should be read with that in mind.

## Build

### Windows (MSVC)

```
cmake -B build -S . -A x64
cmake --build build --config Release
build\Release\dx12_base.exe
```

### Linux cross-compile (MinGW)

```
sudo apt install mingw-w64
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build
build/dx12_base.exe
```

### CI

`.github/workflows/build.yml` builds Debug and Release on a Windows runner and
runs `--info`, which exercises adapter enumeration and device creation. Latest
run on the WARP software driver:

```
Adapter: Microsoft Basic Render Driver
Ray tracing tier 1.0: yes
Exit code: 0
```

That confirms the device path works end to end. It does **not** confirm the
window or present path, because the runner has no GPU to present to.

## Run

```
dx12_base.exe [options]

  --width N        Initial window width  (default 1280)
  --height N       Initial window height (default 720)
  --fullscreen     Start borderless at desktop resolution
  --vsync          Present with vsync (default)
  --novsync        Present without vsync
  --info           Print adapter details and exit
```

`--fullscreen` covers the monitor with no title bar using `WS_POPUP`. It is
borderless, not an exclusive mode switch; DWM still composites it, so alt-tab
behaves normally.

## Requirements

- Windows 10 1809 or newer (flip-model swapchain with tearing)
- A GPU with DirectX 12 feature level 11_0 or above

`--info` prints the selected adapter and whether it reports ray tracing tier
1.0, which is what a later DXR pass would key off.

## Layout

```
src/
  main.cpp          Entry point, argument parsing, the frame loop
  win32_window.*    Window creation, message pump, fullscreen toggle, resize
  dx12_device.*     Adapter selection, device, command queues, descriptor heaps
  dx12_swapchain.*  Swapchain, back buffer RTVs, depth buffer, resize handling
  dx12_pipeline.*   HLSL compilation, root signature, graphics PSO
  dx12_renderer.*   Per-frame resources, resource barriers, the draw path
```

The entry point is `main()`, not `wWinMain`, so the sample keeps a console for
the adapter printout and shader errors. `UNICODE` is still defined because the
W-suffixed Win32 and DXGI entry points are the ones declared in the headers.

## Design notes for whoever extends this

**The root signature uses `HEAP_DIRECTLY_INDEXED` for both the CBV/SRV/UAV and
sampler heaps.** That lets HLSL bind with `register(t0, space0)` and
`register(s0, space0)` with no descriptor tables, which removes an entire
class of binding bugs. The trade-off is that the heaps must be fixed size. They
are `D3DDevice::kMaxDescriptors` (1024) here, which is generous for Quake.

**Frames in flight is 2.** Each has its own command allocator, command list,
fence and transform constant buffer. `RenderFrame` writes into
`frames_[CurrentFrameIndex()]`, never a shared buffer, because the GPU may
still be reading the previous frame's copy. `D3D12_GPU_VIRTUAL_ADDRESS` is read
from `GetGPUVirtualAddress()` and stored separately from the mapped CPU pointer;
they are different values and one cannot be cast to the other.

**`WaitForGpu` is called once per frame in the main loop, before recording.**
This is correct but not fast. A real port moves to per-frame fence values and
only blocks when reusing an allocator that is still in flight. It is left
simple here so the pacing is obvious.

**Resource barriers are explicit and minimal.** Only the back buffer transitions
are recorded, because nothing else is uploaded per frame. Anything added later
needs its own transition, and a mismatched `StateBefore` is the most common
cause of a device removal.

**The swapchain uses `FLIP_DISCARD`** and passes `DXGI_PRESENT_ALLOW_TEARING`
to `Present`. That combination requires Windows 10 1809.
`DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING` is set at creation to make the flag legal.

**`Device::Initialize` prefers the adapter with the most dedicated VRAM.** On
hybrid laptops that means the discrete part. The software adapter is rejected
explicitly because it cannot run this meaningfully. `QueryVideoMemoryInfo` only
exists on `IDXGIAdapter4`, so that interface is queried separately during
enumeration and the budget comparison degrades to first-capable-wins if it is
unavailable.

**`--info` exists so CI can smoke-test device creation** on a GPU-less
runner. It reports the failure cleanly rather than crashing.

**`Swapchain::Resize` handles the depth buffer.** It waits for the GPU, releases
the old back buffers and depth buffer, calls `ResizeBuffers`, then rebuilds all
render targets. `IDXGISwapChain4` has no `GetBufferCount`, so the count is
remembered from the create descriptor.

## Where Quake III plugs in

| Quake III concept | Where it goes |
|-------------------|---------------|
| `tr_bsp.c` BSP parse | Vertex/index buffer creation in `Renderer` |
| `tr_image.c` texture load | `CreateTextureDescriptors`, then `IASetVertexBuffers` / descriptor tables in `RenderFrame` |
| `tr_shader.c` shader parse | HLSL generation fed to `TexturePipeline::Create` |
| `tr_backend.c` surface loop | `DrawIndexedInstanced` per surface in `RenderFrame` |
| `tr_raytracing.c` | New `dx12_dxr.*` alongside this directory, using `RayTracingTier1()` |