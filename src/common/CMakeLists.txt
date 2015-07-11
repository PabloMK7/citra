# Generate cpp with Git revision from template
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp" @ONLY)

set(SRCS
            break_points.cpp
            emu_window.cpp
            file_util.cpp
            key_map.cpp
            logging/filter.cpp
            logging/text_formatter.cpp
            logging/backend.cpp
            memory_util.cpp
            misc.cpp
            profiler.cpp
            scm_rev.cpp
            string_util.cpp
            symbols.cpp
            thread.cpp
            timer.cpp
            )

set(HEADERS
            assert.h
            bit_field.h
            break_points.h
            chunk_file.h
            color.h
            common_funcs.h
            common_paths.h
            common_types.h
            cpu_detect.h
            debug_interface.h
            emu_window.h
            file_util.h
            key_map.h
            linear_disk_cache.h
            logging/text_formatter.h
            logging/filter.h
            logging/log.h
            logging/backend.h
            make_unique.h
            math_util.h
            memory_util.h
            platform.h
            profiler.h
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

create_directory_groups(${SRCS} ${HEADERS})

add_library(common STATIC ${SRCS} ${HEADERS})
