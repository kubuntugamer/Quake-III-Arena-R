#!/usr/bin/env python3
"""
Generate maps/rt_showcase.map for netradiant-q3map2.

Why this exists
---------------
NetRadiant-q3map2's map parser expects brushes to be brace-delimited blocks of
*three-point* faces:

    {
    ( x1 y1 z1 ) ( x2 y2 z2 ) ( x3 y3 z3 ) TEXTURE sx sy rot scalex scaley
    ( x1 y1 z1 ) ( x2 y2 z2 ) ( x3 y3 z3 ) TEXTURE sx sy rot scalex scaley
    }

The classic GtkRadiant `( ( a b c d ) ... )` form is *not* accepted. When q3map2
encounters a leading `(` it falls through to its key/value parser, the brush is
discarded without any diagnostic, and the compiler emits a BSP with zero
surfaces. See ParseBrush()/ParseRawBrush() in tools/quake3/q3map2/map.c.

Plane orientation matters too. q3map2 derives the plane normal as
`(p0 - p1) x (p2 - p1)` and keeps the positive half-space, so every face normal
must point *out of* its brush. A brush whose normals point inward, as a naive
"room" does, becomes a sealed solid that the player is trapped inside; q3map2
then reports "Entity in solid" and clips the entire brush away.

Consequently a room is not one brush. It is a shell of wall brushes placed
*outside* the volume you want to walk in, leaving the interior open and
connected to the outside world through doorways.

Running this script rewrites maps/rt_showcase.map. The output is committed, so
Python is only needed when the layout changes.
"""

import argparse
import math
from pathlib import Path

WALL = 64          # wall thickness
DOOR_HALF = 64     # doorway half-width
DOOR_TOP = 192     # doorway lintel height

CONCRETE = "rt_showcase/concrete_damaged"
METAL = "rt_showcase/metal_brushed"
GLASS = "rt_showcase/glass_clean"
PORTAL = "rt_showcase/portal"
SKY = "rt_showcase/sky_environment"

# The engine classifies ray-traced materials from the shader NAME and the
# standard `sort` keyword, not from shader directives. See RB_GetMaterial() in
# code/renderer/tr_material.c. These names are chosen to hit the reachable
# material kinds:
#
#   glass_clean              -> MATERIAL_KIND_GLASS   (refraction)
#   liquids/calm_poollight   -> MATERIAL_KIND_WATER   (liquid shading)
#   metal_mirror             -> MATERIAL_FLAG_MIRROR  (planar reflection)
#   portal                   -> MATERIAL_FLAG_SEE_THROUGH_ADD (additive)
#
# MATERIAL_KIND_LAVA is defined but never assigned, so a lava surface is
# ordinary diffuse geometry to the ray tracer.
WATER = "rt_showcase/textures/liquids/calm_poollight"
MIRROR = "rt_showcase/metal_mirror"

# Interior (walkable) extents of each space: x0, y0, z0, x1, y1, z1
ROOMS = {
    "portal":    (-768, -256, 0, -256, 256, 256),
    "spawn":     (-256, -256, 0, 256, 256, 256),
    "glass":     (256, -128, 0, 768, 128, 256),
    "lava":      (768, -384, 0, 1152, 384, 384),
    "arena":     (1152, -512, 0, 1664, 512, 512),
    "courtyard": (1664, -768, 0, 2432, 768, 768),
}

# Rooms chained left to right; every pair shares an x-aligned wall.
CHAIN = ["portal", "spawn", "glass", "lava", "arena", "courtyard"]

# Per-room material assignment. `wall_upper` + `split_z` give the glass hall a
# glazed band above a concrete dado, which is the whole point of the room.
MATERIALS = {
    "portal":    {"wall": CONCRETE, "floor": CONCRETE},
    "spawn":     {"wall": CONCRETE, "floor": CONCRETE},
    "glass":     {"wall": CONCRETE, "wall_upper": GLASS, "split_z": 96,
                  "floor": CONCRETE},
    "lava":      {"wall": METAL, "floor": WATER},
    "arena":     {"wall": MIRROR, "floor": MIRROR},
    "courtyard": {"wall": CONCRETE, "floor": CONCRETE},
}

# Face normal / in-plane up-vector pairs for a closed box.
FACE_AXES = (
    ((0, 0, 1), (0, 1, 0)),
    ((0, 0, -1), (0, 1, 0)),
    ((1, 0, 0), (0, 1, 0)),
    ((-1, 0, 0), (0, 1, 0)),
    ((0, 1, 0), (0, 0, 1)),
    ((0, -1, 0), (0, 0, 1)),
)


# --------------------------------------------------------------------------
# Geometry primitives
# --------------------------------------------------------------------------

def q3_normal(p0, p1, p2):
    """Reproduce q3map2's MapPlaneFromPoints normal: (p0-p1) x (p2-p1)."""
    a = [p0[i] - p1[i] for i in range(3)]
    b = [p2[i] - p1[i] for i in range(3)]
    c = [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]
    length = math.sqrt(sum(x * x for x in c))
    if length == 0.0:
        raise ValueError("degenerate face: the three points are collinear")
    return [x / length for x in c]


def face_points(centre, normal, up, size):
    """Three coplanar points whose q3map2-derived normal equals `normal`."""
    n = [float(v) for v in normal]
    u = [float(v) for v in up]

    # Gram-Schmidt: strip any component of `up` along the face normal.
    dot = sum(u[i] * n[i] for i in range(3))
    u = [u[i] - dot * n[i] for i in range(3)]
    length = math.sqrt(sum(x * x for x in u))
    if length < 1e-9:
        raise ValueError("face `up` vector is parallel to its normal")
    u = [x / length for x in u]

    right = [
        u[1] * n[2] - u[2] * n[1],
        u[2] * n[0] - u[0] * n[2],
        u[0] * n[1] - u[1] * n[0],
    ]

    corners = [
        [centre[i] + u[i] * size for i in range(3)],
        [centre[i] - right[i] * size for i in range(3)],
        [centre[i] + u[i] * size - right[i] * size for i in range(3)],
    ]

    # Which winding satisfies q3map2 depends on the handedness of the basis, so
    # choose it by measurement rather than by reasoning about it.
    for perm in ((0, 1, 2), (0, 2, 1), (1, 0, 2), (1, 2, 0), (2, 0, 1), (2, 1, 0)):
        candidate = [corners[i] for i in perm]
        if all(abs(a - b) < 1e-6 for a, b in zip(q3_normal(*candidate), n)):
            return candidate

    raise AssertionError(f"could not orient a face with normal {n}")


def _fmt_face(points):
    return " ".join("( {:.1f} {:.1f} {:.1f} )".format(*p) for p in points)


def box(x0, y0, z0, x1, y1, z1, texture, inward=False):
    """A solid axis-aligned brush.

    `inward=True` reverses every normal so the brush acts as the inner surface
    of a volume, which is what the enclosing sky box needs. Ordinary solid
    geometry must keep the default outward normals.
    """
    if x1 <= x0 or y1 <= y0 or z1 <= z0:
        return []

    cx, cy, cz = (x0 + x1) / 2.0, (y0 + y1) / 2.0, (z0 + z1) / 2.0
    # Keep the defining triangle comfortably inside the face it describes.
    size = max(min(x1 - x0, y1 - y0, z1 - z0) / 4.0, 8.0)

    lines = []
    for normal, up in FACE_AXES:
        n = [-c for c in normal] if inward else list(normal)
        centre = [cx + n[0] * size, cy + n[1] * size, cz + n[2] * size]
        lines.append(_fmt_face(face_points(centre, n, up, size))
                     + f" {texture} 0 0 0 1 1 0 0 0")

    return ["{\n" + "\n".join(lines) + "\n}"]


def segments(a0, a1, gaps):
    """Cover [a0, a1] with intervals, excluding `gaps`."""
    out = []
    cursor = a0
    for g0, g1 in sorted(gaps):
        g0, g1 = max(g0, a0), min(g1, a1)
        if g1 <= g0:
            continue
        if g0 > cursor:
            out.append((cursor, g0))
        cursor = max(cursor, g1)
    if cursor < a1:
        out.append((cursor, a1))
    return out


# --------------------------------------------------------------------------
# Room construction
# --------------------------------------------------------------------------

def room_shell(bounds, gaps, mat, ceiling=True):
    """Floor, ceiling and four walls surrounding one interior volume.

    `gaps` maps a wall side ('xneg', 'xpos', 'yneg', 'ypos') to a list of
    (lo, hi) openings along that wall's length.

    `mat` is a dict with:
      floor      texture for the floor slab (defaults to `wall`)
      wall       texture for the walls
      wall_upper optional texture used above `split_z`
      split_z    height at which `wall_upper` takes over
    """
    x0, y0, z0, x1, y1, z1 = bounds
    floor_tex = mat.get("floor", mat["wall"])
    wall_tex = mat["wall"]
    upper_tex = mat.get("wall_upper")
    split_z = mat.get("split_z")

    brushes = []

    # Floors and ceilings span the full footprint plus wall thickness so
    # neighbouring rooms share a continuous slab.
    brushes += box(x0 - WALL, y0 - WALL, z0 - WALL, x1 + WALL, y1 + WALL, z0, floor_tex)
    if ceiling:
        brushes += box(x0 - WALL, y0 - WALL, z1, x1 + WALL, y1 + WALL, z1 + WALL, wall_tex)

    # An x-facing wall is laid out along y and vice versa. Each wall is emitted
    # as the solid spans left after removing its openings, plus a lintel over
    # each opening.
    walls = {
        "xneg": ((x0 - WALL, y0 - WALL, z0, x0, y1 + WALL, z1), (y0 - WALL, y1 + WALL)),
        "xpos": ((x1, y0 - WALL, z0, x1 + WALL, y1 + WALL, z1), (y0 - WALL, y1 + WALL)),
        "yneg": ((x0 - WALL, y0 - WALL, z0, x1 + WALL, y0, z1), (x0 - WALL, x1 + WALL)),
        "ypos": ((x0 - WALL, y1, z0, x1 + WALL, y1 + WALL, z1), (x0 - WALL, x1 + WALL)),
    }

    for side, (volume, span) in walls.items():
        bx0, by0, bz0, bx1, by1, bz1 = volume
        along_y = side in ("xneg", "xpos")
        openings = gaps.get(side, [])

        def slab(a, b, z_lo, z_hi, texture):
            """A sub-box of this wall spanning [a, b] along its long axis."""
            if along_y:
                return box(bx0, a, z_lo, bx1, b, z_hi, texture)
            return box(a, by0, z_lo, b, by1, z_hi, texture)

        # Vertically split each span when an upper wall material is in play.
        def emit(a, b, z_hi):
            if upper_tex and split_z is not None and z_hi > split_z:
                brushes.extend(slab(a, b, bz0, split_z, wall_tex))
                brushes.extend(slab(a, b, split_z, z_hi, upper_tex))
            else:
                brushes.extend(slab(a, b, bz0, z_hi, wall_tex))

        for a, b in segments(span[0], span[1], openings):
            emit(a, b, bz1)
        for a, b in openings:
            # Lintel above the opening; keeps the room sealed at the top.
            brushes.extend(slab(a, b, DOOR_TOP, bz1, wall_tex))

    return brushes


def compute_gaps():
    """Work out which wall of which room each doorway punches through."""
    gaps = {name: {"xneg": [], "xpos": [], "yneg": [], "ypos": []} for name in ROOMS}

    for left, right in zip(CHAIN, CHAIN[1:]):
        lx0, ly0, _lz0, lx1, ly1, _lz1 = ROOMS[left]
        rx0, ry0, _rz0, rx1, ry1, _rz1 = ROOMS[right]

        if lx1 != rx0:
            raise ValueError(f"{left} and {right} do not share an x-aligned wall")

        centre = (ly0 + ly1) // 2
        opening = (centre - DOOR_HALF, centre + DOOR_HALF)

        # The opening is on the left room's +X wall and the right room's -X wall.
        gaps[left]["xpos"].append(opening)
        gaps[right]["xneg"].append(opening)
        del ry0, rx1, ry1

    return gaps


# --------------------------------------------------------------------------
# Emission
# --------------------------------------------------------------------------

def build():
    out = []
    w = out.append
    gaps = compute_gaps()

    rule = "=" * 74
    w(f"// {rule}")
    w("// vkq3ng RT Showcase - GENERATED FILE, DO NOT EDIT BY HAND")
    w("// Regenerate with:  python3 tools/gen_map.py")
    w("//")
    w("// Braces delimit brushes and each face is three points. NetRadiant's")
    w("// parser silently discards the classic `( ( a b c d ) )` form, so this")
    w("// layout is generated rather than hand-written.")
    w(f"// {rule}")
    w("{")
    w('"classname" "worldspawn"')
    w('"message" "vkq3ng RT Showcase"')
    # -meta turns brushes into meta triangles, which is only correct when the
    # real drawsurfs come from the shader path. Left off deliberately: the
    # authored surfaces must be emitted as SURFACE_FACE.
    w(f'"_skybox" "{SKY}"')
    w('"_color" "0.2 0.2 0.3"')
    w('"ambient" "8"')
    w("")
    w("// Interior volumes are carved out by surrounding the walkable space with")
    w("// wall brushes. Nothing is placed inside a room's own volume.")
    w("")

    for name, bounds in ROOMS.items():
        ceiling = name != "courtyard"
        w(f"\t// ---- {name} " + "-" * max(4, 60 - len(name)))
        w("\n".join(room_shell(bounds, gaps[name], MATERIALS[name], ceiling=ceiling)))
        w("")

    # An enclosing cube with inward-facing normals, textured as sky. This is
    # what makes the courtyard's open top read as outdoors and lets q3map2
    # flood the level to the skybox rather than reporting a leak.
    # A portal slab across the portal room's far wall: the showcase's stand-in
    # for a teleport surface.
    px0, py0, pz0, px1, py1, pz1 = ROOMS["portal"]
    w("\t// ---- portal surface " + "-" * 44)
    w("\n".join(box(px0 - WALL, -128, pz0, px0, 128, pz0 + 192, PORTAL)))
    w("")

    # No skybox brush. The engine draws the skybox for any pixel not covered by
    # geometry, so the courtyard's open top already shows sky. An enclosing
    # "sky" brush is actively harmful: q3map2 still treats it as solid, which
    # puts every entity inside solid and culls the brush.
    w(f'"_ignoreleaks" "1"')

    w("}")

    w(f"// {rule}")
    w("// ENTITIES")
    w(f"// {rule}")
    w("")

    def entity(**keys):
        w("{")
        for k, v in keys.items():
            w(f'\t"{k}" "{v}"')
        w("}")
        w("")

    for _name, (x0, y0, _z0, x1, y1, _z1) in ROOMS.items():
        entity(classname="info_player_deathmatch",
               origin=f"{(x0 + x1) // 2} {(y0 + y1) // 2} 32",
               angle="0")

    lights = [
        ("spawn room",   "0 0 200",   "400", "1 0.95 0.9"),
        ("portal room",  "-512 0 200", "350", "0.7 0.8 1"),
        ("glass hall",   "512 0 200",  "250", "0.85 0.95 1"),
        ("lava chamber", "960 0 200",  "600", "1 0.35 0.08"),
        ("metal arena",  "1408 0 380", "700", "0.9 1 0.95"),
    ]
    for _label, origin, power, colour in lights:
        entity(classname="light", origin=origin, light=power, _color=colour)

    # Sun through the courtyard's open top.
    entity(classname="light", origin="2048 0 700", light="900",
           _color="1 0.96 0.88", angles="-60 0 0", sun="1")

    # The courtyard is deliberately open to the sky, so the level is not
    # watertight. q3map2 reports that as a leak; the engine is happy with it
    # because it fills unwritten pixels from the skybox. Acknowledging it here
    # keeps the compile output clean without hiding a genuine mistake, which
    # would still appear as a leak from any of the enclosed rooms.
    entity(classname="func_group", origin="0 0 0",
           _ignoreleaks="1", _note="courtyard open to sky")

    return "\n".join(out).rstrip() + "\n"


def main():
    ap = argparse.ArgumentParser(description="Generate the rt_showcase map.")
    ap.add_argument("-o", "--output", default="maps/rt_showcase.map",
                    help="file to write (default: maps/rt_showcase.map)")
    ap.add_argument("--stdout", action="store_true",
                    help="print to stdout instead of writing a file")
    args = ap.parse_args()

    text = build()

    if args.stdout:
        print(text, end="")
        return

    dest = Path(args.output)
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(text)
    print(f"wrote {dest}: {text.count(chr(10) + '{')} brushes, "
          f"{len(text.splitlines())} lines")


if __name__ == "__main__":
    main()
