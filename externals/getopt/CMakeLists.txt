set(SRCS
            getopt.c
            )
set(HEADERS
            getopt.h
            )

create_directory_groups(${SRCS} ${HEADERS})
add_library(getopt ${SRCS} ${HEADERS})
target_compile_definitions(getopt PUBLIC STATIC_GETOPT)
target_include_directories(getopt INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
