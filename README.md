Quake III Arena R
==================================

![Comparison](images/start_scene.png)

##### Branches:
`original` : A refactor of the original Quake III source code, to make it compatible with modern computers. Contains cmake files for easy building on win32/linux/macos. <br>
`reforged` : A modified version of the original Quake III source code with certain enhancements (e.g. vulkan, rtx).

GENERAL NOTES
=============
##### Command Arguments
Use the following arguments to run Quake III R. Use a `fs_basePath` that fits your Quake III install location.
```bash
+set fs_basePath "C:\GOG Games\Quake III" +set sv_pure 0 +set vm_game 0 +set vm_cgame 0 +set vm_ui 0
```
In order to run the game, blue noise textures are requiered. Copy the folder `blue_noise_textures` into your `"Quake III Arena\baseq3"` folder.

##### Requirements:
* [CMake](https://cmake.org/ "CMake") >= 3.14.0
* [SDL2](https://www.libsdl.org/) (audio, Linux)
* Vulkan SDK (Vulkan renderer)
* Wayland (`wayland-client`) for the native Linux backend, X11 as fallback

##### Submodules:
- [TinyJPEG](https://github.com/serge-rgb/TinyJPEG "TinyJPEG")
- [stb](https://github.com/nothings/stb.git "stb")
Please clone this git with submodules

CHANGES ON `reforged`
====================

* `VMI_COMPILED` was removed

##### Audio
* The old OSS `/dev/dsp` sound driver was replaced with an SDL2 backend
  (`code/unix/linux_snd.c`). The game now routes audio through
  PipeWire/PulseAudio/ALSA. See the commit message of `Replace OSS audio
  driver with SDL2 backend` for the engine-side details (playback position
  tracking, ring buffer sizing).

##### Renderer (Vulkan)
* Native Wayland backend with X11 fallback (`code/unix/` — xdg-shell).
* Optional-ext tolerance and loader soname fixes in Vulkan init; swapchain
  / frame-resource sizing fixes.
* Removed dead SMP/GLX thread block in `linux_vkimp.c`.
* FSR/NRD shader loader brace/build fixes.
* Dropped reference to the removed `VK_AMD_GPA_INTERFACE` extension macro
  so the renderer builds against newer SDK headers.

##### Multi-GPU
* NVIDIA primary GPU + AMD iGPU secondary support (`reforged` recent
  commits).

##### AI Integration
* Denoiser, Temporal Super Resolution (TSR), AS predictor, and material
  enhancement passes integrated into the renderer (`rt_aiDenoiser`,
  `rt_aiTSR`, `rt_aiASPredict`, `rt_aiMaterial`).

##### I/O
* `io_uring` stage 1: sync file-read backend (Linux).
* `io_uring` stage 2: async submit/collect batch backend.
* `io_uring` stage 3: opt-in async read-ahead prefetch.

##### Misc fixes
* botlib `GetMemory` kept 16-byte aligned.
* Swapchain frame resources no longer undersized vs real image count.

COMPILING ON LINUX
==================
```bash
mkdir build && cd build
cmake ..
make quake3
```
The binary lands in `bin/Release/quake3`. Audio uses SDL2; set
`SDL_AUDIODRIVER=alsa` to bypass PipeWire/PulseAudio if needed.

COMPILING ON WIN
===============

Use the provided `bat` file to generate a Visual Studio project. In order to run the game, make sure to set `fs_basePath` to a directory containing all the assets. Binarys can be found in `bin/`.

IMAGES
==================


| | |
:-------------------------:|:-------------------------:
![](images/scene_lava.png)  |  ![](images/scene_statue.png) |
![](images/q3rt.png)  |  ![](images/q3rt2.png) |
![](images/glass.png)  |  ![](images/glass2.png) |

Left: original Quake III; Right: RTX Version.
![](images/q3origvsrt.png)
