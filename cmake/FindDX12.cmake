# FindDX12.cmake - Find DirectX 12 libraries
# Based on FindDX12 from various sources

if (DX12_FOUND)
    return()
endif()

# DX12 is part of Windows SDK
# We need to find the Windows SDK path

if (WIN32)
    # Check common Windows SDK locations
    set(WINDOWS_SDK_PATHS
        "$ENV{WINDOWSSDKDIR}"
        "$ENV{WindowsSDKDir}"
        "$ENV{WindowsSdkDir}"
        "C:/Program Files (x86)/Windows Kits/10"
        "C:/Program Files/Windows Kits/10"
    )

    # Find the latest Windows 10 SDK
    foreach(path ${WINDOWS_SDK_PATHS})
        if (EXISTS "${path}")
            file(GLOB sdk_versions "${path}/Lib/*/um/x64/d3d12.lib")
            if (sdk_versions)
                # Sort versions and pick latest
                list(SORT sdk_versions)
                list(GET sdk_versions -1 latest_lib)
                get_filename_component(sdk_version "${latest_lib}" DIRECTORY)
                get_filename_component(sdk_version "${sdk_version}" DIRECTORY)
                get_filename_component(sdk_version "${sdk_version}" NAME)
                set(DX12_LIBRARY_DIR "${sdk_version}")
                set(WINDOWS_SDK_VERSION "${sdk_version}")
                break()
            endif()
        endif()
    endforeach()

    if (DX12_LIBRARY_DIR)
        # Construct full paths
        set(DX12_INCLUDE_DIRS "${path}/Include/${DX12_LIBRARY_DIR}/um")
        set(DX12_LIBRARY_DIRS "${path}/Lib/${DX12_LIBRARY_DIR}/um/x64")
        
        # Verify libraries exist
        set(DX12_LIBRARIES
            "${DX12_LIBRARY_DIRS}/d3d12.lib"
            "${DX12_LIBRARY_DIRS}/dxgi.lib"
            "${DX12_LIBRARY_DIRS}/d3dcompiler.lib"
            "${DX12_LIBRARY_DIRS}/dxguid.lib"
        )
        
        foreach(lib ${DX12_LIBRARIES})
            if (NOT EXISTS "${lib}")
                message(WARNING "DX12 library not found: ${lib}")
            endif()
        endforeach()
        
        set(DX12_FOUND TRUE)
        message(STATUS "Found DX12: ${WINDOWS_SDK_VERSION}")
    else()
        message(FATAL_ERROR "DirectX 12 not found. Please install Windows 10 SDK.")
    endif()
else()
    # Cross-compiling from Linux - use MinGW or vcpkg
    find_library(DX12_D3D12_LIB d3d12)
    find_library(DX12_DXGI_LIB dxgi)
    find_library(DX12_D3DCOMPILER_LIB d3dcompiler)
    find_library(DX12_DXGUID_LIB dxguid)
    
    if (DX12_D3D12_LIB AND DX12_DXGI_LIB AND DX12_D3DCOMPILER_LIB AND DX12_DXGUID_LIB)
        set(DX12_LIBRARIES ${DX12_D3D12_LIB} ${DX12_DXGI_LIB} ${DX12_D3DCOMPILER_LIB} ${DX12_DXGUID_LIB})
        set(DX12_FOUND TRUE)
    else()
        message(FATAL_ERROR "DirectX 12 libraries not found for cross-compilation. Use vcpkg or install mingw-w64.")
    endif()
endif()

# Set variables for consumers
set(DX12_INCLUDE_DIRS ${DX12_INCLUDE_DIRS} CACHE INTERNAL "DX12 include directories")
set(DX12_LIBRARY_DIRS ${DX12_LIBRARY_DIRS} CACHE INTERNAL "DX12 library directories")
set(DX12_LIBRARIES ${DX12_LIBRARIES} CACHE INTERNAL "DX12 libraries")

mark_as_advanced(DX12_INCLUDE_DIRS DX12_LIBRARY_DIRS DX12_LIBRARIES)