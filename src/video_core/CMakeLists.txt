set(SRCS
            src/bp_mem.cpp
            src/cp_mem.cpp
            src/xf_mem.cpp
            src/fifo.cpp
            src/fifo_player.cpp
            src/vertex_loader.cpp
            src/vertex_manager.cpp
            src/video_core.cpp
            src/shader_manager.cpp
            src/texture_decoder.cpp
            src/texture_manager.cpp
            src/utils.cpp
            src/renderer_gl3/renderer_gl3.cpp
            src/renderer_gl3/shader_interface.cpp
            src/renderer_gl3/texture_interface.cpp
            src/renderer_gl3/uniform_manager.cpp)

add_library(video_core STATIC ${SRCS})
