set(SRCS    citra.cpp
            emuwindow/emuwindow_glfw.cpp)

# NOTE: This is a workaround for CMake bug 0006976 (missing X11_xf86vmode_LIB variable)
if (NOT X11_xf86vmode_LIB)
    set(X11_xv86vmode_LIB Xxf86vm)
endif()

add_executable(citra ${SRCS})
target_link_libraries(citra core common ${OPENGL_LIBRARIES} ${GLFW_LIBRARIES} GLEW rt ${X11_Xrandr_LIB} ${X11_xv86vmode_LIB})

#install(TARGETS citra RUNTIME DESTINATION ${bindir})
