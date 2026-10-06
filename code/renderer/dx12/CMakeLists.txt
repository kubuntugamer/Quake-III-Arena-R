# DX12 Renderer
file(GLOB_RECURSE DX12_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

foreach(FILE ${DX12_SOURCES})
    get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")
    string(REPLACE "/" "\\" GROUP "${GROUP}")
    if ("${FILE}" MATCHES ".*\\.(cpp|c|h)")
        set(GROUP "Source Files${GROUP}")
    else()
        set(GROUP "Header Files${GROUP}")
    endif()
    source_group("${GROUP}" FILES "${FILE}")
endforeach()

add_library(renderer ${DX12_SOURCES})
target_include_directories(renderer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/code/qcommon
    ${CMAKE_SOURCE_DIR}/code/renderer
)

target_compile_options(renderer PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/std:c++17>
    $<$<CXX_COMPILER_ID:GNU>:-std=c++17>
    $<$<CXX_COMPILER_ID:Clang>:-std=c++17>
)

# Link DX12 libraries
if (MSVC)
    target_link_libraries(renderer PRIVATE
        d3d12.lib
        dxgi.lib
        d3dcompiler.lib
        dxguid.lib
    )
    # dxc.lib for shader compilation
    find_library(DXC_LIB dxc.lib PATHS "${WindowsSDK_LibraryPath_x64}" NO_DEFAULT_PATH)
    if (DXC_LIB)
        target_link_libraries(renderer PRIVATE ${DXC_LIB})
    endif()
else()
    # MinGW/clang on Windows
    target_link_libraries(renderer PRIVATE
        d3d12
        dxgi
        d3dcompiler
        dxguid
    )
    find_library(DXC_LIB dxc PATHS "${CMAKE_INSTALL_PREFIX}/lib" NO_DEFAULT_PATH)
    if (DXC_LIB)
        target_link_libraries(renderer PRIVATE ${DXC_LIB})
    endif()
endif()

# Install
install(TARGETS renderer
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)