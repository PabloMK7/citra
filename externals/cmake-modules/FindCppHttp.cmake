if(NOT CppHttp_FOUND)
    pkg_check_modules(CPP_HTTPLIB cpp-httplib)

    if (CPP_HTTPLIB_FOUND)
        find_path(HTTPLIB_INCLUDE_DIR NAMES httplib.h
                PATHS
                ${CPP_HTTPLIB_INCLUDE_DIRS}
                /usr/include
                /usr/local/include
            )

        find_library(HTTPLIB_LIBRARY NAMES cpp-httplib
                PATHS
                ${CPP_HTTPLIB_LIBRARY_DIRS}
                /usr/lib
                /usr/local/lib
            )

        set(HTTPLIB_VERSION ${CPP_HTTPLIB_VERSION})

        if (NOT TARGET cpp-httplib::cpp-httplib)
            add_library(cpp-httplib::cpp-httplib INTERFACE IMPORTED)
            set_target_properties(cpp-httplib::cpp-httplib PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${HTTPLIB_INCLUDE_DIR}"
                INTERFACE_LINK_LIBRARIES "${HTTPLIB_LIBRARY}"
                IMPORTED_LOCATION "${HTTPLIB_LIBRARY}"
            )
            add_library(httplib::httplib ALIAS cpp-httplib::cpp-httplib)
        endif()
    else()
        message(STATUS "Cpp-httplib not found via pkg-config, trying CMake...")
        find_package(httplib)
    endif()

    find_package_handle_standard_args(CppHttp REQUIRED_VARS HTTPLIB_INCLUDE_DIR HTTPLIB_LIBRARY VERSION_VAR HTTPLIB_VERSION)
	
endif()

if(CppHttp_FOUND AND NOT TARGET cpp-httplib::cpp-httplib)
    add_library(cpp-httplib::cpp-httplib INTERFACE IMPORTED)
    set_target_properties(cpp-httplib::cpp-httplib PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CPP-HTTP_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${CPP-HTTP_LIBRARIES}"
        IMPORTED_LOCATION "${CPP-HTTP_LIBRARIES}"
    )
endif()
