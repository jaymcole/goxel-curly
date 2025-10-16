cmake_minimum_required(VERSION 3.15)
project(goxel VERSION 0.15.1 LANGUAGES C CXX)

# On Windows, prefer Ninja or MinGW Makefiles
if(WIN32 AND NOT CMAKE_GENERATOR)
    set(CMAKE_GENERATOR "MinGW Makefiles" CACHE STRING "CMake generator" FORCE)
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)  # gnu99

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)  # gnu++17

# Build options
option(ENABLE_WERROR "Treat warnings as errors" ON)
option(ENABLE_SOUND "Enable sound support" OFF)
option(ENABLE_YOCTO "Enable yocto renderer" OFF)
option(ENABLE_ASAN "Enable address sanitizer (Debug only)" OFF)

# Platform detection
if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

# NFD backend option for Linux
if(LINUX)
    set(NFD_BACKEND "gtk" CACHE STRING "Native file dialog backend")
    set_property(CACHE NFD_BACKEND PROPERTY STRINGS "gtk" "portal")
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ext_src
    ${CMAKE_SOURCE_DIR}/ext_src/uthash
    ${CMAKE_SOURCE_DIR}/ext_src/stb
    ${CMAKE_SOURCE_DIR}/ext_src/nfd
    ${CMAKE_SOURCE_DIR}/ext_src/noc
    ${CMAKE_SOURCE_DIR}/ext_src/xxhash
    ${CMAKE_SOURCE_DIR}/ext_src/meshoptimizer
)

# Compiler flags
if(MSVC)
    add_compile_options(/W4)
    if(ENABLE_WERROR)
        add_compile_options(/WX)
    endif()
else()
    add_compile_options(
        -Wall
        -Wno-unknown-pragma
        -Wno-unknown-warning-option
    )
    if(ENABLE_WERROR)
        add_compile_options(-Werror)
    endif()

    # C++ specific flags
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-narrowing>)

    # Windows (MSYS2) specific flags
    if(WIN32)
        add_compile_options(
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-attributes>
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-variable>
            $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-function>
            -Wno-error=address
        )
    endif()

    # macOS specific flags
    if(APPLE)
        add_compile_options(-Wno-deprecated-declarations)
    endif()
endif()

# Build type specific flags
if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_options(-O0 -g)
    if(LINUX AND ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fsanitize=undefined)
        add_link_options(-fsanitize=address -fsanitize=undefined)
        link_libraries(asan ubsan)
    endif()
elseif(CMAKE_BUILD_TYPE MATCHES Release OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
    add_compile_options(-O3)
    add_compile_definitions(NDEBUG)
elseif(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
    add_compile_options(-g)
endif()

# Force include config.h
add_compile_options(-include ${CMAKE_SOURCE_DIR}/src/config.h)

# Collect all source files (only from src/, not ext_src/)
file(GLOB_RECURSE GOXEL_C_SOURCES src/*.c)
file(GLOB_RECURSE GOXEL_CXX_SOURCES src/*.cpp)
set(GOXEL_SOURCES ${GOXEL_C_SOURCES} ${GOXEL_CXX_SOURCES})

# External library sources (C)
# Note: Most external C libraries are compiled via unity build files in src/:
#   - src/xxhash.c includes ext_src/xxhash/xxhash.c
#   - src/quickjs.c includes ext_src/quickjs/*.c
#   - src/utils/json.c includes ext_src/json/*.c
#   - src/utils/ini.c includes ext_src/inih/ini.c
# Only platform-specific sources without unity builds are listed below.
set(EXT_C_SOURCES
)

# External library sources (C++)
# Note: imgui, meshoptimizer, and yocto are compiled via unity build files in src/
# (src/imgui.cpp, src/meshoptimizer.cpp, src/yocto.cpp)
set(EXT_CXX_SOURCES
)

# Yocto renderer sources
# Note: Yocto files are included via src/yocto.cpp unity build
if(NOT ENABLE_YOCTO)
    add_compile_definitions(YOCTO=0)
endif()

# Platform-specific sources and dependencies
if(WIN32)
    # Windows (MSYS2/MinGW)
    list(APPEND EXT_C_SOURCES ext_src/glew/glew.c)
    list(APPEND EXT_CXX_SOURCES ext_src/nfd/nfd_win.cpp)
    include_directories(ext_src/glew)
    add_compile_definitions(GLEW_STATIC FREE_WINDOWS)

    # Windows libraries
    # Add MSYS2/MinGW library search paths
    if(MINGW)
        list(APPEND CMAKE_PREFIX_PATH "C:/msys64/mingw64")
        list(APPEND CMAKE_LIBRARY_PATH "C:/msys64/mingw64/lib")
    endif()

    find_library(GLFW3_LIBRARY NAMES glfw3 glfw REQUIRED)
    find_library(OPENGL_LIBRARY opengl32 REQUIRED)
    find_library(Z_LIBRARY z REQUIRED)
    find_library(TRE_LIBRARY tre REQUIRED)

    set(PLATFORM_LIBS
        ${GLFW3_LIBRARY}
        ${OPENGL_LIBRARY}
        ${Z_LIBRARY}
        ${TRE_LIBRARY}
        gdi32
        Comdlg32
        ole32
        uuid
        shell32
    )

    # Static linking on Windows
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

elseif(APPLE)
    # macOS
    list(APPEND EXT_CXX_SOURCES ext_src/nfd/nfd_cocoa.m)

    # Find required packages
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW3 REQUIRED glfw3)

    # macOS frameworks
    find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(UNIFORM_TYPE_IDENTIFIERS_FRAMEWORK UniformTypeIdentifiers REQUIRED)

    set(PLATFORM_LIBS
        ${GLFW3_LIBRARIES}
        ${OPENGL_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${APPKIT_FRAMEWORK}
        ${UNIFORM_TYPE_IDENTIFIERS_FRAMEWORK}
        m
        objc
    )

    include_directories(${GLFW3_INCLUDE_DIRS})
    link_directories(${GLFW3_LIBRARY_DIRS})

    # Sound is disabled on macOS by default
    set(ENABLE_SOUND OFF CACHE BOOL "Sound disabled on macOS" FORCE)

elseif(LINUX)
    # Linux
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW3 REQUIRED glfw3)

    # NFD backend selection
    if(NFD_BACKEND STREQUAL "portal")
        pkg_check_modules(DBUS REQUIRED dbus-1)
        list(APPEND EXT_CXX_SOURCES ext_src/nfd/nfd_portal.cpp)
        include_directories(${DBUS_INCLUDE_DIRS})
        list(APPEND PLATFORM_LIBS ${DBUS_LIBRARIES})
    elseif(NFD_BACKEND STREQUAL "gtk")
        pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
        list(APPEND EXT_CXX_SOURCES ext_src/nfd/nfd_gtk.cpp)
        include_directories(${GTK3_INCLUDE_DIRS})
        list(APPEND PLATFORM_LIBS ${GTK3_LIBRARIES})
    endif()

    set(PLATFORM_LIBS
        ${PLATFORM_LIBS}
        ${GLFW3_LIBRARIES}
        GL
        m
        dl
        pthread
    )

    include_directories(${GLFW3_INCLUDE_DIRS})
    link_directories(${GLFW3_LIBRARY_DIRS})
endif()

# Check for libpng
find_package(PNG)
if(PNG_FOUND)
    add_compile_definitions(HAVE_LIBPNG=1)
    include_directories(${PNG_INCLUDE_DIRS})
    list(APPEND PLATFORM_LIBS ${PNG_LIBRARIES})
endif()

# Sound support
if(ENABLE_SOUND)
    find_library(OPENAL_LIBRARY openal REQUIRED)
    add_compile_definitions(SOUND=1)
    list(APPEND PLATFORM_LIBS ${OPENAL_LIBRARY})
endif()

# Create executable
add_executable(goxel
    ${GOXEL_SOURCES}
    ${EXT_C_SOURCES}
    ${EXT_CXX_SOURCES}
)

# Link libraries
target_link_libraries(goxel ${PLATFORM_LIBS})

# Set output directory
set_target_properties(goxel PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# Installation
install(TARGETS goxel DESTINATION bin)

# Print configuration summary
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Warnings as errors: ${ENABLE_WERROR}")
message(STATUS "Sound support: ${ENABLE_SOUND}")
message(STATUS "Yocto renderer: ${ENABLE_YOCTO}")
if(LINUX)
    message(STATUS "NFD backend: ${NFD_BACKEND}")
endif()
if(LINUX AND CMAKE_BUILD_TYPE MATCHES Debug)
    message(STATUS "Address sanitizer: ${ENABLE_ASAN}")
endif()
