add_library(audio_core STATIC
    audio_types.h
    codec.cpp
    codec.h
    dsp_interface.cpp
    dsp_interface.h
    hle/adts.h
    hle/adts_reader.cpp
    hle/common.h
    hle/decoder.cpp
    hle/decoder.h
    hle/filter.cpp
    hle/filter.h
    hle/hle.cpp
    hle/hle.h
    hle/mixers.cpp
    hle/mixers.h
    hle/shared_memory.h
    hle/source.cpp
    hle/source.h
    lle/lle.cpp
    lle/lle.h
    interpolate.cpp
    interpolate.h
    null_sink.h
    sink.h
    sink_details.cpp
    sink_details.h
    time_stretch.cpp
    time_stretch.h

    $<$<BOOL:${SDL2_FOUND}>:sdl2_sink.cpp sdl2_sink.h>
    $<$<BOOL:${ENABLE_CUBEB}>:cubeb_sink.cpp cubeb_sink.h cubeb_input.cpp cubeb_input.h>
)

create_target_directory_groups(audio_core)

target_link_libraries(audio_core PUBLIC common)
target_link_libraries(audio_core PRIVATE SoundTouch teakra)

if(ENABLE_MF)
    target_sources(audio_core PRIVATE
        hle/wmf_decoder.cpp
        hle/wmf_decoder.h
        hle/wmf_decoder_utils.cpp
        hle/wmf_decoder_utils.h
    )
    # We dynamically load the required symbols from mf.dll and mfplat.dll but mfuuid is not a dll
    # just a static library of GUIDS so include that one directly.
    target_link_libraries(audio_core PRIVATE mfuuid.lib)
    target_compile_definitions(audio_core PUBLIC HAVE_MF)
elseif(ENABLE_FFMPEG_AUDIO_DECODER)
    target_sources(audio_core PRIVATE
        hle/ffmpeg_decoder.cpp
        hle/ffmpeg_decoder.h
        hle/ffmpeg_dl.cpp
        hle/ffmpeg_dl.h
    )
    if(UNIX)
        target_link_libraries(audio_core PRIVATE FFmpeg::avcodec)
    else()
        target_include_directories(audio_core PRIVATE ${FFMPEG_DIR}/include)
    endif()
    target_compile_definitions(audio_core PUBLIC HAVE_FFMPEG)
elseif(ENABLE_FDK)
    target_sources(audio_core PRIVATE
        hle/fdk_decoder.cpp
        hle/fdk_decoder.h
    )
    target_link_libraries(audio_core PRIVATE ${FDK_AAC})
    target_compile_definitions(audio_core PUBLIC HAVE_FDK)
endif()

if(ANDROID)
    target_sources(audio_core PRIVATE
        hle/mediandk_decoder.cpp
        hle/mediandk_decoder.h
    )
    target_link_libraries(audio_core PRIVATE mediandk)
endif()

if(SDL2_FOUND)
    target_link_libraries(audio_core PRIVATE SDL2)
    target_compile_definitions(audio_core PRIVATE HAVE_SDL2)
endif()

if(ENABLE_CUBEB)
    target_link_libraries(audio_core PRIVATE cubeb)
    target_compile_definitions(audio_core PUBLIC HAVE_CUBEB)
endif()
