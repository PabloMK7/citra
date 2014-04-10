set(SRCS    src/video_core.cpp
            src/utils.cpp
            src/renderer_opengl/renderer_opengl.cpp)

add_library(video_core STATIC ${SRCS})
