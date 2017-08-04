set(SRCS
            command_processor.cpp
            debug_utils/debug_utils.cpp
            geometry_pipeline.cpp
            pica.cpp
            primitive_assembly.cpp
            regs.cpp
            renderer_base.cpp
            renderer_opengl/gl_rasterizer.cpp
            renderer_opengl/gl_rasterizer_cache.cpp
            renderer_opengl/gl_shader_gen.cpp
            renderer_opengl/gl_shader_util.cpp
            renderer_opengl/gl_state.cpp
            renderer_opengl/renderer_opengl.cpp
            shader/shader.cpp
            shader/shader_interpreter.cpp
            swrasterizer/clipper.cpp
            swrasterizer/framebuffer.cpp
            swrasterizer/lighting.cpp
            swrasterizer/proctex.cpp
            swrasterizer/rasterizer.cpp
            swrasterizer/swrasterizer.cpp
            swrasterizer/texturing.cpp
            texture/etc1.cpp
            texture/texture_decode.cpp
            vertex_loader.cpp
            video_core.cpp
            )

set(HEADERS
            command_processor.h
            debug_utils/debug_utils.h
            geometry_pipeline.h
            gpu_debugger.h
            pica.h
            pica_state.h
            pica_types.h
            primitive_assembly.h
            rasterizer_interface.h
            regs.h
            regs_framebuffer.h
            regs_lighting.h
            regs_pipeline.h
            regs_rasterizer.h
            regs_shader.h
            regs_texturing.h
            renderer_base.h
            renderer_opengl/gl_rasterizer.h
            renderer_opengl/gl_rasterizer_cache.h
            renderer_opengl/gl_resource_manager.h
            renderer_opengl/gl_shader_gen.h
            renderer_opengl/gl_shader_util.h
            renderer_opengl/gl_state.h
            renderer_opengl/pica_to_gl.h
            renderer_opengl/renderer_opengl.h
            shader/debug_data.h
            shader/shader.h
            shader/shader_interpreter.h
            swrasterizer/clipper.h
            swrasterizer/framebuffer.h
            swrasterizer/lighting.h
            swrasterizer/proctex.h
            swrasterizer/rasterizer.h
            swrasterizer/swrasterizer.h
            swrasterizer/texturing.h
            texture/etc1.h
            texture/texture_decode.h
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
target_link_libraries(video_core PUBLIC common core)
target_link_libraries(video_core PRIVATE glad nihstro-headers)

if (ARCHITECTURE_x86_64)
    target_link_libraries(video_core PRIVATE xbyak)
endif()

if (PNG_FOUND)
    target_link_libraries(video_core PRIVATE PNG::PNG)
    target_compile_definitions(video_core PRIVATE HAVE_PNG)
endif()
