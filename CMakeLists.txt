# cmake -H. -B_build

cmake_minimum_required(VERSION 3.14.0)
project(vkq3ng VERSION 1.0.0 LANGUAGES C)

# vkq3ng - Vulkan Quake 3 Next Generation

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

# Linux/Unix only - Vulkan + X11/Wayland
find_package(X11 REQUIRED)
if (X11_FOUND)
    message(STATUS "Found X11")
endif (X11_FOUND)

# Vulkan
find_package(Vulkan REQUIRED)
find_path(VULKAN_INCLUDE_DIR NAMES vulkan/vulkan.h HINTS
    "$ENV{VULKAN_SDK}/include")
if (VULKAN_FOUND)
    message(STATUS "Found Vulkan: ${Vulkan_VERSION}")
else(VULKAN_FOUND)
    message(STATUS "Vulkan not found")
endif (VULKAN_FOUND)

# OUTPUT DIR - only set defaults for non-packaging builds
if(NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  # Packaging build (e.g., dpkg-buildpackage) - use CMAKE_INSTALL_PREFIX
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