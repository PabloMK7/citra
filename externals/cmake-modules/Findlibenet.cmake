if(NOT libenet_FOUND)
    pkg_check_modules(ENET_TMP libenet)

    find_path(libenet_INCLUDE_DIRS NAMES enet.h PATH_SUFFIXES enet
            PATHS
            ${ENET_TMP_INCLUDE_DIRS}
            /usr/include
            /usr/local/include
            )

    find_library(libenet_LIBRARY_DIRS NAMES enet
            PATHS
            ${ENET_TMP_LIBRARY_DIRS}
            /usr/lib
            /usr/local/lib
            )

    if(libenet_INCLUDE_DIRS AND libenet_LIBRARY_DIRS)
        set(libenet_FOUND TRUE CACHE INTERNAL "Found libenet")
        message(STATUS "Found libenet ${libenet_LIBRARY_DIRS}, ${libenet_INCLUDE_DIRS}")
    else()
        set(libenet_FOUND FALSE CACHE INTERNAL "Found libenet")
        message(STATUS "Libenet not found.")
    endif()
endif()

if(libenet_FOUND AND NOT TARGET libenet::libenet)
    add_library(libenet::libenet UNKNOWN IMPORTED)
    set_target_properties(libenet::libenet PROPERTIES
            INCLUDE_DIRECTORIES ${libenet_INCLUDE_DIRS}
            INTERFACE_LINK_LIBRARIES ${libenet_LIBRARY_DIRS}
            IMPORTED_LOCATION ${libenet_LIBRARY_DIRS}
            )
endif()
