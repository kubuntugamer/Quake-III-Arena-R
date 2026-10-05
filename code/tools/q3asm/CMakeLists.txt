# SOURCE AND HEADER FILES
# add all src files and create file hierarchy for IDE
file(GLOB_RECURSE SOURCES *.cpp *.c *.m *.mm *.hpp *.h)
foreach(FILE ${SOURCES}) 
    # Get the directory of the source file
    get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)

    # Remove common directory prefix to make the group
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")

    # Make sure we are using windows slashes
    string(REPLACE "/" "\\" GROUP "${GROUP}")

    # Group into "Source Files" and "Header Files"
    if ("${FILE}" MATCHES ".*\\.\(cpp|c|m\)")
       set(GROUP "Source Files${GROUP}")
    elseif("${FILE}" MATCHES ".*\\.\(hpp|h\)")
       set(GROUP "Header Files${GROUP}")
    endif()

    source_group("${GROUP}" FILES "${FILE}")
endforeach()

add_executable(${TOOL_Q3ASM} ${SOURCES})
set_target_properties(${TOOL_Q3ASM} PROPERTIES LINKER_LANGUAGE C)

# Disable -Werror for q3asm as it's an older tool with warnings we can't easily fix
target_compile_options(${TOOL_Q3ASM} PRIVATE -Wno-error=format-overflow -Wno-error=implicit-function-declaration -Wno-error=format-security)

# INSTALL
install(TARGETS ${TOOL_Q3ASM}
        RUNTIME DESTINATION lib/vkq3ng
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

