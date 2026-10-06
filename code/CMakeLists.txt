# STATIC LIB
add_subdirectory(botlib)
add_subdirectory(splines)

# Renderer backend
if (WIN32 AND BUILD_DX12_RENDERER)
    add_subdirectory(renderer/dx12)
else()
    add_subdirectory(renderer)
endif()

# DYNAMIC LIB
add_subdirectory(cgame)
add_subdirectory(game)
add_subdirectory(q3_ui)
add_subdirectory(ui)

# Platform layer
if (WIN32)
    add_subdirectory(win32)
else()
    # EXECUTABLE (Linux/Unix only)
    add_subdirectory(unix)
endif()

# tools
add_subdirectory(tools)