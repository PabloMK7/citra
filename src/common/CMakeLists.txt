# Generate cpp with Git revision from template
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp" @ONLY)

set(SRCS
            break_points.cpp
            console_listener.cpp
            extended_trace.cpp
            file_search.cpp
            file_util.cpp
            hash.cpp
            key_map.cpp
            log_manager.cpp
            math_util.cpp
            mem_arena.cpp
            memory_util.cpp
            misc.cpp
            msg_handler.cpp
            scm_rev.cpp
            string_util.cpp
            symbols.cpp
            thread.cpp
            timer.cpp
            utf8.cpp
            )

set(HEADERS
            bit_field.h
            break_points.h
            chunk_file.h
            common.h
            common_funcs.h
            common_paths.h
            common_types.h
            console_listener.h
            cpu_detect.h
            debug_interface.h
            emu_window.h
            extended_trace.h
            fifo_queue.h
            file_search.h
            file_util.h
            hash.h
            key_map.h
            linear_disk_cache.h
            log.h
            log_manager.h
            math_util.h
            mem_arena.h
            memory_util.h
            msg_handler.h
            platform.h
            scm_rev.h
            string_util.h
            swap.h
            symbols.h
            thread.h
            thread_queue_list.h
            thunk.h
            timer.h
            utf8.h
            )

create_directory_groups(${SRCS} ${HEADERS})

add_library(common STATIC ${SRCS} ${HEADERS})
