# Add a custom command to generate a new shader_cache_version hash when any of the following files change
# NOTE: This is an approximation of what files affect shader generation, its possible something else
# could affect the result, but much more unlikely than the following files. Keeping a list of files
# like this allows for much better caching since it doesn't force the user to recompile binary shaders every update
set(VIDEO_CORE "${CMAKE_SOURCE_DIR}/src/video_core")
if (DEFINED ENV{CI})
  if (DEFINED ENV{TRAVIS})
    set(BUILD_REPOSITORY $ENV{TRAVIS_REPO_SLUG})
    set(BUILD_TAG $ENV{TRAVIS_TAG})
  elseif(DEFINED ENV{APPVEYOR})
    set(BUILD_REPOSITORY $ENV{APPVEYOR_REPO_NAME})
    set(BUILD_TAG $ENV{APPVEYOR_REPO_TAG_NAME})
  endif()
endif()

# Pass the path to git to the GenerateSCMRev.cmake as well
find_package(Git QUIET)

add_custom_command(OUTPUT scm_rev.cpp
    COMMAND ${CMAKE_COMMAND}
      -DSRC_DIR="${CMAKE_SOURCE_DIR}"
      -DBUILD_REPOSITORY="${BUILD_REPOSITORY}"
      -DBUILD_TAG="${BUILD_TAG}"
      -DGIT_EXECUTABLE="${GIT_EXECUTABLE}"
      -P "${CMAKE_SOURCE_DIR}/CMakeModules/GenerateSCMRev.cmake"
    DEPENDS
      # WARNING! It was too much work to try and make a common location for this list,
      # so if you need to change it, please update CMakeModules/GenerateSCMRev.cmake as well
      "${VIDEO_CORE}/renderer_opengl/gl_shader_decompiler.cpp"
      "${VIDEO_CORE}/renderer_opengl/gl_shader_decompiler.h"
      "${VIDEO_CORE}/renderer_opengl/gl_shader_disk_cache.cpp"
      "${VIDEO_CORE}/renderer_opengl/gl_shader_disk_cache.h"
      "${VIDEO_CORE}/renderer_opengl/gl_shader_gen.cpp"
      "${VIDEO_CORE}/renderer_opengl/gl_shader_gen.h"
      "${VIDEO_CORE}/shader/shader.cpp"
      "${VIDEO_CORE}/shader/shader.h"
      "${VIDEO_CORE}/pica.cpp"
      "${VIDEO_CORE}/pica.h"
      "${VIDEO_CORE}/regs_framebuffer.h"
      "${VIDEO_CORE}/regs_lighting.h"
      "${VIDEO_CORE}/regs_pipeline.h"
      "${VIDEO_CORE}/regs_rasterizer.h"
      "${VIDEO_CORE}/regs_shader.h"
      "${VIDEO_CORE}/regs_texturing.h"
      "${VIDEO_CORE}/regs.cpp"
      "${VIDEO_CORE}/regs.h"
      # and also check that the scm_rev files haven't changed
      "${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp.in"
      "${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.h"
      # technically we should regenerate if the git version changed, but its not worth the effort imo
      "${CMAKE_SOURCE_DIR}/CMakeModules/GenerateSCMRev.cmake"
)

add_library(common STATIC
    alignment.h
    announce_multiplayer_room.h
    archives.h
    assert.h
    detached_tasks.cpp
    detached_tasks.h
    bit_field.h
    bit_set.h
    cityhash.cpp
    cityhash.h
    color.h
    common_funcs.h
    common_paths.h
    common_types.h
    construct.h
    file_util.cpp
    file_util.h
    hash.h
    linear_disk_cache.h
    logging/backend.cpp
    logging/backend.h
    logging/filter.cpp
    logging/filter.h
    logging/log.h
    logging/text_formatter.cpp
    logging/text_formatter.h
    math_util.h
    memory_ref.h
    memory_ref.cpp
    microprofile.cpp
    microprofile.h
    microprofileui.h
    misc.cpp
    param_package.cpp
    param_package.h
    quaternion.h
    ring_buffer.h
    scm_rev.cpp
    scm_rev.h
    scope_exit.h
    serialization/atomic.h
    serialization/boost_discrete_interval.hpp
    serialization/boost_flat_set.h
    serialization/boost_small_vector.hpp
    serialization/boost_vector.hpp
    string_util.cpp
    string_util.h
    swap.h
    telemetry.cpp
    telemetry.h
    texture.cpp
    texture.h
    thread.cpp
    thread.h
    thread_queue_list.h
    threadsafe_queue.h
    timer.cpp
    timer.h
    vector_math.h
    web_result.h
    zstd_compression.cpp
    zstd_compression.h
)

if(ARCHITECTURE_x86_64)
    target_sources(common
        PRIVATE
            x64/cpu_detect.cpp

            x64/cpu_detect.h
            x64/xbyak_abi.h
            x64/xbyak_util.h
    )
endif()

create_target_directory_groups(common)

target_link_libraries(common PUBLIC fmt microprofile Boost::boost Boost::serialization)
target_link_libraries(common PRIVATE libzstd_static)
if (ARCHITECTURE_x86_64)
    target_link_libraries(common PRIVATE xbyak)
endif()
