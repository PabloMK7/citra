set(SRCS
            emu_window/emu_window_glfw.cpp
            citra.cpp
            config.cpp
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

if (UNIX)
    target_link_libraries(citra -pthread)
endif()

if (APPLE)
    target_link_libraries(citra iconv ${COREFOUNDATION_LIBRARY})
elseif (WIN32)
    target_link_libraries(citra winmm)
    if (MINGW) # GCC does not support codecvt, so use iconv instead
        target_link_libraries(citra iconv)
    endif()    
else() # Unix
    target_link_libraries(citra rt)
endif()

#install(TARGETS citra RUNTIME DESTINATION ${bindir})
