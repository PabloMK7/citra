# CMake 3.6 required for FindBoost to define IMPORTED libs properly on unknown Boost versions
cmake_minimum_required(VERSION 3.6)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cmake-modules")
include(DownloadExternals)

project(citra)

option(ENABLE_SDL2 "Enable the SDL2 frontend" ON)
option(CITRA_USE_BUNDLED_SDL2 "Download bundled SDL2 binaries" OFF)

option(ENABLE_QT "Enable the Qt frontend" ON)
option(CITRA_USE_BUNDLED_QT "Download bundled Qt binaries" OFF)

option(ENABLE_WEB_SERVICE "Enable web services (telemetry, etc.)" ON)
option(CITRA_USE_BUNDLED_CURL "FOR MINGW ONLY: Download curl configured against winssl instead of openssl" OFF)
if (ENABLE_WEB_SERVICE AND CITRA_USE_BUNDLED_CURL AND WINDOWS AND MSVC)
    message("Turning off use bundled curl as msvc can compile curl on cpr")
    SET(CITRA_USE_BUNDLED_CURL OFF CACHE BOOL "" FORCE)
endif()
if (ENABLE_WEB_SERVICE AND NOT CITRA_USE_BUNDLED_CURL AND MINGW)
    message(AUTHOR_WARNING "Turning on CITRA_USE_BUNDLED_CURL. Override it only if you know what you are doing.")
    SET(CITRA_USE_BUNDLED_CURL ON CACHE BOOL "" FORCE)
endif()

if(NOT EXISTS ${CMAKE_SOURCE_DIR}/.git/hooks/pre-commit)
    message(STATUS "Copying pre-commit hook")
    file(COPY hooks/pre-commit
        DESTINATION ${CMAKE_SOURCE_DIR}/.git/hooks)
endif()

# Sanity check : Check that all submodules are present
# =======================================================================

function(check_submodules_present)
    file(READ "${CMAKE_SOURCE_DIR}/.gitmodules" gitmodules)
    string(REGEX MATCHALL "path *= *[^ \t\r\n]*" gitmodules ${gitmodules})
    foreach(module ${gitmodules})
        string(REGEX REPLACE "path *= *" "" module ${module})
        if (NOT EXISTS "${CMAKE_SOURCE_DIR}/${module}/.git")
            message(SEND_ERROR "Git submodule ${module} not found."
                    "Please run: git submodule update --init --recursive")
        endif()
    endforeach()
endfunction()
check_submodules_present()

# Detect current compilation architecture and create standard definitions
# =======================================================================

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


# Configure compilation flags
# ===========================

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-attributes")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

    if (MINGW)
        add_definitions(-DMINGW_HAS_SECURE_API)

        if (MINGW_STATIC_BUILD)
            add_definitions(-DQT_STATICPLUGIN)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
        endif()
    endif()
else()
    # Silence "deprecation" warnings
    add_definitions(/D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE /D_SCL_SECURE_NO_WARNINGS)
    # Avoid windows.h junk
    add_definitions(/DNOMINMAX)
    # Avoid windows.h from including some usually unused libs like winsocks.h, since this might cause some redefinition errors.
    add_definitions(/DWIN32_LEAN_AND_MEAN)

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
    # /Zo - enhanced debug info for optimized builds
    # /permissive- - enables stricter C++ standards conformance checks
    set(CMAKE_C_FLAGS   "/W3 /MP /Zi /Zo /permissive-" CACHE STRING "" FORCE)
    # /EHsc - C++-only exception handling semantics
    # /Zc:throwingNew - let codegen assume `operator new` will never return null
    # /Zc:inline - let codegen omit inline functions in object files
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} /EHsc /Zc:throwingNew,inline" CACHE STRING "" FORCE)

    # /MDd - Multi-threaded Debug Runtime DLL
    set(CMAKE_C_FLAGS_DEBUG   "/Od /MDd" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}" CACHE STRING "" FORCE)

    # /O2 - Optimization level 2
    # /GS- - No stack buffer overflow checks
    # /MD - Multi-threaded runtime DLL
    set(CMAKE_C_FLAGS_RELEASE   "/O2 /GS- /MD" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}" CACHE STRING "" FORCE)

    set(CMAKE_EXE_LINKER_FLAGS_DEBUG   "/DEBUG /MANIFEST:NO" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/DEBUG /MANIFEST:NO /INCREMENTAL:NO /OPT:REF,ICF" CACHE STRING "" FORCE)
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


# System imported libraries
# ======================

find_package(PNG QUIET)
if (NOT PNG_FOUND)
    message(STATUS "libpng not found. Some debugging features have been disabled.")
endif()

find_package(Boost 1.63.0 QUIET)
if (NOT Boost_FOUND)
    message(STATUS "Boost 1.63.0 or newer not found, falling back to externals")

    set(BOOST_ROOT "${CMAKE_SOURCE_DIR}/externals/boost")
    set(Boost_NO_SYSTEM_PATHS OFF)
    find_package(Boost QUIET REQUIRED)
endif()

# Prefer the -pthread flag on Linux.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if (ENABLE_SDL2)
    if (CITRA_USE_BUNDLED_SDL2)
        # Detect toolchain and platform
        if (MSVC14 AND ARCHITECTURE_x86_64)
            set(SDL2_VER "SDL2-2.0.5")
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

    if (SDL2_FOUND)
        # TODO(yuriks): Make FindSDL2.cmake export an IMPORTED library instead
        add_library(SDL2 INTERFACE)
        target_link_libraries(SDL2 INTERFACE "${SDL2_LIBRARY}")
        target_include_directories(SDL2 INTERFACE "${SDL2_INCLUDE_DIR}")
    endif()
else()
    set(SDL2_FOUND NO)
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
endif()

if (ENABLE_WEB_SERVICE)
    add_definitions(-DENABLE_WEB_SERVICE)
endif()

# Platform-specific library requirements
# ======================================

IF (APPLE)
    FIND_LIBRARY(COCOA_LIBRARY Cocoa)           # Umbrella framework for everything GUI-related
    set(PLATFORM_LIBRARIES ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})

    if (CMAKE_CXX_COMPILER_ID STREQUAL Clang)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
    endif()
ELSEIF (WIN32)
    # WSAPoll and SHGetKnownFolderPath (AppData/Roaming) didn't exist before WinNT 6.x (Vista)
    add_definitions(-D_WIN32_WINNT=0x0600 -DWINVER=0x0600)
    set(PLATFORM_LIBRARIES winmm ws2_32)
    IF (MINGW)
        # PSAPI is the Process Status API
        set(PLATFORM_LIBRARIES ${PLATFORM_LIBRARIES} psapi imm32 version)
    ENDIF (MINGW)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU|SunOS)$")
    set(PLATFORM_LIBRARIES rt)
ENDIF (APPLE)

# MINGW: GCC does not support codecvt, so use iconv instead
if (UNIX OR MINGW)
    find_library(ICONV_LIBRARY NAMES iconv)
    if (ICONV_LIBRARY)
        list(APPEND PLATFORM_LIBRARIES ${ICONV_LIBRARY})
    endif()
endif()


# Include source code
# ===================

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

# Gets a UTC timstamp and sets the provided variable to it
function(get_timestamp _var)
    string(TIMESTAMP timestamp UTC)
    set(${_var} "${timestamp}" PARENT_SCOPE)
endfunction()

# generate git/build information
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REF_SPEC GIT_REV)
git_describe(GIT_DESC --always --long --dirty)
git_branch_name(GIT_BRANCH)
get_timestamp(BUILD_DATE)

enable_testing()
add_subdirectory(externals)
add_subdirectory(src)


# Installation instructions
# =========================

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

if(UNIX)
    if(ENABLE_SDL2)
        install(FILES "${CMAKE_SOURCE_DIR}/dist/citra.6"
                DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man6")
    endif()

    if (ENABLE_QT)
        install(FILES "${CMAKE_SOURCE_DIR}/dist/citra-qt.6"
                DESTINATION "${CMAKE_INSTALL_PREFIX}/share/man/man6")
    endif()
endif()
