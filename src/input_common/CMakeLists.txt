set(SRCS
            keyboard.cpp
            main.cpp
            )

set(HEADERS
            keyboard.h
            main.h
            )

create_directory_groups(${SRCS} ${HEADERS})

add_library(input_common STATIC ${SRCS} ${HEADERS})
target_link_libraries(input_common common core)

