add_library(input_common STATIC
    analog_from_button.cpp
    analog_from_button.h
    keyboard.cpp
    keyboard.h
    main.cpp
    main.h
    motion_emu.cpp
    motion_emu.h
    sdl/sdl.cpp
    sdl/sdl.h
    udp/client.cpp
    udp/client.h
    udp/protocol.cpp
    udp/protocol.h
    udp/udp.cpp
    udp/udp.h
)

if(SDL2_FOUND)
    target_sources(input_common PRIVATE
        sdl/sdl_impl.cpp
        sdl/sdl_impl.h
    )
    target_link_libraries(input_common PRIVATE SDL2)
    target_compile_definitions(input_common PRIVATE HAVE_SDL2)
endif()

create_target_directory_groups(input_common)
target_link_libraries(input_common PUBLIC core PRIVATE common ${Boost_LIBRARIES})
