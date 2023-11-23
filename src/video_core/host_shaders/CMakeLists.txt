# Copyright 2023 Citra Emulator Project
# Licensed under GPLv2 or any later version
# Refer to the license.txt file included.

set(SHADER_FILES
    format_reinterpreter/d24s8_to_rgba8.frag
    format_reinterpreter/rgba4_to_rgb5a1.frag
    format_reinterpreter/vulkan_d24s8_to_rgba8.comp
    texture_filtering/bicubic.frag
    texture_filtering/refine.frag
    texture_filtering/scale_force.frag
    texture_filtering/xbrz_freescale.frag
    texture_filtering/mmpx.frag
    texture_filtering/x_gradient.frag
    texture_filtering/y_gradient.frag
    full_screen_triangle.vert
    opengl_present.frag
    opengl_present.vert
    opengl_present_anaglyph.frag
    opengl_present_interlaced.frag
    vulkan_depth_to_buffer.comp
    vulkan_present.frag
    vulkan_present.vert
    vulkan_present_anaglyph.frag
    vulkan_present_interlaced.frag
    vulkan_blit_depth_stencil.frag
)

find_program(GLSLANG "glslang")
if ("${GLSLANG}" STREQUAL "GLSLANG-NOTFOUND")
    find_program(GLSLANG "glslangValidator")
    if ("${GLSLANG}" STREQUAL "GLSLANG-NOTFOUND")
        message(FATAL_ERROR "Required program `glslang` (or `glslangValidator`) not found.")
    endif()
endif()

set(MACROS "-Dgl_VertexID=gl_VertexIndex")
set(QUIET_FLAG "--quiet")

set(SHADER_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/include)
set(SHADER_DIR ${SHADER_INCLUDE}/video_core/host_shaders)
set(HOST_SHADERS_INCLUDE ${SHADER_INCLUDE} PARENT_SCOPE)

set(INPUT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/source_shader.h.in)
set(HEADER_GENERATOR ${CMAKE_CURRENT_SOURCE_DIR}/StringShaderHeader.cmake)

# Check if `--quiet` is available on host's glslang version
# glslang prints to STDERR iff an unrecognized flag is passed to it
execute_process(
    COMMAND
        ${GLSLANG} ${QUIET_FLAG}
    ERROR_VARIABLE
        GLSLANG_ERROR
    # STDOUT variable defined to silence unnecessary output during CMake configuration
    OUTPUT_VARIABLE
        GLSLANG_OUTPUT
)

if (NOT GLSLANG_ERROR STREQUAL "")
    message(WARNING "Refusing to use unavailable flag `${QUIET_FLAG}` on `${GLSLANG}`")
    set(QUIET_FLAG "")
endif()

foreach(FILENAME IN ITEMS ${SHADER_FILES})
    string(REPLACE "." "_" SHADER_NAME ${FILENAME})
    set(SOURCE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME})
    # Skip generating source headers on Vulkan exclusive files
    if (NOT ${FILENAME} MATCHES "vulkan.*")
        set(SOURCE_HEADER_FILE ${SHADER_DIR}/${SHADER_NAME}.h)
        add_custom_command(
            OUTPUT
                ${SOURCE_HEADER_FILE}
            COMMAND
                ${CMAKE_COMMAND} -P ${HEADER_GENERATOR} ${SOURCE_FILE} ${SOURCE_HEADER_FILE} ${INPUT_FILE}
            MAIN_DEPENDENCY
                ${SOURCE_FILE}
            DEPENDS
                ${INPUT_FILE}
                # HEADER_GENERATOR should be included here but msbuild seems to assume it's always modified
        )
        set(SHADER_HEADERS ${SHADER_HEADERS} ${SOURCE_HEADER_FILE})
    endif()
    # Skip compiling to SPIR-V OpenGL exclusive files
    if (NOT ${FILENAME} MATCHES "opengl.*")
        get_filename_component(FILE_NAME ${SHADER_NAME} NAME)
        string(TOUPPER ${FILE_NAME}_SPV SPIRV_VARIABLE_NAME)
        set(SPIRV_HEADER_FILE ${SHADER_DIR}/${SHADER_NAME}_spv.h)
        add_custom_command(
            OUTPUT
                ${SPIRV_HEADER_FILE}
            COMMAND
                ${GLSLANG} --target-env vulkan1.1 --glsl-version 450 ${QUIET_FLAG} ${MACROS} --variable-name ${SPIRV_VARIABLE_NAME} -o ${SPIRV_HEADER_FILE} ${SOURCE_FILE}
            MAIN_DEPENDENCY
                ${SOURCE_FILE}
        )
        set(SHADER_HEADERS ${SHADER_HEADERS} ${SPIRV_HEADER_FILE})
    endif()
endforeach()

set(SHADER_SOURCES ${SHADER_FILES})
list(APPEND SHADER_SOURCES ${GLSL_INCLUDES})

add_custom_target(host_shaders
    DEPENDS
        ${SHADER_HEADERS}
    SOURCES
        ${SHADER_SOURCES}
)
