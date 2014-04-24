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

add_library(common STATIC ${SRCS})
