add_library(audio_core STATIC
    audio_types.h
    codec.cpp
    codec.h
    dsp_interface.cpp
    dsp_interface.h
    hle/common.h
    hle/filter.cpp
    hle/filter.h
    hle/hle.cpp
    hle/hle.h
    hle/mixers.cpp
    hle/mixers.h
    hle/shared_memory.h
    hle/source.cpp
    hle/source.h
    interpolate.cpp
    interpolate.h
    null_sink.h
    sink.h
    sink_details.cpp
    sink_details.h
    time_stretch.cpp
    time_stretch.h

    $<$<BOOL:${SDL2_FOUND}>:sdl2_sink.cpp sdl2_sink.h>
)

create_target_directory_groups(audio_core)

target_link_libraries(audio_core PUBLIC common core)
target_link_libraries(audio_core PRIVATE SoundTouch)

if(SDL2_FOUND)
    target_link_libraries(audio_core PRIVATE SDL2)
    target_compile_definitions(audio_core PRIVATE HAVE_SDL2)
endif()
