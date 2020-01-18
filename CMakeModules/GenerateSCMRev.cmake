# Gets a UTC timstamp and sets the provided variable to it
function(get_timestamp _var)
    string(TIMESTAMP timestamp UTC)
    set(${_var} "${timestamp}" PARENT_SCOPE)
endfunction()

list(APPEND CMAKE_MODULE_PATH "${SRC_DIR}/externals/cmake-modules")

# Find the package here with the known path so that the GetGit commands can find it as well
find_package(Git QUIET PATHS "${GIT_EXECUTABLE}")

# generate git/build information
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REF_SPEC GIT_REV)
git_describe(GIT_DESC --always --long --dirty)
git_branch_name(GIT_BRANCH)
get_timestamp(BUILD_DATE)

# Generate cpp with Git revision from template
# Also if this is a CI build, add the build name (ie: Nightly, Canary) to the scm_rev file as well
set(REPO_NAME "")
set(BUILD_VERSION "0")
if (DEFINED ENV{CI})
  if (DEFINED ENV{TRAVIS})
    set(BUILD_REPOSITORY $ENV{TRAVIS_REPO_SLUG})
    set(BUILD_TAG $ENV{TRAVIS_TAG})
  elseif(DEFINED ENV{APPVEYOR})
    set(BUILD_REPOSITORY $ENV{APPVEYOR_REPO_NAME})
    set(BUILD_TAG $ENV{APPVEYOR_REPO_TAG_NAME})
  elseif(DEFINED ENV{BITRISE_IO})
    set(BUILD_REPOSITORY "$ENV{BITRISEIO_GIT_REPOSITORY_OWNER}/$ENV{BITRISEIO_GIT_REPOSITORY_SLUG}")
    set(BUILD_TAG $ENV{BITRISE_GIT_TAG})
  endif()

  # regex capture the string nightly or canary into CMAKE_MATCH_1
  string(REGEX MATCH "citra-emu/citra-?(.*)" OUTVAR ${BUILD_REPOSITORY})
  if ("${CMAKE_MATCH_COUNT}" GREATER 0)
    # capitalize the first letter of each word in the repo name.
    string(REPLACE "-" ";" REPO_NAME_LIST ${CMAKE_MATCH_1})
    foreach(WORD ${REPO_NAME_LIST})
      string(SUBSTRING ${WORD} 0 1 FIRST_LETTER)
      string(SUBSTRING ${WORD} 1 -1 REMAINDER)
      string(TOUPPER ${FIRST_LETTER} FIRST_LETTER)
      set(REPO_NAME "${REPO_NAME}${FIRST_LETTER}${REMAINDER}")
    endforeach()
    if (BUILD_TAG)
      string(REGEX MATCH "${CMAKE_MATCH_1}-([0-9]+)" OUTVAR ${BUILD_TAG})
      if (${CMAKE_MATCH_COUNT} GREATER 0)
        set(BUILD_VERSION ${CMAKE_MATCH_1})
      endif()
      if (BUILD_VERSION)
        # This leaves a trailing space on the last word, but we actually want that
        # because of how it's styled in the title bar.
        set(BUILD_FULLNAME "${REPO_NAME} ${BUILD_VERSION} ")
      else()
        set(BUILD_FULLNAME "")
      endif()
    endif()
  endif()
endif()

# The variable SRC_DIR must be passed into the script (since it uses the current build directory for all values of CMAKE_*_DIR)
set(VIDEO_CORE "${SRC_DIR}/src/video_core")
set(HASH_FILES
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
)
set(COMBINED "")
foreach (F IN LISTS HASH_FILES)
    file(READ ${F} TMP)
    set(COMBINED "${COMBINED}${TMP}")
endforeach()
string(MD5 SHADER_CACHE_VERSION "${COMBINED}")
configure_file("${SRC_DIR}/src/common/scm_rev.cpp.in" "scm_rev.cpp" @ONLY)
