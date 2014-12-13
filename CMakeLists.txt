# CMake 2.8.11 required for Qt5 settings to be applied automatically on
# dependent libraries.
cmake_minimum_required(VERSION 2.8.11)

project(citra)

if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wno-attributes")
    add_definitions(-pthread)
else()
    # Silence deprecation warnings
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
    # set up output paths for executable binaries (.exe-files, and .dll-files on DLL-capable platforms)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()
add_definitions(-DSINGLETHREADED)

find_package(PNG)
if (PNG_FOUND)
    add_definitions(-DHAVE_PNG)
endif ()

find_package(Boost)
if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
else()
    message(STATUS "Boost not found, falling back to externals")
    include_directories(externals/boost)
endif()

# Include bundled CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/externals/cmake-modules")

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

option(ENABLE_GLFW "Enable the GLFW frontend" ON)
if (ENABLE_GLFW)
    if (WIN32)
        # Detect toolchain and platform
        if (MSVC)
            if (CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(TMP_ARCH "x64")
            else()
                set(TMP_ARCH "Win32")
            endif()

            if (MSVC11) # Visual C++ 2012
                set(TMP_TOOLSET "v110")
            elseif (MSVC12) # Visual C++ 2013
                set(TMP_TOOLSET "v120")
            else()
                set(TMP_TOOLSET "UNSUPPORTED")
                message(SEND_ERROR "We don't supply GLFW binaries for your version of MSVC, you might have to provide them yourself.")
            endif()

            set(TMP_TOOLSET "msvc_${TMP_TOOLSET}-${TMP_ARCH}")
        else()
            # Assume mingw
            if (CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(TMP_ARCH "x86_64")
            else()
                set(TMP_ARCH "i686")
            endif()

            set(TMP_TOOLSET "mingw-${TMP_ARCH}")
        endif()

        set(GLFW_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/externals/glfw-3.0.4.bin")
        set(GLFW_INCLUDE_DIRS "${GLFW_PREFIX}/include" CACHE PATH "Path to GLFW3 headers")
        set(GLFW_LIBRARY_DIRS "${GLFW_PREFIX}/lib-${TMP_TOOLSET}" CACHE PATH "Path to GLFW3 libraries")

        # Clean up after ourselves
        unset(TMP_TOOLSET)
        unset(TMP_ARCH)

        set(GLFW_LIBRARIES glfw3)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_search_module(GLFW REQUIRED glfw3)
    endif()

    include_directories(${GLFW_INCLUDE_DIRS})
    link_directories(${GLFW_LIBRARY_DIRS})
endif()

IF (APPLE)
    # CoreFoundation is required only on OSX
    FIND_LIBRARY(COREFOUNDATION_LIBRARY CoreFoundation)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
ENDIF (APPLE)

option(ENABLE_QT "Enable the Qt frontend" ON)
option(CITRA_FORCE_QT4 "Use Qt4 even if Qt5 is available." OFF)
if (ENABLE_QT)
    # Set CMAKE_PREFIX_PATH if QTDIR is defined in the environment This allows CMake to
    # automatically find the Qt packages on Windows
    if (DEFINED ENV{QTDIR})
        list(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
    endif()

    if (NOT CITRA_FORCE_QT4)
        find_package(Qt5 COMPONENTS Widgets OpenGL)
        set(CITRA_QT_LIBS Qt5::Widgets Qt5::OpenGL)
    endif()

    if (CITRA_FORCE_QT4 OR NOT Qt5_FOUND)
        # Try to fallback to Qt4
        find_package(Qt4 REQUIRED COMPONENTS QtGui QtOpenGL)
        set(CITRA_QT_LIBS Qt4::QtGui Qt4::QtOpenGL)
    endif()
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

# process subdirectories
if(ENABLE_QT)
    include_directories(externals/qhexedit)
    add_subdirectory(externals/qhexedit)
endif()
add_subdirectory(src)
