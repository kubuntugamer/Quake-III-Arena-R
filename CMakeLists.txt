# cmake -H. -B_build

cmake_minimum_required(VERSION 3.16.0)
project(vkq3ng VERSION 1.0.0 LANGUAGES C CXX)

# vkq3ng - Vulkan/DX12 Quake 3 Next Generation

# Custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(APP_UNIX vkq3ng)
set(LIB_RENDERER renderer)
set(LIB_GAME qagame)
set(LIB_CGAME cgame)
set(LIB_BOTLIB botlib)
set(LIB_SPLINES Splines)
set(LIB_UI ui_new)
set(LIB_Q3UI ui)

# TOOLS
set(TOOL_Q3ASM q3asm)

# Use dynamic libs instead of bytecode vm
set(Q3_VM_DYNAMIC TRUE)

# Arguments for IDE
set(Q3_ARGUMENTS "")
# disable ZERO_CHECK target
set(CMAKE_SUPPRESS_REGENERATION true)

# Platform detection
# Vulkan renderer (always evaluated; platform‑agnostic)
set(PLATFORM_LINUX 1)
message(STATUS "Platform: Linux (cross‑compile to Windows)")
find_package(X11 REQUIRED)
if (X11_FOUND)
    message(STATUS "Found X11")
endif (X11_FOUND)

# Vulkan
find_package(Vulkan REQUIRED HINTS
    "$ENV{VULKAN_SDK_PATH}/lib"
    "$ENV{VULKAN_SDK_PATH}/include")
if (VULKAN_FOUND)
    message(STATUS "Found Vulkan: ${Vulkan_VERSION}")
else(VULKAN_FOUND)
    message(FATAL_ERROR "Vulkan SDK not found – set VULKAN_SDK_PATH")
endif (VULKAN_FOUND)

# Renderer backend selection
option(BUILD_VULKAN_RENDERER "Build Vulkan renderer" ON)
option(BUILD_DX12_RENDERER "Build DX12 renderer" OFF)

if (WIN32 AND BUILD_DX12_RENDERER)
    message(STATUS "Using DX12 renderer")
elseif (BUILD_VULKAN_RENDERER)
    message(STATUS "Using Vulkan renderer")
else()
    message(FATAL_ERROR "No renderer backend selected")
endif()

# Always use "renderer" as the target name (both Vulkan and DX12 provide this)
set(LIB_RENDERER renderer)

# OUTPUT DIR - only set defaults for non-packaging builds
#
# Whether this is a packaging build is inferred from an explicitly supplied
# CMAKE_INSTALL_PREFIX (which is how dpkg-buildpackage invokes us, see
# debian/rules), but the answer is cached rather than re-derived.
#
# It has to be cached: the prefix gets FORCE-set to /usr below, and once that
# lands in the cache CMake reports CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT as
# false. Re-deriving from it therefore flipped local development builds from
# ./bin/ into the build tree on the second `cmake` run, silently relocating
# vkq3ng.engine. Pass -DVKQ3NG_PACKAGE_BUILD=ON/OFF to decide explicitly.
if(NOT DEFINED VKQ3NG_PACKAGE_BUILD)
  if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(VKQ3NG_PACKAGE_BUILD OFF CACHE BOOL "Build for packaging (honour CMAKE_INSTALL_PREFIX for output locations)")
  else()
    set(VKQ3NG_PACKAGE_BUILD ON CACHE BOOL "Build for packaging (honour CMAKE_INSTALL_PREFIX for output locations)")
  endif()
endif()

if(VKQ3NG_PACKAGE_BUILD)
  # Packaging build (e.g., dpkg-buildpackage) - leave the output directories
  # alone so everything lands in the build tree.
  message(STATUS "Output directories: build tree (packaging build)")
else()
  # Local development build - use source-relative bin/
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/Debug")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/Debug")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/Debug")

  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release")

  message(STATUS "Output directories: ${CMAKE_SOURCE_DIR}/bin (development build)")
endif()

# Installation prefix for packaging
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix" FORCE)
endif()

# Game data directory
set(Q3_DATADIR "share/games/vkq3ng")

add_subdirectory(code)

# Install game data directory structure
install(DIRECTORY DESTINATION "${Q3_DATADIR}/baseq3"
        FILES_MATCHING PATTERN "*.pk3")