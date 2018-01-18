add_library(input_common STATIC
    analog_from_button.cpp
    analog_from_button.h
    keyboard.cpp
    keyboard.h
    main.cpp
    main.h
    motion_emu.cpp
    motion_emu.h

    $<$<BOOL:${SDL2_FOUND}>:sdl/sdl.cpp sdl/sdl.h>
)

create_target_directory_groups(input_common)

target_link_libraries(input_common PUBLIC core PRIVATE common)

if(SDL2_FOUND)
    target_link_libraries(input_common PRIVATE SDL2)
    target_compile_definitions(input_common PRIVATE HAVE_SDL2)
endif()
