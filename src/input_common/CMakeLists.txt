set(SRCS
            analog_from_button.cpp
            keyboard.cpp
            main.cpp
            motion_emu.cpp
            )

set(HEADERS
            analog_from_button.h
            keyboard.h
            main.h
            motion_emu.h
            )

if(SDL2_FOUND)
    set(SRCS ${SRCS} sdl/sdl.cpp)
    set(HEADERS ${HEADERS} sdl/sdl.h)
endif()

create_directory_groups(${SRCS} ${HEADERS})

add_library(input_common STATIC ${SRCS} ${HEADERS})
target_link_libraries(input_common PUBLIC core PRIVATE common)

if(SDL2_FOUND)
    target_link_libraries(input_common PRIVATE SDL2)
    target_compile_definitions(input_common PRIVATE HAVE_SDL2)
endif()
