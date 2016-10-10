# CMake 3.2 required for cmake to know the right flags for CXX standard on OSX
cmake_minimum_required(VERSION 3.2)

function(download_bundled_external remote_path lib_name prefix_var)
    set(prefix "${CMAKE_BINARY_DIR}/externals/${lib_name}")
    if (NOT EXISTS "${prefix}")
        message(STATUS "Downloading binaries for ${lib_name}...")
        file(DOWNLOAD
            https://github.com/citra-emu/ext-windows-bin/raw/master/${remote_path}${lib_name}.7z
            "${CMAKE_BINARY_DIR}/externals/${lib_name}.7z" SHOW_PROGRESS)
        execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${CMAKE_BINARY_DIR}/externals/${lib_name}.7z"
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/externals")
    endif()
    message(STATUS "Using bundled binaries at ${prefix}")
    set(${prefix_var} "${prefix}" PARENT_SCOPE)
endfunction()

include(CheckSymbolExists)
function(detect_architecture symbol arch)
    if (NOT DEFINED ARCHITECTURE)
        set(CMAKE_REQUIRED_QUIET 1)
        check_symbol_exists("${symbol}" "" ARCHITECTURE_${arch})
        unset(CMAKE_REQUIRED_QUIET)

        # The output variable needs to be unique across invocations otherwise
        # CMake's crazy scope rules will keep it defined
        if (ARCHITECTURE_${arch})
            set(ARCHITECTURE "${arch}" PARENT_SCOPE)
            set(ARCHITECTURE_${arch} 1 PARENT_SCOPE)
            add_definitions(-DARCHITECTURE_${arch}=1)
        endif()
    endif()
endfunction()

project(citra)

option(ENABLE_SDL2 "Enable the SDL2 frontend" ON)
option(CITRA_USE_BUNDLED_SDL2 "Download bundled SDL2 binaries" OFF)

option(ENABLE_QT "Enable the Qt frontend" ON)
option(CITRA_USE_BUNDLED_QT "Download bundled Qt binaries" OFF)

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git/hooks/pre-commit)
    message(STATUS "Copying pre-commit hook")
    file(COPY hooks/pre-commit
        DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/.git/hooks)
endif()

if (MSVC)
    detect_architecture("_M_AMD64" x86_64)
    detect_architecture("_M_IX86" x86)
    detect_architecture("_M_ARM" ARM)
else()
    detect_architecture("__x86_64__" x86_64)
    detect_architecture("__i386__" x86)
    detect_architecture("__arm__" ARM)
endif()
if (NOT DEFINED ARCHITECTURE)
    set(ARCHITECTURE "GENERIC")
    set(ARCHITECTURE_GENERIC 1)
    add_definitions(-DARCHITECTURE_GENERIC=1)
endif()
message(STATUS "Target architecture: ${ARCHITECTURE}")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-attributes")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
else()
    # Silence "deprecation" warnings
    add_definitions(/D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE /D_SCL_SECURE_NO_WARNINGS)
    # Avoid windows.h junk
    add_definitions(/DNOMINMAX)

    # set up output paths for executable binaries (.exe-files, and .dll-files on DLL-capable platforms)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE STRING "" FORCE)

    # Tweak optimization settings
    # As far as I can tell, there's no way to override the CMake defaults while leaving user
    # changes intact, so we'll just clobber everything and say sorry.
    message(STATUS "Cache compiler flags ignored, please edit CMakeLists.txt to change the flags.")

    # /W3 - Level 3 warnings
    # /MP - Multi-threaded compilation
    # /Zi - Output debugging information
    # /Zo - enahnced debug info for optimized builds
    set(CMAKE_C_FLAGS   "/W3 /MP /Zi /Zo" CACHE STRING "" FORCE)
    # /EHsc - C++-only exception handling semantics
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} /EHsc" CACHE STRING "" FORCE)

    # /MDd - Multi-threaded Debug Runtime DLL
    set(CMAKE_C_FLAGS_DEBUG   "/Od /MDd" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}" CACHE STRING "" FORCE)

    # /O2 - Optimization level 2
    # /GS- - No stack buffer overflow checks
    # /MD - Multi-threaded runtime DLL
    set(CMAKE_C_FLAGS_RELEASE   "/O2 /GS- /MD" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}" CACHE STRING "" FORCE)

    set(CMAKE_EXE_LINKER_FLAGS_DEBUG   "/DEBUG" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/DEBUG /INCREMENTAL:NO /OPT:REF,ICF" CACHE STRING "" FORCE)
endif()

# Set file offset size to 64 bits.
#
# On modern Unixes, this is typically already the case. The lone exception is
# glibc, which may default to 32 bits. glibc allows this to be configured
# by setting _FILE_OFFSET_BITS.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_definitions(-D_FILE_OFFSET_BITS=64)
endif()

add_definitions(-DSINGLETHREADED)
# CMake seems to only define _DEBUG on Windows
set_property(DIRECTORY APPEND PROPERTY
    COMPILE_DEFINITIONS $<$<CONFIG:Debug>:_DEBUG> $<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

find_package(PNG QUIET)
if (PNG_FOUND)
    add_definitions(-DHAVE_PNG)
else()
    message(STATUS "libpng not found. Some debugging features have been disabled.")
endif()

find_package(Boost 1.57.0 QUIET)
if (NOT Boost_FOUND)
    message(STATUS "Boost 1.57.0 or newer not found, falling back to externals")
    set(Boost_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/externals/boost")
endif()
include_directories(${Boost_INCLUDE_DIR})

# Include bundled CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/externals/cmake-modules")

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

# Prefer the -pthread flag on Linux.
set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if (ENABLE_SDL2)
    if (CITRA_USE_BUNDLED_SDL2)
        # Detect toolchain and platform
        if (MSVC14 AND ARCHITECTURE_x86_64)
            set(SDL2_VER "SDL2-2.0.4")
        else()
            message(FATAL_ERROR "No bundled SDL2 binaries for your toolchain. Disable CITRA_USE_BUNDLED_SDL2 and provide your own.")
        endif()

        if (DEFINED SDL2_VER)
            download_bundled_external("sdl2/" ${SDL2_VER} SDL2_PREFIX)
        endif()

        set(SDL2_FOUND YES)
        set(SDL2_INCLUDE_DIR "${SDL2_PREFIX}/include" CACHE PATH "Path to SDL2 headers")
        set(SDL2_LIBRARY "${SDL2_PREFIX}/lib/x64/SDL2.lib" CACHE PATH "Path to SDL2 library")
        set(SDL2_DLL_DIR "${SDL2_PREFIX}/lib/x64/" CACHE PATH "Path to SDL2.dll")
    else()
        find_package(SDL2 REQUIRED)
    endif()
else()
    set(SDL2_FOUND NO)
endif()

IF (APPLE)
    FIND_LIBRARY(COCOA_LIBRARY Cocoa)           # Umbrella framework for everything GUI-related
    set(PLATFORM_LIBRARIES ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
ELSEIF(MINGW)
    # PSAPI is the Process Status API
    set(PLATFORM_LIBRARIES winmm ws2_32 psapi)

    # WSAPoll functionality doesn't exist before WinNT 6.x (Vista and up)
    add_definitions(-D_WIN32_WINNT=0x0600)
ELSEIF(WIN32)
    set(PLATFORM_LIBRARIES winmm ws2_32)
ELSEIF(CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU|SunOS)$")
    set(PLATFORM_LIBRARIES rt)
ENDIF (APPLE)

# MINGW: GCC does not support codecvt, so use iconv instead
if (UNIX OR MINGW)
    find_library(ICONV_LIBRARY NAMES iconv)
    if (ICONV_LIBRARY)
        list(APPEND PLATFORM_LIBRARIES ${ICONV_LIBRARY})
    endif()
endif()

if (ENABLE_QT)
    if (CITRA_USE_BUNDLED_QT)
        if (MSVC14 AND ARCHITECTURE_x86_64)
            set(QT_VER qt-5.7-msvc2015_64)
        else()
            message(FATAL_ERROR "No bundled Qt binaries for your toolchain. Disable CITRA_USE_BUNDLED_QT and provide your own.")
        endif()

        if (DEFINED QT_VER)
            download_bundled_external("qt/" ${QT_VER} QT_PREFIX)
        endif()

        set(QT_PREFIX_HINT HINTS "${QT_PREFIX}")
    else()
        # Passing an empty HINTS seems to cause default system paths to get ignored in CMake 2.8 so
        # make sure to not pass anything if we don't have one.
        set(QT_PREFIX_HINT)
    endif()

    find_package(Qt5 REQUIRED COMPONENTS Widgets OpenGL ${QT_PREFIX_HINT})
    set(CITRA_QT_LIBS Qt5::Widgets Qt5::OpenGL)
endif()

# This function should be passed a list of all files in a target. It will automatically generate
# file groups following the directory hierarchy, so that the layout of the files in IDEs matches the
# one in the filesystem.
function(create_directory_groups)
    # Place any files that aren't in the source list in a separate group so that they don't get in
    # the way.
    source_group("Other Files" REGULAR_EXPRESSION ".")

    foreach(file_name ${ARGV})
        get_filename_component(dir_name "${file_name}" PATH)
        # Group names use '\' as a separator even though the entire rest of CMake uses '/'...
        string(REPLACE "/" "\\" group_name "${dir_name}")
        source_group("${group_name}" FILES "${file_name}")
    endforeach()
endfunction()

# generate git revision information
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REF_SPEC GIT_REV)
git_describe(GIT_DESC --always --long --dirty)
git_branch_name(GIT_BRANCH)

set(INI_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/externals/inih")
include_directories(${INI_PREFIX})
add_subdirectory(${INI_PREFIX})

option(DYNARMIC_TESTS OFF)
add_subdirectory(externals/dynarmic)

add_subdirectory(externals/glad)
include_directories(externals/microprofile)
include_directories(externals/nihstro/include)

if (MSVC)
    add_subdirectory(externals/getopt)
endif()

# process subdirectories
if(ENABLE_QT)
    include_directories(externals/qhexedit)
    add_subdirectory(externals/qhexedit)
endif()

add_subdirectory(externals/soundtouch)

enable_testing()

add_subdirectory(src)

# Install freedesktop.org metadata files, following those specifications:
# http://standards.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html
# http://standards.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html
# http://standards.freedesktop.org/shared-mime-info-spec/shared-mime-info-spec-latest.html
if(ENABLE_QT AND UNIX AND NOT APPLE)
    install(FILES "${CMAKE_SOURCE_DIR}/dist/citra.desktop"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/applications")
    install(FILES "${CMAKE_SOURCE_DIR}/dist/citra.svg"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/pixmaps")
    install(FILES "${CMAKE_SOURCE_DIR}/dist/citra.xml"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/share/mime/packages")
endif()
