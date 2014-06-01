cmake_minimum_required(VERSION 2.6)

project(citra)

SET(CXX_COMPILE_FLAGS "-std=c++11")

# silence some spam
add_definitions(-Wno-attributes)
add_definitions(-DSINGLETHREADED)
add_definitions(${CXX_COMPILE_FLAGS})

# dependency checking
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cmake-modules/")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/CMakeTests)
include(FindX11 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

# corefoundation is required only on OSX
IF (APPLE)
    FIND_LIBRARY(COREFOUNDATION_LIBRARY CoreFoundation)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
	SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
ENDIF (APPLE)

#external includes
include_directories(${GLFW_INCLUDE_DIRS})
include_directories(${OPENGL_INCLUDE_DIR})
include_directories(${GLEW_INCLUDE_PATH})

# workaround for GLFW linking on OSX
link_directories(${GLFW_LIBRARY_DIRS})

option(DISABLE_QT4 "Disable Qt4 GUI" OFF)
if(NOT DISABLE_QT4)
    include(FindQt4)
    find_package(Qt4 COMPONENTS QtCore QtGui QtOpenGL)

    if(QT4_FOUND AND QT_QTCORE_FOUND AND QT_QTGUI_FOUND AND QT_QTOPENGL_FOUND)
        include(${QT_USE_FILE})
        include_directories(${QT_INCLUDES})
        include_directories(externals/qhexedit)
    else()
        message("Qt4 libraries not found! Disabling Qt4 GUI")
    endif()
endif()

# generate git revision information
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REF_SPEC GIT_REV)
git_describe(GIT_DESC --always --long --dirty)
git_branch_name(GIT_BRANCH)
    
# internal includes
include_directories(src)

# process subdirectories
if(QT4_FOUND AND QT_QTCORE_FOUND AND QT_QTGUI_FOUND AND QT_QTOPENGL_FOUND AND NOT DISABLE_QT4)
    add_subdirectory(externals/qhexedit)
endif()
add_subdirectory(src)
