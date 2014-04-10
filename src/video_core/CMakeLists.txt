set(SRCS    video_core.cpp
            utils.cpp
            renderer_opengl/renderer_opengl.cpp)

add_library(video_core STATIC ${SRCS})
