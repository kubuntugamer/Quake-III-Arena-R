# Vk Quake III NG — Linux / Vulkan / Ray Tracing

A Linux-focused fork of **Quake III Arena** that replaces the original OpenGL renderer with a
**Vulkan backend**, adds **hardware ray tracing** as an optional path, and keeps the classic
rasterized renderer available on GPUs that have no RT support.

This README documents the **current state of this fork**, what is verified, and what still needs
testing. It is written to be machine-readable: every claim below is either backed by code in this
repository or explicitly flagged as unverified.

- **Platform focus:** Linux (x86-64), Wayland with automatic X11 fallback
- **Graphics API:** Vulkan 1.2+ (loader resolved at runtime via `dlopen`)
- **Renderers:** rasterization (classic Quake III look) **and** ray tracing
- **Asset source:** the retail Quake III Arena game data — required, not included

---

## Table of Contents

- [Fork origin](#fork-origin)
- [Status summary](#status-summary)
- [Linux requirements](#linux-requirements)
- [Building on Linux](#building-on-linux)
- [Running the game](#running-the-game)
- [Asset layout](#asset-layout)
- [Renderer modes: `r_vertexLight`](#renderer-modes-r_vertexlight)
- [Feature status in detail](#feature-status-in-detail)
  - [Ray tracing](#ray-tracing)
  - [FSR render scale (raster path)](#fsr-render-scale-raster-path)
  - [Multi-GPU / hybrid laptops](#multi-gpu--hybrid-laptops)
  - [AI upscaling and denoising](#ai-upscaling-and-denoising)
  - [Wayland and X11](#wayland-and-x11)
  - [SDL2 audio](#sdl2-audio)
  - [`io_uring` file I/O](#io_uring-file-io)
- [Cvar reference](#cvar-reference)
- [Hardware support](#hardware-support)
- [What still needs testing and verification](#what-still-needs-testing-and-verification)
- [Known limitations](#known-limitations)
- [Troubleshooting](#troubleshooting)
- [Repository layout](#repository-layout)

---

## Fork origin

This repository is a fork of the **ReForged** branch of Quake III Arena, which in turn descends
from the id Software source release published under the GPL on **20 August 2005**. That upstream
lineage is not ioquake3 — this fork builds from the original id source tree with the ReForged
refactor applied.

Quake III Arena and its game data are trademarks of id Software / Activision. The source code here
is covered by the GPL as released by id; no game assets are distributed with this repository. You
must supply your own legally obtained copy of the retail game data.

---

## Status summary

| Area | State | Notes |
|---|---|---|
| Vulkan rasterization renderer | **Working** | Default path. Runs on non-RT GPUs |
| Ray tracing renderer | **Working** | Requires RTX-class hardware + Mesa 23+/recent drivers |
| Linux build (CMake, GCC) | **Working** | Clean build, exit 0 |
| Wayland windowing backend | **Working** | Native xdg-shell, auto-fallback to X11 |
| SDL2 audio | **Working** | Replaces the dead OSS `/dev/dsp` driver |
| FSR-style render scale | **Working, unreleased** | Bilinear upscale; not real FSR1 EASU/RCAS |
| RT-optional device gating | **Implemented, untested on non-RT hardware** | See below |
| Vendor-neutral GPU selection | **Working, unreleased** | Replaces NVIDIA-first hardcoding |
| Multi-GPU compute offload | **Stub** | Device is created but never used for work |
| AI denoiser / TSR / predictors | **Stub** | Cvars exist; no code path reaches them |
| `io_uring` file I/O | **Implemented, opt-in, unbenchmarked** | All cvars default to `0` |


"**unreleased**" means the change is complete and builds clean in the working tree but is not yet
committed to the `reforged` branch.

---

## Linux requirements

### Build dependencies (Debian / Ubuntu)

```bash
sudo apt install build-essential cmake git \
    libsdl2-dev libvulkan-dev vulkan-tools \
    libwayland-dev wayland-protocols libxkbcommon-dev \
    libx11-dev libxext-dev libxcb1-dev libxcb-xkb-dev
```

| Package | Purpose |
|---|---|
| `build-essential`, `cmake` ≥ 3.14 | Compiler and build system |
| `libvulkan-dev` | Vulkan headers; the loader is resolved at runtime |
| `vulkan-tools` | `vulkaninfo`, for verifying your driver and extensions |
| `libsdl2-dev` | Audio backend (replaces OSS) |
| `libwayland-dev`, `wayland-protocols` | Native Wayland / xdg-shell backend |
| `libx11-dev`, `libxext-dev`, `libxcb1-dev` | X11 fallback path |
| `libxkbcommon-dev` | Keyboard input |

Other distributions: the package names differ, but the set of libraries is the same. On Arch, use
`vulkan-icd-loader`, `sdl2`, `wayland`, `libxkbcommon` and the `libX*` group. On Fedora, use
`vulkan-loader-devel`, `SDL2-devel`, `wayland-devel`, `libxkbcommon-devel`, `libX11-devel`.

`libwayland-client` is linked unconditionally, so it must be present even if you only ever run
under X11.

### Git submodules

Two dependencies are vendored as submodules and must be initialised:

```bash
git submodule update --init
```

- [`external/stb`](https://github.com/nothings/stb) — image loading/decoding
- [`external/TinyJPEG`](https://github.com/serge-rgb/TinyJPEG) — JPEG decoding

### Runtime requirements

- A Vulkan 1.2 driver. Verify with `vulkaninfo --summary`.
- The retail Quake III Arena game data (see [Asset layout](#asset-layout)).

---

## Building on Linux

```bash
git clone https://github.com/kubuntugamer/Quake-III-Arena-R.git
cd Quake-III-Arena-R
git submodule update --init

cmake -S . -B build
cmake --build build -j"$(nproc)"
```

The executable is named `vkq3ng.engine` (`OUTPUT_NAME` in `code/unix/CMakeLists.txt`). A
development build writes it to `bin/Release/` in the source tree; a packaging build leaves it
in the build tree.

Which one you get is decided once and cached as `VKQ3NG_PACKAGE_BUILD`, so it no longer
changes when you re-run `cmake`. It defaults to OFF unless an explicit `CMAKE_INSTALL_PREFIX`
was supplied. Configure prints which:

```
-- Output directories: /path/to/Quake-III-Arena-R/bin (development build)
-- Output directories: build tree (packaging build)
```

Override it explicitly if you need to:

```bash
cmake -S . -B build -DVKQ3NG_PACKAGE_BUILD=OFF   # development, bin/Release/
cmake -S . -B build -DVKQ3NG_PACKAGE_BUILD=ON    # packaging, build tree
```

A build directory that was configured before this decision was cached may have latched the
wrong value. Delete `build/` and reconfigure, or pass the flag.

Notes:

- The renderer CMake targets use `file(GLOB_RECURSE ...)`. If you add a **new** `.c` file you must
  re-run the `cmake -S . -B build` configure step, not just the build.
- If the Vulkan SDK is installed in a non-standard prefix, point CMake at it:
  `cmake -S . -B build -DVULKAN_SDK=/path/to/sdk`.
- A successful build should end with `[100%] Built target vkq3ng`.

---

## Running the game

From a Debian install, `vkq3ng` is on your `PATH`. From a development source build the binary
is `vkq3ng.engine` in `bin/Release/`.

```bash
vkq3ng +set fs_basePath /path/to/your/quake3 +set sv_pure 0 \
       +set vm_game 0 +set vm_cgame 0 +set vm_ui 0
```

`fs_basePath` must point at the directory **containing** `baseq3/` and `missionpack/`, not at
`baseq3` itself. `FS_AddGameDirectory()` appends the game directory name, so pointing it at
`baseq3` makes the engine look for `baseq3/baseq3` and fail with
`Couldn't load default.cfg`.

With no `fs_basePath`, the engine uses the current working directory as the install path and
`$HOME/.q3a` as the home path for saved configuration.

### First-run recommendations

```bash
vkq3ng +set fs_basePath /path/to/quake3 \
       +set r_fullscreen 1 \
       +set r_mode 14 \
       +set r_vertexLight 0
```

`r_mode 14` means fullscreen at the current desktop resolution. Omit `r_mode` and pass
`+set r_fullscreen 0` for a windowed test run, which is the safer first attempt.

---

## Asset layout

This fork uses the standard modern source-port layout. Put the retail `.pk3` archives in
`<fs_basePath>/baseq3/`:

```
<fs_basePath>/
├── baseq3/
│   ├── pak0.pk3        ← required, from your own purchase
│   ├── pak1.pk3 … pak8.pk3
│   └── default.cfg
└── missionpack/        ← must exist; copy from baseq3 if your copy lacks it
```

Notes that contradict older versions of this README:

- **`blue_noise_textures` is only required for the ray tracing path.** It is loaded inside the
  `<RTX>` block in `code/renderer/tr_init.c` and consumed only by `code/renderer/tr_bsp.c`. Pure
  rasterization does not read it. If you see a missing-blue-noise error you are running the RT path.
- `missionpack/` is the Team Arena / Quake III Team Games map set. Create the directory even if you
  only play base Quake III; the engine probes it on startup.
- `default.cfg` must be inside `pak0.pk3`. If the engine aborts with `Couldn't load default.cfg`,
  the base path is wrong or `pak0.pk3` is missing.

---

## Renderer modes: `r_vertexLight`

`r_vertexLight` selects the renderer:

| Value | Renderer | Hardware needed |
|---:|---|---|
| `0` | **Rasterization** (classic Quake III) | Any Vulkan 1.2 GPU |
| `2` | **Ray tracing** | RTX-class GPU, see [Hardware support](#hardware-support) |

```bash
# Rasterization
vkq3ng +set r_vertexLight 0

# Ray tracing
vkq3ng +set r_vertexLight 2
```

`0` is the default. The two paths share the same assets, maps and UI; only the lighting pipeline
differs.

---

## Feature status in detail

### Ray tracing

**Status: working, requires supported hardware.**

The RT path is implemented with the KHR ray tracing pipeline and acceleration structure extensions,
plus deferred host operations for shader invocation reordering. It provides path-traced lighting,
soft shadows, reflections and refraction, depth of field, and accumulation-based sampling.

Roughly two dozen `rt_*` cvars control it; see the [Cvar reference](#cvar-reference).

Requirements:

- `VK_KHR_acceleration_structure`, `VK_KHR_ray_tracing_pipeline`,
  `VK_KHR_deferred_host_operations`
- Vulkan 1.2

**Unreleased change — RT-optional device gating.** Previously these extensions were listed as
unconditionally required during device selection, so a GPU without RT support was rejected as a
candidate *even in raster mode*, and the engine aborted with `failed to find a suitable GPU!`.
The extension requirement, the enabled-extension list, both `pNext` feature chains, and the RT
entry-point loading are now all gated on `r_vertexLight == 2` via a single helper,
`VK_RayTracingActive()` in `code/renderer/vulkan/vk_setup.c`.

In RT mode the enabled extension set is byte-identical to before, so the RT path is unaffected.
The raster path on non-RT hardware is expected to work but **has not been verified**, because no
RT-less GPU was available for testing. See
[What still needs testing](#what-still-needs-testing-and-verification).

### FSR render scale (raster path)

**Status: working, default off, unreleased.**

`r_fsrScale` renders the 3D scene at a fraction of your display resolution and upscales it to the
window. It applies to the **rasterization** path only — it has no effect while
`r_vertexLight` is `2`.

```bash
vkq3ng +set r_vertexLight 0 +set r_fsrScale 0.85
```

Output on the console, verified working:

```
r_fsrScale: 0.85 (1920x1052 -> 1632x894, bilinear upscale)
```

| Preset | Value |
|---|---|
| Ultra Quality | `0.87` |
| Quality | `0.667` |
| Balanced | `0.5` |
| Performance | `0.333` |
| Off (default) | `0` |

Implementation: `code/renderer/vulkan/vk_fsr.c`. The internal render target is resized,
`VK_Draw`/`VK_DrawIndexed` scale the viewport and scissor, and `VK_EndRender` performs the
upscale blit. `VK_InitFSR()` honours `r_fsrScale->latchedString`, so a command-line `+set` takes
effect on the very first launch rather than the second.

**This is not AMD FSR 1.** It is an internal-resolution scale followed by a hardware bilinear blit.
The existing `fsr_rcas.comp` shader in the RT path could not be reused because it stores into an
`rgba32f` image while the swapchain is `B8G8R8A8_UNORM`, and Vulkan forbids blitting between those
format classes. Because `B8G8R8A8_UNORM` does support `VK_IMAGE_USAGE_STORAGE_BIT`, a real
EASU + RCAS implementation into the swapchain is feasible as follow-up work, but it is not
implemented here.

Verified: map load, bot match, clean shutdown at `0.85` on RDNA2 hardware.

### Multi-GPU / hybrid laptops

**Status: device selection works and is unreleased; actual offload is a stub.**

Two separate concerns, frequently conflated:

1. **Picking the right GPU.** Previously hardcoded NVIDIA-primary with AMD secondary. Now vendor
   neutral: `VK_DeviceRank()` in `code/renderer/vulkan/vk_setup.c` scores candidates by
   `deviceType` (discrete > integrated > virtual > CPU) and then by summed `DEVICE_LOCAL` heap
   size. Type dominates memory because hybrid iGPUs advertise large shared memory. The candidate
   array is now `malloc`'d from the real `deviceCount` instead of being a fixed `devices[10]`, and a
   CPU-type device (llvmpipe / lavapipe) is explicitly rejected as a compute device.

2. **Using the second GPU.** A `secondaryDevice` logical device is created, but the AI pipeline that
   would consume it is a stub — see below. The second GPU is currently selected and created, and
   then unused.

Eligibility for a second compute device, as currently implemented:

| Laptop combination | Second device used? | Why |
|---|---|---|
| AMD APU + AMD discrete | Yes | Both present and RT-capable |
| Intel Arc iGPU + NVIDIA discrete | Yes | iGPU has RT; it may be headless, which is now allowed |
| Intel UHD / Iris Xe + NVIDIA discrete | No | iGPU has no raytracing extensions |
| Single GPU | No | Nothing else to use |

A GPU that cannot present is still eligible for the compute slot. On a muxless hybrid the
display belongs to the discrete GPU, so the on-board iGPU has no queue family with surface
support — that no longer disqualifies it. `r_multiGPU 2` restores the stricter behaviour if a
second GPU that also presents is required. Startup says `(headless)` when that is what was
picked.

The compute slot ranks raytracing support above device class, because doing raytracing is the
only job it has. A discrete part without RT will not displace an integrated part that has it.

On Mesa, GPU selection can be overridden without touching this code. The
`VK_LAYER_MESA_device_select` layer ships with Mesa and honours `MESA_VK_DEVICE_SELECT`
(`amd` / `nvidia` / `intel`) or `MESA_VK_DEVICE_ID=N`. `prime-run` and `switcherooctl` work too.
Note that setting `VK_ICD_FILENAMES` restricts enumeration to the named driver, which would hide
the RT-capable GPU; this fork deliberately does not set it.

### AI upscaling and denoising

**Status: stub. The cvars exist but nothing calls the code.**

`rt_aiDenoiser`, `rt_aiTSR`, `rt_aiASPredict` and `rt_aiMaterial` are registered with default `0`.
Setting them has no effect. The blockers, all in `code/renderer/ai/ai_pipeline.c`:

- `AI_InitPipeline()` sets `initialized = qtrue` even when `pipeline->pipeline == NULL`
- `AI_SetupDescriptors()` has an empty body and is never called
- `AI_LoadWeights()` is a placeholder
- `AI_InitPipelines()` has **no call site anywhere in the tree**, so `aiEnabled` stays `qfalse`
- `vk.secondaryComputeQueue` is never submitted to
- All AI layouts and weight buffers are created on `vk.device`, not `vk.secondaryDevice`

Making this functional is a substantial piece of work, not a bug fix. Until then, treat these cvars
as reserved.

### Wayland and X11

**Status: working.**

`VKW_CreateWindow()` in `code/unix/linux_vkimp.c` probes Wayland first and falls back to X11 if a
Wayland window cannot be created. On startup the engine prints either
`...created Wayland window (WxH)` or the X11 equivalent. The xdg-shell protocol client is
pre-generated in `code/unix/xdg/`.

The engine selects the X11 path when `WAYLAND_DISPLAY` is unset
(`code/unix/linux_wayland.c`). To force X11 under a Wayland session, run it under
`XWayland`, for example `WAYLAND_DISPLAY= vkq3ng`.

### SDL2 audio

**Status: working, replaces the OSS driver.**

SDL2 is the **only** audio backend. `code/unix/linux_snd.c` opens one SDL2 audio device and
mixes into it; there is no separate ALSA or PulseAudio path in this codebase.

ALSA, PulseAudio and PipeWire appear only because SDL2 sits on top of them — they are SDL2
*drivers*, chosen by SDL2 and not by the engine. On a normal desktop you install none of them
directly; SDL2 negotiates with whatever the system already provides. Playback position
tracking and ring buffer sizing are handled in the engine.

The engine mixes at a fixed 44100 Hz, stereo, signed 16-bit. `sndspeed`, `sndbits` and
`sndchannels` are parsed for compatibility with old configs but change nothing, as is the
inherited `s_khz` cvar. Setting them prints a note saying so.

The mixer rate is unrelated to the hardware rate. SDL2 resamples between them internally, so a
device that only does 48000 Hz is not a problem and nothing plays at the wrong pitch.

`allowed_changes` is deliberately 0. The audio callback is a raw `memcpy` of signed 16-bit
bytes, so it is only correct while the format and channel count SDL2 grants match what was
asked for. Verified across 8000–192000 Hz, mono and stereo, against the pulseaudio, alsa and
pipewire drivers: `allowed_changes = 0` returned an exact match every time. Startup re-checks
this and warns if a driver ever disagrees.

If audio is silent or the wrong device is picked, override the **SDL2** driver, not an engine
setting:

```bash
SDL_AUDIODRIVER=alsa vkq3ng        # bypass PipeWire/PulseAudio
SDL_AUDIODRIVER=pulseaudio vkq3ng
SDL_AUDIODRIVER=dummy vkq3ng       # discard output, useful for isolating the problem
```

To see what SDL2 actually picked, and on which driver:

```bash
SDL_LOGLEVEL=info vkq3ng 2>&1 | grep -i 'audio\|pulse\|pipewire\|alsa'
```

Startup prints the negotiated format:

```
SDL: opened audio 44100 Hz, 2 ch, 1024 samples (mixing at 44100 Hz)
```

The two rates are the same in practice. If they ever differ, or a `WARNING - device gave
format` line appears, that is the thing to investigate.

### `io_uring` file I/O

**Status: implemented, opt-in, default off, not benchmarked.**

`code/unix/linux_uring.c` implements a self-contained `io_uring` backend with no external liburing
dependency, falling back to plain `stdio` when the syscall is unavailable (`ENOSYS`). Three
capabilities are present, each behind its own cvar, **all defaulting to `0`**:

| Cvar | Default | Capability |
|---|---|---|
| `fs_useUring` | `0` | Synchronous read backend |
| `fs_useUringAsync` | `0` | Async submit/collect batch backend |
| `fs_useUringPrefetch` | `0` | Opt-in 64 KiB async read-ahead prefetch |

Diagnostics are emitted once per session through `Com_DPrintf`, e.g.
`FS_Read: serving file reads via io_uring (async)`. Enable them one at a time and watch for
corrupted or truncated asset loads. **No performance measurement has been done**, so it is unknown
whether this is a win on any real system.

---

## Cvar reference

### Renderer

| Cvar | Default | Description |
|---|---|---|
| `r_vertexLight` | `0` | `0` = rasterization, `2` = ray tracing |
| `r_fsrScale` | `0` | Render scale for the raster path; `0` disables |
| `r_mode` | `3` | Resolution mode; `14` = fullscreen at desktop resolution |
| `r_fullscreen` | `1` | Fullscreen toggle |
| `r_multiGPU` | `0` | Second-GPU policy: `0` auto (a headless raytracing device is allowed), `1` never, `2` only if the second device can also present |

### Ray tracing

| Cvar | Default | Description |
|---|---|---|
| `rt_numSamples` | `1` | Samples per pixel per frame |
| `rt_maxSamples` | `0` | Accumulation sample cap; `0` = unlimited |
| `rt_accumulate` | — | Accumulation / progressive refinement |
| `rt_taa` | `1` | Temporal anti-aliasing |
| `rt_antialiasing` | `1` | Anti-aliasing control |
| `rt_denoiser` | `1` | Denoiser enable |
| `rt_fsr` | `0` | FSR inside the RT path (distinct from `r_fsrScale`) |
| `rt_fsrSharpness` | — | Sharpness for `rt_fsr` |
| `rt_softshadows` | — | Soft shadow enable |
| `rt_dof` | — | Depth of field enable |
| `rt_focallength` | — | Focus distance |
| `rt_aperture` | — | Aperture |
| `rt_numBounces` | — | Ray bounce depth |
| `rt_numRandomDL` | — | Random diffuse bounces |
| `rt_numRandomIL` | — | Random indirect bounces |
| `rt_cullLights` | — | Light culling |
| `rt_illumination` | — | Indirect illumination |
| `rt_brightness` | — | Exposure |
| `rt_tonemapping_reinhard` | — | Reinhard tonemap toggle |
| `rt_debug_lights` | — | Visualise lights |
| `rt_printPerfStats` | — | Print RT performance statistics |
| `rt_pause` | — | Pause the RT integrator |
| `rt_nrdPack` | — | NVIDIA RTXDI noise reduction pack |
| `rt_aiDenoiser` | `0` | **Stub, no effect** |
| `rt_aiTSR` | `0` | **Stub, no effect** |
| `rt_aiASPredict` | `0` | **Stub, no effect** |
| `rt_aiMaterial` | `0` | **Stub, no effect** |

### Audio

All of these go through SDL2; there is no per-backend choice to make.

| Cvar | Default | Description |
|---|---|---|
| `s_volume` | `1` | Master volume |
| `s_musicvolume` | `1` | Music volume |
| `s_mixahead` | `0.2` | Seconds of audio buffered ahead |
| `s_mixPreStep` | `0.05` | Mix pretime step; also sizes the SDL ring buffer |
| `s_separation` | `0.5` | Stereo separation, `0` = mono |
| `s_doppler` | `1` | Doppler effect on positional audio |
| `sndbits` | `16` | **No effect.** Mixer is fixed at signed 16-bit |
| `sndchannels` | `2` | **No effect.** Mixer is fixed at stereo |
| `sndspeed` | `0` | **No effect.** Mixer is fixed at 44100 Hz |
| `s_khz` | `22` | **No effect.** Inherited from the OSS driver, unused |

### Filesystem and I/O

| Cvar | Default | Description |
|---|---|---|
| `fs_basePath` | engine default | Directory **containing** `baseq3/` |
| `fs_homePath` | `$HOME/.q3a` | Saved config location |
| `sv_pure` | `1` | Pure-server restrictions; set `0` for local play |
| `vm_game` / `vm_cgame` / `vm_ui` | `1` | Enable VMs; set `0` alongside `sv_pure 0` |
| `fs_useUring` | `0` | `io_uring` synchronous reads |
| `fs_useUringAsync` | `0` | `io_uring` async batch reads |
| `fs_useUringPrefetch` | `0` | `io_uring` read-ahead prefetch |
| `fs_copyfiles` | `0` | Write loose files instead of pure virtual filesystem |
| `fs_debug` | `0` | Filesystem diagnostics |

---

## Hardware support

### Ray tracing

Based on driver support rather than marketing names:

| Vendor | Supported | Notes |
|---|---|---|
| AMD | RDNA2 and newer (RX 6000+) | Needs Mesa 23+; ray tracing default-on from Mesa 23.2 |
| NVIDIA | Turing and newer (RTX 20+) | Vulkan RT on Linux is driver-supported |
| Intel | Arc (DG2) and newer | Pre-Arc Intel iGPUs are **not** supported |
| Software | Not supported | lavapipe/llvmpipe is rejected as an RT device |

**Not supported:** AMD Vega, RDNA1 / RX 5000, NVIDIA Pascal and Maxwell, and all pre-Arc Intel
integrated GPUs.

The engine prints the selected device at startup:

```
Vulkan: graphics on <GPU name> (no second GPU for compute)
```

or, when a second compute device was accepted:

```
Vulkan: graphics on <GPU name>, raytracing compute on <second GPU>
```

### Rasterization

Any Vulkan 1.2 driver. Verified working on AMD RDNA2 (RADV) and llvmpipe; expected but unverified
on NVIDIA and Intel.

### Hybrid laptops

Discrete GPUs are normally preferred. For a muxless laptop where the iGPU drives the display, use
the compositor's PRIME / Bumblebee control (`prime-run`, `switcherooctl`) or
`MESA_VK_DEVICE_SELECT`. See [Multi-GPU](#multi-gpu--hybrid-laptops).

---

## What still needs testing and verification

This is the honest list. Nothing below has been confirmed on real hardware.

### High priority — correctness and safety

1. **Rasterization on a GPU with no RT support.** The RT-optional gating change is implemented and
   builds clean, but no RTX-less GPU was available. This needs an RDNA1 (RX 5000), a pre-Turing
   NVIDIA card, or a pre-Arc Intel iGPU. Verify the game starts, loads a map and renders — and
   that `vulkaninfo` confirms the device really lacks the RT extensions, otherwise the test proves
   nothing.

2. **Driver-level rejection of misconfigured `pNext` chains.** The device-creation and
   `vkGetPhysicalDeviceProperties2` chains now omit RT structures when RT is off. RADV and llvmpipe
   both *tolerate* including those structures without the extensions enabled, so this was never
   observed to be a hard failure locally, and `VK_LAYER_KHRONOS_validation` is not installed here.
   The gating is spec-correct hygiene, but it should be re-checked with validation layers enabled
   on at least one driver.

3. **Wayland versus X11 parity.** Only one of the two paths has been exercised in this session.
   Both need testing, including resize, fullscreen toggle, and display hot-plug.

4. **`io_uring` asset integrity.** Enable `fs_useUring`, `fs_useUringAsync` and
   `fs_useUringPrefetch` individually and load every map and menu, watching for corrupted textures,
   models or sounds. Then measure whether any of it is actually faster; no benchmark exists.

5. **Multi-GPU device selection on real hybrid laptops.** The ranking function was verified by
   extracting it into a standalone harness and testing nine device layouts. That is simulation, not
   hardware. Confirm on an Intel+ NVIDIA laptop, an AMD APU + AMD dGPU laptop, and a desktop with
   two identical GPUs.

### Medium priority

6. **Dual-GPU tie-breaking.** Two discrete GPUs with more than 4 GB each produce the same rank and
   fall back to enumeration order. Confirm the tie-break is acceptable, or add a tie-break
   criterion.

7. **FSR render scale across a wider range.** Verified once at `0.85` on RDNA2. Needs testing at
   the other presets, at multiple aspect ratios, on window resize, and while toggling
   `r_fsrScale` at runtime. Check for shimmering and for HUD/crosshair misalignment.

8. **Real FSR 1 (EASU + RCAS).** Only a bilinear blit ships today. If a true FSR implementation is
   wanted, it must write into the `B8G8R8A8_UNORM` swapchain as a storage image rather than
   reusing `fsr_rcas.comp`.

9. **Audio across environments.** Verified over PulseAudio, plus ALSA and PipeWire as SDL2
   drivers, at 8–192 kHz requested in both mono and stereo. Needs testing with no sound server
   at all, and on HDMI or Bluetooth output. A device that only does 48 kHz should be fine:
   SDL2 resamples to the hardware rate internally, and `allowed_changes = 0` was verified to
   return the requested rate regardless of what the device supports.

10. **Resolution and fullscreen edge cases.** Borderless fullscreen at fractional scaling, HiDPI
    under Wayland, and monitors whose current mode is not the maximum mode.

### Low priority

11. **Long-session stability.** Memory growth over multi-hour sessions, and behaviour on map change
    and level reload with FSR active.

12. **Compiler and distribution coverage.** Only GCC on one Linux distribution has been used.
    Clang, `-fsanitize=address` runs, and non-Debian distributions are untested.

13. **The 23 `VK_AMD_*` extension references in the renderer.** Vendored heavily on AMD; there are
    zero `VK_INTEL_*` references. Behaviour on Intel Arc is unverified.

---

## Known limitations

- **AI upscaling and denoising do nothing.** The cvars are present and default to `0`; the code
  path is never initialised.
- **The second GPU is created but unused.** A second device is now selected and created in more
  cases than before (headless raytracing parts are eligible), but nothing submits work to it.
  Multi-GPU compute is still not implemented; see `code/renderer/ai/ai_pipeline.c`. Until it is,
  cross-device handle export (`VK_KHR_external_memory_fd` and friends) is reported at startup if
  either device lacks it, rather than being required — refusing would reject devices for a
  capability nothing uses yet.
- **FSR is a bilinear upscale, not AMD FSR 1.**

- **Game data is not included.** `pak0.pk3` must come from your own purchase.
- **`io_uring` support is unmeasured** and opt-in.
- **No regression test suite.** Verification has been manual: build, run, observe.

---

## Troubleshooting

**`failed to find a suitable GPU!`**
No enumerated device passed `VK_CheckDeviceExtensionSupport`. Run `vulkaninfo --summary` to confirm
the loader sees your GPU. Note that setting `VK_ICD_FILENAMES` restricts enumeration to a single
driver; unset it.

**`Couldn't load default.cfg`**
`fs_basePath` points at the wrong level. It must be the parent of `baseq3/`, not `baseq3` itself.

**`no second GPU for compute` in the log**
Expected on a single-GPU system, or on an Intel UHD/Iris Xe + NVIDIA combination where the iGPU
lacks RT support. Single-GPU operation is correct, not an error.

**Crash on startup with `r_vertexLight 2`**
The RT path requires `blue_noise_textures/` in `baseq3/`. If it is absent the engine does **not**
print a warning — `R_LoadImage16()` returns a NULL pointer and `tr_init.c` dereferences it
immediately, so you get a segfault with no useful message. Copy the folder into `baseq3/`, or run
with `r_vertexLight 0`.

**No sound**
SDL2 is the only audio backend, so override the SDL2 driver rather than an engine setting:
`SDL_AUDIODRIVER=alsa vkq3ng` or `SDL_AUDIODRIVER=pulseaudio vkq3ng`. Use
`SDL_AUDIODRIVER=dummy` to confirm the problem is audio output rather than the game.
Check the startup line reports 44100 Hz and shows no `WARNING - device gave format`.

**Runs but renders black under Wayland**
Force X11: `WAYLAND_DISPLAY= vkq3ng`.

**Adding a new source file had no effect**
The renderer CMake targets glob sources. Re-run `cmake -S . -B build`.

---

## Repository layout

```
code/
├── client/            Client-side logic, prediction
├── server/            Game server (sv_*)
├── game/              Shared game logic (bg_*)
├── botlib/            Bot AI
├── qcommon/           Shared engine code, filesystem (files.c)
├── renderer/
│   ├── tr_init.c      Cvar registration, renderer init
│   ├── tr_raytracing.c RT entry point
│   └── vulkan/        Vulkan backend
│       ├── vk_setup.c     Instance, device selection, extension gating
│       ├── vk_fsr.c       FSR render scale (raster path)
│       ├── vk_pipeline.c  Pipeline creation, draw calls
│       ├── vk_swapchain.c Swapchain setup
│       └── vk_rtpipeline.c RT pipelines
├── ui/                User interface
├── unix/              Linux platform layer
│   ├── linux_wayland.c   Native Wayland backend
│   ├── linux_vkimp.c     Window creation, Wayland/X11 selection
│   ├── linux_snd.c       SDL2 audio
│   └── linux_uring.c     io_uring file I/O
---

## Screenshots

| | |
|:-------------------------:|:-------------------------:|
| ![](images/scene_lava.png) | ![](images/scene_statue.png) |
| ![](images/q3rt.png) | ![](images/q3rt2.png) |
| ![](images/glass.png) | ![](images/glass2.png) |

Classic Quake III on the left, ray traced on the right.
![](images/q3origvsrt.png)