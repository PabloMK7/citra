set(SRCS    video_core.cpp
            utils.cpp
            renderer_opengl/renderer_opengl.cpp)

set(HEADERS video_core.h
            utils.h
            renderer_base.h
            renderer_opengl/renderer_opengl.h)

add_library(video_core STATIC ${SRCS} ${HEADERS})
