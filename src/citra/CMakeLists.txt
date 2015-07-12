set(SRCS
            emu_window/emu_window_glfw.cpp
            citra.cpp
            config.cpp
            citra.rc
            )
set(HEADERS
            emu_window/emu_window_glfw.h
            config.h
            default_ini.h
            resource.h
            )

create_directory_groups(${SRCS} ${HEADERS})

add_executable(citra ${SRCS} ${HEADERS})
target_link_libraries(citra core common video_core)
target_link_libraries(citra ${GLFW_LIBRARIES} ${OPENGL_gl_LIBRARY} inih)
if (MSVC)
    target_link_libraries(citra getopt)
endif()
target_link_libraries(citra ${PLATFORM_LIBRARIES})

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux|FreeBSD|OpenBSD|NetBSD")
    install(TARGETS citra RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()