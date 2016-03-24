set(SRCS
            audio_core.cpp
            codec.cpp
            hle/dsp.cpp
            hle/pipe.cpp
            )

set(HEADERS
            audio_core.h
            codec.h
            hle/dsp.h
            hle/pipe.h
            sink.h
            )

create_directory_groups(${SRCS} ${HEADERS})

add_library(audio_core STATIC ${SRCS} ${HEADERS})