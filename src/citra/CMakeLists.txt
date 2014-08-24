set(SRCS    citra.cpp
            emu_window/emu_window_glfw.cpp)
set(HEADERS resource.h)

# NOTE: This is a workaround for CMake bug 0006976 (missing X11_xf86vmode_LIB variable)
if (NOT X11_xf86vmode_LIB)
    set(X11_xv86vmode_LIB Xxf86vm)
endif()

add_executable(citra ${SRCS} ${HEADERS})

if (APPLE)
    target_link_libraries(citra core common video_core iconv pthread ${COREFOUNDATION_LIBRARY} ${OPENGL_LIBRARIES} ${GLFW_LIBRARIES})
else()
    target_link_libraries(citra core common video_core pthread X11 Xxf86vm Xi Xcursor ${OPENGL_LIBRARIES} ${GLFW_LIBRARIES} rt ${X11_Xrandr_LIB} ${X11_xv86vmode_LIB} ${PNG_LIBRARIES})
endif()

#install(TARGETS citra RUNTIME DESTINATION ${bindir})
