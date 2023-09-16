if(NOT SOUNDTOUCH_FOUND)
    pkg_check_modules(SOUNDTOUCH_TMP soundtouch)
    
    find_path(SOUNDTOUCH_INCLUDE_DIRS NAMES SoundTouch.h
        PATHS
        ${SOUNDTOUCH_TMP_INCLUDE_DIRS}
        /usr/include/soundtouch
        /usr/include
        /usr/local/include/soundtouch
        /usr/local/include
    )
    
    find_library(SOUNDTOUCH_LIBRARIES NAMES SoundTouch
        PATHS
        ${SOUNDTOUCH_TMP_LIBRARY_DIRS}
        /usr/lib
        /usr/local/lib
    )
    
    if(SOUNDTOUCH_INCLUDE_DIRS AND SOUNDTOUCH_LIBRARIES)
        set(SOUNDTOUCH_FOUND TRUE CACHE INTERNAL "SoundTouch found")
        message(STATUS "Found SoundTouch: ${SOUNDTOUCH_INCLUDE_DIRS}, ${SOUNDTOUCH_LIBRARIES}")
    else()
        set(SOUNDTOUCH_FOUND FALSE CACHE INTERNAL "SoundTouch found")
        message(STATUS "SoundTouch not found.")
    endif()
endif()
