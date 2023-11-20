if(NOT CRYPTOPP_FOUND)
    pkg_search_module(CRYPTOPP_TMP crypto++ cryptopp)

    find_path(CRYPTOPP_INCLUDE_DIRS NAMES cryptlib.h
            PATHS
            ${CRYPTOPP_TMP_INCLUDE_DIRS}
            /usr/include
            /usr/local/include
            PATH_SUFFIXES crypto++ cryptopp
            )

    find_library(CRYPTOPP_LIBRARY_DIRS NAMES crypto++ cryptopp
            PATHS
            ${CRYPTOPP_TMP_LIBRARY_DIRS}
            /usr/lib
            /usr/local/lib
            )

    if(CRYPTOPP_INCLUDE_DIRS AND CRYPTOPP_LIBRARY_DIRS)
        set(CRYPTOPP_FOUND TRUE CACHE INTERNAL "Found cryptopp")
        message(STATUS "Found cryptopp: ${CRYPTOPP_LIBRARY_DIRS}, ${CRYPTOPP_INCLUDE_DIRS}")
    else()
        set(CRYPTOPP_FOUND FALSE CACHE INTERNAL "Found cryptopp")
        message(STATUS "Cryptopp not found.")
    endif()
endif()

if(CRYPTOPP_FOUND AND NOT TARGET cryptopp::cryptopp)
    add_library(cryptopp::cryptopp UNKNOWN IMPORTED)
    set_target_properties(cryptopp::cryptopp PROPERTIES
        INCLUDE_DIRECTORIES ${CRYPTOPP_INCLUDE_DIRS}
        INTERFACE_LINK_LIBRARIES ${CRYPTOPP_LIBRARY_DIRS}
        IMPORTED_LOCATION ${CRYPTOPP_LIBRARY_DIRS}
        )
endif()
