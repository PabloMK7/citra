set(SRCS
            audio_core.cpp
            codec.cpp
            hle/dsp.cpp
            hle/filter.cpp
            hle/pipe.cpp
            hle/source.cpp
            interpolate.cpp
            sink_details.cpp
            time_stretch.cpp
            )

set(HEADERS
            audio_core.h
            codec.h
            hle/common.h
            hle/dsp.h
            hle/filter.h
            hle/pipe.h
            hle/source.h
            interpolate.h
            null_sink.h
            sink.h
            sink_details.h
            time_stretch.h
            )

include_directories(../../externals/soundtouch/include)

if(SDL2_FOUND)
    set(SRCS ${SRCS} sdl2_sink.cpp)
    set(HEADERS ${HEADERS} sdl2_sink.h)
    include_directories(${SDL2_INCLUDE_DIR})
endif()

create_directory_groups(${SRCS} ${HEADERS})

add_library(audio_core STATIC ${SRCS} ${HEADERS})
target_link_libraries(audio_core SoundTouch)

if(SDL2_FOUND)
    target_link_libraries(audio_core ${SDL2_LIBRARY})
    set_property(TARGET audio_core APPEND PROPERTY COMPILE_DEFINITIONS HAVE_SDL2)
endif()
