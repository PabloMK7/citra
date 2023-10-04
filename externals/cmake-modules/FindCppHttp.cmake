if(NOT CppHttp_FOUND)
    pkg_check_modules(HTTP_TMP cpp-httplib)

    find_path(CPP-HTTP_INCLUDE_DIR NAMES httplib.h
            PATHS
            ${HTTP_TMP_INCLUDE_DIRS}
            /usr/include
            /usr/local/include
        )
		
    find_library(CPP-HTTP_LIBRARIES NAMES cpp-httplib
            PATHS
            ${HTTP_TMP_LIBRARY_DIRS}
            /usr/lib
            /usr/local/lib
        )
		
    if(CPP-HTTP_INCLUDE_DIR AND CPP-HTTP_LIBRARIES)
        set(CppHttp_FOUND TRUE CACHE INTERNAL "cpp-httplib found")
        message(STATUS "Found cpp-httplib: ${CPP-HTTP_INCLUDE_DIR}, ${CPP-HTTP_LIBRARIES}")
    else()
        set(CppHttp_FOUND FALSE CACHE INTERNAL "cpp-httplib found")
        message(STATUS "Cpp-httplib not found.")
    endif()
	
endif()

if(CppHttp_FOUND AND NOT TARGET cpp-httplib::cpp-httplib)
    add_library(cpp-httplib::cpp-httplib INTERFACE IMPORTED)
    set_target_properties(cpp-httplib::cpp-httplib PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CPP-HTTP_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${CPP-HTTP_LIBRARIES}"
        IMPORTED_LOCATION "${CPP-HTTP_LIBRARIES}"
    )
endif()
