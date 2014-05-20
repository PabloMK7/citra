configure_file("${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp.in" "${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp" @ONLY)

set(SRCS    break_points.cpp
            console_listener.cpp
            extended_trace.cpp
            file_search.cpp
            file_util.cpp
            hash.cpp
            log_manager.cpp
            math_util.cpp
            mem_arena.cpp
            memory_util.cpp
            misc.cpp
            msg_handler.cpp
            string_util.cpp
            scm_rev.cpp
            symbols.cpp
            thread.cpp
            timer.cpp
            utf8.cpp)

set(HEADERS atomic.h
            atomic_gcc.h
            atomic_win32.h
            bit_field.h
            break_points.h
            chunk_file.h
            common_funcs.h
            common_paths.h
            common_types.h
            common.h
            console_listener.h
            cpu_detect.h
            debug_interface.h
            emu_window.h
            extended_trace.h
            fifo_queue.h
            file_search.h
            file_util.h
            hash.h
            linear_disk_cache.h
            log_manager.h
            log.h
            math_util.h
            mem_arena.h
            memory_util.h
            msg_handler.h
            platform.h
            scm_rev.h
            std_condition_variable.h
            std_mutex.h
            std_thread.h
            string_util.h
            swap.h
            symbols.h
            thread.h
            thunk.h
            timer.h
            utf8.h)

add_library(common STATIC ${SRCS} ${HEADERS})
