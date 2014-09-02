# Enable modules to include each other's files
include_directories(.)

add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(video_core)
if (ENABLE_GLFW)
    add_subdirectory(citra)
endif()
if (ENABLE_QT)
    add_subdirectory(citra_qt)
endif()
