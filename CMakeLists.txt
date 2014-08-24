cmake_minimum_required(VERSION 2.8.7)

project(citra)

SET(CXX_COMPILE_FLAGS "-std=c++11")

# silence some spam
add_definitions(-Wno-attributes)
add_definitions(-DSINGLETHREADED)
add_definitions(${CXX_COMPILE_FLAGS})

find_package(PNG)
if (PNG_FOUND)
    add_definitions(-DHAVE_PNG)
endif ()

# dependency checking
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/externals/cmake-modules/")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/CMakeTests)
include(FindX11 REQUIRED)
find_package(PkgConfig REQUIRED)
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

# workaround for GLFW linking on OSX
link_directories(${GLFW_LIBRARY_DIRS})

option(DISABLE_QT "Disable Qt GUI" OFF)
option(USE_QT5 "Use Qt5 when available" ON)
if (NOT DISABLE_QT)
    if(USE_QT5)
        find_package(Qt5Gui)
        find_package(Qt5Widgets)
        find_package(Qt5OpenGL)
        if(NOT Qt5Gui_FOUND OR NOT Qt5Widgets_FOUND OR NOT Qt5OpenGL_FOUND)
            message("Qt5 libraries not found! Using Qt4 instead.")
            set(USE_QT5 OFF)
        endif()
    endif()
    if(NOT USE_QT5)
        include(FindQt4)
        find_package(Qt4 COMPONENTS QtCore QtGui QtOpenGL)

        if(QT4_FOUND AND QT_QTCORE_FOUND AND QT_QTGUI_FOUND AND QT_QTOPENGL_FOUND)
            include(${QT_USE_FILE})
            include_directories(${QT_INCLUDES})
        else()
            message("Qt4 libraries not found! Disabling Qt GUI")
            set(DISABLE_QT ON)
        endif()
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
if(NOT DISABLE_QT)
    include_directories(externals/qhexedit)
    add_subdirectory(externals/qhexedit)
endif()
add_subdirectory(src)
