set(SRCS	src/break_points.cpp
			src/console_listener.cpp
			src/extended_trace.cpp
			src/file_search.cpp
			src/file_util.cpp
			src/hash.cpp
			src/log_manager.cpp
			src/math_util.cpp
			src/mem_arena.cpp
			src/memory_util.cpp
			src/misc.cpp
			src/msg_handler.cpp
			src/string_util.cpp
			src/thread.cpp
			src/timer.cpp
			src/version.cpp)

add_library(common STATIC ${SRCS})
