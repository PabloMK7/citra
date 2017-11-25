# Generate cpp with Git revision from template
# Also if this is a CI build, add the build name (ie: Nightly, Bleeding Edge) to the scm_rev file as well
set(REPO_NAME "")
if ($ENV{CI})
  if ($ENV{TRAVIS})
    set(BUILD_REPOSITORY $ENV{TRAVIS_REPO_SLUG})
  elseif($ENV{APPVEYOR})
    set(BUILD_REPOSITORY $ENV{APPVEYOR_REPO_NAME})
  endif()
  # regex capture the string nightly or bleeding-edge into CMAKE_MATCH_1
  string(REGEX MATCH "citra-emu/citra-?(.*)" OUTVAR ${BUILD_REPOSITORY})
  if (${CMAKE_MATCH_COUNT} GREATER 0)
    # capitalize the first letter of each word in the repo name.
    string(REPLACE "-" ";" REPO_NAME_LIST ${CMAKE_MATCH_1})
    foreach(WORD ${REPO_NAME_LIST})
      string(SUBSTRING ${WORD} 0 1 FIRST_LETTER)
      string(SUBSTRING ${WORD} 1 -1 REMAINDER)
      string(TOUPPER ${FIRST_LETTER} FIRST_LETTER)
      # this leaves a trailing space on the last word, but we actually want that
      # because of how its styled in the title bar.
      set(REPO_NAME "${REPO_NAME}${FIRST_LETTER}${REMAINDER} ")
    endforeach()
  endif()
endif()
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp" @ONLY)

set(SRCS
            break_points.cpp
            file_util.cpp
            hash.cpp
            logging/filter.cpp
            logging/text_formatter.cpp
            logging/backend.cpp
            memory_util.cpp
            microprofile.cpp
            misc.cpp
            param_package.cpp
            scm_rev.cpp
            string_util.cpp
            telemetry.cpp
            thread.cpp
            timer.cpp
            )

set(HEADERS
            alignment.h
            assert.h
            bit_field.h
            bit_set.h
            break_points.h
            chunk_file.h
            code_block.h
            color.h
            common_funcs.h
            common_paths.h
            common_types.h
            file_util.h
            hash.h
            linear_disk_cache.h
            logging/text_formatter.h
            logging/filter.h
            logging/log.h
            logging/backend.h
            math_util.h
            memory_util.h
            microprofile.h
            microprofileui.h
            param_package.h
            platform.h
            quaternion.h
            scm_rev.h
            scope_exit.h
            string_util.h
            swap.h
            synchronized_wrapper.h
            telemetry.h
            thread.h
            thread_queue_list.h
            threadsafe_queue.h
            timer.h
            vector_math.h
            )

if(ARCHITECTURE_x86_64)
    set(SRCS ${SRCS}
            x64/cpu_detect.cpp
            )

    set(HEADERS ${HEADERS}
            x64/cpu_detect.h
            x64/xbyak_abi.h
            x64/xbyak_util.h
            )
endif()

create_directory_groups(${SRCS} ${HEADERS})

add_library(common STATIC ${SRCS} ${HEADERS})
target_link_libraries(common PUBLIC Boost::boost microprofile)
if (ARCHITECTURE_x86_64)
    target_link_libraries(common PRIVATE xbyak)
endif()
