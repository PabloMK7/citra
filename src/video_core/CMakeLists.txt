set(SRCS
            renderer_opengl/generated/gl_3_2_core.c
            renderer_opengl/gl_rasterizer.cpp
            renderer_opengl/gl_rasterizer_cache.cpp
            renderer_opengl/gl_shader_util.cpp
            renderer_opengl/gl_state.cpp
            renderer_opengl/renderer_opengl.cpp
            debug_utils/debug_utils.cpp
            clipper.cpp
            command_processor.cpp
            pica.cpp
            primitive_assembly.cpp
            rasterizer.cpp
            utils.cpp
            vertex_shader.cpp
            video_core.cpp
            )

set(HEADERS
            debug_utils/debug_utils.h
            renderer_opengl/generated/gl_3_2_core.h
            renderer_opengl/gl_rasterizer.h
            renderer_opengl/gl_rasterizer_cache.h
            renderer_opengl/gl_resource_manager.h
            renderer_opengl/gl_shader_util.h
            renderer_opengl/gl_shaders.h
            renderer_opengl/gl_state.h
            renderer_opengl/pica_to_gl.h
            renderer_opengl/renderer_opengl.h
            clipper.h
            command_processor.h
            gpu_debugger.h
            hwrasterizer_base.h
            pica.h
            primitive_assembly.h
            rasterizer.h
            renderer_base.h
            utils.h
            vertex_shader.h
            video_core.h
            )

create_directory_groups(${SRCS} ${HEADERS})

add_library(video_core STATIC ${SRCS} ${HEADERS})

if (PNG_FOUND)
    target_link_libraries(video_core ${PNG_LIBRARIES})
    include_directories(${PNG_INCLUDE_DIRS})
    add_definitions(${PNG_DEFINITIONS})
endif()
