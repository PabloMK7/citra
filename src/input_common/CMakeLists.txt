set(SRCS
            analog_from_button.cpp
            keyboard.cpp
            main.cpp
            )

set(HEADERS
            analog_from_button.h
            keyboard.h
            main.h
            )

create_directory_groups(${SRCS} ${HEADERS})

add_library(input_common STATIC ${SRCS} ${HEADERS})
target_link_libraries(input_common common core)

