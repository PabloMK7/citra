set(SRCS    video_core.cpp
            utils.cpp
            renderer_opengl/renderer_opengl.cpp)

set(HEADERS math.h
            utils.h
            video_core.h
            renderer_base.h
            renderer_opengl/renderer_opengl.h)

add_library(video_core STATIC ${SRCS} ${HEADERS})
