set(SRCS    clipper.cpp
            command_processor.cpp
            primitive_assembly.cpp
            rasterizer.cpp
            utils.cpp
            vertex_shader.cpp
            video_core.cpp
            renderer_opengl/renderer_opengl.cpp)

set(HEADERS clipper.h
            command_processor.h
            math.h
            primitive_assembly.h
            rasterizer.h
            utils.h
            video_core.h
            renderer_base.h
            vertex_shader.h
            video_core.h
            renderer_opengl/renderer_opengl.h)

add_library(video_core STATIC ${SRCS} ${HEADERS})
