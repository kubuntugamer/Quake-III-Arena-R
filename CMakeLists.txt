# cmake -H. -B_project -G Xcode
# cmake -H. -B_project -G  "Visual Studio 15 2017 Win64"

cmake_minimum_required(VERSION 3.14.0)
project(Quake_III_Arena)

# Quake III

set(APP_MACOS quake3)
set(APP_WIN32 quake3)
set(APP_UNIX quake3)
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
# Set path to baseq3 folder
if(WIN32)
	set(BASEQ3_DIR "+set fs_basePath \"C:\\GOG Games\\Quake III\"")
elseif(APPLE)
	#set(BASEQ3_DIR "+set fs_basePath \\\"/dir/to/Quake III\\\"")
else(UNIX)
	#quake III files in home/.q3a
endif()
# disable ZERO_CHECK target
set(CMAKE_SUPPRESS_REGENERATION true)

if(WIN32)
	IF(MSVC)
		message(STATUS "Setting MSVC flags")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /EHc /std:clatest")
	ENDIF(MSVC)
	
	# check Vulkan
	find_package(Vulkan REQUIRED)
	find_path(VULKAN_INCLUDE_DIR NAMES vulkan/vulkan.h HINTS
        "$ENV{VULKAN_SDK}/Include"
        "$ENV{VK_SDK_PATH}/Include")

	if (VULKAN_FOUND)
		message(STATUS "Found Vulkan")
	else(VULKAN_FOUND)
		message(STATUS "Vulkan not found")
	endif (VULKAN_FOUND)
	
	#add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
elseif(APPLE)
	# check Vulkan
	find_package(Vulkan REQUIRED)
	find_path(VULKAN_INCLUDE_DIR NAMES vulkan/vulkan.h HINTS
        "$ENV{VULKAN_SDK}/Include"
        "$ENV{VK_SDK_PATH}/Include")

	if (VULKAN_FOUND)
		message(STATUS "Found Vulkan")
	else(VULKAN_FOUND)
		message(STATUS "Vulkan not found")
	endif (VULKAN_FOUND)
	#add_definitions(-DVK_USE_PLATFORM_MACOS_MVK)
	#set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	set(CMAKE_XCODE_GENERATE_SCHEME TRUE)
else(UNIX)
	find_package(X11 REQUIRED)
	if (X11_FOUND)
		message(STATUS "Found X11")
	endif (X11_FOUND)
	# Vulkan (SDK via $VULKAN_SDK, e.g. /home/chazz/Downloads/1.4.363.0/x86_64)
	find_package(Vulkan REQUIRED)
	find_path(VULKAN_INCLUDE_DIR NAMES vulkan/vulkan.h HINTS
		"$ENV{VULKAN_SDK}/include")
	if (VULKAN_FOUND)
		message(STATUS "Found Vulkan: ${Vulkan_VERSION}")
	else(VULKAN_FOUND)
		message(STATUS "Vulkan not found")
	endif (VULKAN_FOUND)
	#find_package(Threads REQUIRED)
	#set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

# OUTPUT DIR
# DEFAULT
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# DEBUG
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/Debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin/Debug")

# RELEASE
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/Release")

add_subdirectory(code)


