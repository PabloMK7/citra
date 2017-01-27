set(SRCS
            renderer_opengl/gl_rasterizer.cpp
            renderer_opengl/gl_rasterizer_cache.cpp
            renderer_opengl/gl_shader_gen.cpp
            renderer_opengl/gl_shader_util.cpp
            renderer_opengl/gl_state.cpp
            renderer_opengl/renderer_opengl.cpp
            debug_utils/debug_utils.cpp
            clipper.cpp
            command_processor.cpp
            pica.cpp
            primitive_assembly.cpp
            rasterizer.cpp
            renderer_base.cpp
            shader/shader.cpp
            shader/shader_interpreter.cpp
            swrasterizer.cpp
            vertex_loader.cpp
            video_core.cpp
            )

set(HEADERS
            debug_utils/debug_utils.h
            renderer_opengl/gl_rasterizer.h
            renderer_opengl/gl_rasterizer_cache.h
            renderer_opengl/gl_resource_manager.h
            renderer_opengl/gl_shader_gen.h
            renderer_opengl/gl_shader_util.h
            renderer_opengl/gl_state.h
            renderer_opengl/pica_to_gl.h
            renderer_opengl/renderer_opengl.h
            clipper.h
            command_processor.h
            gpu_debugger.h
            pica.h
            pica_state.h
            pica_types.h
            primitive_assembly.h
            rasterizer.h
            rasterizer_interface.h
            renderer_base.h
            shader/debug_data.h
            shader/shader.h
            shader/shader_interpreter.h
            swrasterizer.h
            utils.h
            vertex_loader.h
            video_core.h
            )

if(ARCHITECTURE_x86_64)
    set(SRCS ${SRCS}
            shader/shader_jit_x64.cpp
            shader/shader_jit_x64_compiler.cpp)

    set(HEADERS ${HEADERS}
            shader/shader_jit_x64.h
            shader/shader_jit_x64_compiler.h)
endif()

create_directory_groups(${SRCS} ${HEADERS})

add_library(video_core STATIC ${SRCS} ${HEADERS})
target_link_libraries(video_core glad)
if (ARCHITECTURE_x86_64)
    target_link_libraries(video_core xbyak)
endif()

if (PNG_FOUND)
    target_link_libraries(video_core ${PNG_LIBRARIES})
    include_directories(${PNG_INCLUDE_DIRS})
    add_definitions(${PNG_DEFINITIONS})
endif()
