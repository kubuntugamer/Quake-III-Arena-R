# FindDXC.cmake - Find DirectX Shader Compiler (dxc)

if (DXC_FOUND)
    return()
endif()

if (WIN32)
    # Windows - part of Windows SDK or separate install
    set(DXC_SEARCH_PATHS
        "$ENV{VULKAN_SDK}/Bin"
        "$ENV{WINDOWSSDKDIR}/bin"
        "$ENV{WindowsSDKDir}/bin"
        "C:/Program Files (x86)/Windows Kits/10/bin"
        "C:/Program Files/Windows Kits/10/bin"
        "C:/Program Files/Microsoft DirectX Shader Compiler/bin"
    )
    
    find_program(DXC_EXECUTABLE dxc.exe PATHS ${DXC_SEARCH_PATHS} NO_DEFAULT_PATH)
    find_library(DXC_LIB dxc.lib PATHS
        "$ENV{VULKAN_SDK}/Lib"
        "$ENV{WINDOWSSDKDIR}/Lib"
        "$ENV{WindowsSDKDir}/Lib"
        "C:/Program Files (x86)/Windows Kits/10/Lib"
        "C:/Program Files/Windows Kits/10/Lib"
        "C:/Program Files/Microsoft DirectX Shader Compiler/lib"
        NO_DEFAULT_PATH
    )
    
    if (DXC_EXECUTABLE AND DXC_LIB)
        set(DXC_FOUND TRUE)
        set(DXC_EXECUTABLE ${DXC_EXECUTABLE})
        set(DXC_LIBRARIES ${DXC_LIB})
        message(STATUS "Found DXC: ${DXC_EXECUTABLE}")
    else()
        message(WARNING "DXC not found. Shader compilation will not work.")
    endif()
else()
    # Linux cross-compilation - use vcpkg or system package
    find_library(DXC_LIB dxc PATHS
        "${CMAKE_INSTALL_PREFIX}/lib"
        "/usr/lib"
        "/usr/local/lib"
        "/opt/vcpkg/installed/x64-windows/lib"
        NO_DEFAULT_PATH
    )
    find_program(DXC_EXECUTABLE dxc PATHS
        "${CMAKE_INSTALL_PREFIX}/bin"
        "/usr/bin"
        "/usr/local/bin"
        "/opt/vcpkg/installed/x64-windows/bin"
        NO_DEFAULT_PATH
    )
    
    if (DXC_LIB AND DXC_EXECUTABLE)
        set(DXC_FOUND TRUE)
        set(DXC_LIBRARIES ${DXC_LIB})
    else()
        message(WARNING "DXC not found for cross-compilation.")
    endif()
endif()

if (DXC_FOUND)
    set(DXC_LIBRARIES ${DXC_LIBRARIES} CACHE INTERNAL "DXC libraries")
    set(DXC_EXECUTABLE ${DXC_EXECUTABLE} CACHE INTERNAL "DXC executable")
endif()

mark_as_advanced(DXC_EXECUTABLE DXC_LIBRARIES)