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
            thread.cpp
            timer.cpp
            utf8.cpp
            version.cpp)

add_library(common STATIC ${SRCS})
