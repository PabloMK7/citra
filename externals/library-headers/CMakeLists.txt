add_library(library-headers INTERFACE)

# libfdk-aac headers
find_path(FDK_AAC_INCLUDES NAMES fdk-aac/aacdecoder_lib.h)
if (FDK_AAC_INCLUDES STREQUAL "FDK_AAC_INCLUDES-NOTFOUND")
    message(STATUS "fdk_aac headers not found, using bundled headers.")
    target_include_directories(library-headers INTERFACE ./library-headers/fdk-aac/include)
else()
    message(STATUS "Using headers from system fdk_aac")
    target_include_directories(library-headers SYSTEM INTERFACE ${FDK_AAC_INCLUDES})
endif()

# FFmpeg headers
find_path(AVUTIL_INCLUDES NAMES libavutil/avutil.h)
find_path(AVCODEC_INCLUDES NAMES libavcodec/avcodec.h)
find_path(AVFORMAT_INCLUDES NAMES libavformat/avformat.h)
find_path(AVFILTER_INCLUDES NAMES libavfilter/avfilter.h)
find_path(SWRESAMPLE_INCLUDES NAMES libswresample/swresample.h)
# Make sure all the headers are found and from the same place, so we don't
# have missing or mismatched components.
if (AVUTIL_INCLUDES STREQUAL "AVUTIL_INCLUDES-NOTFOUND"
    OR NOT AVCODEC_INCLUDES STREQUAL AVUTIL_INCLUDES
    OR NOT AVFORMAT_INCLUDES STREQUAL AVUTIL_INCLUDES
    OR NOT AVFILTER_INCLUDES STREQUAL AVUTIL_INCLUDES
    OR NOT SWRESAMPLE_INCLUDES STREQUAL AVUTIL_INCLUDES)
    message(STATUS "Complete FFmpeg headers not found, using bundled headers.")
    target_include_directories(library-headers INTERFACE ./library-headers/ffmpeg/include)
else()
    # Extract libavutil version from header.
    file(STRINGS "${AVUTIL_INCLUDES}/libavutil/version.h" AVUTIL_VERSION_DATA
        REGEX "#define LIBAVUTIL_VERSION_(MAJOR|MINOR|MICRO) ")
    set(AVUTIL_VERSION_REGEX "([0-9]+)")
    foreach(v MAJOR MINOR MICRO)
        if("${AVUTIL_VERSION_DATA}" MATCHES "#define LIBAVUTIL_VERSION_${v}[\\t ]+${AVUTIL_VERSION_REGEX}")
            set(AVUTIL_VERSION_${v} "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    set(AVUTIL_VERSION "${AVUTIL_VERSION_MAJOR}.${AVUTIL_VERSION_MINOR}.${AVUTIL_VERSION_MICRO}")

    message(STATUS "Detected FFmpeg libavutil version is ${AVUTIL_VERSION}")
    if ("${AVUTIL_VERSION}" VERSION_LESS "56.30.100")
        message(WARNING "System FFmpeg version is too low (< 4.2), using bundled headers.")
        target_include_directories(library-headers INTERFACE ./library-headers/ffmpeg/include)
    else()
        message(STATUS "Using headers from system FFmpeg")
        target_include_directories(library-headers SYSTEM INTERFACE ${AVUTIL_INCLUDES})
    endif()
endif()
