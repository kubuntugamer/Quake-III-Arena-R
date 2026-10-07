#!/bin/bash
set -euo pipefail

# ============================================================================
# vkq3ng Showcase Map Build Script
# Compiles map, processes textures, creates .pk3, installs for testing
# ============================================================================

MAP_NAME="showcase"
GAME_DIR="${GAME_DIR:-baseq3}"
BUILD_DIR="build/${MAP_NAME}"
PK3_NAME="${MAP_NAME}.pk3"

# Configurable paths
Q3MAP2="${Q3MAP2:-q3map2}"
# The built engine is vkq3ng.engine; there is no Release/quake3 in this tree.
VKQ3NG_BIN="${VKQ3NG_BIN:-/home/chazz/experimental/vkq3ng-showcase/engine/bin/vkq3ng.engine}"
# The real game profile that contains pak0.pk3. The engine repo itself ships
# no game data, so an empty engine/baseq3 would leave the runtime in restricted
# demo mode and make the tool tags build a map nobody can run.
BASEQ3_PATH="${BASEQ3_PATH:-/home/chazz/.local/share/q3rtx/baseq3}"
# -fs_basepath wants the directory that *contains* the game directory, not the
# game directory itself. Passing .../baseq3 makes q3map2 look for
# .../baseq3/baseq3 and silently find nothing, which surfaces as every surface
# using the default image.
GAME_ROOT="${GAME_ROOT:-$(dirname "${BASEQ3_PATH}")}"

if [ "$(basename "${BASEQ3_PATH}")" != "${GAME_DIR}" ]; then
    echo "ERROR: BASEQ3_PATH (${BASEQ3_PATH}) does not end in ${GAME_DIR}/" >&2
    echo "       q3map2 and the engine expect the game directory itself." >&2
    exit 1
fi

echo "=== Building ${MAP_NAME} ==="
echo "Q3MAP2: ${Q3MAP2}"
echo "Engine: ${VKQ3NG_BIN}"
echo "Game dir: ${BASEQ3_PATH}"

# ----------------------------------------------------------------------------
# 1. Process textures (PBR -> Q3 shader compatible)
# ----------------------------------------------------------------------------
echo ""
echo "[1/6] Processing textures..."
python3 process_textures.py

# ----------------------------------------------------------------------------
# 1b. Regenerate the map from its generator
# ----------------------------------------------------------------------------
# The committed .map is a generated artefact. Regenerating keeps the two in
# step and makes the format constraints checkable in one place.
echo ""
echo "[1b/6] Generating map geometry..."
python3 tools/gen_map.py -o "maps/${MAP_NAME}.map"

# ----------------------------------------------------------------------------
# 2. Stage assets into the game directory
# ----------------------------------------------------------------------------
# This has to happen before compiling, not after. q3map2 reads the shader
# scripts and images out of the game's VFS while it compiles, so a BSP built
# before the assets are staged is lit against q3map2's default image and keeps
# that baked into its lightmaps. The order in this script is load-bearing.
echo ""
echo "[2/6] Staging assets into ${BASEQ3_PATH}..."
mkdir -p "${BASEQ3_PATH}/maps"
mkdir -p "${BASEQ3_PATH}/textures/showcase"
mkdir -p "${BASEQ3_PATH}/scripts"

cp -r textures/showcase/_compiled/. "${BASEQ3_PATH}/textures/showcase/" 2>/dev/null || true
cp scripts/showcase.shader "${BASEQ3_PATH}/scripts/"

# q3map2 only parses the .shader files named in shaderlist.txt. Without this
# entry the shader script is invisible to both q3map2 and the engine, and every
# surface falls back to an unnamed default shader.
SHADERLIST="${BASEQ3_PATH}/scripts/shaderlist.txt"
if ! grep -qxF "showcase" "${SHADERLIST}" 2>/dev/null; then
    touch "${SHADERLIST}"
    echo "showcase" >> "${SHADERLIST}"
fi

# ----------------------------------------------------------------------------
# 3. Compile BSP
# ----------------------------------------------------------------------------
echo ""
echo "[3/6] Compiling BSP..."
mkdir -p "${BUILD_DIR}"
cp "maps/${MAP_NAME}.map" "${BUILD_DIR}/"
cd "${BUILD_DIR}"

# q3map2 resolves shaders and images through the game's VFS, so it must be told
# where baseq3 is.
Q3MAP2_COMMON=(-fs_basepath "${GAME_ROOT}")

# NOTE: -meta is intentionally omitted. It converts brush faces into meta
# triangles, which is only what you want for a collision proxy. Left on, every
# authored surface is emitted as SURFACE_META instead of SURFACE_FACE and the
# map's visible geometry comes from the shader path alone.

echo "  BSP..."
${Q3MAP2} "${Q3MAP2_COMMON[@]}" -bsp "${MAP_NAME}.map"

# Light stage. This previously died with SIGSEGV and the map geometry was
# blamed. That diagnosis was wrong: q3map2 segfaults on an empty BSP, and this
# map file was producing zero brushes (see tools/gen_map.py). With real
# geometry present this stage completes normally.
echo "  Light (raster)..."
${Q3MAP2} "${Q3MAP2_COMMON[@]}" -light -fast -filter -super 2 \
    -bounce 4 -samples 2 -threads "$(nproc)" "${MAP_NAME}.map"

# Fail loudly if the BSP came out empty. q3map2 does not treat this as an
# error: it writes a small, valid, geometry-free .bsp and exits 0, which is how
# an entirely unparseable map can look like a successful build.
#
# Note that `q3map2 -info` exits 1 even on a perfectly good BSP, so its exit
# status must be discarded. Under `set -o pipefail` the status of the whole
# pipeline would otherwise fail the build here rather than at the check below.
SURFACES=$(${Q3MAP2} -info "${MAP_NAME}.bsp" 2>/dev/null \
    | awk '/drawsurfaces/ { print $1; exit }' || true)
if [ -z "${SURFACES}" ] || [ "${SURFACES}" -eq 0 ]; then
    echo "ERROR: ${MAP_NAME}.bsp has 0 drawsurfaces." >&2
    echo "       q3map2 accepted the map but emitted no geometry." >&2
    echo "       Check that brushes are brace-delimited with 3-point faces." >&2
    exit 1
fi
echo "  Verified: ${SURFACES} drawsurfaces"

# ----------------------------------------------------------------------------
# 4. Install the compiled BSP for testing
# ----------------------------------------------------------------------------
echo ""
echo "[4/6] Installing BSP..."
cp "${MAP_NAME}.bsp" "${BASEQ3_PATH}/maps/"

# ----------------------------------------------------------------------------
# 5. Create .pk3 for distribution
# ----------------------------------------------------------------------------
echo ""
echo "[5/6] Creating ${PK3_NAME}..."
cd ../..
mkdir -p "release/${GAME_DIR}/maps"
mkdir -p "release/${GAME_DIR}/textures/showcase"
mkdir -p "release/${GAME_DIR}/scripts"

cp "${BUILD_DIR}/${MAP_NAME}.bsp" "release/${GAME_DIR}/maps/"
cp -r textures/showcase/_compiled/* "release/${GAME_DIR}/textures/showcase/" 2>/dev/null || true
cp scripts/showcase.shader "release/${GAME_DIR}/scripts/"
cp config/showcase.cfg "release/${GAME_DIR}/"

# Ship a shaderlist.txt that references our shader script. A .pk3 that carries
# the .shader but no shaderlist entry loads nothing: the file sits in the
# archive, unread.
SHADERLIST="release/${GAME_DIR}/scripts/shaderlist.txt"
if ! grep -qxF "showcase" "${SHADERLIST}" 2>/dev/null; then
    if [ ! -f "${SHADERLIST}" ]; then
        : > "${SHADERLIST}"
    fi
    echo "showcase" >> "${SHADERLIST}"
fi

# Rebuild the archive from scratch. `zip -r` on an existing pk3 appends and
# leaves entries from the previous build in place, so a deleted or renamed
# texture would survive in the shipped package indefinitely.
cd release
rm -f "../${PK3_NAME}"
zip -r -q "../${PK3_NAME}" .
cd ..

# ----------------------------------------------------------------------------
# 6. Verify the package
# ----------------------------------------------------------------------------
echo ""
echo "[6/6] Verifying package..."

PK3_SURFACES=$(unzip -p "${PK3_NAME}" "${GAME_DIR}/maps/${MAP_NAME}.bsp" \
    > "${BUILD_DIR}/.verify.bsp" 2>/dev/null && \
    ${Q3MAP2} -info "${BUILD_DIR}/.verify.bsp" 2>/dev/null \
    | awk '/drawsurfaces/ { print $1; exit }' || true)
rm -f "${BUILD_DIR}/.verify.bsp"

if [ -z "${PK3_SURFACES}" ] || [ "${PK3_SURFACES}" -eq 0 ]; then
    echo "ERROR: ${PK3_NAME} does not contain usable geometry." >&2
    exit 1
fi
echo "  ${PK3_NAME}: ${PK3_SURFACES} drawsurfaces"

if ! unzip -l "${PK3_NAME}" | grep -q "${GAME_DIR}/scripts/shaderlist.txt"; then
    echo "ERROR: ${PK3_NAME} has no shaderlist.txt; the shaders will not load." >&2
    exit 1
fi
echo "  shaderlist.txt present"

# ----------------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------------
echo ""
echo "Done!"
echo "===================="
echo "Map BSP:     ${BUILD_DIR}/${MAP_NAME}.bsp"
echo "Package:     ${PK3_NAME}"
echo "Installed:   ${BASEQ3_PATH}/maps/${MAP_NAME}.bsp"
echo ""
echo "Test commands:"
echo "  Raster:  ${VKQ3NG_BIN} +set fs_basePath ${BASEQ3_PATH} +set r_vertexLight 0 +map ${MAP_NAME}"
echo "  RT:      ${VKQ3NG_BIN} +set fs_basePath ${BASEQ3_PATH} +set r_vertexLight 2 +set rt_numSamples 4 +set rt_accumulate 1 +map ${MAP_NAME}"
echo "  Config:  ${VKQ3NG_BIN} +set fs_basePath ${BASEQ3_PATH} +exec showcase.cfg +map ${MAP_NAME}"
echo ""
echo "In-game screenshot: bind a key to 'screenshot' or use console:"
echo "  screenshot ${MAP_NAME}_rt"
echo "  screenshot ${MAP_NAME}_raster"