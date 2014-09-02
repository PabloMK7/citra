set(SRCS
            emu_window/emu_window_glfw.cpp
            citra.cpp
            )
set(HEADERS
            emu_window/emu_window_glfw.h
            resource.h
            )

create_directory_groups(${SRCS} ${HEADERS})

# NOTE: This is a workaround for CMake bug 0006976 (missing X11_xf86vmode_LIB variable)
if (NOT X11_xf86vmode_LIB)
    set(X11_xv86vmode_LIB Xxf86vm)
endif()

add_executable(citra ${SRCS} ${HEADERS})
target_link_libraries(citra core common video_core)
target_link_libraries(citra ${OPENGL_gl_LIBRARY} ${GLFW_LIBRARIES})

if (APPLE)
    target_link_libraries(citra iconv pthread ${COREFOUNDATION_LIBRARY})
elseif (WIN32)
    target_link_libraries(citra winmm)
else() # Unix
    target_link_libraries(citra pthread rt)
    target_link_libraries(citra ${X11_X11_LIB} ${X11_Xi_LIB} ${X11_Xcursor_LIB} ${X11_Xrandr_LIB} ${X11_xv86vmode_LIB})
endif()

#install(TARGETS citra RUNTIME DESTINATION ${bindir})
