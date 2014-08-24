set(SRCS    clipper.cpp
            command_processor.cpp
            primitive_assembly.cpp
            rasterizer.cpp
            utils.cpp
            vertex_shader.cpp
            video_core.cpp
            renderer_opengl/generated/gl_3_2_core.c
            renderer_opengl/renderer_opengl.cpp
            renderer_opengl/gl_shader_util.cpp
            debug_utils/debug_utils.cpp)

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
            renderer_opengl/generated/gl_3_2_core.h
            renderer_opengl/renderer_opengl.h
            renderer_opengl/gl_shader_util.h
            renderer_opengl/gl_shaders.h
            debug_utils/debug_utils.h)

add_library(video_core STATIC ${SRCS} ${HEADERS})
