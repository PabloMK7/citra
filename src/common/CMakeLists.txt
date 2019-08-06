# Generate cpp with Git revision from template
# Also if this is a CI build, add the build name (ie: Nightly, Canary) to the scm_rev file as well
set(REPO_NAME "")
set(BUILD_VERSION "0")
if ($ENV{CI})
  if ($ENV{TRAVIS})
    set(BUILD_REPOSITORY $ENV{TRAVIS_REPO_SLUG})
    set(BUILD_TAG $ENV{TRAVIS_TAG})
  elseif($ENV{APPVEYOR})
    set(BUILD_REPOSITORY $ENV{APPVEYOR_REPO_NAME})
    set(BUILD_TAG $ENV{APPVEYOR_REPO_TAG_NAME})
  elseif($ENV{BITRISE_IO})
    set(BUILD_REPOSITORY "$ENV{BITRISEIO_GIT_REPOSITORY_OWNER}/$ENV{BITRISEIO_GIT_REPOSITORY_SLUG}")
    set(BUILD_TAG $ENV{BITRISE_GIT_TAG})
  endif()
  # regex capture the string nightly or canary into CMAKE_MATCH_1
  string(REGEX MATCH "citra-emu/citra-?(.*)" OUTVAR ${BUILD_REPOSITORY})
  if (${CMAKE_MATCH_COUNT} GREATER 0)
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
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp" @ONLY)

add_library(common STATIC
    alignment.h
    announce_multiplayer_room.h
    assert.h
    detached_tasks.cpp
    detached_tasks.h
    bit_field.h
    bit_set.h
    chunk_file.h
    cityhash.cpp
    cityhash.h
    color.h
    common_funcs.h
    common_paths.h
    common_types.h
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

target_link_libraries(common PUBLIC fmt microprofile)
if (ARCHITECTURE_x86_64)
    target_link_libraries(common PRIVATE xbyak)
endif()
