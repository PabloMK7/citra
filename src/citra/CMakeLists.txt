set(SRCS    citra.cpp
            emu_window/emu_window_glfw.cpp)
set(HEADS   citra.h
            resource.h)

add_executable(citra ${SRCS} ${HEADS})

if (APPLE)
    target_link_libraries(citra core common video_core iconv pthread ${COREFOUNDATION_LIBRARY} ${OPENGL_LIBRARIES} ${GLEW_LIBRARY} ${GLFW_LIBRARIES})
else()
    target_link_libraries(citra core common video_core pthread ${OPENGL_LIBRARIES} ${GLEW_LIBRARY} ${GLFW_LIBRARIES})
endif()

#install(TARGETS citra RUNTIME DESTINATION ${bindir})
