# Generate cpp with Git revision from template
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp" @ONLY)

set(SRCS
            break_points.cpp
            emu_window.cpp
            file_util.cpp
            framebuffer_layout.cpp
            hash.cpp
            key_map.cpp
            logging/filter.cpp
            logging/text_formatter.cpp
            logging/backend.cpp
            memory_util.cpp
            microprofile.cpp
            misc.cpp
            profiler.cpp
            scm_rev.cpp
            string_util.cpp
            symbols.cpp
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
            emu_window.h
            file_util.h
            framebuffer_layout.h
            hash.h
            key_map.h
            linear_disk_cache.h
            logging/text_formatter.h
            logging/filter.h
            logging/log.h
            logging/backend.h
            math_util.h
            memory_util.h
            microprofile.h
            microprofileui.h
            platform.h
            profiler_reporting.h
            scm_rev.h
            scope_exit.h
            string_util.h
            swap.h
            symbols.h
            synchronized_wrapper.h
            thread.h
            thread_queue_list.h
            timer.h
            vector_math.h
            )

if(ARCHITECTURE_x86_64)
    set(SRCS ${SRCS}
            x64/abi.cpp
            x64/cpu_detect.cpp
            x64/emitter.cpp)

    set(HEADERS ${HEADERS}
            x64/abi.h
            x64/cpu_detect.h
            x64/emitter.h
            x64/xbyak_abi.h
            x64/xbyak_util.h
            )
endif()

create_directory_groups(${SRCS} ${HEADERS})

add_library(common STATIC ${SRCS} ${HEADERS})
if (ARCHITECTURE_x86_64)
    target_link_libraries(common xbyak)
endif()
