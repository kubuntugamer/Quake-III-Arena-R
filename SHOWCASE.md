# vkq3ng RT‑Showcase

A Quake III Arena / vkq3ng ray‑traced showcase built on the
`vkq3ng` Vulkan renderer.

## Layout

```
vkq3ng-showcase/
├── engine/           # Submodule: the vkq3ng engine (kubuntugamer/vkq3ng, branch vkq3ng)
├── dx12_base/        # Submodule: standalone DirectX 12 renderer skeleton (kubuntugamer/dx12-base)
├── build_map.sh      # The whole pipeline: textures → map → stage → compile → package → verify
├── process_textures.py   # PBR → Q3-compatible textures (TGA + mips), placeholders by default
├── tools/gen_map.py      # Generates maps/showcase.map in the format q3map2 expects
├── maps/showcase.map  # Generated map (committed for readability)
├── scripts/showcase.shader  # Q3 shader script for the materials
├── config/showcase.cfg      # vkq3ng RT configuration
└── release/          # Build output (gitignored)
```

The two submodules keep their own upstream remotes. The engine additionally
carries a local commit (`Guard RE_Shutdown Vulkan teardown on device‑creation
failure`) on the `vkq3ng` branch; see `engine/` for its log. That fix is
required for the engine to shut down cleanly when device creation fails
instead of segfaulting inside `VK_DestroyImage`, which happens in environments
with no usable Vulkan device.

## Prerequisites

- `q3map2` (NetRadiant fork, `netradiant-q3map2` on Ubuntu) — the map
  compiler.
- Python 3 + Pillow (`pip install Pillow`) — texture processing.
- The game data live in the profile `~/.local/share/q3rtx/baseq3` (needs
  `pak0.pk3`). `build_map.sh` expects the engine built under `engine/` with
  runnable `engine/bin/vkq3ng.engine`.

## Build

```bash
bash build_map.sh
```

This stages the generated map and textures into the game profile, compiles the
BSP (`build/showcase/`), runs the raster light stage, copies the BSP into
the game dir, and packages everything into `showcase.pk3` with a
`shaderlist.txt` that actually loads the custom materials.

## Running

```bash
# Raster path tracing off; legacy lighting
engine/bin/vkq3ng.engine +set fs_basePath ~/.local/share/q3rtx +set r_vertexLight 0 +map showcase

# RT path
engine/bin/vkq3ng.engine +set fs_basePath ~/.local/share/q3rtx +set r_vertexLight 2 \
    +set rt_numSamples 4 +set rt_accumulate 1 +map showcase
```

> Note: on a machine without a working Vulkan/GPU setup the engine reaches
> device creation and reports the real error (e.g. the RADV driver resource
> failure) instead of crashing. A GPU is required to actually render the map.

## Map & materials

The map is authored by `tools/gen_map.py` rather than by hand because
NetRadiant's map parser expects brace‑delimited brushes of three‑point faces;
the classic `( ( a b c d ) )` form is rejected. The `.map` file the tool emits
is checked in so it can be read and diffed without running Python.

Materials are classified for the ray tracer by shader name/standard `sort`
keyword (`RB_GetMaterial()` in `engine/code/renderer/tr_material.c`). The
script in `scripts/showcase.shader` deliberately uses names such as
`.../glass_clean`, `.../textures/liquids/calm_poollight`, `.../metal_mirror`
and `.../portal` so the rooms exercise distinct RT material behaviours:
refraction (glass), liquid shading (water), planar reflection (mirror) and
additive transparency (portal).
