if(NOT OPENAL_FOUND)
    pkg_check_modules(OPENAL_TMP openal)

    find_path(OPENAL_INCLUDE_DIRS NAMES al.h
            PATHS
            ${OPENAL_TMP_INCLUDE_DIRS}
            /usr/include/AL
            /usr/include
            /usr/local/include/AL
            /usr/local/include
            )

    find_library(OPENAL_LIBRARY_DIRS NAMES openal
            PATHS
            ${OPENAL_TMP_LIBRARY_DIRS}
            /usr/lib
            /usr/local/lib
            )

    if(OPENAL_INCLUDE_DIRS AND OPENAL_LIBRARY_DIRS)
        set(OPENAL_FOUND TRUE CACHE INTERNAL "OpenAL found")
        message(STATUS "Found OpenAL: ${OPENAL_LIBRARY_DIRS}, ${OPENAL_INCLUDE_DIRS}")
    else()
        set(OPENAL_FOUND FALSE CACHE INTERNAL "OpenAL found")
        message(STATUS "OpenAL not found.")
    endif()
endif()

if(OPENAL_FOUND AND NOT TARGET OpenAL::OpenAL)
    add_library(OpenAL::OpenAL UNKNOWN IMPORTED)
    set_target_properties(OpenAL::OpenAL PROPERTIES
            INCLUDE_DIRECTORIES ${OPENAL_INCLUDE_DIRS}
            INTERFACE_LINK_LIBRARIES ${OPENAL_LIBRARY_DIRS}
            IMPORTED_LOCATION ${OPENAL_LIBRARY_DIRS}
            )
endif()
