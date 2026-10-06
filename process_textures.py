#!/usr/bin/env python3
"""
Process PBR textures for vkq3ng showcase map.
Converts source PBR textures to Q3 shader format with mipmaps.
"""
import os
import sys
from pathlib import Path
from PIL import Image

SRC = Path("textures/rt_showcase/_sources")
DST = Path("textures/rt_showcase/_compiled")
DST.mkdir(parents=True, exist_ok=True)

# Material definitions: maps source files to Q3 shader texture names
MATERIALS = {
    "glass_clean": {
        "albedo": "glass_albedo.png",
        "normal": "glass_normal.png",
        "roughness": "glass_roughness.png",
        "emissive": None,
        "metallic": None,
    },
    "lava_emissive": {
        "albedo": "lava_albedo.png",
        "normal": "lava_normal.png",
        "roughness": "lava_roughness.png",
        "emissive": "lava_emissive.png",
        "metallic": None,
    },
    "metal_brushed": {
        "albedo": "metal_albedo.png",
        "normal": "metal_normal.png",
        "roughness": "metal_roughness.png",
        "emissive": None,
        "metallic": "metal_metallic.png",
    },
    "concrete_damaged": {
        "albedo": "concrete_albedo.png",
        "normal": "concrete_normal.png",
        "roughness": "concrete_roughness.png",
        "emissive": None,
        "metallic": None,
    },
    "portal": {
        "albedo": "portal_albedo.png",
        "normal": "portal_normal.png",
        "roughness": "portal_roughness.png",
        "emissive": None,
        "metallic": None,
    },
    "sky_environment": {
        "albedo": "sky_albedo.png",
        "normal": "sky_normal.png",
        "roughness": "sky_roughness.png",
        "emissive": None,
        "metallic": None,
    },
}

# Placeholder colours, used when a material has no source images at all. These
# are deliberately distinguishable from each other: a showcase map built from
# six identical grey walls hides material mistakes, whereas six different greys
# make it obvious at a glance which surface is which.
PLACEHOLDER_COLORS = {
    "glass_clean": (150, 190, 210),
    "lava_emissive": (200, 80, 20),
    "metal_brushed": (140, 145, 155),
    "concrete_damaged": (110, 108, 104),
    "portal": (90, 60, 160),
    "sky_environment": (120, 160, 210),
}

# Q3 texture naming conventions
SUFFIX_MAP = {
    "albedo": "_col",      # Color/diffuse
    "normal": "_local",    # Normal map (Q3 uses _local)
    "roughness": "_spec",  # Specular/roughness approx
    "emissive": "_emissive",
    "metallic": "_metal",
}

DEFAULT_SIZE = (512, 512)
MIP_LEVELS = 4  # 512 -> 256 -> 128 -> 64 -> 32


def convert_texture(src_name, dst_base, suffix, size=DEFAULT_SIZE, normal_map=False):
    """Convert single texture: resize, save as TGA, generate mipmaps."""
    src = SRC / src_name
    if not src.exists():
        return False

    try:
        img = Image.open(src)
        img = img.resize(size, Image.LANCZOS)

        if normal_map and img.mode == 'RGBA':
            img = img.convert('RGB')

        # Main texture
        dst_main = DST / f"{dst_base}{suffix}.tga"
        img.save(dst_main, format='TGA', compression='rle')

        # Mipmaps
        for i in range(1, MIP_LEVELS + 1):
            mip_size = (size[0] // (2**i), size[1] // (2**i))
            if mip_size[0] < 1 or mip_size[1] < 1:
                break
            mip_img = img.resize(mip_size, Image.LANCZOS)
            mip_name = DST / f"{dst_base}{suffix}_mip{i}.tga"
            mip_img.save(mip_name, format='TGA', compression='rle')

        return True
    except Exception as e:
        print(f"  ERROR converting {src_name}: {e}", file=sys.stderr)
        return False


def create_placeholder_texture(name, color, size=DEFAULT_SIZE):
    """Create a placeholder texture if source missing."""
    img = Image.new('RGB', size, color)
    dst = DST / f"{name}.tga"
    img.save(dst, format='TGA', compression='rle')
    # Mipmaps
    for i in range(1, MIP_LEVELS + 1):
        mip_size = (size[0] // (2**i), size[1] // (2**i))
        if mip_size[0] < 1 or mip_size[1] < 1:
            break
        mip_img = img.resize(mip_size, Image.LANCZOS)
        mip_name = DST / f"{name}_mip{i}.tga"
        mip_img.save(mip_name, format='TGA', compression='rle')
    return True


def main():
    print("Processing PBR textures for rt_showcase...")
    print(f"Source: {SRC}")
    print(f"Dest:   {DST}")
    print()

    missing_sources = []

    for mat_name, maps in MATERIALS.items():
        print(f"Material: {mat_name}")
        has_any = False

        for map_type, src_name in maps.items():
            if src_name is None:
                continue

            suffix = SUFFIX_MAP.get(map_type, f"_{map_type}")
            is_normal = (map_type == "normal")

            if convert_texture(src_name, mat_name, suffix, normal_map=is_normal):
                print(f"  ✓ {src_name} -> {mat_name}{suffix}.tga + mipmaps")
                has_any = True
            else:
                print(f"  ✗ {src_name} NOT FOUND")
                missing_sources.append(f"{mat_name}/{src_name}")

        # Create placeholders for missing required maps
        if not has_any:
            colour = PLACEHOLDER_COLORS.get(mat_name, (128, 128, 128))
            print(f"  ! No sources found for {mat_name}, creating placeholders")
            create_placeholder_texture(f"{mat_name}_col", colour)
            create_placeholder_texture(f"{mat_name}_local", (128, 128, 255))
            create_placeholder_texture(f"{mat_name}_spec", (64, 64, 64))
            # q3map2 and the engine both resolve a shader's image by its own
            # name before falling back to the editor image. Without a file at
            # <mat>.tga the shader silently renders as the default image, which
            # looks like a texture problem rather than a missing file.
            create_placeholder_texture(mat_name, colour)

    print()

    if missing_sources:
        print("Missing source textures (placeholders created):")
        for m in missing_sources:
            print(f"  textures/rt_showcase/_sources/{m}")
        print()
        print("Replace placeholders with actual PBR textures for best results.")

    print("✓ Texture processing complete")
    print(f"Output: {DST}/")
    for f in sorted(DST.glob("*.tga")):
        print(f"  {f.name}")


if __name__ == "__main__":
    main()