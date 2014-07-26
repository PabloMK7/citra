set(SRCS    command_processor.cpp
            utils.cpp
            video_core.cpp
            renderer_opengl/renderer_opengl.cpp)

set(HEADERS command_processor.h
            math.h
            utils.h
            video_core.h
            renderer_base.h
            video_core.h
            renderer_opengl/renderer_opengl.h)

add_library(video_core STATIC ${SRCS} ${HEADERS})
