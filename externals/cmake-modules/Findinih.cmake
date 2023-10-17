if(NOT inih_FOUND)
    # Inih includes both a base library and INIReader
    # We must link against both to avoid linker errors.

    pkg_check_modules(INIR_TEMP INIReader)
    pkg_check_modules(INIH_TEMP inih)
    
    
    find_path(INIR_INCLUDE_DIR NAMES INIReader.h
            PATHS
            ${INIR_TEMP_INCLUDE_DIRS}
            /usr/include
            /usr/local/include
            )
            
    find_path(INIH_INCLUDE_DIR NAMES ini.h
            PATHS
            ${INIH_TEMP_INCLUDE_DIRS}
            /usr/include
            /use/local/include
            )
            
    find_library(INIR_LIBRARIES NAMES INIReader
            PATHS
            ${INIR_TEMP_LIBRARY_DIRS}
            /usr/lib
            /usr/local/lib
            )
            
    find_library(INIH_LIBRARIES NAMES inih
            PATHS
            ${INIH_TEMP_LIBRARY_DIRS}
            /usr/lib
            /usr/local/lib
            )
            
    if(INIR_INCLUDE_DIR AND INIH_INCLUDE_DIR AND INIR_LIBRARIES AND INIH_LIBRARIES)
        set(inih_FOUND TRUE CACHE INTERNAL "Found inih library")
        message(STATUS "Found inih ${INIR_INCLUDE_DIR} ${INIH_INCLUDE_DIR} ${INIR_LIBRARIES} ${INIH_LIBRARIES}")
    else()
        set(inih_FOUND FALSE CACHE INTERNAL "Found inih library")
        message(STATUS "Inih not found.")
    endif()
    
endif()

if(inih_FOUND AND NOT TARGET inih::inir OR NOT TARGET inih::inih)
    add_library(inih::inir INTERFACE IMPORTED)
    set_target_properties(inih::inir PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${INIR_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${INIR_LIBRARIES}"
        IMPORTED_LOCATION "${INIR_LIBRARIES}"
        )

    add_library(inih::inih INTERFACE IMPORTED)
    set_target_properties(inih::inih PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${INIH_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${INIH_LIBRARES}"
        IMPORTED_LOCATION ${INIH_LIBRARIES}
        )

endif()
    