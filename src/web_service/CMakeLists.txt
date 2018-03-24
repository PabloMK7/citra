add_library(web_service STATIC
    announce_room_json.cpp
    announce_room_json.h
    telemetry_json.cpp
    telemetry_json.h
    verify_login.cpp
    verify_login.h
    web_backend.cpp
    web_backend.h
)

create_target_directory_groups(web_service)

get_directory_property(OPENSSL_LIBS
        DIRECTORY ${CMAKE_SOURCE_DIR}/externals/libressl
        DEFINITION OPENSSL_LIBS)
add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(web_service PUBLIC common json-headers ${OPENSSL_LIBS} httplib lurlparser)
