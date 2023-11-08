if(NOT LODEPNG_FOUND)
    find_path(LODEPNG_INCLUDE_DIRS NAMES lodepng.h
            PATHS
            /usr/include
            /usr/local/include
            )

    find_library(LODEPNG_LIBRARY_DIRS NAMES lodepng
            PATHS
            /usr/lib
            /usr/local/lib
            )

    if(LODEPNG_INCLUDE_DIRS AND LODEPNG_LIBRARY_DIRS)
        set(LODEPNG_FOUND TRUE CACHE INTERNAL "Found lodepng")
        message(STATUS "Found lodepng: ${LODEPNG_LIBRARY_DIRS}, ${LODEPNG_INCLUDE_DIRS}")
    else()
        set(LODEPNG_FOUND FALSE CACHE INTERNAL "Found lodepng")
        message(STATUS "Lodepng not found.")
    endif()
endif()

if(LODEPNG_FOUND AND NOT TARGET lodepng::lodepng)
    add_library(lodepng::lodepng UNKNOWN IMPORTED)
    set_target_properties(lodepng::lodepng PROPERTIES
        INCLUDE_DIRECTORIES ${LODEPNG_INCLUDE_DIRS}
        INTERFACE_LINK_LIBRARIES ${LODEPNG_LIBRARY_DIRS}
        IMPORTED_LOCATION ${LODEPNG_LIBRARY_DIRS}
        )

endif()
