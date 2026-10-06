# Win32 Platform Layer
file(GLOB_RECURSE WIN32_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.rc
)

foreach(FILE ${WIN32_SOURCES})
    get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")
    string(REPLACE "/" "\\" GROUP "${GROUP}")
    if ("${FILE}" MATCHES ".*\\.(cpp|c|rc)")
        set(GROUP "Source Files${GROUP}")
    else()
        set(GROUP "Header Files${GROUP}")
    endif()
    source_group("${GROUP}" FILES "${FILE}")
endforeach()

add_executable(vkq3ng_win32 ${WIN32_SOURCES})
target_include_directories(vkq3ng_win32 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/code/qcommon
    ${CMAKE_SOURCE_DIR}/code/renderer/dx12
)

target_link_libraries(vkq3ng_win32 PRIVATE
    dx12
    ${DX12_LIBRARIES}
    ${DXC_LIBRARIES}
)

target_compile_options(vkq3ng_win32 PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/std:c++17 /DWIN32 /D_WIN32 /D_CRT_SECURE_NO_WARNINGS>
    $<$<CXX_COMPILER_ID:GNU>:-std=c++17 -DWIN32 -D_WIN32>
)

# Windows-specific settings
if (MSVC)
    target_link_options(vkq3ng_win32 PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:WinMain)
    set_target_properties(vkq3ng_win32 PROPERTIES
        WIN32_EXECUTABLE TRUE
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()

# Install
install(TARGETS vkq3ng_win32
    RUNTIME DESTINATION bin
)