# DX12 Base Sample
#
# Standalone DX12 foundation. No Quake III code is referenced, so this builds
# on any Windows machine with a DX12-capable GPU and nothing else installed.
#
#   cmake -B build -S .
#   cmake --build build --config Release
#   ./build/Release/dx12_base.exe

cmake_minimum_required(VERSION 3.20)
project(dx12_base VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(FATAL_ERROR
        "This sample targets Windows because it uses Win32 windowing and "
        "DirectX 12 headers. To build it from Linux, install the mingw-w64 "
        "cross toolchain and configure with "
        "-DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake")
endif()

add_executable(dx12_base
    src/main.cpp
    src/win32_window.cpp
    src/dx12_device.cpp
    src/dx12_swapchain.cpp
    src/dx12_pipeline.cpp
    src/dx12_renderer.cpp
)

target_include_directories(dx12_base PRIVATE src)

# UNICODE is needed so the W-suffixed Win32 and DXGI entry points are declared.
target_compile_definitions(dx12_base PRIVATE UNICODE _UNICODE)

target_link_libraries(dx12_base PRIVATE
    d3d12
    dxgi
    d3dcompiler
    dxguid
)

# Release builds get optimisation; Debug builds get the D3D12 debug layer.
target_compile_options(dx12_base PRIVATE
    /W4
    $<$<CONFIG:Release>:/O2>
    $<$<CONFIG:Debug>:/Od>
)

# Keep the console so the adapter printout and any shader errors are visible.
# Entry point is main(), so CONSOLE is the correct subsystem.
set_target_properties(dx12_base PROPERTIES
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)